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

#include "kinetic_client.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "mock_kinetic_operation.h"
#include "mock_kinetic_connection.h"
#include "mock_kinetic_controller.h"

#include "kinetic_logger.h"
#include "kinetic_proto.h"
#include "protobuf-c/protobuf-c.h"
#include "byte_array.h"
#include "unity.h"
#include "unity_helper.h"

static ByteArray Value;
static uint8_t ValueData[64];
static KineticSessionHandle DummyHandle = 1;

void setUp(void)
{
    KineticLogger_Init("stdout", 3);
}

void tearDown(void)
{
    KineticLogger_Close();
}

void test_KineticClient_Get_should_execute_GET_operation(void)
{
    LOG_LOCATION;

    Value = ByteArray_Create(ValueData, sizeof(ValueData));
    ByteBuffer valueBuffer = ByteBuffer_CreateWithArray(Value);
    KineticEntry entry = {.value = valueBuffer};
    KineticOperation operation;

    KineticController_CreateOperation_ExpectAndReturn(DummyHandle, &operation);
    KineticOperation_BuildGet_Expect(&operation, &entry);
    KineticController_ExecuteOperation_ExpectAndReturn(&operation, NULL, KINETIC_STATUS_DEVICE_BUSY);

    KineticStatus status = KineticClient_Get(DummyHandle, &entry, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_DEVICE_BUSY, status);
}

void test_KineticClient_Get_should_execute_GET_operation_and_populate_supplied_buffer_with_value(void)
{
    LOG_LOCATION;

    Value = ByteArray_Create(ValueData, sizeof(ValueData));
    ByteBuffer valueBuffer = ByteBuffer_CreateWithArray(Value);
    ByteBuffer_AppendDummyData(&valueBuffer, Value.len);
    KineticEntry entry = {.value = valueBuffer};
    KineticOperation operation;

    KineticController_CreateOperation_ExpectAndReturn(DummyHandle, &operation);
    KineticOperation_BuildGet_Expect(&operation, &entry);
    KineticController_ExecuteOperation_ExpectAndReturn(&operation, NULL, KINETIC_STATUS_CLUSTER_MISMATCH);

    KineticStatus status = KineticClient_Get(DummyHandle, &entry, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_CLUSTER_MISMATCH, status);
}

void test_KineticClient_Get_should_execute_GET_operation_and_retrieve_only_metadata(void)
{
    LOG_LOCATION;
    
    KineticEntry entry = {.metadataOnly = true};
    KineticOperation operation;

    KineticController_CreateOperation_ExpectAndReturn(DummyHandle, &operation);
    KineticOperation_BuildGet_Expect(&operation, &entry);
    KineticController_ExecuteOperation_ExpectAndReturn(&operation, NULL, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticClient_Get(DummyHandle, &entry, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}