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

#include <AzTest/AzTest.h>
#include <AssetBuilderSDK/AssetBuilderSDK.h>
#include "native/tests/AssetProcessorTest.h"
#include "AzToolsFramework/API/AssetDatabaseBus.h"
#include "AssetDatabase/AssetDatabase.h"
#include "FileProcessor/FileProcessor.h"
#include "utilities/PlatformConfiguration.h"
#include <QCoreApplication>
#include <utilities/AssetUtilEBusHelper.h>

namespace UnitTests
{
    using namespace testing;
    using ::testing::NiceMock;
    using namespace AssetProcessor;

    using AzToolsFramework::AssetDatabase::ProductDatabaseEntry;
    using AzToolsFramework::AssetDatabase::ScanFolderDatabaseEntry;
    using AzToolsFramework::AssetDatabase::SourceDatabaseEntry;
    using AzToolsFramework::AssetDatabase::SourceFileDependencyEntry;
    using AzToolsFramework::AssetDatabase::SourceFileDependencyEntryContainer;
    using AzToolsFramework::AssetDatabase::JobDatabaseEntry;
    using AzToolsFramework::AssetDatabase::ProductDatabaseEntryContainer;
    using AzToolsFramework::AssetDatabase::ProductDependencyDatabaseEntry;
    using AzToolsFramework::AssetDatabase::ProductDependencyDatabaseEntryContainer;
    using AzToolsFramework::AssetDatabase::AssetDatabaseConnection;
    using AzToolsFramework::AssetDatabase::FileDatabaseEntry;
    using AzToolsFramework::AssetDatabase::FileDatabaseEntryContainer;

    class MockDatabaseLocationListener : public AzToolsFramework::AssetDatabase::AssetDatabaseRequests::Bus::Handler
    {
    public:
        MOCK_METHOD1(GetAssetDatabaseLocation, bool(AZStd::string&));
    };

    class FileProcessorTests 
        : public AssetProcessorTest,
        public ConnectionBus::Handler
    {
    public:
        void SetUp() override;
        void TearDown() override;

        //////////////////////////////////////////////////////////////////////////

        // Sends an unsolicited message to the connection
        size_t Send(unsigned int serial, const AzFramework::AssetSystem::BaseAssetProcessorMessage& message);
        // Sends a raw buffer to the connection
        size_t SendRaw(unsigned int type, unsigned int serial, const QByteArray& data) { return 0; };

        // Sends a message to the connection if the platform match
        size_t SendPerPlatform(unsigned int serial, const AzFramework::AssetSystem::BaseAssetProcessorMessage& message, const QString& platform){ return 0; };
        // Sends a raw buffer to the connection if the platform match
        size_t SendRawPerPlatform(unsigned int type, unsigned int serial, const QByteArray& data, const QString& platform){ return 0; };

        // Sends a message to the connection which expects a response.
        unsigned int SendRequest(const AzFramework::AssetSystem::BaseAssetProcessorMessage& message, const ResponseCallback& callback){ return 0; };

        // Sends a response to the connection
        size_t SendResponse(unsigned int serial, const AzFramework::AssetSystem::BaseAssetProcessorMessage& message){ return 0; };

        // Removes a response handler that is no longer needed
        void RemoveResponseHandler(unsigned int serial){};

    protected:
        struct StaticData
        {
            QTemporaryDir m_temporaryDir;
            QDir m_temporarySourceDir;

            // these variables are created during SetUp() and destroyed during TearDown() and thus are always available during tests using this fixture:
            AZStd::string m_databaseLocation;
            NiceMock<MockDatabaseLocationListener> m_databaseLocationListener;
            AssetProcessor::AssetDatabaseConnection m_connection;

            AZStd::unique_ptr<AssetProcessor::PlatformConfiguration> m_config;

            // The following database entry variables are initialized only when you call coverage test data CreateCoverageTestData().
            // Tests which don't need or want a pre-made database should not call CreateCoverageTestData() but note that in that case
            // these entries will be empty and their identifiers will be -1.
            ScanFolderDatabaseEntry m_scanFolder;

            AZStd::unique_ptr<FileProcessor> m_fileProcessor;

            FileDatabaseEntryContainer m_fileEntries;
            QCoreApplication m_coreApp;
            int m_argc = 0;
            int m_messagesSent = 0;

            StaticData() : m_coreApp(m_argc, nullptr)
            {

            }
        };

        // we store the above data in a unique_ptr so that its memory can be cleared during TearDown() in one call, before we destroy the memory
        // allocator, reducing the chance of missing or forgetting to destroy one in the future.
        AZStd::unique_ptr<StaticData> m_data;
    };
}
