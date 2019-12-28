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

#include <source/utils/ApplicationManager.h>

#if defined(AZ_TESTS_ENABLED)
#include <AzTest/AzTest.h>
DECLARE_AZ_UNIT_TEST_MAIN();
#endif

int main(int argc, char* argv[])
{
#if defined(AZ_TESTS_ENABLED)
    INVOKE_AZ_UNIT_TEST_MAIN();
#endif
    AZ::AllocatorInstance<AZ::SystemAllocator>::Create();
    int runSuccess = 0;
    {
        AssetBundler::ApplicationManager applicationManger(&argc, &argv);
        applicationManger.Init();
        runSuccess = applicationManger.Run() ? 0 : 1;
    }
    AZ::AllocatorInstance<AZ::SystemAllocator>::Destroy();
    return runSuccess;
}
