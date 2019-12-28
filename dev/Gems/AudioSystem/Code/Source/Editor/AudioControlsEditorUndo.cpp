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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#include <AudioControlsEditorUndo.h>

#include <ATLControlsModel.h>
#include <AudioControlsEditorPlugin.h>

#include <QStandardItemModel>
#include <QList>

namespace AudioControls
{
    //-------------------------------------------------------------------------------------------//
    QStandardItem* GetParent(QATLTreeModel* pTree, const TPath& path)
    {
        QStandardItem* pParent = pTree->invisibleRootItem();
        if (pParent)
        {
            const size_t size = path.size();
            for (size_t i = 0; i < size - 1; ++i)
            {
                pParent = pParent->child(path[(size - 1) - i]);
            }
        }
        return pParent;
    }

    //-------------------------------------------------------------------------------------------//
    QStandardItem* GetItem(QATLTreeModel* pTree, const TPath& path)
    {
        QStandardItem* pParent = GetParent(pTree, path);
        if (pParent && path.size() > 0)
        {
            return pParent->child(path[0]);
        }
        return nullptr;
    }

    //-------------------------------------------------------------------------------------------//
    void UpdatePath(QStandardItem* pItem, TPath& path)
    {
        if (pItem)
        {
            path.clear();
            path.push_back(pItem->row());
            QStandardItem* pParent = pItem->parent();
            while (pParent)
            {
                path.push_back(pParent->row());
                pParent = pParent->parent();
            }
        }
    }

    //-------------------------------------------------------------------------------------------//
    void IUndoControlOperation::AddStoredControl()
    {
        const CUndoSuspend suspendUndo;
        CATLControlsModel* pModel = CAudioControlsEditorPlugin::GetATLModel();
        QATLTreeModel* pTree = CAudioControlsEditorPlugin::GetControlsTree();
        if (pModel && pTree)
        {
            if (m_pStoredControl)
            {
                pModel->InsertControl(m_pStoredControl);
                m_id = m_pStoredControl->GetId();

                QStandardItem* pParent = GetParent(pTree, m_path);
                if (pParent && m_path.size() > 0)
                {
                    pTree->AddControl(m_pStoredControl.get(), pParent, m_path[0]);
                }
            }
            m_pStoredControl = nullptr;
        }
    }

    //-------------------------------------------------------------------------------------------//
    void IUndoControlOperation::RemoveStoredControl()
    {
        const CUndoSuspend suspendUndo;
        CATLControlsModel* pModel = CAudioControlsEditorPlugin::GetATLModel();
        QATLTreeModel* pTree = CAudioControlsEditorPlugin::GetControlsTree();
        if (pModel && pTree)
        {
            m_pStoredControl = pModel->TakeControl(m_id);
            QStandardItem* pItem = pTree->GetItemFromControlID(m_id);
            if (pItem)
            {
                UpdatePath(pItem, m_path);
                pTree->RemoveItem(pTree->indexFromItem(pItem));
            }
        }
    }

    //-------------------------------------------------------------------------------------------//
    CUndoControlAdd::CUndoControlAdd(CID id)
    {
        m_id = id;
    }

    //-------------------------------------------------------------------------------------------//
    void CUndoControlAdd::Undo(bool bUndo)
    {
        RemoveStoredControl();
    }

    //-------------------------------------------------------------------------------------------//
    void CUndoControlAdd::Redo()
    {
        AddStoredControl();
    }

    //-------------------------------------------------------------------------------------------//
    CUndoControlRemove::CUndoControlRemove(AZStd::shared_ptr<CATLControl>& pControl)
    {
        CUndoSuspend suspendUndo;
        m_pStoredControl = pControl;
        QATLTreeModel* pTree = CAudioControlsEditorPlugin::GetControlsTree();
        if (pTree)
        {
            QStandardItem* pItem = pTree->GetItemFromControlID(m_pStoredControl->GetId());
            if (pItem)
            {
                UpdatePath(pItem, m_path);
            }
        }
    }

    //-------------------------------------------------------------------------------------------//
    void CUndoControlRemove::Undo(bool bUndo)
    {
        AddStoredControl();
    }

    //-------------------------------------------------------------------------------------------//
    void CUndoControlRemove::Redo()
    {
        RemoveStoredControl();
    }

    //-------------------------------------------------------------------------------------------//
    IUndoFolderOperation::IUndoFolderOperation(QStandardItem* pItem)
    {
        if (pItem)
        {
            m_sName = pItem->text();
            UpdatePath(pItem, m_path);
        }
    }

    //-------------------------------------------------------------------------------------------//
    void IUndoFolderOperation::AddFolderItem()
    {
        CUndoSuspend suspendUndo;
        QATLTreeModel* pTree = CAudioControlsEditorPlugin::GetControlsTree();
        if (pTree)
        {
            QStandardItem* pParent = GetParent(pTree, m_path);
            if (pParent && m_path.size() > 0)
            {
                pTree->CreateFolder(pParent, m_sName.toUtf8().data(), m_path[0]);
            }
        }
    }

    //-------------------------------------------------------------------------------------------//
    void IUndoFolderOperation::RemoveItem()
    {
        CUndoSuspend suspendUndo;
        QATLTreeModel* pTree = CAudioControlsEditorPlugin::GetControlsTree();
        if (pTree)
        {
            QStandardItem* pItem = GetItem(pTree, m_path);
            if (pItem)
            {
                pTree->RemoveItem(pTree->indexFromItem(pItem));
            }
        }
    }

    //-------------------------------------------------------------------------------------------//
    CUndoFolderRemove::CUndoFolderRemove(QStandardItem* pItem)
        : IUndoFolderOperation(pItem)
    {
    }

    //-------------------------------------------------------------------------------------------//
    void CUndoFolderRemove::Undo(bool bUndo)
    {
        AddFolderItem();
    }

    //-------------------------------------------------------------------------------------------//
    void CUndoFolderRemove::Redo()
    {
        RemoveItem();
    }

    //-------------------------------------------------------------------------------------------//
    CUndoFolderAdd::CUndoFolderAdd(QStandardItem* pItem)
        : IUndoFolderOperation(pItem)
    {
    }

    //-------------------------------------------------------------------------------------------//
    void CUndoFolderAdd::Undo(bool bUndo)
    {
        RemoveItem();
    }

    //-------------------------------------------------------------------------------------------//
    void CUndoFolderAdd::Redo()
    {
        AddFolderItem();
    }

    //-------------------------------------------------------------------------------------------//
    CUndoControlModified::CUndoControlModified(CID id)
        : m_id(id)
    {
        CATLControlsModel* pModel = CAudioControlsEditorPlugin::GetATLModel();
        if (pModel)
        {
            CATLControl* pControl = pModel->GetControlByID(m_id);
            if (pControl)
            {
                m_name = pControl->GetName();
                m_scope = pControl->GetScope();
                m_bAutoLoad = pControl->IsAutoLoad();
                m_groupPerPlatform = pControl->m_groupPerPlatform;
                m_connectedControls = pControl->m_connectedControls;
            }
        }
    }

    //-------------------------------------------------------------------------------------------//
    void CUndoControlModified::Undo(bool bUndo)
    {
        SwapData();
    }

    //-------------------------------------------------------------------------------------------//
    void CUndoControlModified::Redo()
    {
        SwapData();
    }

    //-------------------------------------------------------------------------------------------//
    void CUndoControlModified::SwapData()
    {
        const CUndoSuspend suspendUndo;
        CATLControlsModel* pModel = CAudioControlsEditorPlugin::GetATLModel();
        if (pModel)
        {
            CATLControl* pControl = pModel->GetControlByID(m_id);
            if (pControl)
            {
                AZStd::string name = pControl->GetName();
                AZStd::string scope = pControl->GetScope();
                bool bAutoLoad = pControl->IsAutoLoad();
                AZStd::map<AZStd::string, int> groupPerPlatform = pControl->m_groupPerPlatform;
                AZStd::vector<TConnectionPtr> connectedControls = pControl->m_connectedControls;

                pControl->SetName(m_name);
                pControl->SetScope(m_scope);
                pControl->SetAutoLoad(m_bAutoLoad);
                pControl->m_groupPerPlatform = m_groupPerPlatform;
                pControl->m_connectedControls = m_connectedControls;
                pModel->OnControlModified(pControl);

                m_name = name;
                m_scope = scope;
                m_bAutoLoad = bAutoLoad;
                m_groupPerPlatform = groupPerPlatform;
                m_connectedControls = connectedControls;
            }
        }
    }

    //-------------------------------------------------------------------------------------------//
    void CUndoItemMove::Undo(bool bUndo)
    {
        QATLTreeModel* pTree = CAudioControlsEditorPlugin::GetControlsTree();
        if (pTree)
        {
            if (!bModifiedInitialised)
            {
                Copy(pTree->invisibleRootItem(), m_modified.invisibleRootItem());
                bModifiedInitialised = true;
            }

            pTree->clear();
            Copy(m_original.invisibleRootItem(), pTree->invisibleRootItem());
        }
    }

    //-------------------------------------------------------------------------------------------//
    void CUndoItemMove::Redo()
    {
        QATLTreeModel* pTree = CAudioControlsEditorPlugin::GetControlsTree();
        if (pTree)
        {
            pTree->clear();
            Copy(m_modified.invisibleRootItem(), pTree->invisibleRootItem());
        }
    }

    //-------------------------------------------------------------------------------------------//
    void CUndoItemMove::Copy(QStandardItem* pSource, QStandardItem* pDest)
    {
        if (pSource && pDest)
        {
            for (int i = 0; i < pSource->rowCount(); ++i)
            {
                QStandardItem* pSourceItem = pSource->child(i);
                QStandardItem* pDestItem = pSourceItem->clone();
                Copy(pSourceItem, pDestItem);
                pDest->appendRow(pDestItem);
            }
        }
    }

    //-------------------------------------------------------------------------------------------//
    CUndoItemMove::CUndoItemMove()
        : bModifiedInitialised(false)
    {
        QATLTreeModel* pTree = CAudioControlsEditorPlugin::GetControlsTree();
        if (pTree)
        {
            Copy(pTree->invisibleRootItem(), m_original.invisibleRootItem());
        }
    }
} // namespace AudioControls
