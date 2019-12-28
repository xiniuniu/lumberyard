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

#include <QCompleter>
#include <QSortFilterProxyModel>

#include <AzCore/Component/EntityId.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/std/containers/unordered_set.h>

namespace GraphCanvas
{
    class GraphCanvasTreeItem;
    class NodePaletteSortFilterProxyModel;

    // Should be a private class of the other model.
    // Needs to be external because of Q_OBJECT not supporting nested classes.
    class NodePaletteAutoCompleteModel
        : public QAbstractItemModel
    {
        Q_OBJECT
    public:
        friend class NodePaletteSortFilterProxyModel;

        AZ_CLASS_ALLOCATOR(NodePaletteAutoCompleteModel, AZ::SystemAllocator, 0);

        NodePaletteAutoCompleteModel();
        ~NodePaletteAutoCompleteModel() = default;

        QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
        QModelIndex parent(const QModelIndex& index = QModelIndex()) const override;
        int columnCount(const QModelIndex& parent = QModelIndex()) const override;
        int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

        const GraphCanvas::GraphCanvasTreeItem* FindItemForIndex(const QModelIndex& index);

    private:

        void ClearAvailableItems();
        void AddAvailableItem(const GraphCanvas::GraphCanvasTreeItem* item);

        AZStd::vector<const GraphCanvas::GraphCanvasTreeItem*> m_availableItems;
    };

    class NodePaletteSortFilterProxyModel
        : public QSortFilterProxyModel
    {
        Q_OBJECT
    public:
        AZ_CLASS_ALLOCATOR(NodePaletteSortFilterProxyModel, AZ::SystemAllocator, 0);

        NodePaletteSortFilterProxyModel(QObject* parent);

        bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

        void PopulateUnfilteredModel();

        void ResetSourceSlotFilter();
        void FilterForSourceSlot(const AZ::EntityId& sceneId, const AZ::EntityId& sourceSlotId);

        bool HasFilter() const;
        void SetFilter(const QString& filter);
        void ClearFilter();

        QCompleter* GetCompleter();

    private:

        QCompleter m_unfilteredCompleter;
        QCompleter m_sourceSlotCompleter;

        NodePaletteAutoCompleteModel* m_unfilteredAutoCompleteModel;
        NodePaletteAutoCompleteModel* m_sourceSlotAutoCompleteModel;

        bool m_hasSourceSlotFilter;
        AZStd::unordered_set<const GraphCanvas::GraphCanvasTreeItem*> m_sourceSlotFilter;

        QString m_filter;
        QRegExp m_filterRegex;
    };
}
