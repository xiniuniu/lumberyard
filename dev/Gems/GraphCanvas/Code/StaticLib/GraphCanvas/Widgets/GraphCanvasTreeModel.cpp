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
#include <qobject.h>

#include <GraphCanvas/Widgets/GraphCanvasTreeModel.h>

namespace GraphCanvas
{
    /////////////////////////
    // GraphCanvasTreeModel
    /////////////////////////

    void GraphCanvasTreeModel::Reflect(AZ::ReflectContext* reflectContext)
    {        
        GraphCanvasMimeContainer::Reflect(reflectContext);
    }

    GraphCanvasTreeModel::GraphCanvasTreeModel(GraphCanvasTreeItem* treeRoot, QObject* parent)
        : QAbstractItemModel(parent)
        , m_treeRoot(treeRoot)
    {
        layoutAboutToBeChanged();
        m_treeRoot->RegisterModel(this);
        layoutChanged();
    }

    QModelIndex GraphCanvasTreeModel::index(int row, int column, const QModelIndex& parent) const
    {
        if (!hasIndex(row, column, parent))
        {
            return QModelIndex();
        }

        GraphCanvasTreeItem* parentItem = nullptr;

        if (!parent.isValid())
        {
            parentItem = m_treeRoot.get();
        }
        else
        {
            parentItem = static_cast<GraphCanvasTreeItem*>(parent.internalPointer());
        }

        GraphCanvasTreeItem* childItem = parentItem->FindChildByRow(row);

        if (childItem)
        {
            return createIndex(row, column, childItem);
        }
        else
        {
            return QModelIndex();
        }
    }

    QModelIndex GraphCanvasTreeModel::parent(const QModelIndex& index) const
    {
        if (!index.isValid())
        {
            return QModelIndex();
        }

        GraphCanvasTreeItem* childItem = static_cast<GraphCanvasTreeItem*>(index.internalPointer());
        GraphCanvasTreeItem* parentItem = childItem->GetParent();

        if ((!parentItem) || (parentItem == m_treeRoot.get()))
        {
            return QModelIndex();
        }

        return createIndex(parentItem->FindRowUnderParent(), index.column(), parentItem);
    }

    int GraphCanvasTreeModel::columnCount(const QModelIndex& parent) const
    {
        if (parent.isValid() && parent.internalPointer() != nullptr)
        {
            return static_cast<GraphCanvasTreeItem*>(parent.internalPointer())->GetColumnCount();
        }
        else
        {
            return m_treeRoot->GetColumnCount();
        }
    }

    int GraphCanvasTreeModel::rowCount(const QModelIndex& parent) const
    {
        GraphCanvasTreeItem* parentItem = nullptr;

        if (parent.column() > 0)
        {
            return 0;
        }

        if (!parent.isValid())
        {
            parentItem = m_treeRoot.get();
        }
        else
        {
            parentItem = static_cast<GraphCanvasTreeItem*>(parent.internalPointer());
        }

        return parentItem->GetChildCount();
    }

    QVariant GraphCanvasTreeModel::data(const QModelIndex& index, int role) const
    {
        if (!index.isValid())
        {
            return QVariant();
        }

        GraphCanvasTreeItem* item = static_cast<GraphCanvasTreeItem*>(index.internalPointer());
        if (!item)
        {
            return QVariant();
        }

        return item->Data(index, role);
    }

    bool GraphCanvasTreeModel::setData(const QModelIndex& index, const QVariant& value, int role)
    {
        if (!index.isValid())
        {
            return false;
        }

        GraphCanvasTreeItem* item = static_cast<GraphCanvasTreeItem*>(index.internalPointer());
        return item->SetData(index, value, role);
    }

    Qt::ItemFlags GraphCanvasTreeModel::flags(const QModelIndex& index) const
    {
        if (!index.isValid())
        {
            return 0;
        }

        GraphCanvasTreeItem* item = static_cast<GraphCanvasTreeItem*>(index.internalPointer());
        return item->Flags(index);
    }

    void GraphCanvasTreeModel::setMimeType(const char* mimeType)
    {
        m_mimeType = mimeType;
    }

    QStringList GraphCanvasTreeModel::mimeTypes() const
    {
        QStringList list;
        list.append(m_mimeType);
        return list;
    }

    QMimeData* GraphCanvasTreeModel::mimeData(const QModelIndexList& indexes) const
    {
        if (m_mimeType.isEmpty())
        {
            return nullptr;
        }

        GraphCanvasMimeContainer container;
        for (const QModelIndex& index : indexes)
        {
            if (index.column() == 0)
            {
                GraphCanvasTreeItem* item = static_cast<GraphCanvasTreeItem*>(index.internalPointer());
                GraphCanvasMimeEvent* mimeEvent = item->CreateMimeEvent();

                if (mimeEvent)
                {
                    container.m_mimeEvents.push_back(mimeEvent);
                }
            }
        }

        if (container.m_mimeEvents.empty())
        {
            return nullptr;
        }

        AZStd::vector<char> encoded;
        if (!container.ToBuffer(encoded))
        {
            return nullptr;
        }

        QMimeData* mimeDataPtr = new QMimeData();
        QByteArray encodedData;
        encodedData.resize((int)encoded.size());
        memcpy(encodedData.data(), encoded.data(), encoded.size());
        mimeDataPtr->setData(m_mimeType, encodedData);

        return mimeDataPtr;
    }

    bool GraphCanvasTreeModel::removeRows(int row, int count, const QModelIndex &parent)
    {
        GraphCanvasTreeItem* parentItem = static_cast<GraphCanvasTreeItem*>(parent.internalPointer());

        if (parent.isValid())
        {
            if (parentItem == nullptr)
            {
                return false;
            }
        }
        else
        {
            parentItem = m_treeRoot.get();
        }

        if (row > parentItem->m_childItems.size())
        {
            AZ_Error("Graph Canvas", false, "Trying to remove invalid row from GraphCanvasTreeModel.");
            return false;
        }
        else if (row + count > parentItem->m_childItems.size())
        {
            AZ_Warning("Graph Canvas", false, "Trying to remove too many rows from GraphCanvasTreeModel.");
            count = (static_cast<int>(parentItem->m_childItems.size()) - row);
        }

        if (count == 0)
        {
            return true;
        }

        GraphCanvasTreeModelRequestBus::Event(this, &GraphCanvasTreeModelRequests::ClearSelection);

        beginRemoveRows(parent, row, row + (count - 1));

        for (int i = 0; i < count; ++i)
        {
            GraphCanvasTreeItem* childItem = parentItem->m_childItems[row + i];
            childItem->RemoveParent(parentItem);

            if (parentItem->m_deleteRemoveChildren)
            {
                delete childItem;
            }
        }

        parentItem->m_childItems.erase(parentItem->m_childItems.begin() + row, parentItem->m_childItems.begin() + row + count);

        endRemoveRows();

        return true;
    }

    const GraphCanvas::GraphCanvasTreeItem* GraphCanvasTreeModel::GetTreeRoot() const
    {
        return m_treeRoot.get();
    }

    QModelIndex GraphCanvasTreeModel::CreateIndex(GraphCanvasTreeItem* treeItem, int column)
    {
        if (treeItem == m_treeRoot.get())
        {
            return QModelIndex();
        }

        return createIndex(treeItem->FindRowUnderParent(), column, treeItem);
    }

    QModelIndex GraphCanvasTreeModel::CreateParentIndex(GraphCanvasTreeItem* treeItem, int column)
    {
        return parent(CreateIndex(treeItem, column));
    }

    void GraphCanvasTreeModel::ChildAboutToBeAdded(GraphCanvasTreeItem* treeItem, int position)
    {
        if (position < 0)
        {
            position = treeItem->GetChildCount() - 1;
        }

        beginInsertRows(CreateIndex(treeItem), position, position);
    }

    void GraphCanvasTreeModel::OnChildAdded()
    {
        endInsertRows();
    }
}