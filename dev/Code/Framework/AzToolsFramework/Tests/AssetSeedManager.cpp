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

#include <AzCore/UnitTest/TestTypes.h>
#include <AzToolsFramework/Asset/AssetSeedManager.h>
#include <AzFramework/Asset/AssetRegistry.h>
#include <AzCore/IO/FileIO.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzFramework/Asset/AssetCatalog.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzTestShared/Utils/Utils.h>
#include <AzToolsFramework/Application/ToolsApplication.h>
#include <AzFramework/Platform/PlatformDefaults.h>
#include <AzToolsFramework/AssetCatalog/PlatformAddressedAssetCatalog.h>

namespace // anonymous
{
    static const int s_totalAssets = 12;
    static const int s_totalTestPlatforms = 2;
    const char* s_catalogFile = "AssetCatalog.xml";

    AZ::Data::AssetId assets[s_totalAssets];

    bool Search(const AzToolsFramework::AssetFileInfoList& assetList, const AZ::Data::AssetId& assetId)
    {
        return AZStd::find_if(assetList.m_fileInfoList.begin(), assetList.m_fileInfoList.end(), 
            [&](AzToolsFramework::AssetFileInfo fileInfo) 
            { 
                return fileInfo.m_assetId == assetId; 
            });
    }
}


namespace UnitTest
{
    class AssetSeedManagerTest
        : public AllocatorsFixture
        , public AZ::Data::AssetCatalogRequestBus::Handler
    {
    public:
        void SetUp() override
        {
            using namespace AZ::Data;
            m_application = new AzToolsFramework::ToolsApplication();
            m_assetSeedManager = new AzToolsFramework::AssetSeedManager();
            m_assetRegistry = new AzFramework::AssetRegistry();

            m_localFileIO = aznew AZ::IO::LocalFileIO();

            m_priorFileIO = AZ::IO::FileIOBase::GetInstance();
            AZ::IO::FileIOBase::SetInstance(m_localFileIO);

            AZ::IO::FileIOBase::GetInstance()->SetAlias("@assets@", GetTestFolderPath().c_str());

            for (int idx = 0; idx < s_totalAssets; idx++)
            {
                assets[idx] = AssetId(AZ::Uuid::CreateRandom(), 0);
                AZ::Data::AssetInfo info;
                info.m_relativePath = AZStd::string::format("Asset%d.txt", idx);
                m_assetsPath[idx] = info.m_relativePath;
                info.m_assetId = assets[idx];
                m_assetRegistry->RegisterAsset(assets[idx], info);
            }

            m_testPlatforms[0] = AzFramework::PlatformId::PC;
            m_testPlatforms[1] = AzFramework::PlatformId::ES3;

            int platformCount = 0;
            for(auto thisPlatform : m_testPlatforms)
            {
                AZStd::string assetRoot = AzToolsFramework::PlatformAddressedAssetCatalog::GetAssetRootForPlatform(thisPlatform);

                for (int idx = 0; idx < s_totalAssets; idx++)
                {
                    AzFramework::StringFunc::Path::Join(assetRoot.c_str(), m_assetsPath[idx].c_str(), m_assetsPathFull[platformCount][idx]);
                    if (m_fileStreams[platformCount][idx].Open(m_assetsPathFull[platformCount][idx].c_str(), AZ::IO::OpenMode::ModeWrite | AZ::IO::OpenMode::ModeBinary | AZ::IO::OpenMode::ModeCreatePath))
                    {
                        m_fileStreams[platformCount][idx].Write(m_assetsPath[idx].size(), m_assetsPath[idx].data());
                        m_fileStreams[platformCount][idx].Close();
                    }
                    else
                    {
                        GTEST_FATAL_FAILURE_(AZStd::string::format("Unable to create temporary file ( %s ) in AssetSeedManager unit tests.\n", m_assetsPath[idx].c_str()).c_str());
                    }
                }
                ++platformCount;
            }

            // asset0 -> asset1 -> asset2 -> asset4
            //                 --> asset3
            m_assetRegistry->RegisterAssetDependency(assets[0], AZ::Data::ProductDependency(assets[1], 0));
            m_assetRegistry->RegisterAssetDependency(assets[1], AZ::Data::ProductDependency(assets[2], 0));
            m_assetRegistry->RegisterAssetDependency(assets[1], AZ::Data::ProductDependency(assets[3], 0));
            m_assetRegistry->RegisterAssetDependency(assets[2], AZ::Data::ProductDependency(assets[4], 0));

            // asset5 -> asset6 -> asset7
            m_assetRegistry->RegisterAssetDependency(assets[5], AZ::Data::ProductDependency(assets[6], 0));
            m_assetRegistry->RegisterAssetDependency(assets[6], AZ::Data::ProductDependency(assets[7], 0));

            // asset8 -> asset6 
            m_assetRegistry->RegisterAssetDependency(assets[8], AZ::Data::ProductDependency(assets[6], 0));

            // asset10 -> asset11 
            m_assetRegistry->RegisterAssetDependency(assets[10], AZ::Data::ProductDependency(assets[11], 0));

            // asset11 -> asset10 
            m_assetRegistry->RegisterAssetDependency(assets[11], AZ::Data::ProductDependency(assets[10], 0));

            m_application->Start(AzFramework::Application::Descriptor());

            AZ::SerializeContext* context;
            AZ::ComponentApplicationBus::BroadcastResult(context, &AZ::ComponentApplicationBus::Events::GetSerializeContext);
            ASSERT_TRUE(context) << "No serialize context.\n";

            AzToolsFramework::AssetSeedManager::Reflect(context);

            // Asset Catalog does not exposes its internal asset registry and the only way to set it is through LoadCatalog API
            // Currently i am serializing the asset registry to disk
            // and invoking the LoadCatalog API to populate the asset catalog created by the azframework app.

            bool useRequestBus = false;
            AzFramework::AssetCatalog assetCatalog(useRequestBus);

            AZStd::string pcCatalogFile = AzToolsFramework::PlatformAddressedAssetCatalog::GetCatalogRegistryPathForPlatform(AzFramework::PlatformId::PC);
            AZStd::string es3CatalogFile = AzToolsFramework::PlatformAddressedAssetCatalog::GetCatalogRegistryPathForPlatform(AzFramework::PlatformId::ES3);

            if (!assetCatalog.SaveCatalog(pcCatalogFile.c_str(), m_assetRegistry))
            {
                GTEST_FATAL_FAILURE_(AZStd::string::format("Unable to save the asset catalog (PC) file.\n").c_str());
            }

            if (!assetCatalog.SaveCatalog(es3CatalogFile.c_str(), m_assetRegistry))
            {
                GTEST_FATAL_FAILURE_(AZStd::string::format("Unable to save the asset catalog (ES3) file.\n").c_str());
            }

            m_pcCatalog = new AzToolsFramework::PlatformAddressedAssetCatalog(AzFramework::PlatformId::PC);
            m_es3Catalog = new AzToolsFramework::PlatformAddressedAssetCatalog(AzFramework::PlatformId::ES3);
        }

        void TearDown() override
        {
            AZ::IO::FileIOBase* fileIO = AZ::IO::FileIOBase::GetInstance();

            if (fileIO->Exists(s_catalogFile))
            {
                fileIO->Remove(s_catalogFile);
            }

            int platformCount = 0;
            for (auto thisPlatform : m_testPlatforms)
            {
                // Deleting all the temporary files
                for (int idx = 0; idx < s_totalAssets; idx++)
                {
                    // we need to close the handle before we try to remove the file
                    if (fileIO->Exists(m_assetsPathFull[platformCount][idx].c_str()))
                    {
                        fileIO->Remove(m_assetsPathFull[platformCount][idx].c_str());
                    }
                }
                ++platformCount;
            }

            auto pcCatalogFile = AzToolsFramework::PlatformAddressedAssetCatalog::GetCatalogRegistryPathForPlatform(AzFramework::PlatformId::PC);
            auto es3CatalogFile = AzToolsFramework::PlatformAddressedAssetCatalog::GetCatalogRegistryPathForPlatform(AzFramework::PlatformId::ES3);
            if (fileIO->Exists(pcCatalogFile.c_str()))
            {
                fileIO->Remove(pcCatalogFile.c_str());
            }

            if (fileIO->Exists(es3CatalogFile.c_str()))
            {
                fileIO->Remove(es3CatalogFile.c_str());
            }

            delete m_localFileIO;
            m_localFileIO = nullptr;
            AZ::IO::FileIOBase::SetInstance(m_priorFileIO);
            delete m_assetSeedManager;
            delete m_assetRegistry;
            delete m_pcCatalog;
            delete m_es3Catalog;
            m_application->Stop();
            delete m_application;
        }

        AZ::Data::AssetInfo GetAssetInfoById(const AZ::Data::AssetId& id) 
        {
            auto foundIter = m_assetRegistry->m_assetIdToInfo.find(id);
            if (foundIter != m_assetRegistry->m_assetIdToInfo.end())
            {
                return foundIter->second;
            }

            return AZ::Data::AssetInfo();
        }

        void AssetSeedManager_SaveSeedListFile_FileIsReadOnly()
        {
            char fileName[] = "ReadOnlyTestFile";
            AZStd::string filePath;
            AzFramework::StringFunc::Path::ConstructFull(AZ::IO::FileIOBase::GetInstance()->GetAlias("@assets@"), fileName, "seed", filePath);

            // Create the file
            EXPECT_TRUE(m_assetSeedManager->Save(filePath));

            // Mark the file Read-Only
            AZ::IO::SystemFile::SetWritable(filePath.c_str(), false);

            // Attempt to save to the same file. Should not be allowed.
            EXPECT_FALSE(m_assetSeedManager->Save(filePath));

            // Clean up the test environment
            AZ::IO::SystemFile::SetWritable(filePath.c_str(), true);
            AZ::IO::SystemFile::Delete(filePath.c_str());
        }

        void AssetSeedManager_SaveAssetInfoFile_FileIsReadOnly()
        {
            char fileName[] = "ReadOnlyTestFile";
            AZStd::string filePath;
            AzFramework::StringFunc::Path::ConstructFull(AZ::IO::FileIOBase::GetInstance()->GetAlias("@assets@"), fileName, "assetlist", filePath);

            // Create the file
            EXPECT_TRUE(m_assetSeedManager->SaveAssetFileInfo(filePath, AzFramework::PlatformFlags::Platform_PC));

            // Mark the file Read-Only
            AZ::IO::SystemFile::SetWritable(filePath.c_str(), false);

            // Attempt to save to the same file. Should not be allowed.
            EXPECT_FALSE(m_assetSeedManager->SaveAssetFileInfo(filePath, AzFramework::PlatformFlags::Platform_PC));

            // Clean up the test environment
            AZ::IO::SystemFile::SetWritable(filePath.c_str(), true);
            AZ::IO::SystemFile::Delete(filePath.c_str());
        }

        void ValidateSeedFileExtension_CorrectFileExtension_ExpectSuccess()
        {
            AZStd::string path("some/test/path/file.seed");
            AZ::Outcome<void, AZStd::string> validationOutcome = AzToolsFramework::AssetSeedManager::ValidateSeedFileExtension(path);

            EXPECT_TRUE(validationOutcome.IsSuccess());
        }

        void ValidateSeedFileExtension_IncorrectFileExtension_ExpectFailure()
        {
            AZStd::string path("some/test/path/file.xml");
            AZ::Outcome<void, AZStd::string> validationOutcome = AzToolsFramework::AssetSeedManager::ValidateSeedFileExtension(path);

            EXPECT_FALSE(validationOutcome.IsSuccess());
        }

        void ValidateAssetListFileExtension_CorrectFileExtension_ExpectSuccess()
        {
            AZStd::string path("some/test/path/file.assetlist");
            AZ::Outcome<void, AZStd::string> validationOutcome = AzToolsFramework::AssetSeedManager::ValidateAssetListFileExtension(path);

            EXPECT_TRUE(validationOutcome.IsSuccess());
        }

        void ValidateAssetListFileExtension_IncorrectFileExtension_ExpectFailure()
        {
            AZStd::string path("some/test/path/file.xml");
            AZ::Outcome<void, AZStd::string> validationOutcome = AzToolsFramework::AssetSeedManager::ValidateAssetListFileExtension(path);

            EXPECT_FALSE(validationOutcome.IsSuccess());
        }

        void AddPlatformToAllSeeds_SeedsAreValidForPlatform_AllSeedsUpdated()
        {
            // Setup
            m_assetSeedManager->AddSeedAsset(assets[0], AzFramework::PlatformFlags::Platform_PC);
            m_assetSeedManager->AddSeedAsset(assets[1], AzFramework::PlatformFlags::Platform_PC);
            m_assetSeedManager->AddSeedAsset(assets[2], AzFramework::PlatformFlags::Platform_PC);

            // Step we are testing
            m_assetSeedManager->AddPlatformToAllSeeds(AzFramework::PlatformId::ES3);

            // Verification
            AzFramework::PlatformFlags expectedPlatformFlags = AzFramework::PlatformFlags::Platform_PC | AzFramework::PlatformFlags::Platform_ES3;
            for (const auto& seedInfo : m_assetSeedManager->GetAssetSeedList())
            {
                EXPECT_EQ(seedInfo.m_platformFlags, expectedPlatformFlags);
            }
        }

        void AddPlatformToAllSeeds_NotAllSeedsAreValidForPlatform_InvalidSeedsNotChanged()
        {
            // Setup
            m_assetSeedManager->AddSeedAsset(assets[0], AzFramework::PlatformFlags::Platform_PC);
            m_assetSeedManager->AddSeedAsset(assets[1], AzFramework::PlatformFlags::Platform_PC);

            m_es3Catalog->UnregisterAsset(assets[2]);
            m_assetSeedManager->AddSeedAsset(assets[2], AzFramework::PlatformFlags::Platform_PC);

            // Step we are testing
            m_assetSeedManager->AddPlatformToAllSeeds(AzFramework::PlatformId::ES3);

            // Verification
            AzFramework::PlatformFlags expectedPlatformFlags = AzFramework::PlatformFlags::Platform_PC | AzFramework::PlatformFlags::Platform_ES3;
            for (const auto& seedInfo : m_assetSeedManager->GetAssetSeedList())
            {
                if (seedInfo.m_assetId == assets[2])
                {
                    EXPECT_EQ(seedInfo.m_platformFlags, AzFramework::PlatformFlags::Platform_PC);
                }
                else
                {
                    EXPECT_EQ(seedInfo.m_platformFlags, expectedPlatformFlags);
                }
            }
        }

        void RemovePlatformFromAllSeeds_PlatformIsPresentInAllSeeds_PlatformIsRemoved()
        {
            // Setup
            m_assetSeedManager->AddSeedAsset(assets[0], AzFramework::PlatformFlags::Platform_PC);
            m_assetSeedManager->AddSeedAsset(assets[0], AzFramework::PlatformFlags::Platform_ES3);
            m_assetSeedManager->AddSeedAsset(assets[1], AzFramework::PlatformFlags::Platform_PC);
            m_assetSeedManager->AddSeedAsset(assets[1], AzFramework::PlatformFlags::Platform_ES3);
            m_assetSeedManager->AddSeedAsset(assets[2], AzFramework::PlatformFlags::Platform_PC);
            m_assetSeedManager->AddSeedAsset(assets[2], AzFramework::PlatformFlags::Platform_ES3);

            // Step we are testing
            m_assetSeedManager->RemovePlatformFromAllSeeds(AzFramework::PlatformId::ES3);

            // Verification
            for (const auto& seedInfo : m_assetSeedManager->GetAssetSeedList())
            {
                EXPECT_EQ(seedInfo.m_platformFlags, AzFramework::PlatformFlags::Platform_PC);
            }
        }

        void RemovePlatformFromAllSeeds_SeedsOnlyHaveOnePlatform_SeedsAreNotChanged()
        {
            // Setup
            m_assetSeedManager->AddSeedAsset(assets[0], AzFramework::PlatformFlags::Platform_PC);
            m_assetSeedManager->AddSeedAsset(assets[1], AzFramework::PlatformFlags::Platform_PC);
            m_assetSeedManager->AddSeedAsset(assets[2], AzFramework::PlatformFlags::Platform_PC);

            // Step we are testing
            m_assetSeedManager->RemovePlatformFromAllSeeds(AzFramework::PlatformId::PC);

            // Verification
            for (const auto& seedInfo : m_assetSeedManager->GetAssetSeedList())
            {
                EXPECT_EQ(seedInfo.m_platformFlags, AzFramework::PlatformFlags::Platform_PC);
            }
        }

        void DependencyValidation_SingleAssetSeed_ListValid()
        {
            m_assetSeedManager->AddSeedAsset(assets[0], AzFramework::PlatformFlags::Platform_PC);

            AzToolsFramework::AssetFileInfoList assetList = m_assetSeedManager->GetDependencyList(AzFramework::PlatformId::PC);

            EXPECT_EQ(assetList.m_fileInfoList.size(), 5);
            EXPECT_TRUE(Search(assetList, assets[0]));
            EXPECT_TRUE(Search(assetList, assets[1]));
            EXPECT_TRUE(Search(assetList, assets[2]));
            EXPECT_TRUE(Search(assetList, assets[3]));
            EXPECT_TRUE(Search(assetList, assets[4]));

            m_assetSeedManager->RemoveSeedAsset(assets[0], AzFramework::PlatformFlags::Platform_PC);
            m_assetSeedManager->AddSeedAsset(assets[5], AzFramework::PlatformFlags::Platform_PC);

            assetList.m_fileInfoList.clear();

            assetList = m_assetSeedManager->GetDependencyList(AzFramework::PlatformId::PC);

            EXPECT_EQ(assetList.m_fileInfoList.size(), 3);
            EXPECT_TRUE(Search(assetList, assets[5]));
            EXPECT_TRUE(Search(assetList, assets[6]));
            EXPECT_TRUE(Search(assetList, assets[7]));

            m_assetSeedManager->RemoveSeedAsset(assets[5], AzFramework::PlatformFlags::Platform_PC);
            m_assetSeedManager->AddSeedAsset(assets[8], AzFramework::PlatformFlags::Platform_PC);

            assetList.m_fileInfoList.clear();

            assetList = m_assetSeedManager->GetDependencyList(AzFramework::PlatformId::PC);

            EXPECT_EQ(assetList.m_fileInfoList.size(), 3);
            EXPECT_TRUE(Search(assetList, assets[6]));
            EXPECT_TRUE(Search(assetList, assets[7]));
            EXPECT_TRUE(Search(assetList, assets[8]));

            assetList.m_fileInfoList.clear();

            m_assetSeedManager->RemoveSeedAsset(assets[8], AzFramework::PlatformFlags::Platform_PC);
            m_assetSeedManager->AddSeedAsset(assets[9], AzFramework::PlatformFlags::Platform_PC);

            assetList = m_assetSeedManager->GetDependencyList(AzFramework::PlatformId::PC);
            EXPECT_EQ(assetList.m_fileInfoList.size(), 1);
        }

        void DependencyValidation_MultipleAssetSeed_ListValid()
        {
            m_assetSeedManager->AddSeedAsset(assets[0], AzFramework::PlatformFlags::Platform_PC);
            m_assetSeedManager->AddSeedAsset(assets[5], AzFramework::PlatformFlags::Platform_PC);

            AzToolsFramework::AssetFileInfoList assetList = m_assetSeedManager->GetDependencyList(AzFramework::PlatformId::PC);

            EXPECT_EQ(assetList.m_fileInfoList.size(), 8);
            EXPECT_TRUE(Search(assetList, assets[0]));
            EXPECT_TRUE(Search(assetList, assets[1]));
            EXPECT_TRUE(Search(assetList, assets[2]));
            EXPECT_TRUE(Search(assetList, assets[3]));
            EXPECT_TRUE(Search(assetList, assets[4]));
            EXPECT_TRUE(Search(assetList, assets[5]));
            EXPECT_TRUE(Search(assetList, assets[6]));
            EXPECT_TRUE(Search(assetList, assets[7]));

            assetList.m_fileInfoList.clear();

            m_assetSeedManager->AddSeedAsset(assets[8], AzFramework::PlatformFlags::Platform_PC);

            assetList = m_assetSeedManager->GetDependencyList(AzFramework::PlatformId::PC);

            EXPECT_EQ(assetList.m_fileInfoList.size(), 9);
            EXPECT_TRUE(Search(assetList, assets[0]));
            EXPECT_TRUE(Search(assetList, assets[1]));
            EXPECT_TRUE(Search(assetList, assets[2]));
            EXPECT_TRUE(Search(assetList, assets[3]));
            EXPECT_TRUE(Search(assetList, assets[4]));
            EXPECT_TRUE(Search(assetList, assets[5]));
            EXPECT_TRUE(Search(assetList, assets[6]));
            EXPECT_TRUE(Search(assetList, assets[7]));
            EXPECT_TRUE(Search(assetList, assets[8]));

            assetList.m_fileInfoList.clear();
            m_assetSeedManager->RemoveSeedAsset(assets[5], AzFramework::PlatformFlags::Platform_PC);

            assetList = m_assetSeedManager->GetDependencyList(AzFramework::PlatformId::PC);

            EXPECT_EQ(assetList.m_fileInfoList.size(), 8);
            EXPECT_TRUE(Search(assetList, assets[0]));
            EXPECT_TRUE(Search(assetList, assets[1]));
            EXPECT_TRUE(Search(assetList, assets[2]));
            EXPECT_TRUE(Search(assetList, assets[3]));
            EXPECT_TRUE(Search(assetList, assets[4]));
            EXPECT_TRUE(Search(assetList, assets[6]));
            EXPECT_TRUE(Search(assetList, assets[7]));
            EXPECT_TRUE(Search(assetList, assets[8]));
        }

        void DependencyValidation_MultipleAssetSeeds_MultiplePlatformFlags_ListValid()
        {
            m_assetSeedManager->AddSeedAsset(assets[0], AzFramework::PlatformFlags::Platform_PC | AzFramework::PlatformFlags::Platform_ES3);
            m_assetSeedManager->AddSeedAsset(assets[5], AzFramework::PlatformFlags::Platform_PC | AzFramework::PlatformFlags::Platform_ES3);

            AzToolsFramework::AssetFileInfoList assetList = m_assetSeedManager->GetDependencyList(AzFramework::PlatformId::PC);

            EXPECT_EQ(assetList.m_fileInfoList.size(), 8);
            EXPECT_TRUE(Search(assetList, assets[0]));
            EXPECT_TRUE(Search(assetList, assets[1]));
            EXPECT_TRUE(Search(assetList, assets[2]));
            EXPECT_TRUE(Search(assetList, assets[3]));
            EXPECT_TRUE(Search(assetList, assets[4]));
            EXPECT_TRUE(Search(assetList, assets[5]));
            EXPECT_TRUE(Search(assetList, assets[6]));
            EXPECT_TRUE(Search(assetList, assets[7]));

            assetList.m_fileInfoList.clear();

            m_assetSeedManager->AddSeedAsset(assets[8], AzFramework::PlatformFlags::Platform_PC | AzFramework::PlatformFlags::Platform_ES3);

            assetList = m_assetSeedManager->GetDependencyList(AzFramework::PlatformId::PC);

            EXPECT_EQ(assetList.m_fileInfoList.size(), 9);
            EXPECT_TRUE(Search(assetList, assets[0]));
            EXPECT_TRUE(Search(assetList, assets[1]));
            EXPECT_TRUE(Search(assetList, assets[2]));
            EXPECT_TRUE(Search(assetList, assets[3]));
            EXPECT_TRUE(Search(assetList, assets[4]));
            EXPECT_TRUE(Search(assetList, assets[5]));
            EXPECT_TRUE(Search(assetList, assets[6]));
            EXPECT_TRUE(Search(assetList, assets[7]));
            EXPECT_TRUE(Search(assetList, assets[8]));

            assetList.m_fileInfoList.clear();
            m_assetSeedManager->RemoveSeedAsset(assets[5], AzFramework::PlatformFlags::Platform_PC | AzFramework::PlatformFlags::Platform_ES3);

            assetList = m_assetSeedManager->GetDependencyList(AzFramework::PlatformId::PC);

            EXPECT_EQ(assetList.m_fileInfoList.size(), 8);
            EXPECT_TRUE(Search(assetList, assets[0]));
            EXPECT_TRUE(Search(assetList, assets[1]));
            EXPECT_TRUE(Search(assetList, assets[2]));
            EXPECT_TRUE(Search(assetList, assets[3]));
            EXPECT_TRUE(Search(assetList, assets[4]));
            EXPECT_TRUE(Search(assetList, assets[6]));
            EXPECT_TRUE(Search(assetList, assets[7]));
            EXPECT_TRUE(Search(assetList, assets[8]));

            // Removing the android flag from the asset should still produce the same result 
            m_assetSeedManager->RemoveSeedAsset(assets[8], AzFramework::PlatformFlags::Platform_ES3);

            assetList = m_assetSeedManager->GetDependencyList(AzFramework::PlatformId::PC);

            EXPECT_EQ(assetList.m_fileInfoList.size(), 8);
            EXPECT_TRUE(Search(assetList, assets[0]));
            EXPECT_TRUE(Search(assetList, assets[1]));
            EXPECT_TRUE(Search(assetList, assets[2]));
            EXPECT_TRUE(Search(assetList, assets[3]));
            EXPECT_TRUE(Search(assetList, assets[4]));
            EXPECT_TRUE(Search(assetList, assets[6]));
            EXPECT_TRUE(Search(assetList, assets[7]));
            EXPECT_TRUE(Search(assetList, assets[8]));

            assetList = m_assetSeedManager->GetDependencyList(AzFramework::PlatformId::ES3);

            EXPECT_EQ(assetList.m_fileInfoList.size(), 5);
            EXPECT_TRUE(Search(assetList, assets[0]));
            EXPECT_TRUE(Search(assetList, assets[1]));
            EXPECT_TRUE(Search(assetList, assets[2]));
            EXPECT_TRUE(Search(assetList, assets[3]));
            EXPECT_TRUE(Search(assetList, assets[4]));

            // Adding the android flag again to the asset 
            m_assetSeedManager->AddSeedAsset(assets[8], AzFramework::PlatformFlags::Platform_ES3);
            assetList = m_assetSeedManager->GetDependencyList(AzFramework::PlatformId::ES3);

            EXPECT_EQ(assetList.m_fileInfoList.size(), 8);
            EXPECT_TRUE(Search(assetList, assets[0]));
            EXPECT_TRUE(Search(assetList, assets[1]));
            EXPECT_TRUE(Search(assetList, assets[2]));
            EXPECT_TRUE(Search(assetList, assets[3]));
            EXPECT_TRUE(Search(assetList, assets[4]));
            EXPECT_TRUE(Search(assetList, assets[6]));
            EXPECT_TRUE(Search(assetList, assets[7]));
            EXPECT_TRUE(Search(assetList, assets[8]));
        }

        void DependencyValidation_EmptyAssetSeed_ListValid()
        {
            AzToolsFramework::AssetFileInfoList assetList = m_assetSeedManager->GetDependencyList(AzFramework::PlatformId::PC);
            EXPECT_EQ(assetList.m_fileInfoList.size(), 0);
        }

        void DependencyValidation_CyclicAssetSeedDependency_ListValid()
        {

            m_assetSeedManager->AddSeedAsset(assets[10], AzFramework::PlatformFlags::Platform_PC);
            m_assetSeedManager->AddSeedAsset(assets[11], AzFramework::PlatformFlags::Platform_PC);

            AzToolsFramework::AssetFileInfoList assetList = m_assetSeedManager->GetDependencyList(AzFramework::PlatformId::PC);

            EXPECT_EQ(assetList.m_fileInfoList.size(), 2);
            EXPECT_TRUE(Search(assetList, assets[10]));
            EXPECT_TRUE(Search(assetList, assets[11]));
        }

        void FileModTimeValidation_SingleAssetSeed_ModTimeChanged()
        {
            int fileIndex = 4;
            m_assetSeedManager->AddSeedAsset(assets[fileIndex], AzFramework::PlatformFlags::Platform_PC);

            AzToolsFramework::AssetFileInfoList assetList1 = m_assetSeedManager->GetDependencyList(AzFramework::PlatformId::PC);

            EXPECT_EQ(assetList1.m_fileInfoList.size(), 1);
            EXPECT_TRUE(Search(assetList1, assets[fileIndex]));
            if (m_fileStreams[0][fileIndex].Open(m_assetsPathFull[0][fileIndex].c_str(), AZ::IO::OpenMode::ModeWrite | AZ::IO::OpenMode::ModeBinary | AZ::IO::OpenMode::ModeCreatePath))
            {
                AZStd::string fileContent = AZStd::string::format("Asset%d.txt", fileIndex);
                m_fileStreams[0][fileIndex].Write(fileContent.size(), fileContent.c_str());
                m_fileStreams[0][fileIndex].Close();
            }

            AzToolsFramework::AssetFileInfoList assetList2 = m_assetSeedManager->GetDependencyList(AzFramework::PlatformId::PC);

            EXPECT_EQ(assetList2.m_fileInfoList.size(), 1);
            EXPECT_TRUE(Search(assetList2, assets[fileIndex]));

            EXPECT_EQ(assetList1.m_fileInfoList[0].m_assetId, assetList2.m_fileInfoList[0].m_assetId);
            EXPECT_GE(assetList2.m_fileInfoList[0].m_modificationTime, assetList1.m_fileInfoList[0].m_modificationTime); // file mod time should change
            
            // file hash should not change
            for (int idx = 0; idx < 5; idx++)
            {
                EXPECT_EQ(assetList1.m_fileInfoList[0].m_hash[idx], assetList2.m_fileInfoList[0].m_hash[idx]);
            }
        }

        void FileHashValidation_SingleAssetSeed_FileHashChanged()
        {
            int fileIndex = 4;
            m_assetSeedManager->AddSeedAsset(assets[fileIndex], AzFramework::PlatformFlags::Platform_PC);

            AzToolsFramework::AssetFileInfoList assetList1 = m_assetSeedManager->GetDependencyList(AzFramework::PlatformId::PC);

            EXPECT_EQ(assetList1.m_fileInfoList.size(), 1);
            EXPECT_TRUE(Search(assetList1, assets[fileIndex]));
            if (m_fileStreams[0][fileIndex].Open(m_assetsPathFull[0][fileIndex].c_str(), AZ::IO::OpenMode::ModeWrite | AZ::IO::OpenMode::ModeBinary | AZ::IO::OpenMode::ModeCreatePath))
            {
                AZStd::string fileContent = AZStd::string::format("Asset%d.txt", fileIndex + 1);// changing file content
                m_fileStreams[0][fileIndex].Write(fileContent.size(), fileContent.c_str());
                m_fileStreams[0][fileIndex].Close();
            }

            AzToolsFramework::AssetFileInfoList assetList2 = m_assetSeedManager->GetDependencyList(AzFramework::PlatformId::PC);

            EXPECT_EQ(assetList2.m_fileInfoList.size(), 1);
            EXPECT_TRUE(Search(assetList2, assets[fileIndex]));

            EXPECT_EQ(assetList1.m_fileInfoList[0].m_assetId, assetList2.m_fileInfoList[0].m_assetId);
            EXPECT_GE(assetList2.m_fileInfoList[0].m_modificationTime, assetList1.m_fileInfoList[0].m_modificationTime);

            bool fileHashDifferent = false;
            // file hash should change since we have modified the file content.
            for (int idx = 0; idx < 5; idx++)
            {
                if (assetList1.m_fileInfoList[0].m_hash[idx] != assetList2.m_fileInfoList[0].m_hash[idx])
                {
                    fileHashDifferent = true;
                    break;
                }
            }

            EXPECT_TRUE(fileHashDifferent);
        }

        void SeedFilePath_UpdatePath_Valid()
        {
            int validFileIndex = 4;
            int invalidFileIndex = 5;
            m_assetSeedManager->AddSeedAsset(assets[validFileIndex], AzFramework::PlatformFlags::Platform_PC, m_assetsPath[invalidFileIndex]);

            const AzFramework::AssetSeedList& oldSeedList = m_assetSeedManager->GetAssetSeedList();
            
            for (const auto& seedInfo : oldSeedList)
            {
                if (seedInfo.m_assetId == assets[validFileIndex])
                {
                    EXPECT_EQ(seedInfo.m_assetRelativePath, m_assetsPath[invalidFileIndex]);
                }
            }

            m_assetSeedManager->UpdateSeedPath();

            const AzFramework::AssetSeedList& newSeedList = m_assetSeedManager->GetAssetSeedList();

            for (const auto& seedInfo : newSeedList)
            {
                if (seedInfo.m_assetId == assets[validFileIndex])
                {
                    EXPECT_EQ(seedInfo.m_assetRelativePath, m_assetsPath[validFileIndex]);
                }
            }
        }

        void SeedFilePath_RemovePath_Valid()
        {
            m_assetSeedManager->RemoveSeedPath();
            const AzFramework::AssetSeedList& seedList = m_assetSeedManager->GetAssetSeedList();

            for (const auto& seedInfo : seedList)
            {
                EXPECT_TRUE(seedInfo.m_assetRelativePath.empty());
            }
        }

        void RemoveSeed_ByAssetId_Valid()
        {
            m_assetSeedManager->AddSeedAsset(assets[0], AzFramework::PlatformFlags::Platform_PC);

            m_assetSeedManager->RemoveSeedAsset(assets[0].ToString<AZStd::string>(), AzFramework::PlatformFlags::Platform_PC);
            const AzFramework::AssetSeedList& seedList = m_assetSeedManager->GetAssetSeedList();

            EXPECT_EQ(seedList.size(), 0);
        }

        void RemoveSeed_ByAssetHint_Valid()
        {
            m_assetSeedManager->AddSeedAsset(assets[0], AzFramework::PlatformFlags::Platform_PC);

            m_pcCatalog->UnregisterAsset(assets[0]); // Unregister the asset from the asset catalog

            m_assetSeedManager->RemoveSeedAsset(m_assetsPath[0], AzFramework::PlatformFlags::Platform_PC);
            const AzFramework::AssetSeedList& seedList = m_assetSeedManager->GetAssetSeedList();
            EXPECT_EQ(seedList.size(), 0);
        }

        AzToolsFramework::AssetSeedManager* m_assetSeedManager;
        AzFramework::AssetRegistry* m_assetRegistry;
        AzToolsFramework::ToolsApplication* m_application;
        AzToolsFramework::PlatformAddressedAssetCatalog* m_pcCatalog;
        AzToolsFramework::PlatformAddressedAssetCatalog* m_es3Catalog;
        AZ::IO::FileIOBase* m_priorFileIO = nullptr;
        AZ::IO::FileIOBase* m_localFileIO = nullptr;
        AZ::IO::FileIOStream m_fileStreams[s_totalTestPlatforms][s_totalAssets];
        AzFramework::PlatformId m_testPlatforms[s_totalTestPlatforms];
        AZStd::string m_assetsPath[s_totalAssets];
        AZStd::string m_assetsPathFull[s_totalTestPlatforms][s_totalAssets];
    };

    TEST_F(AssetSeedManagerTest, AssetSeedManager_SaveSeedListFile_FileIsReadOnly)
    {
        AssetSeedManager_SaveSeedListFile_FileIsReadOnly();
    }

    TEST_F(AssetSeedManagerTest, AssetSeedManager_SaveAssetInfoFile_FileIsReadOnly)
    {
        AssetSeedManager_SaveAssetInfoFile_FileIsReadOnly();
    }

    TEST_F(AssetSeedManagerTest, ValidateSeedFileExtension_CorrectFileExtension_ExpectSuccess)
    {
        ValidateSeedFileExtension_CorrectFileExtension_ExpectSuccess();
    }

    TEST_F(AssetSeedManagerTest, ValidateSeedFileExtension_IncorrectFileExtension_ExpectFailure)
    {
        ValidateSeedFileExtension_IncorrectFileExtension_ExpectFailure();
    }

    TEST_F(AssetSeedManagerTest, ValidateAssetListFileExtension_CorrectFileExtension_ExpectSuccess)
    {
        ValidateAssetListFileExtension_CorrectFileExtension_ExpectSuccess();
    }

    TEST_F(AssetSeedManagerTest, ValidateAssetListFileExtension_IncorrectFileExtension_ExpectFailure)
    {
        ValidateAssetListFileExtension_IncorrectFileExtension_ExpectFailure();
    }

    TEST_F(AssetSeedManagerTest, AddPlatformToAllSeeds_SeedsAreValidForPlatform_AllSeedsUpdated)
    {
        AddPlatformToAllSeeds_SeedsAreValidForPlatform_AllSeedsUpdated();
    }

    TEST_F(AssetSeedManagerTest, AddPlatformToAllSeeds_NotAllSeedsAreValidForPlatform_InvalidSeedsNotChanged)
    {
        AddPlatformToAllSeeds_NotAllSeedsAreValidForPlatform_InvalidSeedsNotChanged();
    }

    TEST_F(AssetSeedManagerTest, RemovePlatformFromAllSeeds_PlatformIsPresentInAllSeeds_PlatformIsRemoved)
    {
        RemovePlatformFromAllSeeds_PlatformIsPresentInAllSeeds_PlatformIsRemoved();
    }

    TEST_F(AssetSeedManagerTest, RemovePlatformFromAllSeeds_SeedsOnlyHaveOnePlatform_SeedsAreNotChanged)
    {
        RemovePlatformFromAllSeeds_SeedsOnlyHaveOnePlatform_SeedsAreNotChanged();
    }

    TEST_F(AssetSeedManagerTest, DependencyValidation_EmptyAssetSeed_ListValid)
    {
        DependencyValidation_EmptyAssetSeed_ListValid();
    }

    TEST_F(AssetSeedManagerTest, DependencyValidation_SingleAssetSeed_ListValid)
    {
        DependencyValidation_SingleAssetSeed_ListValid();
    }

    TEST_F(AssetSeedManagerTest, DependencyValidation_MultipleAssetSeeds_MultiplePlatformFlags_ListValid)
    {
        DependencyValidation_MultipleAssetSeeds_MultiplePlatformFlags_ListValid();
    }

    TEST_F(AssetSeedManagerTest, DependencyValidation_MultipleAssetSeed_ListValid)
    {
        DependencyValidation_MultipleAssetSeed_ListValid();
    }

    TEST_F(AssetSeedManagerTest, FileModTimeValidation_SingleAssetSeed_ModTimeChanged)
    {
        FileModTimeValidation_SingleAssetSeed_ModTimeChanged();
    }

    TEST_F(AssetSeedManagerTest, FileHashValidation_SingleAssetSeed_FileHashChanged)
    {
        FileHashValidation_SingleAssetSeed_FileHashChanged();
    }

    TEST_F(AssetSeedManagerTest, SeedFilePath_UpdatePath_Valid)
    {
        SeedFilePath_UpdatePath_Valid();
    }

    TEST_F(AssetSeedManagerTest, SeedFilePath_RemovePath_Valid)
    {
        SeedFilePath_RemovePath_Valid();
    }

    TEST_F(AssetSeedManagerTest, RemoveSeed_ByAssetId_Valid)
    {
        RemoveSeed_ByAssetId_Valid();
    }

    TEST_F(AssetSeedManagerTest, RemoveSeed_ByAssetHint_Valid)
    {
        RemoveSeed_ByAssetHint_Valid();
    }
}
