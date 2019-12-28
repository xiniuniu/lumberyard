/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates, or 
* a third party where indicated.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,  
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  
*
*/

#include "CloudGemFramework_precompiled.h"

#include <AzTest/AzTest.h>

#include <CloudGemFramework/AwsApiRequestJob.h>

#include <aws/lambda/LambdaClient.h>
#include <aws/lambda/model/AddPermissionRequest.h>
#include <aws/lambda/model/AddPermissionResult.h>
#include <aws/core/utils/Outcome.h>

#include <aws/core/auth/AWSCredentialsProvider.h>

class CloudGemFrameworkStaticLibraryTests
    : public ::testing::Test
{
protected:
    void SetUp() override
    {

    }

    void TearDown() override
    {

    }
};

TEST_F(CloudGemFrameworkStaticLibraryTests, TestAwsApiJob)
{

    ASSERT_TRUE(true); // FooBar doesn't exist

}

AZ_UNIT_TEST_HOOK();
