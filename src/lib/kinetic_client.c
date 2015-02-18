/*
* kinetic-c
* Copyright (C) 2014 Seagate Technology.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#include "kinetic_types_internal.h"
#include "kinetic_client.h"
#include "kinetic_allocator.h"
#include "kinetic_session.h"
#include "kinetic_controller.h"
#include "kinetic_operation.h"
#include "kinetic_logger.h"
#include "kinetic_pdu.h"
#include "kinetic_memory.h"
#include <stdlib.h>
#include <sys/time.h>

KineticClient * KineticClient_Init(KineticClientConfig *config)
{
    KineticLogger_Init(config->logFile, config->logLevel);
    KineticClient * client = KineticCalloc(1, sizeof(*client));
    if (client == NULL) { return NULL; }

    /* Use defaults if set to 0. */
    if (config->writerThreads == 0) {
        config->writerThreads = KINETIC_CLIENT_DEFAULT_WRITER_THREADS;
    }
    if (config->readerThreads == 0) {
        config->readerThreads = KINETIC_CLIENT_DEFAULT_READER_THREADS;
    }
    if (config->maxThreadpoolThreads == 0) {
        config->maxThreadpoolThreads = KINETIC_CLIENT_DEFAULT_MAX_THREADPOOL_THREADS;
    }

    bool success = KineticPDU_InitBus(client, config);
    if (!success) {
        KineticFree(client);
        return NULL;
    }
    return client;
}

void KineticClient_Shutdown(KineticClient * const client)
{
    KineticPDU_DeinitBus(client);
    KineticFree(client);
    KineticLogger_Close();
}

KineticStatus KineticClient_CreateSession(KineticSessionConfig* const config,
    KineticClient * const client, KineticSession** session)
{
    if (config == NULL) {
        LOG0("KineticSessionConfig is NULL!");
        return KINETIC_STATUS_SESSION_INVALID;
    }

    if (session == NULL) {
        LOG0("Pointer to KineticSession pointer is NULL!");
        return KINETIC_STATUS_SESSION_EMPTY;
    }

    if (strlen(config->host) == 0) {
        LOG0("Host is empty!");
        return KINETIC_STATUS_HOST_EMPTY;
    }

    if (config->hmacKey.len < 1 || config->hmacKey.data == NULL)
    {
        LOG0("HMAC key is NULL or empty!");
        return KINETIC_STATUS_HMAC_REQUIRED;
    }

    // Create a new session
    KineticSession* s = KineticAllocator_NewSession(client->bus, config);
    if (s == NULL) {
        LOG0("Failed to create session instance!");
        return KINETIC_STATUS_MEMORY_ERROR;
    }
    KineticSession_Create(s, client);
    if (s->connection == NULL) {
        LOG0("Failed to create connection instance!");
        KineticAllocator_FreeSession(s);
        return KINETIC_STATUS_CONNECTION_ERROR;
    }

    // Establish the connection
    KineticStatus status = KineticSession_Connect(s);
    if (status != KINETIC_STATUS_SUCCESS) {
        LOGF0("Failed creating connection to %s:%d", config->host, config->port);
        s->connection = NULL;
        KineticSession_Destroy(s);
        KineticAllocator_FreeSession(s);
        return status;
    }

    *session = s;

    return status;
}

KineticStatus KineticClient_DestroySession(KineticSession* const session)
{
    if (session == NULL) {
        LOG0("KineticSession is NULL!");
        return KINETIC_STATUS_SESSION_INVALID;
    }

    if (session->connection == NULL) {
        LOG0("Connection instance is NULL!");
        return KINETIC_STATUS_CONNECTION_ERROR;
    }

    KineticStatus status = KineticSession_Disconnect(session);
    if (status != KINETIC_STATUS_SUCCESS) {LOG0("Disconnection failed!");}
    KineticSession_Destroy(session);

    return status;
}

KineticStatus KineticClient_NoOp(KineticSession const * const session)
{
    KINETIC_ASSERT(session != NULL);
    KINETIC_ASSERT(session->connection != NULL);

    KineticOperation* operation = KineticAllocator_NewOperation(session->connection);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    KineticOperation_BuildNoop(operation);
    return KineticController_ExecuteOperation(operation, NULL);
}

KineticStatus KineticClient_Put(KineticSession const * const session,
                                KineticEntry* const entry,
                                KineticCompletionClosure* closure)
{
    KINETIC_ASSERT(session != NULL);
    KINETIC_ASSERT(session->connection != NULL);
    KINETIC_ASSERT(entry != NULL);
    KINETIC_ASSERT(entry->value.array.data != NULL);
    
    KINETIC_ASSERT(session->connection->pSession == session);
    KINETIC_ASSERT(session->connection == session->connection->pSession->connection);

    KineticOperation* operation = KineticAllocator_NewOperation(session->connection);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    // Initialize request
    KineticOperation_BuildPut(operation, entry);

    // Execute the operation
    KINETIC_ASSERT(operation->connection == session->connection);
    KineticStatus res = KineticController_ExecuteOperation(operation, closure);
    return res;
}

KineticStatus KineticClient_Flush(KineticSession const * const session,
                                  KineticCompletionClosure* closure)
{
    KINETIC_ASSERT(session != NULL);
    KINETIC_ASSERT(session->connection != NULL);

    KineticOperation* operation = KineticAllocator_NewOperation(session->connection);
    if (operation == NULL) { return KINETIC_STATUS_MEMORY_ERROR; }

    // Initialize request
    KineticOperation_BuildFlush(operation);

    // Execute the operation
    return KineticController_ExecuteOperation(operation, closure);
}

static bool has_key(KineticEntry* const entry)
{
    return entry->key.array.data != NULL;
}

static bool has_value_buffer(KineticEntry* const entry)
{
    return entry->value.array.data != NULL;
}

typedef enum {
    CMD_GET,
    CMD_GET_NEXT,
    CMD_GET_PREVIOUS,
} GET_COMMAND;

static KineticStatus handle_get_command(GET_COMMAND cmd,
                                        KineticSession const * const session,
                                        KineticEntry* const entry,
                                        KineticCompletionClosure* closure)
{
    KINETIC_ASSERT(session != NULL);
    KINETIC_ASSERT(session->connection != NULL);
    KINETIC_ASSERT(entry != NULL);

    if (!has_key(entry)) {return KINETIC_STATUS_MISSING_KEY;}
    if (!has_value_buffer(entry) && !entry->metadataOnly) {
        return KINETIC_STATUS_MISSING_VALUE_BUFFER;
    }

    KineticOperation* operation = KineticAllocator_NewOperation(session->connection);
    if (operation == NULL) {
        return KINETIC_STATUS_MEMORY_ERROR;
    }

    // Initialize request
    switch (cmd)
    {
    case CMD_GET:
        KineticOperation_BuildGet(operation, entry);
        break;
    case CMD_GET_NEXT:
        KineticOperation_BuildGetNext(operation, entry);
        break;
    case CMD_GET_PREVIOUS:
        KineticOperation_BuildGetPrevious(operation, entry);
        break;
    default:
        KINETIC_ASSERT(false);
    }

    // Execute the operation
    return KineticController_ExecuteOperation(operation, closure);
}

KineticStatus KineticClient_Get(KineticSession const * const session,
                                KineticEntry* const entry,
                                KineticCompletionClosure* closure)
{
    return handle_get_command(CMD_GET, session, entry, closure);
}

KineticStatus KineticClient_GetPrevious(KineticSession const * const session,
                                        KineticEntry* const entry,
                                        KineticCompletionClosure* closure)
{
    return handle_get_command(CMD_GET_PREVIOUS, session, entry, closure);
}

KineticStatus KineticClient_GetNext(KineticSession const * const session,
                                    KineticEntry* const entry,
                                    KineticCompletionClosure* closure)
{
    return handle_get_command(CMD_GET_NEXT, session, entry, closure);
}

KineticStatus KineticClient_Delete(KineticSession const * const session,
                                   KineticEntry* const entry,
                                   KineticCompletionClosure* closure)
{
    KINETIC_ASSERT(session != NULL);
    KINETIC_ASSERT(session->connection != NULL);
    KINETIC_ASSERT(entry != NULL);

    KineticOperation* operation = KineticAllocator_NewOperation(session->connection);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    // Initialize request
    KineticOperation_BuildDelete(operation, entry);

    // Execute the operation
    return KineticController_ExecuteOperation(operation, closure);
}

KineticStatus KineticClient_GetKeyRange(KineticSession const * const session,
                                        KineticKeyRange* range,
                                        ByteBufferArray* keys,
                                        KineticCompletionClosure* closure)
{
    KINETIC_ASSERT(session != NULL);
    KINETIC_ASSERT(session->connection != NULL);
    KINETIC_ASSERT(range != NULL);
    KINETIC_ASSERT(keys != NULL);
    KINETIC_ASSERT(keys->buffers != NULL);
    KINETIC_ASSERT(keys->count > 0);

    KineticOperation* operation = KineticAllocator_NewOperation(session->connection);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    // Initialize request
    KineticOperation_BuildGetKeyRange(operation, range, keys);

    // Execute the operation
    return KineticController_ExecuteOperation(operation, closure);
}

KineticStatus KineticClient_P2POperation(KineticSession const * const session,
                                         KineticP2P_Operation* const p2pOp,
                                         KineticCompletionClosure* closure)
{
    KINETIC_ASSERT(session != NULL);
    KINETIC_ASSERT(session->connection != NULL);
    KINETIC_ASSERT(p2pOp != NULL);

    KineticOperation* operation = KineticAllocator_NewOperation(session->connection);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    // Initialize request
    KineticStatus status = KineticOperation_BuildP2POperation(operation, p2pOp);
    if (status != KINETIC_STATUS_SUCCESS) {
        // TODO we need to find a more generic way to handle errors on command construction
        if (closure != NULL) {
            operation->closure = *closure;
        }
        KineticOperation_Complete(operation, status);
        return status;
    }

    // Execute the operation
    return KineticController_ExecuteOperation(operation, closure);
}
