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

#include <AzToolsFramework/AssetBrowser/AssetBrowserFilterModel.h>

namespace AzToolsFramework
{
    using namespace AzToolsFramework::AssetBrowser;
    
    //! Model storing all the files that can be suggested in the Asset Autocompleter for PropertyAssetCtrl
    class AssetCompleterModel
        : public QAbstractTableModel
    {
        Q_OBJECT

    public:
        AZ_CLASS_ALLOCATOR(AssetCompleterModel, AZ::SystemAllocator, 0);
        explicit AssetCompleterModel(QObject* parent = nullptr);
        ~AssetCompleterModel();

        int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        int columnCount(const QModelIndex &parent = QModelIndex()) const override;
        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

        void SetFilter(AZ::Data::AssetType filterType);
        void SearchStringHighlight(QString searchString);

        Qt::ItemFlags flags(const QModelIndex &index) const override;

        const AZStd::string_view GetNameFromIndex(const QModelIndex& index);
        const AZ::Data::AssetId GetAssetIdFromIndex(const QModelIndex& index);

    private:
        struct AssetItem 
        {
            AZStd::string m_displayName;
            AZStd::string m_path;
            AZ::Data::AssetId m_assetId;
        };

        AssetBrowserEntry* GetAssetEntry(QModelIndex index) const;
        void FetchResources(AssetBrowserFilterModel* filter, QModelIndex index);

        //! Stores list of assets to suggest via the autocompleter
        AZStd::vector<AssetItem> m_assets;
        //! String that will be highlighted in the suggestions
        QString m_highlightString;
    };

}