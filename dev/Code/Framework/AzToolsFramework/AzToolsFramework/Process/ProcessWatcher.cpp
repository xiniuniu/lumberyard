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
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/smart_ptr/scoped_ptr.h>
#include <AzCore/std/parallel/thread.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzToolsFramework/Process/ProcessWatcher.h>
#include <AzToolsFramework/Process/ProcessCommunicator.h>

namespace AzToolsFramework
{
    bool ProcessWatcher::LaunchProcessAndRetrieveOutput(const ProcessLauncher::ProcessLaunchInfo& processLaunchInfo, ProcessCommunicationType communicationType, AzToolsFramework::ProcessOutput& outProcessOutput)
    {
        // launch the process

        AZStd::scoped_ptr<ProcessWatcher> pWatcher(LaunchProcess(processLaunchInfo, communicationType));
        // this may fail - but that's okay, it means it cannot find the P4 Process.  This is not an error situation, the user may not have P4 installed.
        if (!pWatcher)
        {
            AZ_TracePrintf("Process Watcher", "ProcessWatcher::LaunchProcessAndRetrieveOutput: Unable to launch process '%s %s'", processLaunchInfo.m_processExecutableString.c_str(), processLaunchInfo.m_commandlineParameters.c_str());
            return false;
        }
        else
        {
            // get the communicator and ensure it is valid
            ProcessCommunicator* pCommunicator = pWatcher->GetCommunicator();
            if (!pCommunicator || !pCommunicator->IsValid())
            {
                AZ_TracePrintf("Process Watcher", "ProcessWatcher::LaunchProcessAndRetrieveOutput: No communicator for watcher's process (%s %s)!", processLaunchInfo.m_processExecutableString.c_str(), processLaunchInfo.m_commandlineParameters.c_str());
                return false;
            }
            else
            {
                pCommunicator->ReadIntoProcessOutput(outProcessOutput);
            }
        }
        return true;
    }


    bool ProcessWatcher::SpawnProcess(const ProcessLauncher::ProcessLaunchInfo& processLaunchInfo, ProcessCommunicationType communicationType)
    {
        InitProcessData(communicationType == COMMUNICATOR_TYPE_STDINOUT);

        if (communicationType == COMMUNICATOR_TYPE_STDINOUT)
        {
            StdProcessCommunicator* pStdCommunicator = CreateStdCommunicator();
            if (pStdCommunicator->CreatePipesForProcess(m_pWatcherData))
            {
                m_pCommunicator = pStdCommunicator;
            }
            else
            {
                // Communicator failure, just clean it up
                delete pStdCommunicator;
            }
        }
        else if (communicationType == COMMUNICATOR_TYPE_NONE)
        {
            //Implemented, but don't do anything.
        }
        else
        {
            AZ_Assert(false, "communicationType %d not implemented", communicationType);
        }

        return ProcessLauncher::LaunchProcess(processLaunchInfo, *m_pWatcherData);
    }

    class ProcessCommunicator* ProcessWatcher::GetCommunicator()
    {
        return m_pCommunicator;
    }

    AZStd::shared_ptr<ProcessCommunicatorForChildProcess> ProcessWatcher::GetCommunicatorForChildProcess(ProcessCommunicationType communicationType)
    {
        if (communicationType == COMMUNICATOR_TYPE_STDINOUT)
        {
            StdProcessCommunicatorForChildProcess* communicator = CreateStdCommunicatorForChildProcess();
            if (!communicator->AttachToExistingPipes())
            {
                // Delete the communicator if attaching fails, it is useless
                delete communicator;
                communicator = nullptr;
            }
            return AZStd::shared_ptr<ProcessCommunicatorForChildProcess>{
                       communicator
            };
        }
        else if (communicationType == COMMUNICATOR_TYPE_NONE)
        {
            AZ_Assert(false, "No communicator for communicationType %d", communicationType);
        }
        else
        {
            AZ_Assert(false, "communicationType %d not implemented", communicationType);
        }
        return AZStd::shared_ptr<ProcessCommunicatorForChildProcess>{};
    }
}
