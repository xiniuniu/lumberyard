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

#pragma once

#include <AzCore/base.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/containers/vector.h>
#include <AzToolsFramework/Process/ProcessCommon_fwd.h>

namespace AzToolsFramework
{
    namespace ProcessLauncher
    {
        enum ProcessLaunchResult : AZ::u32
        {
            PLR_Success,        // Process Launched Normally
            PLR_MissingFile,    // Missing file or command
        };

        struct ProcessLaunchInfo
        {
            //! This is the process to execute.  Do not escape spaces here.
            AZStd::string m_processExecutableString;
            
            /** 
             * Command line parameters, concatenated.
             * In order to prevent a proliferation of ifdefs all over client code to convert into various standards,
             * instead we assume windows style use of "double quotes" to escape spaces
             * for example: params="hello world" "/Users/JOE SMITH/Desktop"
             * On windows, the command line will be passed as-is to the shell (with quotes)
             * on UNIX/OSX, the command line will be converted as appropraite (quotes removed, but used to chop up parameters)
             */
            AZStd::string m_commandlineParameters;
            
            /**
             * (optional) If you specify a working directory, the command will be executed with that directory as the current directory.
             * Do not use quotes around the working directory string.
             */
            AZStd::string m_workingDirectory;
            ProcessPriority m_processPriority = PROCESSPRIORITY_NORMAL;
            AZStd::vector<AZStd::string>* m_environmentVariables = nullptr;
            mutable ProcessLaunchResult m_launchResult = PLR_Success;

            //Not Supported On Mac
            bool m_showWindow = true;
        };

        static const AZ::u32 INFINITE_TIMEOUT = (AZ::u32) -1;
        bool LaunchProcess(const ProcessLaunchInfo& processLaunchInfo, ProcessData& processData);
        bool LaunchUnwatchedProcess(const ProcessLaunchInfo& processLaunchInfo);
    }  // namespace ProccessLauncher

    class ProcessWatcher
    {
    public:
        // Use LaunchProcess to launch a child process at a given path with a commandline and communication type, optional environment variables (null means inherit from parent environment)
        static ProcessWatcher* LaunchProcess(const ProcessLauncher::ProcessLaunchInfo& processLaunchInfo, ProcessCommunicationType communicationType);

        // Use LaunchProcessAndRetrieveOutput to launch a process via LaunchProcess and return its output, as of now used for fire-and-forget executables (exe's that do something and close immediately)
        static bool LaunchProcessAndRetrieveOutput(const ProcessLauncher::ProcessLaunchInfo& processLaunchInfo, ProcessCommunicationType communicationType, AzToolsFramework::ProcessOutput& outProcessOutput);

        // GetCommunicatorForChildProcess is used when you are implementing the given child process and want a better interface than std::in/std::out (not required)
        static AZStd::shared_ptr<class ProcessCommunicatorForChildProcess> GetCommunicatorForChildProcess(ProcessCommunicationType communicationType);

        // GetCommunicator returns a ProcessCommunicator to communicate with the child process
        ProcessCommunicator* GetCommunicator();

        // Check if child process is running, outExitCode returns exit code if process terminated
        bool IsProcessRunning(AZ::u32* outExitCode = nullptr);

        // Wait for process to exit, waitTime is in seconds, returns true if process exited, false if still running
        bool WaitForProcessToExit(AZ::u32 waitTimeInSeconds);

        // Terminate child process with a given exit code (if still running)
        void TerminateProcess(AZ::u32 exitCode);

        // Delete ProcessWatcher when done with child process
        virtual ~ProcessWatcher();
    protected:
        bool SpawnProcess(const ProcessLauncher::ProcessLaunchInfo& processLaunchInfo, ProcessCommunicationType communicationType);

    private:

        StdProcessCommunicator* CreateStdCommunicator();
        static StdProcessCommunicatorForChildProcess* CreateStdCommunicatorForChildProcess();

        void InitProcessData(bool stdCommunication);

        ProcessWatcher();

        ProcessWatcher(const ProcessWatcher&) = delete;
        ProcessWatcher& operator= (const ProcessWatcher&) = delete;

        ProcessData* m_pWatcherData;
        ProcessCommunicator* m_pCommunicator;
        ProcessCommunicator* m_pChildCommunicator;
    };
} // namespace AzToolsFramework
