﻿/*
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

#include <QSimpleAudioControlListWidget.h>

#include <ACEEnums.h>
#include <AudioControlsEditorPlugin.h>
#include <IAudioSystemEditor.h>
#include <QAudioControlEditorIcons.h>

#include <QApplication>
#include <QMimeData>

namespace AudioControls
{
    //-------------------------------------------------------------------------------------------//
    class QMiddlewareControlItem
        : public QTreeWidgetItem
    {
    public:
        bool operator< (const QTreeWidgetItem& other) const override
        {
            TImplControlType type = data(0, eMDR_TYPE).toUInt();
            TImplControlType otherType = other.data(0, eMDR_TYPE).toUInt();
            if (type == otherType)
            {
                return text(0) < other.text(0);
            }
            return type < otherType;
        }
    };

    //-------------------------------------------------------------------------------------------//
    QSimpleAudioControlListWidget::QSimpleAudioControlListWidget(QWidget* parent)
        : QTreeWidget(parent)
        , m_connectedColor(QColor(0x99, 0x99, 0x99))
        , m_disconnectedColor(QColor(0xf3, 0x81, 0x1d))
        , m_localisedColor(QColor(0x42, 0x85, 0xf4))
    {
        CATLControlsModel* pATLModel = CAudioControlsEditorPlugin::GetATLModel();
        if (pATLModel)
        {
            pATLModel->AddListener(this);
        }
    }

    //-------------------------------------------------------------------------------------------//
    QSimpleAudioControlListWidget::~QSimpleAudioControlListWidget()
    {
        CATLControlsModel* pATLModel = CAudioControlsEditorPlugin::GetATLModel();
        if (pATLModel)
        {
            pATLModel->RemoveListener(this);
        }
    }

    //-------------------------------------------------------------------------------------------//
    void QSimpleAudioControlListWidget::UpdateModel()
    {
        setSortingEnabled(false);
        Refresh(true);
        sortByColumn(0, Qt::AscendingOrder);
        setSortingEnabled(true);
    }

    //-------------------------------------------------------------------------------------------//
    void QSimpleAudioControlListWidget::LoadControls()
    {
        clear();
        AudioControls::IAudioSystemEditor* pAudioSystemEditorImpl = CAudioControlsEditorPlugin::GetAudioSystemEditorImpl();
        if (pAudioSystemEditorImpl)
        {
            IAudioSystemControl* pControl = pAudioSystemEditorImpl->GetRoot();
            if (pControl)
            {
                LoadControl(pControl, invisibleRootItem());
            }
        }
    }

    //-------------------------------------------------------------------------------------------//
    void QSimpleAudioControlListWidget::LoadControl(IAudioSystemControl* pControl, QTreeWidgetItem* pRoot)
    {
        if (pControl)
        {
            size_t size = pControl->ChildCount();
            for (size_t i = 0; i < size; ++i)
            {
                IAudioSystemControl* pChild = pControl->GetChildAt(i);
                if (pChild && !pChild->IsPlaceholder())
                {
                    QTreeWidgetItem* pItem = InsertControl(pChild, pRoot);
                    if (pItem)
                    {
                        LoadControl(pChild, pItem);
                    }
                }
            }
        }
    }

    //-------------------------------------------------------------------------------------------//
    void QSimpleAudioControlListWidget::UpdateControl(AudioControls::IAudioSystemControl* pControl)
    {
        if (pControl)
        {
            QTreeWidgetItem* pItem = GetItem(pControl->GetId(), pControl->IsLocalised());
            if (pItem)
            {
                InitItemData(pItem, pControl);
            }
        }
    }

    //-------------------------------------------------------------------------------------------//
    QTreeWidgetItem* QSimpleAudioControlListWidget::InsertControl(AudioControls::IAudioSystemControl* pControl, QTreeWidgetItem* pRoot)
    {
        if (pRoot && pControl)
        {
            QTreeWidgetItem* pItem = new QMiddlewareControlItem();
            pItem->setText(0, QString(pControl->GetName().c_str()));
            InitItemData(pItem, pControl);
            pRoot->addChild(pItem);
            return pItem;
        }
        return nullptr;
    }

    //-------------------------------------------------------------------------------------------//
    QTreeWidgetItem* QSimpleAudioControlListWidget::GetItem(AudioControls::CID id, bool bLocalised)
    {
        QTreeWidgetItemIterator it(this);
        while (*it)
        {
            QTreeWidgetItem* item = *it;
            if (GetItemId(item) == id && IsLocalised(item) == bLocalised)
            {
                return item;
            }
            ++it;
        }
        return nullptr;
    }

    //-------------------------------------------------------------------------------------------//
    TImplControlType QSimpleAudioControlListWidget::GetControlType(QTreeWidgetItem* item)
    {
        if (item)
        {
            return (TImplControlType) item->data(0, eMDR_TYPE).toUInt();
        }
        return AUDIO_IMPL_INVALID_TYPE;
    }

    //-------------------------------------------------------------------------------------------//
    AudioControls::CID QSimpleAudioControlListWidget::GetItemId(QTreeWidgetItem* item)
    {
        if (item)
        {
            return (AudioControls::CID) item->data(0, eMDR_ID).toUInt();
        }
        return ACE_INVALID_CID;
    }

    //-------------------------------------------------------------------------------------------//
    bool QSimpleAudioControlListWidget::IsLocalised(QTreeWidgetItem* item)
    {
        return item->data(0, eMDR_LOCALISED).toBool();
    }

    //-------------------------------------------------------------------------------------------//
    bool QSimpleAudioControlListWidget::IsConnected(QTreeWidgetItem* item)
    {
        if (item)
        {
            return item->data(0, eMDR_CONNECTED).toBool();
        }
        return false;
    }

    //-------------------------------------------------------------------------------------------//
    void QSimpleAudioControlListWidget::InitItem(QTreeWidgetItem* pItem)
    {
        AudioControls::IAudioSystemEditor* pAudioSystemEditorImpl = CAudioControlsEditorPlugin::GetAudioSystemEditorImpl();
        if (pItem && pAudioSystemEditorImpl)
        {
            TImplControlType type = GetControlType(pItem);
            EACEControlType compatibleType = pAudioSystemEditorImpl->ImplTypeToATLType(type);

            pItem->setIcon(0, QIcon(QString(pAudioSystemEditorImpl->GetTypeIcon(type).data())));
            pItem->setFlags(pItem->flags() & ~Qt::ItemIsDropEnabled);

            if (compatibleType != eACET_NUM_TYPES)
            {
                pItem->setFlags(pItem->flags() | Qt::ItemIsDragEnabled);
                if (pItem->data(0, eMDR_LOCALISED).toBool())
                {
                    pItem->setToolTip(0, tr("Localized control"));
                    pItem->setForeground(0, m_localisedColor);
                }
                else
                {
                    if (IsConnected(pItem))
                    {
                        pItem->setForeground(0, m_connectedColor);
                    }
                    else
                    {
                        pItem->setToolTip(0, tr("Unassigned control"));
                        pItem->setForeground(0, m_disconnectedColor);
                    }
                }
            }
            else
            {
                pItem->setFlags(pItem->flags() & ~Qt::ItemIsDragEnabled);
            }
        }
    }

    //-------------------------------------------------------------------------------------------//
    void QSimpleAudioControlListWidget::InitItemData(QTreeWidgetItem* pItem, AudioControls::IAudioSystemControl* pControl)
    {
        if (pItem && pControl)
        {
            pItem->setData(0, eMDR_ID, pControl->GetId());
            if (pControl->GetId() != ACE_INVALID_CID)
            {
                pItem->setData(0, eMDR_TYPE, pControl->GetType());
                pItem->setData(0, eMDR_LOCALISED, pControl->IsLocalised());
                pItem->setData(0, eMDR_CONNECTED, pControl->IsConnected());
            }
            InitItem(pItem);
        }
    }

    //-------------------------------------------------------------------------------------------//
    ControlList QSimpleAudioControlListWidget::GetSelectedIds()
    {
        ControlList ids;
        QList<QTreeWidgetItem*> selected = selectedItems();
        int size = selected.length();
        for (int i = 0; i < size; ++i)
        {
            ids.push_back(GetItemId(selected[i]));
        }
        return ids;
    }

    //-------------------------------------------------------------------------------------------//
    void QSimpleAudioControlListWidget::Refresh(bool reload)
    {
        AudioControls::IAudioSystemEditor* pAudioSystemEditorImpl = CAudioControlsEditorPlugin::GetAudioSystemEditorImpl();
        if (pAudioSystemEditorImpl)
        {
            // store the currently selected control to select it again
            ControlList ids = GetSelectedIds();

            if (reload)
            {
                LoadControls();
            }

            QTreeWidgetItemIterator it(this);
            while (*it)
            {
                InitItem(*it);
                ++it;
            }

            // select the control that was previously selected
            size_t size = ids.size();
            for (size_t i = 0; i < size; ++i)
            {
                QTreeWidgetItem* pItem = GetItem(ids[i], false);
                if (pItem)
                {
                    pItem->setSelected(true);
                    setCurrentItem(pItem, 0);
                    scrollToItem(pItem);
                }
            }
        }
    }

    //-------------------------------------------------------------------------------------------//
    void QSimpleAudioControlListWidget::OnConnectionAdded(CATLControl* pControl, IAudioSystemControl* pMiddlewareControl)
    {
        UpdateControl(pMiddlewareControl);
    }

    //-------------------------------------------------------------------------------------------//
    void QSimpleAudioControlListWidget::OnConnectionRemoved(CATLControl* pControl, IAudioSystemControl* pMiddlewareControl)
    {
        UpdateControl(pMiddlewareControl);
    }
} // namespace AudioControls

#include <Source/Editor/QSimpleAudioControlListWidget.moc>
