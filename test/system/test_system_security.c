/*
* kinetic-c
* Copyright (C) 2015 Seagate Technology.
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
#include "system_test_fixture.h"
#include "kinetic_admin_client.h"

#define TEST_DIR(F) ("test/unit/acl/" F)

void setUp(void)
{
}

void tearDown(void)
{
    SystemTestShutDown();
}

void test_Secure_should_set_ACL_and_only_be_able_to_executed_permitted_operations(void)
{
    /* Set the system an ACL that retains the default HMAC key and all
     * permissions on identity 1, but restricts identity 2 and changes
     * the HMAC key. */
    SystemTestSetup(1);
    const char *ACL_path = TEST_DIR("system_test.json");
    KineticStatus res = KineticAdminClient_SetACL(Fixture.adminSession,
        ACL_path);

    TEST_ASSERT_EQUAL(KINETIC_STATUS_SUCCESS, res);

    SystemTestShutDown();

    SystemTestSetupWithIdentity(3, 2, (const uint8_t *)"test_key", strlen("test_key"));

    /* Put some test data on identity 2 */
    uint8_t KeyData[256];
    uint8_t TagData[256];
    uint8_t VersionData[256];
    uint8_t ValueData[256];
    ByteBuffer KeyBuffer = ByteBuffer_CreateAndAppendCString(KeyData,
        sizeof(KeyData), "id2key");
    ByteBuffer TagBuffer = ByteBuffer_CreateAndAppendCString(TagData,
        sizeof(TagData), "SomeTagValue");
    ByteBuffer VersionBuffer = ByteBuffer_CreateAndAppendCString(VersionData,
        sizeof(VersionData), "v1.0");
    ByteBuffer ValueBuffer = ByteBuffer_CreateAndAppendCString(ValueData,
        sizeof(ValueData), "test");

    KineticEntry putEntry = {
        .key = KeyBuffer,
        .tag = TagBuffer,
        .newVersion = VersionBuffer,
        .algorithm = KINETIC_ALGORITHM_SHA1,
        .value = ValueBuffer,
        .force = true,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };
    KineticStatus status = KineticClient_Put(Fixture.session, &putEntry, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);

    memset(ValueData, 0, sizeof(ValueData));

    /* Get some test data on identity 2 */
    KineticEntry getEntry = {
        .key = KeyBuffer,
        .dbVersion = VersionBuffer,
        .tag = TagBuffer,
        .value = ValueBuffer,
        .synchronization = KINETIC_SYNCHRONIZATION_WRITETHROUGH,
    };

    status = KineticClient_Get(Fixture.session, &getEntry, NULL);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
    TEST_ASSERT_EQUAL(0, strncmp("test", (const char *)ValueBuffer.array.data,
            ValueBuffer.array.len));

    /* Erase on identity 2 --> Permission Denied! */

    // Leave the erase PIN empty so other erase tests won't fail.
    static char OldPinData[4];
    static char NewPinData[4];
    static ByteArray OldPin, NewPin;
    strcpy(NewPinData, "");
    OldPin = ByteArray_Create(OldPinData, 0);
    NewPin = ByteArray_Create(NewPinData, strlen(NewPinData));
    status = KineticAdminClient_SetErasePin(Fixture.adminSession,
        OldPin, NewPin);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_NOT_AUTHORIZED, status);

    SystemTestShutDown();
    SystemTestSetup(1);
    
    /* Erase on identity 1 --> Permitted */
    status = KineticAdminClient_SetErasePin(Fixture.adminSession,
        OldPin, NewPin);
    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}
