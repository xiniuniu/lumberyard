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
#if !defined(BATCH_MODE)
#include "utilities/GUIApplicationManager.h"
#else
#include "utilities/BatchApplicationManager.h"
#endif

#include <AzCore/PlatformDef.h>

#if defined(AZ_TESTS_ENABLED)
#include <AzTest/AzTest.h>
#include <AzTest/Utils.h>
#include <native/tests/BaseAssetProcessorTest.h>
#endif

#if defined(AZ_TESTS_ENABLED)
DECLARE_AZ_UNIT_TEST_MAIN()
#endif

static AZ_THREAD_LOCAL AZ::s64 s_currentJobId = 0;

namespace AssetProcessor
{
    AZ::s64 GetThreadLocalJobId()
    {
        return s_currentJobId;
    }

    void SetThreadLocalJobId(AZ::s64 jobId)
    {
        s_currentJobId = jobId;
    }
}

#if defined(AZ_TESTS_ENABLED)
int RunUnitTests(int argc, char* argv[], bool& ranUnitTests)
{
    ranUnitTests = true;

    INVOKE_AZ_UNIT_TEST_MAIN(nullptr);  // nullptr turns off default test environment used to catch stray asserts

    // This looks a bit weird, but the macro returns conditionally, so *if* we get here, it means the unit tests didn't run
    ranUnitTests = false;
    return 0;
}
#endif

int main(int argc, char* argv[])
{
    qputenv("QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM", "1");
#if defined(AZ_TESTS_ENABLED)
    // If "--unittest" is present on the command line, run unit testing
    // and return immediately. Otherwise, continue as normal.
    AZ::Test::addTestEnvironment(new BaseAssetProcessorTestEnvironment());
    
    bool pauseOnComplete = false;

    if (AZ::Test::ContainsParameter(argc, argv, "--pause-on-completion"))
    {
        pauseOnComplete = true;
    }
    
    bool ranUnitTests;
    int result = RunUnitTests(argc, argv, ranUnitTests);

    if (ranUnitTests)
    {
        if (pauseOnComplete)
        {
            system("pause");
        }

        return result;
    }
#endif

#if defined(BATCH_MODE)
    BatchApplicationManager applicationManager(&argc, &argv);
#else
    GUIApplicationManager applicationManager(&argc, &argv);
#endif

    ApplicationManager::BeforeRunStatus status = applicationManager.BeforeRun();

    if (status != ApplicationManager::BeforeRunStatus::Status_Success)
    {
        if (status == ApplicationManager::BeforeRunStatus::Status_Restarting)
        {
            //AssetProcessor will restart
            return 0;
        }
        else
        {
            //Initialization failed
            return 1;
        }
    }

    return applicationManager.Run() ? 0 : 1;
}

