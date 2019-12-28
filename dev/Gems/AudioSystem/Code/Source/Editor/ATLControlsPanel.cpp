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

#include <ATLControlsPanel.h>

#include <AzFramework/StringFunc/StringFunc.h>

#include <ACEEnums.h>
#include <ATLControlsModel.h>
#include <AudioControl.h>
#include <AudioControlsEditorPlugin.h>
#include <CryFile.h>
#include <CryPath.h>
#include <Cry_Camera.h>
#include <IAudioSystem.h>
#include <IAudioSystemControl.h>
#include <IAudioSystemEditor.h>
#include <IEditor.h>
#include <QAudioControlEditorIcons.h>

#include <QWidgetAction>
#include <QPushButton>
#include <QPaintEvent>
#include <QPainter>
#include <QMessageBox>
#include <QMimeData>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QKeyEvent>
#include <QSortFilterProxyModel>
#include <QModelIndex>

namespace AudioControls
{
    //-------------------------------------------------------------------------------------------//
    class QFilterButton
        : public QPushButton
    {
    public:
        QFilterButton(const QIcon& icon, const QString& text, QWidget* parent = 0)
            : QPushButton(icon, text, parent)
        {
            setStyleSheet("text-align: left;");
            setCheckable(true);
            setFlat(true);
        }
    protected:
        void paintEvent(QPaintEvent* event)
        {
            QPushButton::paintEvent(event);
            if (isChecked())
            {
                QPainter painter(this);
                const int heightPadding = 4;
                const int widthPadding = 3;
                painter.setPen(QPen(QApplication::palette().color(QPalette::Highlight), 2));
                painter.drawLine(QPoint(event->rect().width() - widthPadding, heightPadding),
                    QPoint(event->rect().width() - widthPadding, event->rect().height() - heightPadding));
            }
        }
    };

    //-------------------------------------------------------------------------------------------//
    CATLControlsPanel::CATLControlsPanel(CATLControlsModel* pATLModel, QATLTreeModel* pATLControlsModel)
        : m_pATLModel(pATLModel)
        , m_pTreeModel(pATLControlsModel)
        , m_showUnassignedControls(false)
    {
        setupUi(this);

        m_pATLControlsTree->installEventFilter(this);
        m_pATLControlsTree->viewport()->installEventFilter(this);

        // ************ Context Menu ************
        m_addItemMenu.addAction(GetControlTypeIcon(eACET_TRIGGER), tr("Trigger"), this, SLOT(CreateTriggerControl()));
        m_addItemMenu.addAction(GetControlTypeIcon(eACET_RTPC), tr("RTPC"), this, SLOT(CreateRTPCControl()));
        m_addItemMenu.addAction(GetControlTypeIcon(eACET_SWITCH), tr("Switch"), this, SLOT(CreateSwitchControl()));
        m_addItemMenu.addAction(GetControlTypeIcon(eACET_ENVIRONMENT), tr("Environment"), this, SLOT(CreateEnvironmentsControl()));
        m_addItemMenu.addAction(GetControlTypeIcon(eACET_PRELOAD), tr("Preload"), this, SLOT(CreatePreloadControl()));
        m_addItemMenu.addSeparator();
        m_addItemMenu.addAction(GetFolderIcon(), tr("Folder"), this, SLOT(CreateFolder()));
        m_pAddButton->setMenu(&m_addItemMenu);
        m_pATLControlsTree->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_pATLControlsTree, SIGNAL(customContextMenuRequested(const QPoint&)), SLOT(ShowControlsContextMenu(const QPoint&)));
        // *********************************

        // ************ Filtering ************
        const int margin = 5;
        for (int i = 0; i < eACET_NUM_TYPES; ++i)
        {
            EACEControlType type = (EACEControlType)i;
            QWidgetAction* pWidgetAction = new QWidgetAction(this);
            QWidget* pWidget = new QWidget(this);
            QHBoxLayout* pLayout = new QHBoxLayout(pWidget);
            pLayout->setSpacing(1);
            m_pControlTypeFilterButtons[type] = new QFilterButton(GetControlTypeIcon(type), "", this);
            m_pControlTypeFilterButtons[type]->setChecked(true);
            pLayout->addWidget(m_pControlTypeFilterButtons[type]);
            pLayout->setContentsMargins(margin, margin, margin, 0);
            pWidget->setLayout(pLayout);
            pWidgetAction->setDefaultWidget(pWidget);
            m_filterMenu.addAction(pWidgetAction);
        }

        QWidgetAction* pWidgetAction = new QWidgetAction(this);
        QWidget* pWidget = new QWidget(this);
        QHBoxLayout* pLayout = new QHBoxLayout(pWidget);
        pLayout->setSpacing(1);
        m_unassignedFilterButton = new QFilterButton(QIcon(":/AudioControlsEditor/Icons/Config_Red_Icon.png"), "", this);
        m_unassignedFilterButton->setChecked(m_showUnassignedControls);
        m_unassignedFilterButton->setText("Unassigned");
        pLayout->addWidget(m_unassignedFilterButton);
        pLayout->setContentsMargins(margin, margin, margin, 0);
        pWidget->setLayout(pLayout);
        pWidgetAction->setDefaultWidget(pWidget);
        m_filterMenu.addAction(pWidgetAction);

        m_pFiltersButton->setMenu(&m_filterMenu);
        m_pControlTypeFilterButtons[eACET_TRIGGER]->setText("Triggers");
        m_pControlTypeFilterButtons[eACET_RTPC]->setText("RTPCs");
        m_pControlTypeFilterButtons[eACET_SWITCH]->setText("Switches");
        m_pControlTypeFilterButtons[eACET_ENVIRONMENT]->setText("Environments");
        m_pControlTypeFilterButtons[eACET_PRELOAD]->setText("Preloads");
        connect(m_pControlTypeFilterButtons[eACET_TRIGGER], SIGNAL(clicked(bool)), this, SLOT(ShowTriggers(bool)));
        connect(m_pControlTypeFilterButtons[eACET_RTPC], SIGNAL(clicked(bool)), this, SLOT(ShowRTPCs(bool)));
        connect(m_pControlTypeFilterButtons[eACET_SWITCH], SIGNAL(clicked(bool)), this, SLOT(ShowSwitches(bool)));
        connect(m_pControlTypeFilterButtons[eACET_ENVIRONMENT], SIGNAL(clicked(bool)), this, SLOT(ShowEnvironments(bool)));
        connect(m_pControlTypeFilterButtons[eACET_PRELOAD], SIGNAL(clicked(bool)), this, SLOT(ShowPreloads(bool)));
        connect(m_unassignedFilterButton, SIGNAL(clicked(bool)), this, SLOT(ShowUnassigned(bool)));
        m_pControlTypeFilterButtons[eACET_SWITCH_STATE]->setHidden(true);
        connect(m_pTextFilter, SIGNAL(textChanged(QString)), this, SLOT(SetFilterString(QString)));
        //  *********************************

        m_pATLModel->AddListener(this);

        // Load data into tree control
        QAudioControlSortProxy* pProxyModel = new QAudioControlSortProxy(this);
        pProxyModel->setSourceModel(m_pTreeModel);
        m_pATLControlsTree->setModel(pProxyModel);
        m_pProxyModel = pProxyModel;

        connect(m_pATLControlsTree->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)), this, SIGNAL(SelectedControlChanged()));
        connect(m_pATLControlsTree->selectionModel(), SIGNAL(currentChanged(const QModelIndex&, const QModelIndex&)), this, SLOT(StopControlExecution()));
        connect(m_pTreeModel, SIGNAL(itemChanged(QStandardItem*)), this, SLOT(ItemModified(QStandardItem*)));
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::ApplyFilter()
    {
        QModelIndex index = m_pProxyModel->index(0, 0);
        for (int i = 0; index.isValid(); ++i)
        {
            ApplyFilter(index);
            index = index.sibling(i, 0);
        }
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::SetFilterString(const QString& sFilterText)
    {
        m_sFilter = sFilterText;
        ApplyFilter();
    }

    //-------------------------------------------------------------------------------------------//
    bool CATLControlsPanel::ApplyFilter(const QModelIndex parent)
    {
        if (parent.isValid())
        {
            bool bIsValid = false;
            bool bHasChildren = false;
            QModelIndex child = parent.child(0, 0);
            for (int i = 1; child.isValid(); ++i)
            {
                bHasChildren = true;
                if (ApplyFilter(child))
                {
                    bIsValid = true;
                }
                child = parent.child(i, 0);
            }

            if (!bIsValid && IsValid(parent))
            {
                if (!bHasChildren || parent.data(eDR_TYPE) != eIT_FOLDER)
                {
                    // we want to hide empty folders, but show controls (ie. switches) even
                    // if their children are hidden
                    bIsValid = true;
                }
            }

            m_pATLControlsTree->setRowHidden(parent.row(), parent.parent(), !bIsValid);
            return bIsValid;
        }
        return false;
    }

    //-------------------------------------------------------------------------------------------//
    bool CATLControlsPanel::IsValid(const QModelIndex index)
    {
        const QString sName = index.data(Qt::DisplayRole).toString();
        if (m_sFilter.isEmpty() || sName.contains(m_sFilter, Qt::CaseInsensitive))
        {
            if (index.data(eDR_TYPE) == eIT_AUDIO_CONTROL)
            {
                const CATLControl* pControl = m_pATLModel->GetControlByID(index.data(eDR_ID).toUInt());
                if (pControl)
                {
                    // This treats switches and switchstates the same so the ATL controls panel filters work.
                    EACEControlType eType = pControl->GetType();
                    if (eType == eACET_SWITCH_STATE)
                    {
                        eType = eACET_SWITCH;
                    }

                    if (m_visibleTypes[eType])
                    {
                        if (m_showUnassignedControls)
                        {
                            return !pControl->IsFullyConnected();
                        }
                        else
                        {
                            return true;
                        }
                    }
                    return false;
                }
            }
            return true;
        }
        return false;
    }

    //-------------------------------------------------------------------------------------------//
    CATLControlsPanel::~CATLControlsPanel()
    {
        StopControlExecution();
        m_pATLModel->RemoveListener(this);
    }

    //-------------------------------------------------------------------------------------------//
    bool CATLControlsPanel::eventFilter(QObject* pObject, QEvent* pEvent)
    {
        if (pEvent->type() == QEvent::KeyRelease)
        {
            QKeyEvent* pKeyEvent = static_cast<QKeyEvent*>(pEvent);
            if (pKeyEvent && !m_pATLControlsTree->IsEditing())
            {
                if (pKeyEvent->key() == Qt::Key_Delete)
                {
                    DeleteSelectedControl();
                }
                else if (pKeyEvent->key() == Qt::Key_Space)
                {
                    ExecuteControl();
                }
                else if (pKeyEvent->key() == Qt::Key_Escape)
                {
                    DeselectAll();
                }
            }
        }
        else if (pEvent->type() == QEvent::MouseButtonRelease)
        {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(pEvent);
            if (mouseEvent && mouseEvent->button() == Qt::LeftButton)
            {
                QModelIndex index = m_pATLControlsTree->indexAt(mouseEvent->pos());
                if (!index.isValid())
                {
                    DeselectAll();
                }
            }
        }
        else if (pEvent->type() == QEvent::Drop)
        {
            QDropEvent* pDropEvent = static_cast<QDropEvent*>(pEvent);
            if (pDropEvent)
            {
                if (pDropEvent->source() != m_pATLControlsTree)
                {
                    // External Drop
                    HandleExternalDropEvent(pDropEvent);
                    pDropEvent->accept();
                }
            }
        }
        return QWidget::eventFilter(pObject, pEvent);
    }

    //-------------------------------------------------------------------------------------------//
    ControlList CATLControlsPanel::GetSelectedControls()
    {
        ControlList controls;
        QModelIndexList indexes = m_pATLControlsTree->selectionModel()->selectedIndexes();
        const int size = indexes.size();
        for (int i = 0; i < size; ++i)
        {
            if (indexes[i].isValid())
            {
                CID nID = indexes[i].data(eDR_ID).toUInt();
                if (nID != ACE_INVALID_CID)
                {
                    controls.push_back(nID);
                }
            }
        }
        return controls;
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::Reload()
    {
        ResetFilters();
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::ShowControlType(EACEControlType type, bool bShow, bool bExclusive)
    {
        if (bExclusive)
        {
            for (int i = 0; i < eACET_NUM_TYPES; ++i)
            {
                EACEControlType controlType = (EACEControlType)i;
                m_visibleTypes[controlType] = !bShow;
                ControlTypeFiltered(controlType, !bShow);
                m_pControlTypeFilterButtons[controlType]->setChecked(!bShow);
            }
        }
        m_visibleTypes[type] = bShow;
        ControlTypeFiltered(type, bShow);
        m_pControlTypeFilterButtons[type]->setChecked(bShow);
        ApplyFilter();
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::ShowTriggers(bool bShow)
    {
        ShowControlType(eACET_TRIGGER, bShow, QGuiApplication::keyboardModifiers() & Qt::ControlModifier);
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::ShowRTPCs(bool bShow)
    {
        ShowControlType(eACET_RTPC, bShow, QGuiApplication::keyboardModifiers() & Qt::ControlModifier);
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::ShowEnvironments(bool bShow)
    {
        ShowControlType(eACET_ENVIRONMENT, bShow, QGuiApplication::keyboardModifiers() & Qt::ControlModifier);
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::ShowSwitches(bool bShow)
    {
        ShowControlType(eACET_SWITCH, bShow, QGuiApplication::keyboardModifiers() & Qt::ControlModifier);
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::ShowPreloads(bool bShow)
    {
        ShowControlType(eACET_PRELOAD, bShow, QGuiApplication::keyboardModifiers() & Qt::ControlModifier);
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::ShowUnassigned(bool bShow)
    {
        m_showUnassignedControls = bShow;
        ApplyFilter();
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::CreateRTPCControl()
    {
        CATLControl* pControl = m_pTreeModel->CreateControl(eACET_RTPC, "rtpc");
        if (pControl)
        {
            SelectItem(AddControl(pControl));
            m_pATLControlsTree->setFocus();
            m_pATLControlsTree->edit(m_pATLControlsTree->currentIndex());
        }
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::CreateSwitchControl()
    {
        CATLControl* pControl = m_pTreeModel->CreateControl(eACET_SWITCH, "switch");
        if (pControl)
        {
            SelectItem(AddControl(pControl));
            m_pATLControlsTree->setFocus();
            m_pATLControlsTree->edit(m_pATLControlsTree->currentIndex());
        }
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::CreateStateControl()
    {
        QStandardItem* pSelectedItem = GetCurrentItem();
        if (pSelectedItem && IsValidParent(pSelectedItem, eACET_SWITCH_STATE))
        {
            CATLControl* pControl = m_pTreeModel->CreateControl(eACET_SWITCH_STATE, "state", GetControlFromItem(pSelectedItem));
            if (pControl)
            {
                SelectItem(AddControl(pControl));
                m_pATLControlsTree->setFocus();
                m_pATLControlsTree->edit(m_pATLControlsTree->currentIndex());
            }
        }
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::CreateTriggerControl()
    {
        CATLControl* pControl = m_pTreeModel->CreateControl(eACET_TRIGGER, "trigger");
        if (pControl)
        {
            SelectItem(AddControl(pControl));
            m_pATLControlsTree->setFocus();
            m_pATLControlsTree->edit(m_pATLControlsTree->currentIndex());
        }
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::CreateEnvironmentsControl()
    {
        CATLControl* pControl = m_pTreeModel->CreateControl(eACET_ENVIRONMENT, "environment");
        if (pControl)
        {
            SelectItem(AddControl(pControl));
            m_pATLControlsTree->setFocus();
            m_pATLControlsTree->edit(m_pATLControlsTree->currentIndex());
        }
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::CreatePreloadControl()
    {
        CATLControl* pControl = m_pTreeModel->CreateControl(eACET_PRELOAD, "preload");
        if (pControl)
        {
            SelectItem(AddControl(pControl));
            m_pATLControlsTree->setFocus();
            m_pATLControlsTree->edit(m_pATLControlsTree->currentIndex());
        }
    }

    //-------------------------------------------------------------------------------------------//
    QStandardItem* CATLControlsPanel::CreateFolder()
    {
        QModelIndex parent = m_pATLControlsTree->currentIndex();
        while (parent.isValid() && (parent.data(eDR_TYPE) != eIT_FOLDER))
        {
            parent = parent.parent();
        }

        QStandardItem* pParentItem = nullptr;
        QModelIndexList indexes = m_pATLControlsTree->selectionModel()->selectedIndexes();

        if (parent.isValid() && indexes.size() > 0)
        {
            pParentItem = m_pTreeModel->itemFromIndex(m_pProxyModel->mapToSource(parent));
            if (parent.isValid() && m_pATLControlsTree->isRowHidden(parent.row(), parent.parent()))
            {
                ResetFilters();
            }
        }
        else
        {
            pParentItem = m_pTreeModel->invisibleRootItem();
        }

        QStandardItem* pFolder = m_pTreeModel->CreateFolder(pParentItem, "new_folder");
        SelectItem(pFolder);
        m_pATLControlsTree->setFocus();
        m_pATLControlsTree->edit(m_pATLControlsTree->currentIndex());
        return pFolder;
    }

    //-------------------------------------------------------------------------------------------//
    bool CATLControlsPanel::IsValidParent(QStandardItem* pParent, const EACEControlType eControlType)
    {
        if (pParent->data(eDR_TYPE) == eIT_FOLDER)
        {
            return eControlType != eACET_SWITCH_STATE;
        }
        else if (eControlType == eACET_SWITCH_STATE)
        {
            CATLControl* pControl = GetControlFromItem(pParent);
            if (pControl)
            {
                return pControl->GetType() == eACET_SWITCH;
            }
        }
        return false;
    }

    //-------------------------------------------------------------------------------------------//
    QStandardItem* CATLControlsPanel::AddControl(CATLControl* pControl)
    {
        if (pControl)
        {
            // Find a suitable parent for the new control starting from the one currently selected
            QStandardItem* pParent = GetCurrentItem();
            if (pParent)
            {
                while (pParent && !IsValidParent(pParent, pControl->GetType()))
                {
                    pParent = pParent->parent();
                }
            }

            if (!pParent)
            {
                AZ_Assert(pControl->GetType() != eACET_SWITCH_STATE, "AddControl - SwitchState control being added needs to be placed under a suitable parent (Switch)");
                pParent = CreateFolder();
            }

            return m_pTreeModel->AddControl(pControl, pParent);
        }
        return nullptr;
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::ShowControlsContextMenu(const QPoint& pos)
    {
        QMenu contextMenu(tr("Context menu"), this);
        QMenu addMenu(tr("Add"));

        CATLControl* pControl = GetControlFromIndex(m_pATLControlsTree->currentIndex());
        if (pControl)
        {
            EACEControlType eControlType = pControl->GetType();
            switch (pControl->GetType())
            {
            case eACET_TRIGGER:
                contextMenu.addAction(tr("Execute Trigger"), this, SLOT(ExecuteControl()));
                contextMenu.addSeparator();
                break;
            case eACET_SWITCH:
            case eACET_SWITCH_STATE:
                addMenu.addAction(GetControlTypeIcon(eACET_SWITCH_STATE), tr("State"), this, SLOT(CreateStateControl()));
                addMenu.addSeparator();
                break;
            }
        }

        addMenu.addAction(GetControlTypeIcon(eACET_TRIGGER), tr("Trigger"), this, SLOT(CreateTriggerControl()));
        addMenu.addAction(GetControlTypeIcon(eACET_RTPC), tr("RTPC"), this, SLOT(CreateRTPCControl()));
        addMenu.addAction(GetControlTypeIcon(eACET_SWITCH), tr("Switch"), this, SLOT(CreateSwitchControl()));
        addMenu.addAction(GetControlTypeIcon(eACET_ENVIRONMENT), tr("Environment"), this, SLOT(CreateEnvironmentsControl()));
        addMenu.addAction(GetControlTypeIcon(eACET_PRELOAD), tr("Preload"), this, SLOT(CreatePreloadControl()));
        addMenu.addSeparator();
        addMenu.addAction(GetFolderIcon(), tr("Folder"), this, SLOT(CreateFolder()));
        contextMenu.addMenu(&addMenu);

        QAction* pAction = new QAction(tr("Rename"), this);
        connect(pAction, &QAction::triggered, m_pATLControlsTree,
            [&]()
            {
                m_pATLControlsTree->edit(m_pATLControlsTree->currentIndex());
            }
        );
        contextMenu.addAction(pAction);

        pAction = new QAction(tr("Delete"), this);
        connect(pAction, SIGNAL(triggered()), this, SLOT(DeleteSelectedControl()));
        contextMenu.addAction(pAction);

        contextMenu.addSeparator();
        pAction = new QAction(tr("Expand All"), this);
        connect(pAction, SIGNAL(triggered()), m_pATLControlsTree, SLOT(expandAll()));
        contextMenu.addAction(pAction);
        pAction = new QAction(tr("Collapse All"), this);
        connect(pAction, SIGNAL(triggered()), m_pATLControlsTree, SLOT(collapseAll()));
        contextMenu.addAction(pAction);

        contextMenu.exec(m_pATLControlsTree->mapToGlobal(pos));
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::DeleteSelectedControl()
    {
        QMessageBox messageBox(this);
        QModelIndexList indexList = m_pATLControlsTree->selectionModel()->selectedIndexes();
        const int size = indexList.length();
        if (size > 0)
        {
            if (size == 1)
            {
                QModelIndex index = m_pProxyModel->mapToSource(indexList[0]);
                QStandardItem* pItem = m_pTreeModel->itemFromIndex(index);
                if (pItem)
                {
                    messageBox.setText("Are you sure you want to delete \"" + pItem->text() + "\"?");
                }
            }
            else
            {
                messageBox.setText("Are you sure you want to delete the selected controls and folders?");
            }
            messageBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            messageBox.setDefaultButton(QMessageBox::Yes);
            messageBox.setWindowTitle("Audio Controls Editor");
            if (messageBox.exec() == QMessageBox::Yes)
            {
                CUndo undo("Audio Control Removed");

                QModelIndexList sourceIndexList;
                for (int i = 0; i < size; ++i)
                {
                    sourceIndexList.push_back(m_pProxyModel->mapToSource(indexList[i]));
                }
                m_pTreeModel->RemoveItems(sourceIndexList);
            }
        }
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::OnControlAdded(CATLControl* pControl)
    {
        // Remove filters if the control added is hidden
        EACEControlType controlType = pControl->GetType();
        if (!m_visibleTypes[controlType])
        {
            m_visibleTypes[controlType] = true;
            m_pControlTypeFilterButtons[controlType]->setChecked(true);
        }
        m_pTextFilter->setText("");
        ApplyFilter();
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::ExecuteControl()
    {
        const CATLControl* const pControl = GetControlFromIndex(m_pATLControlsTree->currentIndex());
        if (pControl)
        {
            CAudioControlsEditorPlugin::ExecuteTrigger(pControl->GetName());
        }
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::StopControlExecution()
    {
        CAudioControlsEditorPlugin::StopTriggerExecution();
    }

    //-------------------------------------------------------------------------------------------//
    CATLControl* CATLControlsPanel::GetControlFromItem(QStandardItem* pItem)
    {
        if (pItem && (pItem->data(eDR_TYPE) == eIT_AUDIO_CONTROL))
        {
            return m_pATLModel->GetControlByID(pItem->data(eDR_ID).toUInt());
        }
        return nullptr;
    }

    //-------------------------------------------------------------------------------------------//
    CATLControl* CATLControlsPanel::GetControlFromIndex(QModelIndex index)
    {
        if (index.isValid() && (index.data(eDR_TYPE) == eIT_AUDIO_CONTROL))
        {
            return m_pATLModel->GetControlByID(index.data(eDR_ID).toUInt());
        }
        return nullptr;
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::SelectItem(QStandardItem* pItem)
    {
        // Expand and scroll to the new item
        if (pItem)
        {
            QModelIndex index = m_pTreeModel->indexFromItem(pItem);
            if (index.isValid())
            {
                m_pATLControlsTree->selectionModel()->setCurrentIndex(m_pProxyModel->mapFromSource(index), QItemSelectionModel::ClearAndSelect);
            }
        }
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::DeselectAll()
    {
        m_pATLControlsTree->selectionModel()->clear();
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::HandleExternalDropEvent(QDropEvent* pDropEvent)
    {
        AudioControls::IAudioSystemEditor* pAudioSystemEditorImpl = CAudioControlsEditorPlugin::GetAudioSystemEditorImpl();
        if (pAudioSystemEditorImpl)
        {
            const QMimeData* pData = pDropEvent->mimeData();
            const QString format = "application/x-qabstractitemmodeldatalist";
            if (pData->hasFormat(format))
            {
                QByteArray encoded = pData->data(format);
                QDataStream stream(&encoded, QIODevice::ReadOnly);
                while (!stream.atEnd())
                {
                    int row, col;
                    QMap<int,  QVariant> roleDataMap;
                    stream >> row >> col >> roleDataMap;
                    if (!roleDataMap.isEmpty())
                    {
                        // Dropped item mime data
                        IAudioSystemControl* pAudioSystemControl = pAudioSystemEditorImpl->GetControl(roleDataMap[eMDR_ID].toUInt());
                        if (pAudioSystemControl)
                        {
                            const EACEControlType eControlType = pAudioSystemEditorImpl->ImplTypeToATLType(pAudioSystemControl->GetType());

                            // If dropped outside any folder, create a folder at the root to contain the new control
                            const QModelIndex index = m_pProxyModel->mapToSource(m_pATLControlsTree->indexAt(pDropEvent->pos()));
                            QStandardItem* pTargetItem = m_pTreeModel->itemFromIndex(index);
                            if (!pTargetItem)
                            {
                                pTargetItem = m_pTreeModel->CreateFolder(m_pTreeModel->invisibleRootItem(), "new_folder");
                            }

                            // Find a suitable parent for the dropped control
                            CATLControl* pATLParent = nullptr;
                            CATLControl* pTargetControl = GetControlFromItem(pTargetItem);
                            if (pTargetControl && pTargetControl->GetType() == eControlType)
                            {
                                // If dropped in a control of the same type, we can assume the parent is valid, so we select it
                                pTargetItem = pTargetItem->parent();
                                pATLParent = GetControlFromItem(pTargetItem);
                            }
                            else
                            {
                                // If the dragged control has a parent, need to find a suitable parent on the ATL side first
                                const IAudioSystemControl* pAudioControlParent = pAudioSystemControl->GetParent();
                                if (pAudioControlParent && pAudioControlParent->GetType() != AUDIO_IMPL_INVALID_TYPE)
                                {
                                    // Is the place where item was dropped compatible with the parent we are looking for
                                    if (IsValidParent(pTargetItem, eControlType))
                                    {
                                        pATLParent = GetControlFromItem(pTargetItem);
                                    }
                                    else
                                    {
                                        // If place where item was dropped is not valid as a parent, we have to look for a valid control where to create one that is
                                        const EACEControlType eParentType = pAudioSystemEditorImpl->ImplTypeToATLType(pAudioControlParent->GetType());
                                        QStandardItem* pParent = pTargetItem;
                                        while (pParent && !IsValidParent(pParent, eParentType))
                                        {
                                            pParent = pParent->parent();
                                        }

                                        if (pParent)
                                        {
                                            pATLParent = m_pTreeModel->CreateControl(eParentType, pAudioControlParent->GetName());
                                            pTargetItem = m_pTreeModel->AddControl(pATLParent, pParent);
                                        }
                                    }
                                }
                            }

                            if (pTargetItem)
                            {
                                while (pTargetItem && !IsValidParent(pTargetItem, eControlType))
                                {
                                    pTargetItem = pTargetItem->parent();
                                }

                                // Create the new control and connect it to the one dragged in externally
                                AZStd::string sControlName(roleDataMap[Qt::DisplayRole].toString().toUtf8().data());
                                if (eControlType  == eACET_PRELOAD)
                                {
                                    AzFramework::StringFunc::Path::StripExtension(sControlName);
                                }
                                CATLControl* pTargetControl = m_pTreeModel->CreateControl(eControlType, sControlName, pATLParent);
                                if (pTargetControl)
                                {
                                    TConnectionPtr pAudioConnection = pAudioSystemEditorImpl->CreateConnectionToControl(pTargetControl->GetType(), pAudioSystemControl);
                                    if (pAudioConnection)
                                    {
                                        if (eControlType == eACET_PRELOAD)
                                        {
                                            // Try finding the "default" group, otherwise set the 0th group...
                                            int defaultGroupIndex = m_pATLModel->GetConnectionGroupId("default");
                                            if (defaultGroupIndex < 0)
                                            {
                                                defaultGroupIndex = 0;
                                            }
                                            pAudioConnection->SetGroup(m_pATLModel->GetConnectionGroupAt(defaultGroupIndex));
                                        }
                                        pTargetControl->AddConnection(pAudioConnection);
                                    }
                                    pTargetItem = m_pTreeModel->AddControl(pTargetControl, pTargetItem);

                                    SelectItem(pTargetItem);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    //-------------------------------------------------------------------------------------------//
    QStandardItem* CATLControlsPanel::GetCurrentItem()
    {
        return m_pTreeModel->itemFromIndex(m_pProxyModel->mapToSource(m_pATLControlsTree->currentIndex()));
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::ResetFilters()
    {
        // Remove filters
        for (int i = 0; i < eACET_NUM_TYPES; ++i)
        {
            EACEControlType controlType = (EACEControlType)i;
            m_visibleTypes[controlType] = true;
            m_pControlTypeFilterButtons[controlType]->setChecked(true);
        }
        m_pTextFilter->setText("");
        ApplyFilter();
    }

    //-------------------------------------------------------------------------------------------//
    void CATLControlsPanel::ItemModified(QStandardItem* pItem)
    {
        if (pItem)
        {
            AZStd::string sName(pItem->text().toUtf8().data());
            if (pItem->data(eDR_TYPE) == eIT_AUDIO_CONTROL)
            {
                CATLControl* pControl = m_pATLModel->GetControlByID(pItem->data(eDR_ID).toUInt());
                if (pControl)
                {
                    if (pControl->GetName().compare(pItem->text().toUtf8().data()) != 0)
                    {
                        sName = m_pATLModel->GenerateUniqueName(sName, pControl->GetType(), pControl->GetScope(), pControl->GetParent());
                        pControl->SetName(sName);
                    }
                }
                m_pTreeModel->blockSignals(true);
                pItem->setText(QString(sName.c_str()));
                m_pTreeModel->blockSignals(false);
            }
            m_pTreeModel->SetItemAsDirty(pItem);
        }
    }
} // namespace AudioControls

#include <Source/Editor/ATLControlsPanel.moc>
