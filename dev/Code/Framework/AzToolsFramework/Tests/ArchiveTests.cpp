/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of thistoolsApp
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Math/UUID.h>
#include <AzCore/Memory/Memory.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzTestShared/Utils/Utils.h>
#include <AzToolsFramework/Archive/ArchiveAPI.h>
#include <AzToolsFramework/Application/ToolsApplication.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzToolsFramework/Archive/ArchiveAPI.h>
#include <AzToolsFramework/AssetBundle/AssetBundleAPI.h>
#include <QString>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>

namespace UnitTest
{
    namespace
    {

        bool CreateDummyFile(const QString& fullPathToFile, const QString& tempStr = {})
        {
            QFileInfo fi(fullPathToFile);
            QDir fp(fi.path());
            fp.mkpath(".");
            QFile writer(fullPathToFile);
            if (!writer.open(QFile::ReadWrite))
            {
                return false;
            }
            {
                QTextStream stream(&writer);
                stream << tempStr << endl;
            }
            writer.close();
            return true;
        }

        class ArchiveTest :
            public ::testing::Test
        {

        public:
            QStringList CreateArchiveFileList()
            {
                QStringList returnList;
                returnList.append("basicfile.txt");
                returnList.append("basicfile2.txt");
                returnList.append("testfolder/folderfile.txt");
                returnList.append("testfolder2/sharedfolderfile.txt");
                returnList.append("testfolder2/sharedfolderfile2.txt");
                returnList.append("testfolder3/testfolder4/depthfile.bat");

                return returnList;
            }

            QString GetArchiveFolderName()
            {
                return "Archive";
            }

            void CreateArchiveFolder( QString archiveFolderName, QStringList fileList )
            {
                QDir tempPath = QDir(m_tempDir.path()).filePath(archiveFolderName);

                for (const auto& thisFile : fileList)
                {
                    QString absoluteTestFilePath = tempPath.absoluteFilePath(thisFile);
                    EXPECT_TRUE(CreateDummyFile(absoluteTestFilePath));
                }
            }

            void CreateArchiveFolder()
            {
                CreateArchiveFolder(GetArchiveFolderName(), CreateArchiveFileList());
            }

            QString GetArchivePath()
            {
                return  QDir(m_tempDir.path()).filePath("TestArchive.pak");
            }

            QString GetArchiveFolder()
            {
                return QDir(m_tempDir.path()).filePath(GetArchiveFolderName());
            }

            bool CreateArchive()
            {
                bool createResult{ false };
                AzToolsFramework::ArchiveCommandsBus::BroadcastResult(createResult, &AzToolsFramework::ArchiveCommandsBus::Events::CreateArchiveBlocking, GetArchivePath().toStdString().c_str(), GetArchiveFolder().toStdString().c_str());
                return createResult;
            }

            void SetUp() override
            {
                m_app.reset(aznew AzToolsFramework::ToolsApplication);
                m_app->Start(AzFramework::Application::Descriptor());
            }

            void TearDown() override
            {
                m_app->Stop();
                m_app.reset();
            }

            AZStd::unique_ptr<AzToolsFramework::ToolsApplication> m_app;
            QTemporaryDir m_tempDir {QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath("ArchiveTests-")};
        };

        TEST_F(ArchiveTest, CreateArchiveBlocking_FilesAtThreeDepths_ArchiveCreated)
        {
            EXPECT_TRUE(m_tempDir.isValid());
            CreateArchiveFolder();

            bool createResult = CreateArchive();

            EXPECT_EQ(createResult, true);
        }

        TEST_F(ArchiveTest, ListFilesInArchiveBlocking_FilesAtThreeDepths_FilesFound)
        {
            EXPECT_TRUE(m_tempDir.isValid());
            CreateArchiveFolder();
            
            EXPECT_EQ(CreateArchive(), true);

            AZStd::vector<AZStd::string> fileList;
            bool listResult{ false };
            AzToolsFramework::ArchiveCommandsBus::BroadcastResult(listResult, &AzToolsFramework::ArchiveCommandsBus::Events::ListFilesInArchiveBlocking, GetArchivePath().toStdString().c_str(), fileList);

            EXPECT_EQ(fileList.size(), 6);
        }

        TEST_F(ArchiveTest, CreateDeltaCatalog_AssetsNotRegistered_Failure)
        {
            QStringList fileList = CreateArchiveFileList();

            CreateArchiveFolder(GetArchiveFolderName(), fileList);

            bool createResult = CreateArchive();

            EXPECT_EQ(createResult, true);

            bool catalogCreated{ true };
            AZ::Test::AssertAbsorber assertAbsorber;
            AzToolsFramework::AssetBundleCommandsBus::BroadcastResult(catalogCreated, &AzToolsFramework::AssetBundleCommandsBus::Events::CreateDeltaCatalog, GetArchivePath().toStdString().c_str(), true);

            EXPECT_EQ(catalogCreated, false);
        }

        TEST_F(ArchiveTest, CreateDeltaCatalog_ArchiveWithoutCatalogAssetsRegistered_Success)
        {
            QStringList fileList = CreateArchiveFileList();

            CreateArchiveFolder(GetArchiveFolderName(), fileList);

            bool createResult = CreateArchive();

            EXPECT_EQ(createResult, true);

            for (const auto& thisPath : fileList)
            {
                AZ::Data::AssetInfo newInfo;
                newInfo.m_relativePath = thisPath.toStdString().c_str();
                newInfo.m_assetType = AZ::Uuid::CreateRandom();
                newInfo.m_sizeBytes = 100; // Arbitrary
                AZ::Data::AssetId generatedID(AZ::Uuid::CreateRandom());
                newInfo.m_assetId = generatedID;

                AZ::Data::AssetCatalogRequestBus::Broadcast(&AZ::Data::AssetCatalogRequestBus::Events::RegisterAsset, generatedID, newInfo);
            }

            bool catalogCreated{ false };
            AzToolsFramework::AssetBundleCommandsBus::BroadcastResult(catalogCreated, &AzToolsFramework::AssetBundleCommandsBus::Events::CreateDeltaCatalog, GetArchivePath().toStdString().c_str(), true);
            EXPECT_EQ(catalogCreated, true);
        }
    }

}
