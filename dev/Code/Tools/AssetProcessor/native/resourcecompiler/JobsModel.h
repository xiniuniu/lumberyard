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

#include <AzToolsFramework/API/EditorAssetSystemAPI.h>
#include <native/resourcecompiler/RCCommon.h>
#include <QAbstractItemModel>
#include <QDateTime>
#include <QHash>
#include <QIcon>
#include <QVector>

// Don't reorder above RCCommon
#include <native/assetprocessor.h>

// Do this here, rather than EditorAssetSystemAPI.h so that we don't have to link against Qt5Core to
// use EditorAssetSystemAPI.h
Q_DECLARE_METATYPE(AzToolsFramework::AssetSystem::JobStatus);

namespace AssetProcessor
{
    //! CachedJobInfo stores all the necessary information needed for showing a particular job including its log
    struct CachedJobInfo
    {
        QueueElementID m_elementId;
        QDateTime m_completedTime;
        AzToolsFramework::AssetSystem::JobStatus m_jobState;
        AZ::u32 m_warningCount;
        AZ::u32 m_errorCount;
        unsigned int m_jobRunKey;
        AZ::Uuid m_builderGuid;
    };
    /**
    * The JobsModel class contains list of jobs from both the Database and the RCController
    */
    class JobsModel
        : public QAbstractItemModel
    {
        Q_OBJECT
    public:

        enum DataRoles
        {
            logRole = Qt::UserRole + 1,
            statusRole,
            logFileRole,
            SortRole,
        };

        enum Column
        {
            ColumnStatus,
            ColumnSource,
            ColumnCompleted,
            ColumnPlatform,
            ColumnJobKey,
            Max
        };

        explicit JobsModel(QObject* parent = nullptr);
        virtual ~JobsModel();
        QModelIndex parent(const QModelIndex& index) const override;
        QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
        int columnCount(const QModelIndex& parent = QModelIndex()) const override;
        int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
        QVariant data(const QModelIndex& index, int role) const override;
        int itemCount() const;
        CachedJobInfo* getItem(int index) const;
        static QString GetStatusInString(const AzToolsFramework::AssetSystem::JobStatus& state, AZ::u32 warningCount, AZ::u32 errorCount);
        void PopulateJobsFromDatabase();

public Q_SLOTS:
        void OnJobStatusChanged(JobEntry entry, AzToolsFramework::AssetSystem::JobStatus status);
        void OnJobRemoved(AzToolsFramework::AssetSystem::JobInfo jobInfo);
        void OnSourceRemoved(QString sourceDatabasePath);
        void OnFolderRemoved(QString folderPath);

    protected:
        QIcon m_pendingIcon;
        QIcon m_errorIcon;
        QIcon m_warningIcon;
        QIcon m_okIcon;
        QIcon m_processingIcon;
        AZStd::vector<CachedJobInfo*> m_cachedJobs;
        QHash<AssetProcessor::QueueElementID, int> m_cachedJobsLookup; // QVector uses int as type of index.  

        void RemoveJob(const AssetProcessor::QueueElementID& elementId);
    };

} //namespace AssetProcessor