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

#include <QTreeView>

#include <AzCore/Memory/SystemAllocator.h>

#include <AzToolsFramework/UI/UICore/QTreeViewStateSaver.hxx>

namespace GraphCanvas
{
    class NodePaletteTreeItem;

    class NodePaletteTreeView
        : public AzToolsFramework::QTreeViewWithStateSaving
    {
        Q_OBJECT

    public:
        AZ_CLASS_ALLOCATOR(NodePaletteTreeView, AZ::SystemAllocator, 0);
        explicit NodePaletteTreeView(QWidget* parent = nullptr);
        
        void resizeEvent(QResizeEvent* event) override;

        void selectionChanged(const QItemSelection& selected, const QItemSelection &deselected) override;

    protected:
        void mousePressEvent(QMouseEvent* ev) override;
        void mouseMoveEvent(QMouseEvent* ev) override;
        void mouseReleaseEvent(QMouseEvent* ev) override;
        void leaveEvent(QEvent* ev) override;

        void OnClicked(const QModelIndex& modelIndex);

    private:
        void UpdatePointer(const QModelIndex &modelIndex, bool isMousePressed);

        QModelIndex m_lastIndex;
        NodePaletteTreeItem* m_lastItem;
    };
}
