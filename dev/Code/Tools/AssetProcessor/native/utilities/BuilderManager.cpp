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

#include "BuilderManager.h"
#include <AzCore/std/parallel/binary_semaphore.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/Utils/Utils.h>

#include <AzFramework/API/ApplicationAPI.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzToolsFramework/Process/ProcessCommunicator.h>
#include <native/connection/connectionManager.h>
#include <native/connection/connection.h>
#include <native/utilities/assetUtils.h>
#include <native/utilities/AssetBuilderInfo.h>
#include <QDir>
#include <QElapsedTimer>
#include <QCoreApplication>

namespace AssetProcessor
{
    //! Amount of time in milliseconds to wait between checking the status of the AssetBuilder process and pumping the stdout/err pipes
    static const int s_MaximumSleepTimeMS = 10;

    //! Time in milliseconds to wait after each message pump cycle
    static const int s_IdleBuilderPumpingDelayMS = 100;

    //! Amount of time in seconds to wait for a builder to start up and connect
    // sometimes, builders take a long time to start because of things like virus scanners scanning each
    // builder DLL, so we give them a large margin.
    static const int s_StartupConnectionWaitTimeS = 120;

    static const int s_MillisecondsInASecond = 1000;

    static const char* s_buildersFolderName = "Builders";

    bool Builder::IsConnected() const
    {
        return m_connectionId > 0;
    }

    bool Builder::WaitForConnection()
    {
        if (m_connectionId == 0)
        {
            bool result = false;
            QElapsedTimer ticker;

            ticker.start();

            while (!result)
            {
                result = m_connectionEvent.try_acquire_for(AZStd::chrono::milliseconds(s_MaximumSleepTimeMS));

                PumpCommunicator();

                if (ticker.elapsed() > s_StartupConnectionWaitTimeS * s_MillisecondsInASecond
                    || m_quitListener.WasQuitRequested()
                    || !IsRunning())
                {
                    break;
                }
            }

            PumpCommunicator();
            FlushCommunicator();

            if (result)
            {
                return true;
            }

            AZ::u32 exitCode;

            if (m_quitListener.WasQuitRequested())
            {
                AZ_TracePrintf(AssetProcessor::DebugChannel, "Aborting waiting for builder, quit requested\n");
            }
            else if (!IsRunning(&exitCode))
            {
                AZ_Error("Builder", false, "AssetBuilder terminated during start up with exit code %d", exitCode);
            }
            else
            {
                AZ_Error("Builder", false, "AssetBuilder failed to connect within %d seconds", s_StartupConnectionWaitTimeS);
            }

            return false;
        }

        return true;
    }

    void Builder::SetConnection(AZ::u32 connId)
    {
        m_connectionId = connId;
        m_connectionEvent.release();
    }

    AZ::u32 Builder::GetConnectionId() const
    {
        return m_connectionId;
    }

    AZ::Uuid Builder::GetUuid() const
    {
        return m_uuid;
    }

    AZStd::string Builder::UuidString() const
    {
        return m_uuid.ToString<AZStd::string>(false, false);
    }

    void Builder::PumpCommunicator() const
    {
        if (m_tracePrinter)
        {
            m_tracePrinter->Pump();
        }
    }

    void Builder::FlushCommunicator() const
    {
        if (m_tracePrinter)
        {
            // flush both STDOUT and STDERR
            m_tracePrinter->WriteCurrentString(true);
            m_tracePrinter->WriteCurrentString(false);
        }
    }

    void Builder::TerminateProcess(AZ::u32 exitCode) const
    {
        if (m_processWatcher)
        {
            m_processWatcher->TerminateProcess(exitCode);
        }
    }

    bool Builder::Start()
    {
        // Get the app root to locate the builders
        AZStd::string appRootString;
        AzFramework::ApplicationRequests::Bus::BroadcastResult(appRootString, &AzFramework::ApplicationRequests::GetAppRoot);

        // Get the current BinXXX folder based on the current running AP
        QString projectBinFolder = QCoreApplication::instance()->applicationDirPath();

        // Construct the Builders subfolder path
        AZStd::string buildersFolder;
        AzFramework::StringFunc::Path::Join(projectBinFolder.toUtf8().constData(), s_buildersFolderName, buildersFolder);

        // Construct the full exe for the builder.exe
        const AZStd::string fullExePathString = QDir(projectBinFolder).absoluteFilePath(AssetProcessor::s_assetBuilderRelativePath).toUtf8().constData();

        if (m_quitListener.WasQuitRequested())
        {
            return false;
        }

        const AZStd::string params = BuildParams("resident", buildersFolder.c_str(), UuidString(), "", "");

        m_processWatcher = LaunchProcess(fullExePathString.c_str(), params);

        if (!m_processWatcher)
        {
            return false;
        }

        m_tracePrinter = AZStd::make_unique<CommunicatorTracePrinter>(m_processWatcher->GetCommunicator(), "AssetBuilder");

        return WaitForConnection();
    }

    bool Builder::IsValid() const
    {
        return m_connectionId != 0 && IsRunning();
    }

    bool Builder::IsRunning(AZ::u32* exitCode) const
    {
        return !m_processWatcher || (m_processWatcher && m_processWatcher->IsProcessRunning(exitCode));
    }

    AZStd::string Builder::BuildParams(const char* task, const char* moduleFilePath, const AZStd::string& builderGuid, const AZStd::string& jobDescriptionFile, const AZStd::string& jobResponseFile) const
    {
        QDir projectCacheRoot;
        AssetUtilities::ComputeProjectCacheRoot(projectCacheRoot);

        QString gameName = AssetUtilities::ComputeGameName();

        AZStd::string appRootString;
        AzFramework::ApplicationRequests::Bus::BroadcastResult(appRootString, &AzFramework::ApplicationRequests::GetAppRoot);
        QDir appRoot(QString(appRootString.c_str()));
        QString gameRoot = appRoot.absoluteFilePath(gameName);

        int portNumber = 0;
        ApplicationServerBus::BroadcastResult(portNumber, &ApplicationServerBus::Events::GetServerListeningPort);

        auto params = AZStd::string::format(R"(-task=%s -id="%s" -gamename="\"%s\"" -gamecache="\"%s\"" -gameroot="\"%s\"" -port %d)",
                task, builderGuid.c_str(), gameName.toUtf8().constData(), projectCacheRoot.absolutePath().toUtf8().constData(), gameRoot.toUtf8().constData(), portNumber);

        if (moduleFilePath && moduleFilePath[0])
        {
            params.append(AZStd::string::format(R"( -module="\"%s\"")", moduleFilePath).c_str());
        }

        if (!jobDescriptionFile.empty() && !jobResponseFile.empty())
        {
            params = AZStd::string::format(R"(%s -input="\"%s\"" -output="\"%s\"")", params.c_str(), jobDescriptionFile.c_str(), jobResponseFile.c_str());
        }

        return params;
    }

    AZStd::unique_ptr<AzToolsFramework::ProcessWatcher> Builder::LaunchProcess(const char* fullExePath, const AZStd::string& params) const
    {
        AzToolsFramework::ProcessLauncher::ProcessLaunchInfo processLaunchInfo;
        processLaunchInfo.m_processExecutableString = fullExePath;
        processLaunchInfo.m_commandlineParameters = AZStd::string::format("\"%s\" %s", fullExePath, params.c_str());
        processLaunchInfo.m_showWindow = false;
        processLaunchInfo.m_processPriority = AzToolsFramework::ProcessPriority::PROCESSPRIORITY_IDLE;

        AZ_TracePrintf(AssetProcessor::DebugChannel, "Executing AssetBuilder with parameters: %s\n", processLaunchInfo.m_commandlineParameters.c_str());

        auto processWatcher = AZStd::unique_ptr<AzToolsFramework::ProcessWatcher>(AzToolsFramework::ProcessWatcher::LaunchProcess(processLaunchInfo, AzToolsFramework::COMMUNICATOR_TYPE_STDINOUT));

        AZ_Error(AssetProcessor::ConsoleChannel, processWatcher, "Failed to start %s", fullExePath);

        return processWatcher;
    }

    BuilderRunJobOutcome Builder::WaitForBuilderResponse(AssetBuilderSDK::JobCancelListener* jobCancelListener, AZ::u32 processTimeoutLimitInSeconds, AZStd::binary_semaphore* waitEvent) const
    {
        AZ::u32 exitCode = 0;
        bool finishedOK = false;
        QElapsedTimer ticker;

        ticker.start();

        while (!finishedOK)
        {
            finishedOK = waitEvent->try_acquire_for(AZStd::chrono::milliseconds(s_MaximumSleepTimeMS));

            PumpCommunicator();

            if (!IsValid() || ticker.elapsed() > processTimeoutLimitInSeconds * s_MillisecondsInASecond || (jobCancelListener && jobCancelListener->IsCancelled()))
            {
                break;
            }
        }

        PumpCommunicator();
        FlushCommunicator();


        if (finishedOK)
        {
            return BuilderRunJobOutcome::Ok;
        }
        else if (!IsConnected())
        {
            AZ_Error(AssetProcessor::DebugChannel, false, "Lost connection to asset builder");
            return BuilderRunJobOutcome::LostConnection;
        }
        else if (!IsRunning(&exitCode))
        {
            // these are written to the debug channel because other messages are given for when asset builders die
            // that are more appropriate
            AZ_Error(AssetProcessor::DebugChannel, false, "AssetBuilder terminated with exit code %d", exitCode);
            return BuilderRunJobOutcome::ProcessTerminated;
        }
        else if (jobCancelListener && jobCancelListener->IsCancelled())
        {
            AZ_Error(AssetProcessor::DebugChannel, false, "Job request was cancelled\n");
            TerminateProcess(AZ::u32(-1)); // Terminate the builder. Even if it isn't deadlocked, we can't put it back in the pool while it's busy.
            return BuilderRunJobOutcome::JobCancelled;
        }
        else
        {
            AZ_Error(AssetProcessor::DebugChannel, false, "AssetBuilder failed to respond within %d seconds", processTimeoutLimitInSeconds);
            TerminateProcess(AZ::u32(-1)); // Terminate the builder. Even if it isn't deadlocked, we can't put it back in the pool while it's busy.
            return BuilderRunJobOutcome::ResponseFailure;
        }
    }

    //////////////////////////////////////////////////////////////////////////

    BuilderRef::BuilderRef(const AZStd::shared_ptr<Builder>& builder)
        : m_builder(builder)
    {
        if (m_builder)
        {
            m_builder->m_busy = true;
        }
    }

    BuilderRef::BuilderRef(BuilderRef&& rhs)
        : m_builder(AZStd::move(rhs.m_builder))
    {
    }

    BuilderRef& BuilderRef::operator=(BuilderRef&& rhs)
    {
        m_builder = AZStd::move(rhs.m_builder);
        return *this;
    }

    BuilderRef::~BuilderRef()
    {
        if (m_builder)
        {
            AZ_Warning("BuilderRef", m_builder->m_busy, "Builder reference is valid but is already set to not busy");

            m_builder->m_busy = false;
            m_builder = nullptr;
        }
    }

    const Builder* BuilderRef::operator->() const
    {
        return m_builder.get();
    }

    BuilderRef::operator bool() const
    {
        return m_builder != nullptr;
    }

    //////////////////////////////////////////////////////////////////////////

    BuilderManager::BuilderManager(ConnectionManager* connectionManager)
    {
        using namespace AZStd::placeholders;
        connectionManager->RegisterService(AssetBuilderSDK::BuilderHelloRequest::MessageType(), AZStd::bind(&BuilderManager::IncomingBuilderPing, this, _1, _2, _3, _4, _5));

        // Setup a background thread to pump the idle builders so they don't get blocked trying to output to stdout/err
        m_pollingThread = AZStd::thread([this]()
                {
                    while (!m_quitListener.WasQuitRequested())
                    {
                        PumpIdleBuilders();
                        AZStd::this_thread::sleep_for(AZStd::chrono::milliseconds(s_IdleBuilderPumpingDelayMS));
                    }
                });

        m_quitListener.BusConnect();
        BusConnect();
    }

    BuilderManager::~BuilderManager()
    {
        BusDisconnect();
        m_quitListener.BusDisconnect();
        m_quitListener.ApplicationShutdownRequested();

        if (m_pollingThread.joinable())
        {
            m_pollingThread.join();
        }
    }

    void BuilderManager::ConnectionLost(AZ::u32 connId)
    {
        AZ_Assert(connId > 0, "ConnectionId was 0");
        AZStd::lock_guard<AZStd::mutex> lock(m_buildersMutex);

        for (auto itr = m_builders.begin(); itr != m_builders.end(); ++itr)
        {
            auto& builder = itr->second;

            if (builder->GetConnectionId() == connId)
            {
                AZ_TracePrintf(AssetProcessor::DebugChannel, "Lost connection to builder %s\n", builder->UuidString().c_str());
                builder->m_connectionId = 0;
                m_builders.erase(itr);
                break;
            }
        }
    }

    void BuilderManager::IncomingBuilderPing(AZ::u32 connId, AZ::u32 /*type*/, AZ::u32 serial, QByteArray payload, QString platform)
    {
        AssetBuilderSDK::BuilderHelloRequest requestPing;
        AssetBuilderSDK::BuilderHelloResponse responsePing;

        if (!AZ::Utils::LoadObjectFromBufferInPlace(payload.data(), payload.length(), requestPing))
        {
            AZ_Error(AssetBuilderSDK::ErrorWindow, false,
                "Failed to deserialize BuilderHelloRequest.\n"
                "Your builder(s) may need recompilation to function correctly as this kind of failure usually indicates that "
                "there is a disparity between the version of asset processor running and the version of builder dll files present in the "
                "'builders' subfolder.\n");
        }
        else
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_buildersMutex);

            AZStd::shared_ptr<Builder> builder;
            auto itr = m_builders.find(requestPing.m_uuid);

            if (itr != m_builders.end())
            {
                builder = itr->second;
            }
            else if (m_allowUnmanagedBuilderConnections)
            {
                AZ_TracePrintf(AssetProcessor::DebugChannel, "External builder connection accepted\n");
                builder = AddNewBuilder();
            }
            else
            {
                AZ_Warning("BuilderManager", false, "Received request ping from builder but could not match uuid %s", requestPing.m_uuid.ToString<AZStd::string>().c_str());
            }

            if (builder)
            {
                AZ_TracePrintf(AssetProcessor::DebugChannel, "Builder %s connected, connId: %d\n", builder->UuidString().c_str(), connId);
                builder->SetConnection(connId);
                responsePing.m_accepted = true;
                responsePing.m_uuid = builder->GetUuid();
            }
        }

        AssetProcessor::ConnectionBus::Event(connId, &AssetProcessor::ConnectionBusTraits::SendResponse, serial, responsePing);
    }

    AZStd::shared_ptr<Builder> BuilderManager::AddNewBuilder()
    {
        auto builder = AZStd::make_shared<Builder>(m_quitListener);

        m_builders.insert({ builder->GetUuid(), builder });

        return builder;
    }

    BuilderRef BuilderManager::GetBuilder()
    {
        AZStd::shared_ptr<Builder> newBuilder;
        BuilderRef builderRef;

        {
            AZStd::unique_lock<AZStd::mutex> lock(m_buildersMutex);

            for (auto itr = m_builders.begin(); itr != m_builders.end(); )
            {
                auto& builder = itr->second;

                if (!builder->m_busy)
                {
                    builder->PumpCommunicator();

                    if (builder->IsValid())
                    {
                        return BuilderRef(builder);
                    }
                    else
                    {
                        itr = m_builders.erase(itr);
                    }
                }
                else
                {
                    ++itr;
                }
            }

            AZ_TracePrintf(AssetProcessor::DebugChannel, "Starting new builder for job request\n");

            // None found, start up a new one
            newBuilder = AddNewBuilder();

            // Grab a reference so no one else can take it while we're outside the lock
            builderRef = BuilderRef(newBuilder);
        }

        if (!newBuilder->Start())
        {
            builderRef = {};

            AZStd::unique_lock<AZStd::mutex> lock(m_buildersMutex);
            m_builders.erase(newBuilder->GetUuid());
        }
        else
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Builder started successfully\n");
        }

        return builderRef;
    }

    void BuilderManager::PumpIdleBuilders()
    {
        AZStd::lock_guard<AZStd::mutex> lock(m_buildersMutex);

        for (auto pair : m_builders)
        {
            auto builder = pair.second;

            if (!builder->m_busy)
            {
                builder->PumpCommunicator();
            }
        }
    }
} // namespace AssetProcessor
