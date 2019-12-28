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

#include "FileStateCacheTests.h"
#include <unittests/UnitTestRunner.h>
#include <utilities/assetUtils.h>

namespace UnitTests
{
    void FileStateCacheTests::SetUp()
    {
        m_temporarySourceDir = QDir(m_temporaryDir.path());
        m_fileStateCache = AZStd::make_unique<FileStateCache>();
    }

    void FileStateCacheTests::TearDown()
    {
        m_fileStateCache = nullptr;
    }

    void FileStateCacheTests::CheckForFile(QString path, bool shouldExist)
    {
        bool exists = false;
        FileStateInfo fileInfo;

        FileStateRequestBus::BroadcastResult(exists, &FileStateRequestBus::Events::Exists, path);

        ASSERT_EQ(exists, shouldExist);

        exists = false;

        FileStateRequestBus::BroadcastResult(exists, &FileStateRequestBus::Events::GetFileInfo, path, &fileInfo);

        ASSERT_EQ(exists, shouldExist);

        if (exists)
        {
            ASSERT_EQ(AssetUtilities::NormalizeFilePath(fileInfo.m_absolutePath), AssetUtilities::NormalizeFilePath(path));
            ASSERT_FALSE(fileInfo.m_isDirectory);
            ASSERT_EQ(fileInfo.m_fileSize, 0);
        }
    }

    TEST_F(FileStateCacheTests, QueryFile_ShouldNotExist)
    {
        QString testPath = m_temporarySourceDir.absoluteFilePath("test.txt");

        // Make the file but don't tell the cache about it
        ASSERT_TRUE(UnitTestUtils::CreateDummyFile(testPath));

        CheckForFile(testPath, false);
    }

    TEST_F(FileStateCacheTests, QueryAddedFile_ShouldExist)
    {
        QString testPath = m_temporarySourceDir.absoluteFilePath("test.txt");

        ASSERT_TRUE(UnitTestUtils::CreateDummyFile(testPath));

        m_fileStateCache->AddFile(testPath);

        CheckForFile(testPath, true);
    }

    TEST_F(FileStateCacheTests, QueryBulkAddedFile_ShouldExist)
    {
        QString testPath = m_temporarySourceDir.absoluteFilePath("test.txt");

        ASSERT_TRUE(UnitTestUtils::CreateDummyFile(testPath));

        QSet<AssetFileInfo> infoSet;
        AssetFileInfo fileInfo;
        fileInfo.m_filePath = testPath;
        fileInfo.m_isDirectory = false;
        fileInfo.m_fileSize = 0;
        fileInfo.m_modTime = QFileInfo(testPath).lastModified();
        infoSet.insert(fileInfo);

        m_fileStateCache->AddInfoSet(infoSet);

        CheckForFile(testPath, true);
    }

    TEST_F(FileStateCacheTests, QueryRemovedFile_ShouldNotExist)
    {
        QString testPath = m_temporarySourceDir.absoluteFilePath("test.txt");

        ASSERT_TRUE(UnitTestUtils::CreateDummyFile(testPath));

        m_fileStateCache->AddFile(testPath);
        m_fileStateCache->RemoveFile(testPath);

        CheckForFile(testPath, false);
    }

    TEST_F(FileStateCacheTests, AddAndRemoveFolder_ShouldAddAndRemoveSubFiles)
    {
        QDir testFolder = m_temporarySourceDir.absoluteFilePath("subfolder");
        QString testPath1 = testFolder.absoluteFilePath("test1.txt");
        QString testPath2 = testFolder.absoluteFilePath("test2.txt");

        ASSERT_TRUE(UnitTestUtils::CreateDummyFile(testPath1));
        ASSERT_TRUE(UnitTestUtils::CreateDummyFile(testPath2));

        m_fileStateCache->AddFile(testFolder.absolutePath());

        CheckForFile(testPath1, true);
        CheckForFile(testPath2, true);

        m_fileStateCache->RemoveFile(testFolder.absolutePath());

        CheckForFile(testPath1, false);
        CheckForFile(testPath2, false);
    }

    TEST_F(FileStateCacheTests, UpdateFileAndQuery_ShouldExist)
    {
        QString testPath = m_temporarySourceDir.absoluteFilePath("test.txt");

        ASSERT_TRUE(UnitTestUtils::CreateDummyFile(testPath));

        QSet<AssetFileInfo> infoSet;
        AssetFileInfo fileInfo;
        fileInfo.m_filePath = testPath;
        fileInfo.m_isDirectory = false;
        fileInfo.m_fileSize = 1234; // Setting the file size to non-zero (even though the actual file is 0), UpdateFile should update this to 0 and allow CheckForFile to pass as a result
        fileInfo.m_modTime = QFileInfo(testPath).lastModified();
        infoSet.insert(fileInfo);

        m_fileStateCache->AddInfoSet(infoSet);
        m_fileStateCache->UpdateFile(testPath);

        CheckForFile(testPath, true);
    }

    TEST_F(FileStateCacheTests, PassthroughTest)
    {
        m_fileStateCache = nullptr; // Need to release the existing one first since only one handler can exist for the ebus
        m_fileStateCache = AZStd::make_unique<FileStatePassthrough>();
        QString testPath = m_temporarySourceDir.absoluteFilePath("test.txt");

        CheckForFile(testPath, false);

        ASSERT_TRUE(UnitTestUtils::CreateDummyFile(testPath));

        CheckForFile(testPath, true);
    }

    TEST_F(FileStateCacheTests, HandlesMixedSeperators)
    {
        QSet<AssetFileInfo> infoSet;
        AssetFileInfo fileInfo;
        fileInfo.m_filePath = R"(c:\some/test\file.txt)";
        infoSet.insert(fileInfo);

        m_fileStateCache->AddInfoSet(infoSet);

        CheckForFile(R"(c:\some\test\file.txt)", true);
        CheckForFile(R"(c:/some/test/file.txt)", true);
    }
}