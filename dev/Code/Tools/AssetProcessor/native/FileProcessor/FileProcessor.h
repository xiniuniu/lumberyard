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

#include <QObject>
#include <QString>
#include <QMap>

#include <native/AssetDatabase/AssetDatabase.h>
#include <native/assetprocessor.h>

namespace AzToolsFramework
{
    namespace AssetDatabase 
    {
        class FileDatabaseEntry;
    }
}

namespace AssetProcessor
{
    class PlatformConfiguration;

    class FileProcessor
        : public QObject
    {
        Q_OBJECT
    public:
        explicit FileProcessor(PlatformConfiguration* config);
        ~FileProcessor();

    public Q_SLOTS:
        //! AssetScanner changed its status
        void OnAssetScannerStatusChange(AssetScanningStatus status);
        
        //! AssetScanner found a file
        void AssessFilesFromScanner(QSet<AssetFileInfo> files);
        
        //! AssetScanner found a folder
        void AssessFoldersFromScanner(QSet<AssetFileInfo> folders);

        //! FileWatcher detected added file
        void AssessAddedFile(QString fileName);
        //! FileWatcher detected removed file
        void AssessDeletedFile(QString fileName);
        //! Synchronize AssetScanner data with Files table
        void Sync();

        //! its time to shut down!
        void QuitRequested();
    Q_SIGNALS:
        void ReadyToQuit(QObject* source); //After receiving QuitRequested, you must send this when its safe

    private:
        PlatformConfiguration* m_platformConfig = nullptr;
        AZStd::shared_ptr<AssetDatabaseConnection> m_connection;
        //! Files and folders located by AssetScanner during a scan
        QList<AssetFileInfo> m_filesInAssetScanner;
        QString m_normalizedCacheRootPath;

        bool m_shutdownSignalled = false;
        bool GetRelativePath(QString& filePath, QString& relativeFileName, QString& scanFolder) const;
        bool DeleteFileRecursive(const AzToolsFramework::AssetDatabase::FileDatabaseEntry& file) const;
    };
} // namespace AssetProcessor
