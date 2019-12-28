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
#include <QMouseEvent>

#include <GraphCanvas/Widgets/NodePalette/NodePaletteTreeView.h>

#include <GraphCanvas/Widgets/NodePalette/TreeItems/NodePaletteTreeItem.h>
#include <GraphCanvas/Widgets/NodePalette/Model/NodePaletteSortFilterProxyModel.h>

namespace GraphCanvas
{
    ////////////////////////
    // NodePaletteTreeView
    ////////////////////////

    NodePaletteTreeView::NodePaletteTreeView(QWidget* parent)
        : AzToolsFramework::QTreeViewWithStateSaving(parent)
        , m_lastItem(nullptr)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setDragEnabled(true);
        setHeaderHidden(true);
        setAutoScroll(true);
        setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
        setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);
        setDragDropMode(QAbstractItemView::DragDropMode::DragOnly);
        setMouseTracking(true);

        QObject::connect(this, &QTreeView::clicked, this, &NodePaletteTreeView::OnClicked);

        QObject::connect(this,
            &AzToolsFramework::QTreeViewWithStateSaving::entered,
            [this](const QModelIndex &modelIndex)
            {
                UpdatePointer(modelIndex, false);
            });
    }

    void NodePaletteTreeView::resizeEvent(QResizeEvent* event)
    {
        resizeColumnToContents(0);
        QTreeView::resizeEvent(event);
    }

    void NodePaletteTreeView::selectionChanged(const QItemSelection& selected, const QItemSelection &deselected)
    {
        bool lookupModel = true;
        const NodePaletteSortFilterProxyModel* proxyModel = nullptr;

        for (const QModelIndex& selectedIndex : selected.indexes())
        {
            // IMPORTANT: mapToSource() is NECESSARY. Otherwise the internalPointer
            // in the index is relative to the proxy model, NOT the source model.
            QModelIndex sourceIndex = selectedIndex;

            if (lookupModel)
            {
                lookupModel = false;
                proxyModel = qobject_cast<const NodePaletteSortFilterProxyModel*>(sourceIndex.model());
            }

            if (proxyModel)
            {
                sourceIndex = proxyModel->mapToSource(sourceIndex);
            }

            GraphCanvas::NodePaletteTreeItem* treeItem = static_cast<GraphCanvas::NodePaletteTreeItem*>(sourceIndex.internalPointer());

            if (treeItem)
            {
                treeItem->SetSelected(true);
            }
        }

        for (const QModelIndex& deselectedIndex : deselected.indexes())
        {
            QModelIndex sourceIndex = deselectedIndex;

            if (lookupModel)
            {
                lookupModel = false;
                proxyModel = qobject_cast<const NodePaletteSortFilterProxyModel*>(sourceIndex.model());
            }

            if (proxyModel)
            {
                sourceIndex = proxyModel->mapToSource(sourceIndex);
            }

            GraphCanvas::NodePaletteTreeItem* treeItem = static_cast<GraphCanvas::NodePaletteTreeItem*>(sourceIndex.internalPointer());

            if (treeItem)
            {
                treeItem->SetSelected(false);
            }
        }

        AzToolsFramework::QTreeViewWithStateSaving::selectionChanged(selected, deselected);
    }

    void NodePaletteTreeView::mousePressEvent(QMouseEvent* ev)
    {
        UpdatePointer(indexAt(ev->pos()), true);

        AzToolsFramework::QTreeViewWithStateSaving::mousePressEvent(ev);
    }

    void NodePaletteTreeView::mouseMoveEvent(QMouseEvent* ev)
    {
        QModelIndex index = indexAt(ev->pos());
        UpdatePointer(index, false);
        
        if (index.isValid())
        {
            // IMPORTANT: mapToSource() is NECESSARY. Otherwise the internalPointer
            // in the index is relative to the proxy model, NOT the source model.
            QModelIndex sourceIndex = index;

            const NodePaletteSortFilterProxyModel* proxyModel = qobject_cast<const NodePaletteSortFilterProxyModel*>(sourceIndex.model());

            if (proxyModel)
            {
                sourceIndex = proxyModel->mapToSource(sourceIndex);
            }

            GraphCanvas::NodePaletteTreeItem* treeItem = static_cast<GraphCanvas::NodePaletteTreeItem*>(sourceIndex.internalPointer());
            if (treeItem && m_lastItem != treeItem)
            {
                if (m_lastItem)
                {
                    m_lastItem->SetHovered(false);
                }

                treeItem->SetHovered(true);
                m_lastItem = treeItem;
            }
        }
        else if (m_lastItem)
        {
            m_lastItem->SetHovered(false);
            m_lastItem = nullptr;
        }

        AzToolsFramework::QTreeViewWithStateSaving::mouseMoveEvent(ev);
    }

    void NodePaletteTreeView::mouseReleaseEvent(QMouseEvent* ev)
    {
        UpdatePointer(indexAt(ev->pos()), false);

        AzToolsFramework::QTreeViewWithStateSaving::mouseReleaseEvent(ev);
    }

    void NodePaletteTreeView::leaveEvent(QEvent* ev)
    {
        if (m_lastItem)
        {
            m_lastItem->SetHovered(false);
            m_lastItem = nullptr;
        }
    }

    void NodePaletteTreeView::OnClicked(const QModelIndex& modelIndex)
    {
        // IMPORTANT: mapToSource() is NECESSARY. Otherwise the internalPointer
        // in the index is relative to the proxy model, NOT the source model.
        QModelIndex sourceIndex = modelIndex;

        const NodePaletteSortFilterProxyModel* proxyModel = qobject_cast<const NodePaletteSortFilterProxyModel*>(sourceIndex.model());

        if (proxyModel)
        {
            sourceIndex = proxyModel->mapToSource(sourceIndex);
        }

        GraphCanvas::NodePaletteTreeItem* treeItem = static_cast<GraphCanvas::NodePaletteTreeItem*>(sourceIndex.internalPointer());

        if (treeItem)
        {
            treeItem->SignalClicked(sourceIndex.column());
        }
    }

    void NodePaletteTreeView::UpdatePointer(const QModelIndex &modelIndex, bool isMousePressed)
    {
        if (!modelIndex.isValid())
        {
            setCursor(Qt::ArrowCursor);
            return;
        }

        // IMPORTANT: mapToSource() is NECESSARY. Otherwise the internalPointer
        // in the index is relative to the proxy model, NOT the source model.
        QModelIndex sourceIndex = modelIndex;
        
        const NodePaletteSortFilterProxyModel* proxyModel = qobject_cast<const NodePaletteSortFilterProxyModel*>(modelIndex.model());
        
        if (proxyModel)
        {
            sourceIndex = proxyModel->mapToSource(sourceIndex);
        }
        
        GraphCanvas::NodePaletteTreeItem* treeItem = static_cast<GraphCanvas::NodePaletteTreeItem*>(sourceIndex.internalPointer());
        if(treeItem)
        {
            if(treeItem->Flags(QModelIndex()) & Qt::ItemIsDragEnabled && isMousePressed)
            {
                setCursor(Qt::ClosedHandCursor);
            }
            else
            {
                setCursor(Qt::ArrowCursor);
            }
        }
    }

    #include <StaticLib/GraphCanvas/Widgets/NodePalette/NodePaletteTreeView.moc>
}
