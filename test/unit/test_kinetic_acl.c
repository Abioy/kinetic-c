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
#include "unity.h"
#include "kinetic_acl.h"

#include <string.h>
#include <stdio.h>

#include "mock_kinetic_logger.h"

#include "kinetic_proto.h"
#include "protobuf-c.h"

#define TEST_DIR(F) ("test/unit/acl/" F)

void test_acl_of_empty_JSON_object_should_fail(void)
{
    struct ACL *acl = NULL;
    KineticACLLoadResult res = KineticACL_LoadFromFile(TEST_DIR("ex0.json"), &acl);
    TEST_ASSERT_EQUAL(ACL_ERROR_MISSING_FIELD, res);
}

void test_acl_of_nonexistent_file_should_fail(void)
{
    struct ACL *acl = NULL;
    const char *path = TEST_DIR("nonexistent.json");
    KineticACLLoadResult res = KineticACL_LoadFromFile(path, &acl);
    TEST_ASSERT_EQUAL(ACL_ERROR_BAD_JSON, res);
}

void test_KineticACL_LoadFromFile_ex1_should_parse_file_as_expected(void)
{
    struct ACL *acls = NULL;
    
    KineticACLLoadResult res = KineticACL_LoadFromFile(TEST_DIR("ex1.json"), &acls);
    TEST_ASSERT_EQUAL(ACL_OK, res);

    TEST_ASSERT_EQUAL(1, acls->ACL_count);
    KineticProto_Command_Security_ACL *acl = acls->ACLs[0];

    TEST_ASSERT_TRUE(acl->has_identity);
    TEST_ASSERT_EQUAL(1, acl->identity);

    TEST_ASSERT_TRUE(acl->has_hmacAlgorithm);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1,
        acl->hmacAlgorithm);

    TEST_ASSERT_TRUE(acl->has_key);
    TEST_ASSERT_EQUAL(64, acl->key.len);
    TEST_ASSERT_EQUAL(0, strcmp((const char *)acl->key.data,
            "a3b38c37298f7f01a377518dae81dd99655b2be8129c3b2c6357b7e779064159"));

    TEST_ASSERT_EQUAL(2, acl->n_scope);

    KineticProto_Command_Security_ACL_Scope *sc0 = acl->scope[0];
    TEST_ASSERT_EQUAL(1, sc0->n_permission);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SECURITY_ACL_PERMISSION_READ,
        sc0->permission[0]);

    KineticProto_Command_Security_ACL_Scope *sc1 = acl->scope[1];
    TEST_ASSERT_TRUE(sc1->has_offset);
    TEST_ASSERT_EQUAL(0, sc1->offset);
    TEST_ASSERT_EQUAL(1, sc1->n_permission);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SECURITY_ACL_PERMISSION_WRITE,
        sc1->permission[0]);
    TEST_ASSERT_TRUE(sc1->has_value);
    TEST_ASSERT_EQUAL(3, sc1->value.len);
    TEST_ASSERT_EQUAL(0, strcmp((char *)sc1->value.data, "foo"));

    KineticACL_Free(acls);
}

void test_KineticACL_LoadFromFile_ex2_should_parse_file_as_expected(void)
{
    struct ACL *acls = NULL;

    KineticACLLoadResult res = KineticACL_LoadFromFile(TEST_DIR("ex2.json"), &acls);
    TEST_ASSERT_EQUAL(ACL_OK, res);
    TEST_ASSERT_EQUAL(1, acls->ACL_count);
    KineticProto_Command_Security_ACL *acl = acls->ACLs[0];

    TEST_ASSERT_TRUE(acl->has_identity);
    TEST_ASSERT_EQUAL(2, acl->identity);

    TEST_ASSERT_TRUE(acl->has_hmacAlgorithm);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1,
        acl->hmacAlgorithm);

    TEST_ASSERT_TRUE(acl->has_key);
    TEST_ASSERT_EQUAL(64, acl->key.len);
    TEST_ASSERT_EQUAL(0, strcmp((const char *)acl->key.data,
            "13010b8d8acdbe6abc005840aad1dc5dedb4345e681ed4e3c4645d891241d6b2"));

    KineticProto_Command_Security_ACL_Scope *sc0 = acl->scope[0];
    TEST_ASSERT_EQUAL(1, sc0->n_permission);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SECURITY_ACL_PERMISSION_SECURITY,
        sc0->permission[0]);
    TEST_ASSERT_EQUAL(true, sc0->TlsRequired);

    KineticACL_Free(acls);
}

void test_KineticACL_LoadFromFile_should_reject_bad_HMAC_type(void)
{
    struct ACL *acl = NULL;
    KineticACLLoadResult res = KineticACL_LoadFromFile(TEST_DIR("ex_bad_hmac.json"), &acl);
    TEST_ASSERT_EQUAL(ACL_ERROR_INVALID_FIELD, res);
}

void test_KineticACL_LoadFromFile_should_recognize_all_permission_types(void)
{
    struct ACL *acls = NULL;
    KineticACLLoadResult res = KineticACL_LoadFromFile(TEST_DIR("ex3.json"), &acls);
    TEST_ASSERT_EQUAL(ACL_OK, res);

    KineticProto_Command_Security_ACL *acl = acls->ACLs[0];
    TEST_ASSERT_TRUE(acl->has_identity);
    TEST_ASSERT_EQUAL(3, acl->identity);

    TEST_ASSERT_TRUE(acl->has_hmacAlgorithm);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1,
        acl->hmacAlgorithm);

    TEST_ASSERT_TRUE(acl->has_key);
    TEST_ASSERT_EQUAL(64, acl->key.len);
    TEST_ASSERT_EQUAL(0, strcmp((const char *)acl->key.data,
            "a3b38c37298f7f01a377518dae81dd99655b2be8129c3b2c6357b7e779064159"));

    TEST_ASSERT_EQUAL(8, acl->n_scope);

    for (size_t i = 0; i < acl->n_scope; i++) {
        TEST_ASSERT_EQUAL(1, acl->scope[i]->n_permission);
    }

    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SECURITY_ACL_PERMISSION_READ,
        acl->scope[0]->permission[0]);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SECURITY_ACL_PERMISSION_WRITE,
        acl->scope[1]->permission[0]);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SECURITY_ACL_PERMISSION_DELETE,
        acl->scope[2]->permission[0]);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SECURITY_ACL_PERMISSION_RANGE,
        acl->scope[3]->permission[0]);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SECURITY_ACL_PERMISSION_SETUP,
        acl->scope[4]->permission[0]);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SECURITY_ACL_PERMISSION_P2POP,
        acl->scope[5]->permission[0]);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SECURITY_ACL_PERMISSION_GETLOG,
        acl->scope[6]->permission[0]);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SECURITY_ACL_PERMISSION_SECURITY,
        acl->scope[7]->permission[0]);

    KineticACL_Free(acls);
}

void test_KineticACL_LoadFromFile_should_handle_multiple_permissions(void)
{
    struct ACL *acls = NULL;
    KineticACLLoadResult res = KineticACL_LoadFromFile(TEST_DIR("ex-multi-permission.json"), &acls);
    TEST_ASSERT_EQUAL(ACL_OK, res);
    
    KineticProto_Command_Security_ACL *acl = acls->ACLs[0];

    TEST_ASSERT_EQUAL(3, acl->scope[0]->n_permission);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SECURITY_ACL_PERMISSION_READ, acl->scope[0]->permission[0]);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SECURITY_ACL_PERMISSION_WRITE, acl->scope[0]->permission[1]);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SECURITY_ACL_PERMISSION_SETUP, acl->scope[0]->permission[2]);

    KineticACL_Free(acls);
}

void test_acl_should_handle_multiple_JSON_objects(void)
{
    struct ACL *acls = NULL;
    KineticACLLoadResult res = KineticACL_LoadFromFile(TEST_DIR("ex_multi.json"), &acls);
    TEST_ASSERT_EQUAL(ACL_OK, res);
    
    KineticProto_Command_Security_ACL *acl = acls->ACLs[0];
    TEST_ASSERT_TRUE(acl->has_identity);
    TEST_ASSERT_EQUAL(1, acl->identity);

    TEST_ASSERT_TRUE(acl->has_hmacAlgorithm);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1,
        acl->hmacAlgorithm);

    TEST_ASSERT_TRUE(acl->has_key);
    TEST_ASSERT_EQUAL(64, acl->key.len);
    TEST_ASSERT_EQUAL(0, strcmp((const char *)acl->key.data,
            "a3b38c37298f7f01a377518dae81dd99655b2be8129c3b2c6357b7e779064159"));

    TEST_ASSERT_EQUAL(2, acl->n_scope);

    KineticProto_Command_Security_ACL_Scope *sc0 = acl->scope[0];
    TEST_ASSERT_FALSE(sc0->has_offset);
    TEST_ASSERT_EQUAL(1, sc0->n_permission);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SECURITY_ACL_PERMISSION_READ,
        sc0->permission[0]);

    KineticProto_Command_Security_ACL_Scope *sc1 = acl->scope[1];
    TEST_ASSERT_TRUE(sc1->has_offset);
    TEST_ASSERT_EQUAL(0, sc1->offset);
    TEST_ASSERT_EQUAL(1, sc1->n_permission);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SECURITY_ACL_PERMISSION_WRITE,
        sc1->permission[0]);
    TEST_ASSERT_TRUE(sc1->has_value);
    TEST_ASSERT_EQUAL(3, sc1->value.len);
    TEST_ASSERT_EQUAL(0, strcmp((char *)sc1->value.data, "foo"));

    KineticProto_Command_Security_ACL *acl2 = acls->ACLs[1];
    TEST_ASSERT_NOT_NULL(acl2);

    TEST_ASSERT_TRUE(acl2->has_identity);
    TEST_ASSERT_EQUAL(2, acl2->identity);

    TEST_ASSERT_TRUE(acl2->has_hmacAlgorithm);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SECURITY_ACL_HMACALGORITHM_HmacSHA1,
        acl2->hmacAlgorithm);

    TEST_ASSERT_TRUE(acl2->has_key);
    TEST_ASSERT_EQUAL(64, acl2->key.len);
    TEST_ASSERT_EQUAL(0, strcmp((const char *)acl2->key.data,
            "13010b8d8acdbe6abc005840aad1dc5dedb4345e681ed4e3c4645d891241d6b2"));

    TEST_ASSERT_EQUAL(1, acl2->n_scope);
    TEST_ASSERT_EQUAL(1, acl2->scope[0]->n_permission);
    TEST_ASSERT_EQUAL(KINETIC_PROTO_COMMAND_SECURITY_ACL_PERMISSION_SECURITY,
        acl2->scope[0]->permission[0]);
    TEST_ASSERT_EQUAL(true, acl2->scope[0]->TlsRequired);

    KineticACL_Free(acls);
}