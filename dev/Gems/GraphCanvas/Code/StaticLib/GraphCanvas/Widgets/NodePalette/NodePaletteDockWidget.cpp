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
#include <QBoxLayout>
#include <QEvent>
#include <QCompleter>
#include <QCoreApplication>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QSignalBlocker>
#include <QScrollBar>

#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/std/containers/map.h>
#include <AzCore/UserSettings/UserSettings.h>

#include <AzFramework/StringFunc/StringFunc.h>

#include <AzToolsFramework/API/EditorAssetSystemAPI.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

#include <GraphCanvas/Widgets/NodePalette/NodePaletteDockWidget.h>
#include <StaticLib/GraphCanvas/Widgets/NodePalette/ui_NodePaletteDockWidget.h>

#include <GraphCanvas/Widgets/GraphCanvasTreeModel.h>
#include <GraphCanvas/Widgets/NodePalette/TreeItems/NodePaletteTreeItem.h>
#include <GraphCanvas/Widgets/NodePalette/Model/NodePaletteSortFilterProxyModel.h>

namespace GraphCanvas
{
    //////////////////////////
    // NodePaletteDockWidget
    //////////////////////////

    NodePaletteDockWidget::NodePaletteDockWidget(GraphCanvasTreeItem* treeItem, const EditorId& editorId, const QString& windowLabel, QWidget* parent, const char* mimeType, bool inContextMenu, AZStd::string_view identifier)
        : AzQtComponents::StyledDockWidget(parent)
        , m_ui(new Ui::NodePaletteDockWidget())
        , m_editorId(editorId)
    {
        setWindowTitle(windowLabel);
        m_ui->setupUi(this);

        NodePaletteConfig config;

        config.m_rootTreeItem = treeItem;
        config.m_editorId = editorId;
        config.m_mimeType = mimeType;
        config.m_isInContextMenu = inContextMenu;
        config.m_saveIdentifier = identifier;

        m_ui->nodePaletteWidget->SetupNodePalette(config);

        if (inContextMenu)
        {
            setTitleBarWidget(new QWidget());
            setFeatures(NoDockWidgetFeatures);
            setContentsMargins(15, 0, 0, 0);
            m_ui->dockWidgetContents->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        }

        QObject::connect(m_ui->nodePaletteWidget, &NodePaletteWidget::OnCreateSelection, this, &NodePaletteDockWidget::OnContextMenuSelection);
        QObject::connect(m_ui->nodePaletteWidget, &NodePaletteWidget::OnTreeItemDoubleClicked, this, &NodePaletteDockWidget::OnTreeItemDoubleClicked);
    }

    NodePaletteDockWidget::NodePaletteDockWidget(QWidget* parent, const QString& windowLabel, const NodePaletteConfig& nodePaletteConfig)
        : AzQtComponents::StyledDockWidget(parent)
        , m_ui(new Ui::NodePaletteDockWidget())
        , m_editorId(nodePaletteConfig.m_editorId)
    {
        setWindowTitle(windowLabel);
        m_ui->setupUi(this);        

        m_ui->nodePaletteWidget->SetupNodePalette(nodePaletteConfig);

        if (nodePaletteConfig.m_isInContextMenu)
        {
            setTitleBarWidget(new QWidget());
            setFeatures(NoDockWidgetFeatures);
            setContentsMargins(15, 0, 0, 0);
            m_ui->dockWidgetContents->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        }

        QObject::connect(m_ui->nodePaletteWidget, &NodePaletteWidget::OnCreateSelection, this, &NodePaletteDockWidget::OnContextMenuSelection);
        QObject::connect(m_ui->nodePaletteWidget, &NodePaletteWidget::OnTreeItemDoubleClicked, this, &NodePaletteDockWidget::OnTreeItemDoubleClicked);
    }

    NodePaletteDockWidget::~NodePaletteDockWidget()
    {
    }

    void NodePaletteDockWidget::FocusOnSearchFilter()
    {
        m_ui->nodePaletteWidget->FocusOnSearchFilter();
    }

    void NodePaletteDockWidget::ResetModel()
    {
        m_ui->nodePaletteWidget->ResetModel(CreatePaletteRoot());
    }

    void NodePaletteDockWidget::ResetDisplay()
    {
        m_ui->nodePaletteWidget->ResetDisplay();

        setVisible(true);
    }

    GraphCanvasMimeEvent* NodePaletteDockWidget::GetContextMenuEvent() const
    {
        return m_ui->nodePaletteWidget->GetContextMenuEvent();
    }

    void NodePaletteDockWidget::ResetSourceSlotFilter()
    {
        m_ui->nodePaletteWidget->ResetSourceSlotFilter();
    }

    void NodePaletteDockWidget::FilterForSourceSlot(const AZ::EntityId& sceneId, const AZ::EntityId& sourceSlotId)
    {
        m_ui->nodePaletteWidget->FilterForSourceSlot(sceneId, sourceSlotId);
    }

    void NodePaletteDockWidget::SetItemDelegate(NodePaletteTreeDelegate* itemDelegate)
    {
        m_ui->nodePaletteWidget->SetItemDelegate(itemDelegate);
    }

    void NodePaletteDockWidget::AddHeaderWidget(QWidget* widget)
    {
        m_ui->headerCustomization->layout()->addWidget(widget);
    }

    void NodePaletteDockWidget::ConfigureHeaderMargins(const QMargins& margins, int elementSpacing)
    {
        m_ui->headerCustomization->layout()->setContentsMargins(margins);
        m_ui->headerCustomization->layout()->setSpacing(elementSpacing);
    }

    void NodePaletteDockWidget::AddFooterWidget(QWidget* widget)
    {
        m_ui->footerCustomization->layout()->addWidget(widget);
    }

    void NodePaletteDockWidget::ConfigureFooterMargins(const QMargins& margins, int elementSpacing)
    {
        m_ui->footerCustomization->layout()->setContentsMargins(margins);
        m_ui->footerCustomization->layout()->setSpacing(elementSpacing);
    }

    void NodePaletteDockWidget::AddSearchCustomizationWidget(QWidget* widget)
    {
        m_ui->nodePaletteWidget->AddSearchCustomizationWidget(widget);
    }

    void NodePaletteDockWidget::ConfigureSearchCustomizationMargins(const QMargins& margins, int elementSpacing)
    {
        m_ui->nodePaletteWidget->ConfigureSearchCustomizationMargins(margins, elementSpacing);
    }

    const GraphCanvasTreeItem* NodePaletteDockWidget::GetTreeRoot() const
    {
        return m_ui->nodePaletteWidget->GetTreeRoot();
    }

    QTreeView* NodePaletteDockWidget::GetTreeView() const
    {
        return m_ui->nodePaletteWidget->GetTreeView();
    }

    NodePaletteWidget* NodePaletteDockWidget::GetNodePaletteWidget() const
    {
        return m_ui->nodePaletteWidget;
    }

    GraphCanvasTreeItem* NodePaletteDockWidget::CreatePaletteRoot() const
    {
        return m_ui->nodePaletteWidget->CreatePaletteRoot();
    }

    #include <StaticLib/GraphCanvas/Widgets/NodePalette/NodePaletteDockWidget.moc>
}
