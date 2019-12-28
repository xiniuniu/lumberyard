/*
* All or portions of this file Copyright(c) Amazon.com, Inc.or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
*or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
*WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include <TranslationBuilder/Source/TranslationBuilderComponent.h>

#include <sys/stat.h>

#include <AzCore/Debug/Trace.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/containers/array.h>

#include <AzFramework/API/ApplicationAPI.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzCore/std/string/conversions.h>
#include <AzToolsFramework/Process/ProcessCommunicator.h>
#include <AzToolsFramework/Process/ProcessWatcher.h>

namespace TranslationBuilder
{
    BuilderPluginComponent::BuilderPluginComponent()
    {
        // AZ Components should only initialize their members to null and empty in constructor
        // after construction, they may be deserialized from file.
    }

    BuilderPluginComponent::~BuilderPluginComponent()
    {
    }

    void BuilderPluginComponent::Init()
    {
        // init is where you'd actually allocate memory or create objects since it happens after deserialization.
    }

    void BuilderPluginComponent::Activate()
    {
        // activate is where you'd perform registration with other objects and systems.

        // since we want to register our builder, we do that here:
        AssetBuilderSDK::AssetBuilderDesc builderDescriptor;
        builderDescriptor.m_name = "Qt Translation File Builder";
        builderDescriptor.m_patterns.push_back(AssetBuilderSDK::AssetBuilderPattern("*.ts", AssetBuilderSDK::AssetBuilderPattern::PatternType::Wildcard));
        builderDescriptor.m_busId = TranslationBuilderWorker::GetUUID();
        builderDescriptor.m_createJobFunction = AZStd::bind(&TranslationBuilderWorker::CreateJobs, &m_builderWorker, AZStd::placeholders::_1, AZStd::placeholders::_2);
        builderDescriptor.m_processJobFunction = AZStd::bind(&TranslationBuilderWorker::ProcessJob, &m_builderWorker, AZStd::placeholders::_1, AZStd::placeholders::_2);

        m_builderWorker.BusConnect(builderDescriptor.m_busId);

        // (optimization) this builder does not emit source dependencies:
        builderDescriptor.m_flags |= AssetBuilderSDK::AssetBuilderDesc::BF_EmitsNoDependencies;

        EBUS_EVENT(AssetBuilderSDK::AssetBuilderBus, RegisterBuilderInformation, builderDescriptor);
    }

    void BuilderPluginComponent::Deactivate()
    {
        m_builderWorker.BusDisconnect();
    }

    void BuilderPluginComponent::Reflect(AZ::ReflectContext* context)
    {
        // components also get Reflect called automatically
        // this is your opportunity to perform static reflection or type registration of any types you want the serializer to know about
    }

    
    TranslationBuilderWorker::TranslationBuilderWorker()
    {
    }
    TranslationBuilderWorker::~TranslationBuilderWorker()
    {
    }

    void TranslationBuilderWorker::ShutDown()
    {
        // it is important to note that this will be called on a different thread than your process job thread
        m_isShuttingDown = true;
    }

    // this happens early on in the file scanning pass
    // this function should consistently always create the same jobs, and should do no checking whether the job is up to date or not - just be consistent.
    void TranslationBuilderWorker::CreateJobs(const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response)
    {

        for (const AssetBuilderSDK::PlatformInfo& info : request.m_enabledPlatforms)
        {
            if (info.HasTag("tools")) // if we're on a platform which tools run on
            {
                AssetBuilderSDK::JobDescriptor descriptor;
                descriptor.m_jobKey = "Translation Compile";
                descriptor.m_critical = true;
                descriptor.SetPlatformIdentifier(info.m_identifier.c_str());
                descriptor.m_priority = 8;
                response.m_createJobOutputs.push_back(descriptor);
            }
        }
        
        response.m_result = AssetBuilderSDK::CreateJobsResultCode::Success;
    }

    // later on, this function will be called for jobs that actually need doing.
    // the request will contain the CreateJobResponse you constructed earlier, including any keys and values you placed into the hash table
    void TranslationBuilderWorker::ProcessJob(const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response)
    {
        // Here's an example of listening for cancellation requests.  You should listen for cancellation requests and then cancel work if possible.
        //You can always derive from the Job Cancel Listener and reimplement Cancel() if you need to do more things such as signal a semaphore or other threading work.

        AssetBuilderSDK::JobCancelListener jobCancelListener(request.m_jobId);
        AZ_TracePrintf(AssetBuilderSDK::InfoWindow, "Starting Job.");
        AZStd::string fileName;

        AzFramework::StringFunc::Path::GetFullFileName(request.m_fullPath.c_str(), fileName);
        AzFramework::StringFunc::Path::ReplaceExtension(fileName, "qm");

        AZStd::string destPath;

        // you should do all your work inside the tempDirPath.
        // don't write outside this path
        AzFramework::StringFunc::Path::ConstructFull(request.m_tempDirPath.c_str(), fileName.c_str(), destPath, true);

        // use AZ_TracePrintF to communicate job details.  The logging system will automatically file the text under the appropriate log file and category.

        if (!m_isShuttingDown && !jobCancelListener.IsCancelled())
        {
            AZStd::string lRelease( FindLReleaseTool() );

            if( lRelease.empty() )
            {
                AZ_Error(AssetBuilderSDK::ErrorWindow, false, "Can't find the Qt \"lrelease\" tool!");
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                return;
            }
            else
            {
                AzToolsFramework::ProcessLauncher::ProcessLaunchInfo processLaunchInfo;
                AZStd::string command( AZStd::string::format("\"%s\" \"%s\" -qm \"%s\"", lRelease.c_str(), request.m_fullPath.c_str(), destPath.c_str()) );

                AZ_TracePrintf(AssetBuilderSDK::InfoWindow, "Issuing command:%s", command.c_str());

                processLaunchInfo.m_commandlineParameters = command;
                processLaunchInfo.m_showWindow = false;
                processLaunchInfo.m_workingDirectory = request.m_tempDirPath;
                processLaunchInfo.m_processPriority = AzToolsFramework::PROCESSPRIORITY_IDLE;

                AzToolsFramework::ProcessWatcher* watcher = AzToolsFramework::ProcessWatcher::LaunchProcess(processLaunchInfo, AzToolsFramework::ProcessCommunicationType::COMMUNICATOR_TYPE_STDINOUT);

                if (!watcher)
                {
                    AZ_Error(AssetBuilderSDK::ErrorWindow, false, "Error while processing job %s.", request.m_fullPath.c_str());
                    response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                    return;
                }

                bool result = watcher->WaitForProcessToExit(300);

                if (result)
                {
                    // grab output and append to logs, will help with any debugging down the road.
                    AzToolsFramework::ProcessCommunicator* processCommunicator = watcher->GetCommunicator();
                    if ( processCommunicator && processCommunicator->IsValid() )
                    {
                        AzToolsFramework::ProcessOutput rawOutput;
                        processCommunicator->ReadIntoProcessOutput(rawOutput);

                        // note that the rawOutput may contain a formating code, such as "%s" within the text,
                        // so we need make sure that use the "%s" in the AZ_TracePrintf below.

                        if( rawOutput.HasError() )
                        {
                            AZ_TracePrintf(AssetBuilderSDK::ErrorWindow, "%s", rawOutput.errorResult.c_str());
                        }

                        if( rawOutput.HasOutput() )
                        {
                            AZ_TracePrintf(AssetBuilderSDK::InfoWindow, "%s", rawOutput.outputResult.c_str());
                        }
                    }

                    // the process ran, but was it successful in its run?
                    AZ::u32 exitCode = 0;
                    bool wasRunning = watcher->IsProcessRunning(&exitCode);

                    if( !wasRunning && (exitCode == 0) )
                    {
                        // if you succeed in building assets into your temp dir, you should push them back into the response's product list
                        // Assets you created in your temp path can be specified using paths relative to the temp path, since that is assumed where you're writing stuff.
                        AZStd::string relPath = destPath;
                        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;

                        AssetBuilderSDK::JobProduct jobProduct(fileName);
                        response.m_outputProducts.push_back(jobProduct);
                    }
                    else
                    {
                        AZ_TracePrintf(AssetBuilderSDK::ErrorWindow, "The process failed, exit code %u, while processing job %s.", exitCode, request.m_fullPath.c_str());
                        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                    }
                }
                else
                {
                    AZ_Error(AssetBuilderSDK::ErrorWindow, false, "Process timed out while processing job %s.", request.m_fullPath.c_str());
                    response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                }
            }
        }
        else
        {
            if (m_isShuttingDown)
            {
                AZ_TracePrintf(AssetBuilderSDK::ErrorWindow, "Cancelled job %s because shutdown was requested", request.m_fullPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
            }
            else if (jobCancelListener.IsCancelled())
            {
                AZ_TracePrintf(AssetBuilderSDK::ErrorWindow, "Cancelled was requested for job %s", request.m_fullPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
            }
            else
            {
                AZ_Error(AssetBuilderSDK::ErrorWindow, false, "Error while processing job %s.", request.m_fullPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            }
        }
    }

    AZ::Uuid TranslationBuilderWorker::GetUUID()
    {
        return AZ::Uuid::CreateString("{2BCF58C3-C64C-4645-B97B-7DEC597BB6A3}");
    }

    AZStd::string TranslationBuilderWorker::FindLReleaseTool() const
    {
        AZStd::string fileToSearchFor = "lrelease";
        AZStd::list<AZStd::string> otherPaths;

        AZStd::string_view engineRoot;
        AzFramework::ApplicationRequests::Bus::BroadcastResult(engineRoot, &AzFramework::ApplicationRequests::Bus::Events::GetEngineRoot);

#if defined(AZ_PLATFORM_WINDOWS)
        //
        // search for the utility in the sandbox SDKs first, then the windows path 
        // to find the lrelease tool, if it does not exist then return with a empty path.
        //

        otherPaths.push_back( AZStd::string::format(R"(%s\Code\Sandbox\SDKs\Qt\msvc2013_64\bin\)", engineRoot.empty() ? "" : engineRoot.data()) );
        otherPaths.push_back( AZStd::string::format(R"(%s\Code\Sandbox\SDKs\Qt\msvc2015_64\bin\)", engineRoot.empty() ? "" : engineRoot.data()) );
        otherPaths.push_back( AZStd::string::format(R"(%s\Gems\ScriptCanvas\Tools\qt\msvc2015_64\bin\)", engineRoot.empty() ? "" : engineRoot.data()) );
        
        fileToSearchFor += ".exe";

#elif defined(AZ_PLATFORM_MAC)
        otherPaths.push_back(AZStd::string::format(R"(%s/Code/Sandbox/SDKs/Qt/clang_64/bin/)", engineRoot.empty() ? "" : engineRoot.data()) );
        otherPaths.push_back(AZStd::string::format(R"(%s/Gems/ScriptCanvas/Tools/qt/clang_64/bin/)", engineRoot.empty() ? "" : engineRoot.data()) );
#endif
        AZStd::string toolPath;

        for (const auto & i : otherPaths)
        {
            const AZStd::string fullPath( i + fileToSearchFor );
            struct stat buffer;
            if ( stat(fullPath.data(), &buffer) == 0)
            {
                toolPath = fullPath;
                break;
            }
        }

        return toolPath;
    }
}
