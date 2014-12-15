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

#ifndef _KINETIC_TYPES_INTERNAL_H
#define _KINETIC_TYPES_INTERNAL_H

#include "kinetic_types.h"
#include "kinetic_proto.h"
#include <netinet/in.h>
#include <ifaddrs.h>
#include <openssl/sha.h>
#include <time.h>
#include <pthread.h>

#define KINETIC_PDUS_PER_SESSION_DEFAULT (2)
#define KINETIC_PDUS_PER_SESSION_MAX (10)
#define KINETIC_SOCKET_DESCRIPTOR_INVALID (-1)
#define KINETIC_CONNECTION_INITIAL_STATUS_TIMEOUT_SECS (3)
#define KINETIC_OPERATION_TIMEOUT_SECS (20)

// Ensure __func__ is defined (for debugging)
#if !defined __func__
#define __func__ __FUNCTION__
#endif

// Expose normally private data for test builds to allow inspection
#ifdef TEST
#define STATIC
#else
#define STATIC static
#endif


#define NUM_ELEMENTS(ARRAY) (sizeof(ARRAY)/sizeof((ARRAY)[0]))

typedef struct _KineticPDU KineticPDU;
typedef struct _KineticOperation KineticOperation;
typedef struct _KineticConnection KineticConnection;


// Kinetic list item
typedef struct _KineticListItem KineticListItem;
struct _KineticListItem {
    KineticListItem* next;
    void* data;
};

// Kinetic list
typedef struct _KineticList {
    KineticListItem* start;
    pthread_mutex_t mutex;
    bool locked;
} KineticList;
#define KINETIC_LIST_INITIALIZER (KineticList) { \
    .mutex = PTHREAD_MUTEX_INITIALIZER, .locked = false, .start = NULL, }

// Kinetic Thread Instance
typedef struct _KineticThread {
    bool abortRequested;
    bool fatalError;
    bool paused;
    KineticConnection* connection;
} KineticThread;


// Kinetic Device Client Connection
struct _KineticConnection {
    bool            connected;      // state of connection
    int             socket;         // socket file descriptor
    pthread_mutex_t writeMutex;     // socket write mutex
    int64_t         connectionID;   // initialized to seconds since epoch
    int64_t         sequence;       // increments for each request in a session
    KineticList     pdus;           // list of dynamically allocated PDUs
    KineticList     operations;     // list of dynamically allocated operations
    KineticSession  session;        // session configuration
    KineticThread   thread;         // worker thread instance struct
    pthread_t       threadID;       // worker pthread
};

#define KINETIC_CONNECTION_INIT(_con) { (*_con) = (KineticConnection) { \
        .connected = false, \
        .socket = -1, \
        .writeMutex = PTHREAD_MUTEX_INITIALIZER, \
        .operations = KINETIC_LIST_INITIALIZER, \
        .pdus = KINETIC_LIST_INITIALIZER, \
    }; \
}


// Kinetic Message HMAC
typedef struct _KineticHMAC {
    KineticProto_Command_Security_ACL_HMACAlgorithm algorithm;
    uint32_t len;
    uint8_t data[KINETIC_HMAC_MAX_LEN];
} KineticHMAC;


// Kinetic Device Message Request
typedef struct _KineticMessage {
    // Kinetic Protocol Buffer Elements
    KineticProto_Message                message;
    KineticProto_Message_HMACauth       hmacAuth;
    KineticProto_Message_PINauth        pinAuth;
    uint8_t                             hmacData[KINETIC_HMAC_MAX_LEN];

    bool                                         has_command; // Set to `true` to enable command element
    KineticProto_Command                         command;
    KineticProto_Command_Header                  header;
    KineticProto_Command_Body                    body;
    KineticProto_Command_Status                  status;
    KineticProto_Command_Security                security;
    KineticProto_Command_Security_ACL            acl;
    KineticProto_Command_KeyValue                keyValue;
    KineticProto_Command_Range                   keyRange;
    KineticProto_Command_GetLog                  getLog;
    KineticProto_Command_GetLog_Type             getLogType;
    KineticProto_Command_PinOperation            pinOp;
} KineticMessage;

// Kinetic PDU Header
#define PDU_HEADER_LEN              (1 + (2 * sizeof(int32_t)))
#define PDU_PROTO_MAX_LEN           (1024 * 1024)
#define PDU_PROTO_MAX_UNPACKED_LEN  (PDU_PROTO_MAX_LEN * 2)
#define PDU_MAX_LEN                 (PDU_HEADER_LEN + \
                                    PDU_PROTO_MAX_LEN + KINETIC_OBJ_SIZE)
typedef struct __attribute__((__packed__)) _KineticPDUHeader {
    uint8_t     versionPrefix;
    uint32_t    protobufLength;
    uint32_t    valueLength;
} KineticPDUHeader;
typedef enum {
    KINETIC_PDU_TYPE_INVALID = 0,
    KINETIC_PDU_TYPE_REQUEST,
    KINETIC_PDU_TYPE_RESPONSE,
    KINETIC_PDU_TYPE_UNSOLICITED
} KineticPDUType;


// Kinetic PDU
struct _KineticPDU {
    // Binary PDU header
    KineticPDUHeader header;    // Header struct in native byte order
    KineticPDUHeader headerNBO; // Header struct in network-byte-order

    // Message associated with this PDU instance
    union {
        KineticProto_Message base;
        KineticMessage message;
    } protoData;        // Proto will always be first
    KineticProto_Message* proto;
    bool protobufDynamicallyExtracted;
    KineticProto_Command* command;

    // Embedded HMAC instance
    KineticHMAC hmac;

    // Exchange associated with this PDU instance (info gets embedded in protobuf message)
    KineticConnection* connection;

    // The type of this PDU (request, response or unsolicited)
    KineticPDUType type;

    // Kinetic operation associated with this PDU, if any
    KineticOperation* operation;
};

typedef KineticStatus (*KineticOperationCallback)(KineticOperation* const operation, KineticStatus const status);

// Kinetic Operation
struct _KineticOperation {
    KineticConnection* connection;
    KineticPDU* request;
    KineticPDU* response;
    pthread_mutex_t timeoutTimeMutex;
    struct timeval timeoutTime;
    bool valueEnabled;
    bool sendValue;
    KineticEntry* entry;
    ByteBufferArray* buffers;
    KineticDeviceInfo** deviceInfo;
    KineticP2P_Operation* p2pOp;
    KineticOperationCallback callback;
    KineticCompletionClosure closure;
};

// Kintic Serial Allocator
// Used for allocating a contiguous hunk of memory to hold arbitrary variable-length response data
typedef struct _KineticSerialAllocator {
    uint8_t* buffer;
    size_t used;
    size_t total;
} KineticSerialAllocator;


KineticProto_Command_Algorithm KineticProto_Command_Algorithm_from_KineticAlgorithm(
    KineticAlgorithm kinteicAlgorithm);
KineticAlgorithm KineticAlgorithm_from_KineticProto_Command_Algorithm(
    KineticProto_Command_Algorithm protoAlgorithm);

KineticProto_Command_Synchronization KineticProto_Command_Synchronization_from_KineticSynchronization(
    KineticSynchronization sync_mode);
KineticSynchronization KineticSynchronization_from_KineticProto_Command_Synchronization(
    KineticProto_Command_Synchronization sync_mode);

KineticStatus KineticProtoStatusCode_to_KineticStatus(
    KineticProto_Command_Status_StatusCode protoStatus);
ByteArray ProtobufCBinaryData_to_ByteArray(
    ProtobufCBinaryData protoData);
bool Copy_ProtobufCBinaryData_to_ByteBuffer(
    ByteBuffer dest, ProtobufCBinaryData src);
bool Copy_KineticProto_Command_KeyValue_to_KineticEntry(
    KineticProto_Command_KeyValue* keyValue, KineticEntry* entry);
bool Copy_KineticProto_Command_Range_to_ByteBufferArray(
    KineticProto_Command_Range* keyRange, ByteBufferArray* keys);
int Kinetic_GetErrnoDescription(int err_num, char *buf, size_t len);
struct timeval Kinetic_TimevalZero(void);
bool Kinetic_TimevalIsZero(struct timeval const tv);
struct timeval Kinetic_TimevalAdd(struct timeval const a, struct timeval const b);
int Kinetic_TimevalCmp(struct timeval const a, struct timeval const b);

KineticProto_Command_GetLog_Type KineticDeviceInfo_Type_to_KineticProto_Command_GetLog_Type(KineticDeviceInfo_Type type);

KineticMessageType KineticProto_Command_MessageType_to_KineticMessageType(KineticProto_Command_MessageType type);

void KineticMessage_Init(KineticMessage* const message);
void KineticMessage_HeaderInit(KineticProto_Command_Header* hdr, KineticConnection* con);
void KineticOperation_Init(KineticOperation* op, KineticConnection* con);
void KineticPDU_Init(KineticPDU* pdu, KineticConnection* con);
void KineticPDU_InitWithCommand(KineticPDU* pdu, KineticConnection* con);

void Kinetic_auth_init_hmac(KineticMessage* const msg,
    int identity, ByteArray hmac);
void Kinetic_auth_init_pinop(KineticMessage* const msg);

#endif // _KINETIC_TYPES_INTERNAL_H
