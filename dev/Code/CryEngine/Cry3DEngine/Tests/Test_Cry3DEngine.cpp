/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
#include "StdAfx.h"
#include <AzTest/AzTest.h>

AZ_UNIT_TEST_HOOK();

TEST(Cry3DEngineCVarTest, DeclareConstIntCVar_CheckDefault_ReturnsValue_FT)
{
    // Set the default value in the initialization
    struct Foo {
        DeclareConstIntCVar(test_cvar, 15);
    } foo;
    EXPECT_EQ(foo.test_cvar, 15);

    DeclareConstIntCVar(test_cvar2, 15);
    EXPECT_EQ(test_cvar2, 15);
}

TEST(Cry3DEngineSanityTest, Sanity)
{
    EXPECT_EQ(1, 1);
}
