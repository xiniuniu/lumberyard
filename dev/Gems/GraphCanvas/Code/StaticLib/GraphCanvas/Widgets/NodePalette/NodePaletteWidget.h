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

#include <QAction>
#include <QTimer>
#include <qlabel.h>
#include <qitemselectionmodel.h>
#include <qstyleditemdelegate.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzQtComponents/Components/StyledDockWidget.h>

#include <AzToolsFramework/UI/UICore/QTreeViewStateSaver.hxx>

#include <GraphCanvas/Widgets/GraphCanvasTreeModel.h>
#include <GraphCanvas/Widgets/StyledItemDelegates/IconDecoratedNameDelegate.h>

#include <GraphCanvas/Editor/AssetEditorBus.h>

class QSortFilterProxyModel;

namespace Ui
{
    class NodePaletteWidget;
}

namespace GraphCanvas
{
    class GraphCanvasMimeEvent;
    class NodePaletteSortFilterProxyModel;
    class NodePaletteDockWidget;
    
    class NodePaletteTreeDelegate : public IconDecoratedNameDelegate
    {
    public:
        AZ_CLASS_ALLOCATOR(NodePaletteTreeDelegate, AZ::SystemAllocator, 0);

        NodePaletteTreeDelegate(QWidget* parent = nullptr);
        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    };

    struct NodePaletteConfig
    {
        GraphCanvasTreeItem* m_rootTreeItem = nullptr;
        EditorId m_editorId;

        const char* m_mimeType = "";

        AZStd::string_view m_saveIdentifier;

        bool m_isInContextMenu = false;
        bool m_clearSelectionOnSceneChange = true;

        bool m_allowArrowKeyNavigation = true;
    };

    class NodePaletteWidget
        : public QWidget
        , public AssetEditorNotificationBus::Handler
        , public GraphCanvasTreeModelRequestBus::Handler
    {
        Q_OBJECT

    private:
        friend class NodePaletteDockWidget;

    public:
        NodePaletteWidget(QWidget* parent);
        ~NodePaletteWidget();

        void SetupNodePalette(const NodePaletteConfig& paletteConfig);

        void FocusOnSearchFilter();

        void ResetModel(GraphCanvasTreeItem* rootItem = nullptr);
        void ResetDisplay();

        GraphCanvasMimeEvent* GetContextMenuEvent() const;

        void ResetSourceSlotFilter();
        void FilterForSourceSlot(const AZ::EntityId& sceneId, const AZ::EntityId& sourceSlotId);

        void SetItemDelegate(NodePaletteTreeDelegate* itemDelegate);

        void AddSearchCustomizationWidget(QWidget* widget);
        void ConfigureSearchCustomizationMargins(const QMargins& margins, int elementSpacing);

        // AssetEditorNotificationBus
        void PreOnActiveGraphChanged() override;
        void PostOnActiveGraphChanged() override;
        ////

        // GraphCanvasTreeModelRequestBus::Handler
        void ClearSelection() override;
        ////

        // NodeTreeView
        const GraphCanvasTreeItem* GetTreeRoot() const;
        QTreeView* GetTreeView() const;
        ////

        // QWidget
        bool eventFilter(QObject* object, QEvent* event) override;
        ////

    protected:        

        // This method here is to help facilitate resetting the model. This will not be called during
        // the initial construction(because yay virtual functions).
        virtual GraphCanvasTreeItem* CreatePaletteRoot() const;

    public slots:
        void OnSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
        void OnScrollChanged(int scrollPosition);

    signals:
        void OnCreateSelection();
        void OnSelectionCleared();
        void OnTreeItemSelected(const GraphCanvasTreeItem* treeItem);
        void OnTreeItemDoubleClicked(GraphCanvasTreeItem* treeItem);

    private:

        void RefreshFloatingHeader();
        void OnFilterTextChanged();
        void UpdateFilter();
        void ClearFilter();

        void OnIndexDoubleClicked(const QModelIndex& index);

        // Will try and spawn the item specified by the QCompleter
        void TrySpawnItem();

        void HandleSelectedItem(const GraphCanvasTreeItem* treeItem);

        AZStd::string m_mimeType;
        AZStd::string m_saveIdentifier;

        bool m_isInContextMenu;
        bool m_searchFieldSelectionChange;

        AZStd::unique_ptr<Ui::NodePaletteWidget> m_ui;
        NodePaletteTreeDelegate* m_itemDelegate;

        EditorId m_editorId;
        GraphCanvasMimeEvent* m_contextMenuCreateEvent;

        QTimer        m_filterTimer;
        NodePaletteSortFilterProxyModel* m_model;
    };
}
