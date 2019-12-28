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

#include <QTabBar>
#include <QMetaType>

#include <AzCore/Component/EntityId.h>
#include <AzCore/Asset/AssetCommon.h>

#include <ScriptCanvas/Bus/DocumentContextBus.h>

class QGraphicsView;
class QVBoxLayout;

namespace ScriptCanvasEditor
{
    namespace Widget
    {
        struct GraphTabMetadata
        {
            AZ::Data::AssetId m_assetId;
            QWidget* m_hostWidget = nullptr;
            QString m_tabName;
            ScriptCanvasFileState m_fileState = ScriptCanvasFileState::INVALID;
        };

        class GraphTabBar
            : public QTabBar
            , protected DocumentContextNotificationBus::MultiHandler
        {
            Q_OBJECT

        public:

            GraphTabBar(QWidget* parent = nullptr);

            // Adds a new tab to the bar
            void AddGraphTab(const AZ::Data::AssetId& assetId, AZStd::string_view tabName = {});
            void InsertGraphTab(int tabIndex, const AZ::Data::AssetId& assetId, AZStd::string_view tabName = {});
            bool SelectTab(const AZ::Data::AssetId& assetId);

            int FindTab(const AZ::Data::AssetId& assetId) const;
            AZ::Data::AssetId FindAssetId(int tabIndex);

            //! Removes all tabs from the bar
            void RemoveAllBars();

            // Updates the tab at the supplied index with the GraphTabMetadata
            // The host widget field of the tabMetadata is not used and will not overwrite the tab data
            bool SetGraphTabData(int tabIndex, GraphTabMetadata tabMetadata);
            void SetTabText(int tabIndex, const QString& path, ScriptCanvasFileState fileState = ScriptCanvasFileState::INVALID);

            // Closes a tab and cleans up Metadata
            void CloseTab(int index);

            void OnContextMenu(const QPoint& point);

            void mouseReleaseEvent(QMouseEvent* event) override;

        Q_SIGNALS:
            void TabInserted(int index);
            void TabRemoved(int index);
            // Emits a signal to close the tab which is distinct from pressing the close button the actual tab bar.
            // This allows handling of the close tab button being pressed different than the actual closing of the tab.
            // Pressing the close tab button will prompt the user to save file in tab if it is modified
            void TabCloseNoButton(int index); 

            void SaveTab(int index);
            void CloseAllTabs();
            void CloseAllTabsBut(int index);
            void CopyPathToClipboard(int index);

        protected:
            void tabInserted(int index) override;
            void tabRemoved(int index) override;
            void OnAssetModificationStateChanged(ScriptCanvasFileState fileState) override;

        private:
            // Called when the selected tab changes
            void currentChangedTab(int index);

        };
    }
}

Q_DECLARE_METATYPE(ScriptCanvasEditor::Widget::GraphTabMetadata);
