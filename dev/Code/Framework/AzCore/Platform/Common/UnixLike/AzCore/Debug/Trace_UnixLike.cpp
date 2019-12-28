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

#include <AzCore/Debug/Trace.h>

#include <pthread.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <unistd.h>

namespace AZ
{
    namespace Debug
    {
        namespace Platform
        {
#if defined(AZ_ENABLE_DEBUG_TOOLS)
            static void* TestTrace(void* ignored)
            {
                return (void*)ptrace(PTRACE_TRACEME, 0, nullptr, nullptr);
            }

            // In Linux only one process at a time can ptrace another. Here we try to ptrace the running process, if it fails we
            // assume a debugger is present (or a tracer is already installed).
            // In order to install a ptrace we need to launch a separate thread which would be the tracer and the parent the tracee.
            bool IsDebuggerPresent()
            {
                pthread_t thread;
                pthread_attr_t attr;

                pthread_attr_init(&attr);
                pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
                if (pthread_create(&thread, &attr, TestTrace, nullptr) != 0)
                {
                    pthread_attr_destroy(&attr);
                    return false;
                }
                pthread_attr_destroy(&attr);

                void* result = nullptr;
                if (pthread_join(thread, &result) != 0)
                {
                    return false;
                }

                return result != nullptr;
            }

            void HandleExceptions(bool)
            {}

            void DebugBreak()
            {
                raise(SIGINT);
            }
#endif // AZ_ENABLE_DEBUG_TOOLS

            void Terminate(int exitCode)
            {
                _exit(exitCode);
            }
        }
    }
}
