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
#ifndef BATCHAPPLICATIONMANAGER_H
#define BATCHAPPLICATIONMANAGER_H

#include <memory>
#include <AzCore/std/string/string.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/containers/vector.h>
#include <AzToolsFramework/API/AssetDatabaseBus.h>
#include <AzCore/Debug/TraceMessageBus.h>
#include "native/FileWatcher/FileWatcher.h"
#include "native/AssetDatabase/AssetDatabase.h"
#include "native/AssetManager/FileStateCache.h"
#include "native/resourcecompiler/RCBuilder.h"
#include "native/utilities/ApplicationManager.h"
#include "native/utilities/assetUtils.h"

#include "AssetBuilderInfo.h"
#include "BuilderManager.h"

namespace AzToolsFramework
{
    class ProcessWatcher;
    class Ticker;
}

namespace AssetProcessor
{
    class AssetProcessorManager;
    class PlatformConfiguration;
    class AssetScanner;
    class RCController;
    class AssetCatalog;
    class InternalAssetBuilderInfo;
    class AssetRequestHandler;
    class AssetServerHandler;
    class FileProcessor;
    class BuilderConfigurationManager;
}

class ApplicationServer;
class ConnectionManager;
class FolderWatchCallbackEx;
//! This class is the Application manager for Batch Mode


class BatchApplicationManager
    : public ApplicationManager
    , public AssetBuilderSDK::AssetBuilderBus::Handler
    , public AssetProcessor::AssetBuilderInfoBus::Handler
    , public AssetProcessor::AssetBuilderRegistrationBus::Handler
    , public AZ::Debug::TraceMessageBus::Handler
    , protected AzToolsFramework::AssetDatabase::AssetDatabaseRequests::Bus::Handler
    , public AssetProcessor::DiskSpaceInfoBus::Handler
{
    Q_OBJECT
public:
    explicit BatchApplicationManager(int* argc, char*** argv, QObject* parent = 0);
    virtual ~BatchApplicationManager();
    ApplicationManager::BeforeRunStatus BeforeRun() override;
    void Destroy() override;
    bool Run() override;
    bool Activate() override;
    bool PostActivate() override;

    AssetProcessor::PlatformConfiguration* GetPlatformConfiguration() const;

    AssetProcessor::AssetProcessorManager* GetAssetProcessorManager() const;

    AssetProcessor::AssetScanner* GetAssetScanner() const;

    AssetProcessor::RCController* GetRCController() const;

    ConnectionManager* GetConnectionManager() const;
    ApplicationServer* GetApplicationServer() const;

    int ProcessedAssetCount() const;
    int FailedAssetsCount() const;
    void ResetProcessedAssetCount();
    void ResetFailedAssetCount();

    //! AssetBuilderSDK::AssetBuilderBus Interface
    void RegisterBuilderInformation(const AssetBuilderSDK::AssetBuilderDesc& builderDesc) override;
    void RegisterComponentDescriptor(AZ::ComponentDescriptor* descriptor) override;
    void BuilderLog(const AZ::Uuid& builderId, const char* message, ...) override;
    void BuilderLogV(const AZ::Uuid& builderId, const char* message, va_list list) override;

    //! AssetBuilderSDK::InternalAssetBuilderBus Interface
    void UnRegisterBuilderDescriptor(const AZ::Uuid& builderId) override;

    //! AssetProcessor::AssetBuilderInfoBus Interface
    void GetMatchingBuildersInfo(const AZStd::string& assetPath, AssetProcessor::BuilderInfoList& builderInfoList) override;
    void GetAllBuildersInfo(AssetProcessor::BuilderInfoList& builderInfoList) override;

    //! TraceMessageBus Interface
    bool OnError(const char* window, const char* message) override;


    //! DiskSpaceInfoBus::Handler
    bool CheckSufficientDiskSpace(const QString& savePath, qint64 requiredSpace, bool shutdownIfInsufficient) override;

    void RemoveOldTempFolders();

    void Rescan();

Q_SIGNALS:
    void CheckAssetProcessorManagerIdleState();
    void ConnectionStatusMsg(QString message);

    void OnBuildersRegistered();

public Q_SLOTS:
    void OnAssetProcessorManagerIdleState(bool isIdle);

protected:
    virtual void InitAssetProcessorManager();//Deletion of assetProcessor Manager will be handled by the ThreadController
    virtual void InitAssetCatalog();//Deletion of AssetCatalog will be handled when the ThreadController is deleted by the base ApplicationManager
    virtual void InitRCController();
    virtual void DestroyRCController();
    virtual void InitAssetScanner();
    virtual void DestroyAssetScanner();
    virtual bool InitPlatformConfiguration();
    virtual void DestroyPlatformConfiguration();
    virtual void InitFileMonitor();
    virtual void DestroyFileMonitor();
    virtual bool InitBuilderConfiguration();
    bool InitApplicationServer();
    void DestroyApplicationServer();
    virtual void InitConnectionManager();
    void DestroyConnectionManager();
    void InitAssetRequestHandler();
    void InitFileStateCache();
    void CreateQtApplication() override;

    bool InitializeInternalBuilders();
    bool InitializeExternalBuilders();
    void InitBuilderManager();
    void ShutdownBuilderManager();
    bool InitAssetDatabase();
    void ShutDownAssetDatabase();
    void InitAssetServerHandler();
    void DestroyAssetServerHandler();
    void InitFileProcessor();
    void ShutDownFileProcessor();

    void InitMetrics();
    void ShutDownMetrics();

    // IMPLEMENTATION OF -------------- AzToolsFramework::AssetDatabase::AssetDatabaseRequests::Bus::Listener
    bool GetAssetDatabaseLocation(AZStd::string& location) override;
    // ------------------------------------------------------------

    AssetProcessor::AssetCatalog* GetAssetCatalog() const { return m_assetCatalog; }

    static bool WaitForBuilderExit(AzToolsFramework::ProcessWatcher* processWatcher, AssetBuilderSDK::JobCancelListener* jobCancelListener, AZ::u32 processTimeoutLimitInSeconds);

    ApplicationServer* m_applicationServer = nullptr;
    ConnectionManager* m_connectionManager = nullptr;

    // keep track of the critical loading point where we are loading other dlls so the error messages can be better.
    bool m_isCurrentlyLoadingGems = false;

public Q_SLOTS:
    void OnActiveJobsCountChanged(unsigned int count);
private Q_SLOTS:
    void CheckForIdle();

private:
    int m_processedAssetCount = 0;
    int m_failedAssetsCount = 0;
    int m_warningCount = 0;
    int m_errorCount = 0;
    bool m_AssetProcessorManagerIdleState = false;
    
    AZStd::vector<AZStd::unique_ptr<FolderWatchCallbackEx> > m_folderWatches;
    FileWatcher m_fileWatcher;
    AZStd::vector<int> m_watchHandles;
    AssetProcessor::PlatformConfiguration* m_platformConfiguration = nullptr;
    AssetProcessor::AssetProcessorManager* m_assetProcessorManager = nullptr;
    AssetProcessor::AssetCatalog* m_assetCatalog = nullptr;
    AssetProcessor::AssetScanner* m_assetScanner = nullptr;
    AssetProcessor::RCController* m_rcController = nullptr;
    AssetProcessor::AssetRequestHandler* m_assetRequestHandler = nullptr;
    AssetProcessor::BuilderManager* m_builderManager = nullptr;
    AssetProcessor::AssetServerHandler* m_assetServerHandler = nullptr;


    AZStd::unique_ptr<AssetProcessor::FileStateBase> m_fileStateCache;

    AZStd::unique_ptr<AssetProcessor::FileProcessor> m_fileProcessor;

    AZStd::unique_ptr<AssetProcessor::BuilderConfigurationManager> m_builderConfig;

    // The internal builder
    AZStd::shared_ptr<AssetProcessor::InternalRecognizerBasedBuilder> m_internalBuilder;

    // Builder description map based on the builder id
    AZStd::unordered_map<AZ::Uuid, AssetBuilderSDK::AssetBuilderDesc> m_builderDescMap;

    // Lookup for builder ids based on the name.  The builder name must be unique
    AZStd::unordered_map<AZStd::string, AZ::Uuid> m_builderNameToId;

    // Builder pattern matchers to used to locate the builder descriptors that match a pattern
    AZStd::list<AssetUtilities::BuilderFilePatternMatcher> m_matcherBuilderPatterns;

    // Collection of all the external module builders
    AZStd::list<AssetProcessor::ExternalModuleAssetBuilderInfo*>    m_externalAssetBuilders;

    AssetProcessor::ExternalModuleAssetBuilderInfo* m_currentExternalAssetBuilder = nullptr;
    
    QAtomicInt m_connectionsAwaitingAssetCatalogSave = 0;
    int m_remainingAPMJobs = 0;
    bool m_assetProcessorManagerIsReady = false;

    unsigned int m_highestConnId = 0;
    AzToolsFramework::Ticker* m_ticker = nullptr; // for ticking the tickbus.

    QList<QMetaObject::Connection> m_connectionsToRemoveOnShutdown;
    QString m_dependencyScanPattern;
    int m_dependencyScanMaxIteration = AssetProcessor::MissingDependencyScanner::DefaultMaxScanIteration; // The maximum number of times to recurse when scanning a file for missing dependencies.
};


#endif//BATCHAPPLICATIONMANAGER_H
