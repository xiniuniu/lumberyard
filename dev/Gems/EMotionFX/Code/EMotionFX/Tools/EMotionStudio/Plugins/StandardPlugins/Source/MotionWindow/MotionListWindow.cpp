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

#include <AzQtComponents/Components/FilteredSearchWidget.h>
#include "MotionListWindow.h"
#include "MotionWindowPlugin.h"
#include <QMenu>
#include <QTableWidget>
#include <QContextMenuEvent>
#include <QAction>
#include <QPushButton>
#include <QApplication>
#include <QApplication>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMimeData>
#include <QLineEdit>
#include <QMessageBox>
#include <QHeaderView>
#include <MCore/Source/CommandGroup.h>
#include <EMotionFX/CommandSystem/Source/MotionCommands.h>
#include <EMotionFX/CommandSystem/Source/MetaData.h>
#include <EMotionFX/CommandSystem/Source/MotionSetCommands.h>
#include <EMotionFX/Source/MotionManager.h>
#include "../../../../EMStudioSDK/Source/NotificationWindow.h"
#include "../../../../EMStudioSDK/Source/EMStudioManager.h"
#include "../../../../EMStudioSDK/Source/FileManager.h"
#include "../../../../EMStudioSDK/Source/MainWindow.h"
#include "../../../../EMStudioSDK/Source/SaveChangedFilesManager.h"
#include "../MotionSetsWindow/MotionSetsWindowPlugin.h"


namespace EMStudio
{
    // constructor
    MotionListRemoveMotionsFailedWindow::MotionListRemoveMotionsFailedWindow(QWidget* parent, const AZStd::vector<EMotionFX::Motion*>& motions)
        : QDialog(parent)
    {
        // set the window title
        setWindowTitle("Remove Motions Failed");

        // resize the window
        resize(720, 405);

        // create the layout
        QVBoxLayout* layout = new QVBoxLayout();

        // add the top text
        layout->addWidget(new QLabel("The following motions failed to get removed because they are used by a motion set:"));

        // create the table widget
        QTableWidget* tableWidget = new QTableWidget();
        tableWidget->setAlternatingRowColors(true);
        tableWidget->setGridStyle(Qt::SolidLine);
        tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
        tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
        tableWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        tableWidget->setCornerButtonEnabled(false);
        tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

        // set the table widget columns
        tableWidget->setColumnCount(2);
        QStringList headerLabels;
        headerLabels.append("Name");
        headerLabels.append("FileName");
        tableWidget->setHorizontalHeaderLabels(headerLabels);
        tableWidget->horizontalHeader()->setStretchLastSection(true);
        tableWidget->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
        tableWidget->horizontalHeader()->setSortIndicator(0, Qt::AscendingOrder);
        tableWidget->verticalHeader()->setVisible(false);

        // set the number of rows
        const int numMotions = static_cast<int>(motions.size());
        tableWidget->setRowCount(numMotions);

        // add each motion in the table
        for (int i = 0; i < numMotions; ++i)
        {
            // get the motion
            EMotionFX::Motion* motion = motions[i];

            // create the name table widget item
            QTableWidgetItem* nameTableWidgetItem = new QTableWidgetItem(motion->GetName());
            nameTableWidgetItem->setToolTip(motion->GetName());

            // create the filename table widget item
            QTableWidgetItem* fileNameTableWidgetItem = new QTableWidgetItem(motion->GetFileName());
            fileNameTableWidgetItem->setToolTip(motion->GetFileName());

            // set the text of the row
            tableWidget->setItem(i, 0, nameTableWidgetItem);
            tableWidget->setItem(i, 1, fileNameTableWidgetItem);

            // set the row height
            tableWidget->setRowHeight(i, 21);
        }

        // resize the first column to contents
        tableWidget->resizeColumnToContents(0);

        // add the table widget in the layout
        layout->addWidget(tableWidget);

        // add the button to close the window
        QPushButton* okButton = new QPushButton("OK");
        connect(okButton, &QPushButton::clicked, this, &MotionListRemoveMotionsFailedWindow::accept);
        QHBoxLayout* buttonLayout = new QHBoxLayout();
        buttonLayout->setAlignment(Qt::AlignRight);
        buttonLayout->addWidget(okButton);
        layout->addLayout(buttonLayout);

        // set the layout
        setLayout(layout);
    }


    // constructor
    MotionListWindow::MotionListWindow(QWidget* parent, MotionWindowPlugin* motionWindowPlugin)
        : QWidget(parent)
    {
        setObjectName("MotionListWindow");
        mMotionTable        = nullptr;
        mClearMotionsButton = nullptr;
        mRemoveMotionsButton = nullptr;
        mAddMotionsButton   = nullptr;
        mMotionWindowPlugin = motionWindowPlugin;
    }


    // destructor
    MotionListWindow::~MotionListWindow()
    {
    }


    void MotionListWindow::Init()
    {
        mVLayout = new QVBoxLayout();
        mVLayout->setMargin(3);
        mVLayout->setSpacing(2);
        mMotionTable = new MotionTableWidget(mMotionWindowPlugin, this);
        mMotionTable->setAlternatingRowColors(true);
        connect(mMotionTable, &MotionTableWidget::cellDoubleClicked, this, &MotionListWindow::cellDoubleClicked);
        connect(mMotionTable, &MotionTableWidget::itemSelectionChanged, this, &MotionListWindow::itemSelectionChanged);

        // set the table to row single selection
        mMotionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        mMotionTable->setSelectionMode(QAbstractItemView::ExtendedSelection);

        // make the table items read only
        mMotionTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

        // disable the corner button between the row and column selection thingies
        mMotionTable->setCornerButtonEnabled(false);

        // enable the custom context menu for the motion table
        mMotionTable->setContextMenuPolicy(Qt::DefaultContextMenu);

        // set the column count
        mMotionTable->setColumnCount(6);

        // add the name column
        QTableWidgetItem* nameHeaderItem = new QTableWidgetItem("Name");
        nameHeaderItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        mMotionTable->setHorizontalHeaderItem(0, nameHeaderItem);

        // add the length column
        QTableWidgetItem* lengthHeaderItem = new QTableWidgetItem("Length");
        lengthHeaderItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        mMotionTable->setHorizontalHeaderItem(1, lengthHeaderItem);

        // add the sub column
        QTableWidgetItem* subHeaderItem = new QTableWidgetItem("Sub");
        subHeaderItem->setToolTip("Number of submotions");
        subHeaderItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        mMotionTable->setHorizontalHeaderItem(2, subHeaderItem);

        // add the msub column
        QTableWidgetItem* msubHeaderItem = new QTableWidgetItem("MSub");
        msubHeaderItem->setToolTip("Number of morph submotions");
        msubHeaderItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        mMotionTable->setHorizontalHeaderItem(3, msubHeaderItem);

        // add the type column
        QTableWidgetItem* typeHeaderItem = new QTableWidgetItem("Type");
        typeHeaderItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        mMotionTable->setHorizontalHeaderItem(4, typeHeaderItem);

        // add the filename column
        QTableWidgetItem* fileNameHeaderItem = new QTableWidgetItem("FileName");
        fileNameHeaderItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        mMotionTable->setHorizontalHeaderItem(5, fileNameHeaderItem);

        // set the sorting order on the first column
        mMotionTable->horizontalHeader()->setSortIndicator(0, Qt::AscendingOrder);

        // hide the vertical columns
        QHeaderView* verticalHeader = mMotionTable->verticalHeader();
        verticalHeader->setVisible(false);

        // set the last column to take the whole available space
        mMotionTable->horizontalHeader()->setStretchLastSection(true);

        // set the column width
        mMotionTable->setColumnWidth(0, 300);
        mMotionTable->setColumnWidth(1, 55);
        mMotionTable->setColumnWidth(2, 35);
        mMotionTable->setColumnWidth(3, 42);
        mMotionTable->setColumnWidth(4, 86);

        // create buttons
        mAddMotionsButton           = new QPushButton();
        mRemoveMotionsButton        = new QPushButton();
        mClearMotionsButton         = new QPushButton();
        mSaveButton                 = new QPushButton();
        QPushButton* stopAll        = new QPushButton();
        QPushButton* stopSelected   = new QPushButton();

        EMStudioManager::MakeTransparentButton(mAddMotionsButton,      "/Images/Icons/Plus.png",           "Load new motions");
        EMStudioManager::MakeTransparentButton(mRemoveMotionsButton,   "/Images/Icons/Minus.png",          "Remove selected motions");
        EMStudioManager::MakeTransparentButton(mClearMotionsButton,    "/Images/Icons/Clear.png",          "Remove all motions");
        EMStudioManager::MakeTransparentButton(mSaveButton,            "/Images/Menu/FileSave.png",        "Save selected motions");
        EMStudioManager::MakeTransparentButton(stopSelected,           "/Images/Icons/Stop.png",           "Stop the selected motions on the selected actor instances");
        EMStudioManager::MakeTransparentButton(stopAll,                "/Images/Icons/StopAll.png",        "Stop all motions on the selected actor instances");

        // create the buttons layout
        QHBoxLayout* buttonLayout = new QHBoxLayout();
        buttonLayout->setSpacing(0);
        buttonLayout->setAlignment(Qt::AlignLeft);
        buttonLayout->addWidget(mAddMotionsButton);
        buttonLayout->addWidget(mRemoveMotionsButton);
        buttonLayout->addWidget(mClearMotionsButton);
        buttonLayout->addWidget(mSaveButton);
        buttonLayout->addWidget(stopSelected);
        buttonLayout->addWidget(stopAll);

        QWidget* spacerWidget = new QWidget();
        spacerWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        buttonLayout->addWidget(spacerWidget);

        m_searchWidget = new AzQtComponents::FilteredSearchWidget(this);
        connect(m_searchWidget, &AzQtComponents::FilteredSearchWidget::TextFilterChanged, this, &MotionListWindow::OnTextFilterChanged);
        buttonLayout->addWidget(m_searchWidget);

        connect(mClearMotionsButton, &QPushButton::clicked, this, &MotionListWindow::OnClearMotionsButtonPressed);
        connect(mRemoveMotionsButton, &QPushButton::clicked, this, &MotionListWindow::OnRemoveMotionsButtonPressed);
        connect(mAddMotionsButton, &QPushButton::clicked, this, &MotionListWindow::OnAddMotionsButtonPressed);
        connect(mSaveButton, &QPushButton::clicked, this, &MotionListWindow::OnSave);
        connect(stopAll, &QPushButton::clicked, this, &MotionListWindow::OnStopAllMotionsButton);
        connect(stopSelected, &QPushButton::clicked, this, &MotionListWindow::OnStopSelectedMotionsButton);

        mVLayout->addLayout(buttonLayout);
        mVLayout->addWidget(mMotionTable);
        setLayout(mVLayout);

        ReInit();
    }


    // called when the filter string changed
    void MotionListWindow::OnTextFilterChanged(const QString& text)
    {
        m_searchWidgetText = text.toLower().toUtf8().data();
        ReInit();
    }


    bool MotionListWindow::AddMotionByID(uint32 motionID)
    {
        // find the motion entry based on the id
        MotionWindowPlugin::MotionTableEntry* motionEntry = mMotionWindowPlugin->FindMotionEntryByID(motionID);
        if (motionEntry == nullptr)
        {
            return false;
        }

        // if the motion is not visible at all skip it completely
        if (CheckIfIsMotionVisible(motionEntry) == false)
        {
            return true;
        }

        // get the motion
        EMotionFX::Motion* motion = motionEntry->mMotion;

        // disable the sorting
        mMotionTable->setSortingEnabled(false);

        // insert the new row
        const uint32 rowIndex = 0;
        mMotionTable->insertRow(rowIndex);
        mMotionTable->setRowHeight(rowIndex, 21);

        // create the name item
        QTableWidgetItem* nameTableItem = new QTableWidgetItem(motion->GetName());

        // store the motion ID on this item
        nameTableItem->setData(Qt::UserRole, motion->GetID());

        // set the item in the motion table
        mMotionTable->setItem(rowIndex, 0, nameTableItem);

        // create the length item
        AZStd::string length;
        length = AZStd::string::format("%.2f sec", motion->GetMaxTime());
        QTableWidgetItem* lengthTableItem = new QTableWidgetItem(length.c_str());

        // set the item in the motion table
        mMotionTable->setItem(rowIndex, 1, lengthTableItem);

        // set the sub and msub text
        AZStd::string sub, msub;
        if ((motion->GetType() != EMotionFX::SkeletalMotion::TYPE_ID) && (motion->GetType() != EMotionFX::WaveletSkeletalMotion::TYPE_ID))
        {
            sub = "";
            msub = "";
        }
        else
        {
            EMotionFX::SkeletalMotion* skeletalMotion = static_cast<EMotionFX::SkeletalMotion*>(motion);
            sub = AZStd::string::format("%d", skeletalMotion->GetNumSubMotions());
            msub = AZStd::string::format("%d", skeletalMotion->GetNumMorphSubMotions());
        }

        // create the sub and msub item
        QTableWidgetItem* subTableItem = new QTableWidgetItem(sub.c_str());
        QTableWidgetItem* msubTableItem = new QTableWidgetItem(msub.c_str());

        // set the items in the motion table
        mMotionTable->setItem(rowIndex, 2, subTableItem);
        mMotionTable->setItem(rowIndex, 3, msubTableItem);

        // create and set the type item
        QTableWidgetItem* typeTableItem = new QTableWidgetItem(motion->GetTypeString());
        mMotionTable->setItem(rowIndex, 4, typeTableItem);

        // create and set the filename item
        QTableWidgetItem* fileNameTableItem = new QTableWidgetItem(motion->GetFileName());
        mMotionTable->setItem(rowIndex, 5, fileNameTableItem);

        // set the items italic if the motion is dirty
        if (motion->GetDirtyFlag())
        {
            // create the font italic, all columns use the same font
            QFont font = subTableItem->font();
            font.setItalic(true);

            // set the font for each item
            nameTableItem->setFont(font);
            lengthTableItem->setFont(font);
            subTableItem->setFont(font);
            msubTableItem->setFont(font);
            typeTableItem->setFont(font);
            fileNameTableItem->setFont(font);
        }

        // enable the sorting
        mMotionTable->setSortingEnabled(true);

        // update the interface
        UpdateInterface();

        // return true because the row is correctly added
        return true;
    }


    uint32 MotionListWindow::FindRowByMotionID(uint32 motionID)
    {
        // iterate through the rows and compare the motion IDs
        const uint32 rowCount = mMotionTable->rowCount();
        for (uint32 i = 0; i < rowCount; ++i)
        {
            if (GetMotionID(i) == motionID)
            {
                return i;
            }
        }

        // failure, motion id not found in any of the rows
        return MCORE_INVALIDINDEX32;
    }


    bool MotionListWindow::RemoveMotionByID(uint32 motionID)
    {
        // find the row for the motion
        const uint32 rowIndex = FindRowByMotionID(motionID);
        if (rowIndex == MCORE_INVALIDINDEX32)
        {
            return false;
        }

        // remove the row
        mMotionTable->removeRow(rowIndex);

        // update the interface
        UpdateInterface();

        // return true because the row is correctly removed
        return true;
    }


    bool MotionListWindow::CheckIfIsMotionVisible(MotionWindowPlugin::MotionTableEntry* entry)
    {
        if (entry->mMotion->GetIsOwnedByRuntime())
        {
            return false;
        }

        AZStd::string motionNameLowered = entry->mMotion->GetNameString();
        AZStd::to_lower(motionNameLowered.begin(), motionNameLowered.end());
        if (m_searchWidgetText.empty() || motionNameLowered.find(m_searchWidgetText) != AZStd::string::npos)
        {
            return true;
        }
        return false;
    }


    void MotionListWindow::ReInit()
    {
        const CommandSystem::SelectionList selection = GetCommandManager()->GetCurrentSelection();

        size_t numMotions = mMotionWindowPlugin->GetNumMotionEntries();
        mShownMotionEntries.clear();
        mShownMotionEntries.reserve(numMotions);

        for (size_t i = 0; i < numMotions; ++i)
        {
            MotionWindowPlugin::MotionTableEntry* entry = mMotionWindowPlugin->GetMotionEntry(i);
            if (CheckIfIsMotionVisible(entry))
            {
                mShownMotionEntries.push_back(entry);
            }
        }
        numMotions = mShownMotionEntries.size();

        // set the number of rows
        mMotionTable->setRowCount(static_cast<int>(numMotions));

        // set the sorting disabled
        mMotionTable->setSortingEnabled(false);

        // iterate through the motions and fill in the table
        for (int i = 0; i < numMotions; ++i)
        {
            EMotionFX::Motion* motion = mShownMotionEntries[static_cast<size_t>(i)]->mMotion;

            // set the row height
            mMotionTable->setRowHeight(i, 21);

            // create the name item
            QTableWidgetItem* nameTableItem = new QTableWidgetItem(motion->GetName());

            // store the motion ID on this item
            nameTableItem->setData(Qt::UserRole, motion->GetID());

            // set the item in the motion table
            mMotionTable->setItem(i, 0, nameTableItem);

            // create the length item
            AZStd::string length;
            length = AZStd::string::format("%.2f sec", motion->GetMaxTime());
            QTableWidgetItem* lengthTableItem = new QTableWidgetItem(length.c_str());

            // set the item in the motion table
            mMotionTable->setItem(i, 1, lengthTableItem);

            // set the sub and msub text
            AZStd::string sub, msub;
            if ((motion->GetType() != EMotionFX::SkeletalMotion::TYPE_ID) && (motion->GetType() != EMotionFX::WaveletSkeletalMotion::TYPE_ID))
            {
                sub = "";
                msub = "";
            }
            else
            {
                EMotionFX::SkeletalMotion* skeletalMotion = static_cast<EMotionFX::SkeletalMotion*>(motion);
                sub = AZStd::string::format("%d", skeletalMotion->GetNumSubMotions());
                msub = AZStd::string::format("%d", skeletalMotion->GetNumMorphSubMotions());
            }

            // create the sub and msub item
            QTableWidgetItem* subTableItem = new QTableWidgetItem(sub.c_str());
            QTableWidgetItem* msubTableItem = new QTableWidgetItem(msub.c_str());

            // set the items in the motion table
            mMotionTable->setItem(i, 2, subTableItem);
            mMotionTable->setItem(i, 3, msubTableItem);

            // create and set the type item
            QTableWidgetItem* typeTableItem = new QTableWidgetItem(motion->GetTypeString());
            mMotionTable->setItem(i, 4, typeTableItem);

            // create and set the filename item
            QTableWidgetItem* fileNameTableItem = new QTableWidgetItem(motion->GetFileName());
            mMotionTable->setItem(i, 5, fileNameTableItem);

            // set the items italic if the motion is dirty
            if (motion->GetDirtyFlag())
            {
                // create the font italic, all columns use the same font
                QFont font = subTableItem->font();
                font.setItalic(true);

                // set the font for each item
                nameTableItem->setFont(font);
                lengthTableItem->setFont(font);
                subTableItem->setFont(font);
                msubTableItem->setFont(font);
                typeTableItem->setFont(font);
                fileNameTableItem->setFont(font);
            }
        }

        // set the sorting enabled
        mMotionTable->setSortingEnabled(true);

        // set the old selection as before the reinit
        UpdateSelection(selection);
    }


    // update the selection
    void MotionListWindow::UpdateSelection(const CommandSystem::SelectionList& selectionList)
    {
        // block signals to not have the motion table events when selection changed
        mMotionTable->blockSignals(true);

        // clear the selection
        mMotionTable->clearSelection();

        // iterate through the selected motions and select the corresponding rows in the table widget
        const uint32 numSelectedMotions = selectionList.GetNumSelectedMotions();
        for (uint32 i = 0; i < numSelectedMotions; ++i)
        {
            // get the index of the motion inside the motion manager (which is equal to the row in the motion table) and select the row at the motion index
            EMotionFX::Motion* selectedMotion = selectionList.GetMotion(i);
            const uint32 row = FindRowByMotionID(selectedMotion->GetID());
            if (row != MCORE_INVALIDINDEX32)
            {
                // select the entire row
                const int columnCount = mMotionTable->columnCount();
                for (int c = 0; c < columnCount; ++c)
                {
                    QTableWidgetItem* tableWidgetItem = mMotionTable->item(row, c);
                    mMotionTable->setItemSelected(tableWidgetItem, true);
                }
            }
        }

        // enable the signals now all rows are selected
        mMotionTable->blockSignals(false);

        // call the selection changed
        itemSelectionChanged();
    }


    void MotionListWindow::UpdateInterface()
    {
        const CommandSystem::SelectionList& selection = GetCommandManager()->GetCurrentSelection();
        const uint32 numMotions = EMotionFX::GetMotionManager().GetNumMotions();

        // related to the loaded motions
        mClearMotionsButton->setEnabled(numMotions > 0);

        // related to the selected motions
        const bool HasSelectedMotions = selection.GetNumSelectedMotions() > 0;
        mRemoveMotionsButton->setEnabled(HasSelectedMotions);
        mSaveButton->setEnabled(HasSelectedMotions);
    }


    uint32 MotionListWindow::GetMotionID(uint32 rowIndex)
    {
        QTableWidgetItem* tableItem = mMotionTable->item(rowIndex, 0);
        if (tableItem)
        {
            return tableItem->data(Qt::UserRole).toInt();
        }
        return MCORE_INVALIDINDEX32;
    }


    void MotionListWindow::cellDoubleClicked(int row, int column)
    {
        MCORE_UNUSED(column);

        const uint32        motionID    = GetMotionID(row);
        EMotionFX::Motion*  motion      = EMotionFX::GetMotionManager().FindMotionByID(motionID);

        if (motion)
        {
            mMotionWindowPlugin->PlayMotion(motion);
        }
    }


    void MotionListWindow::itemSelectionChanged()
    {
        // get the current selection
        const QList<QTableWidgetItem*> selectedItems = mMotionTable->selectedItems();

        // get the number of selected items
        const uint32 numSelectedItems = selectedItems.count();

        // filter the items
        AZStd::vector<uint32> rowIndices;
        rowIndices.reserve(numSelectedItems);
        for (size_t i = 0; i < numSelectedItems; ++i)
        {
            const uint32 rowIndex = selectedItems[static_cast<uint32>(i)]->row();
            if (AZStd::find(rowIndices.begin(), rowIndices.end(), rowIndex) == rowIndices.end())
            {
                rowIndices.push_back(rowIndex);
            }
        }

        // clear the selected motion IDs
        mSelectedMotionIDs.clear();

        // get the number of selected items and iterate through them
        const size_t numSelectedRows = rowIndices.size();
        mSelectedMotionIDs.reserve(numSelectedRows);
        for (size_t i = 0; i < numSelectedRows; ++i)
        {
            mSelectedMotionIDs.push_back(GetMotionID(rowIndices[i]));
        }

        // unselect all motions
        GetCommandManager()->GetCurrentSelection().ClearMotionSelection();

        // get the number of selected motions and iterate through them
        const size_t numSelectedMotions = mSelectedMotionIDs.size();
        for (size_t i = 0; i < numSelectedMotions; ++i)
        {
            // find the motion by name in the motion library and select it
            EMotionFX::Motion* motion = EMotionFX::GetMotionManager().FindMotionByID(mSelectedMotionIDs[i]);
            if (motion)
            {
                GetCommandManager()->GetCurrentSelection().AddMotion(motion);
            }
        }

        // update the interface
        mMotionWindowPlugin->UpdateInterface();

        // emit signal that tells other windows that the motion selection changed
        emit MotionSelectionChanged();
    }


    void MotionListWindow::OnSave()
    {
        const CommandSystem::SelectionList& selectionList = GetCommandManager()->GetCurrentSelection();
        const AZ::u32 numMotions = selectionList.GetNumSelectedMotions();
        if (numMotions == 0)
        {
            return;
        }

        // Collect motion ids of the motion to be saved.
        AZStd::vector<AZ::u32> motionIds;
        motionIds.reserve(numMotions);
        for (AZ::u32 i = 0; i < numMotions; ++i)
        {
            const EMotionFX::Motion* motion = selectionList.GetMotion(i);
            motionIds.push_back(motion->GetID());
        }

        // Save all selected motions.
        for (AZ::u32 motionId : motionIds)
        {
            GetMainWindow()->GetFileManager()->SaveMotion(motionId);
        }
    }


    // add the selected motions in the selected motion sets
    void MotionListWindow::OnAddMotionsInSelectedMotionSets()
    {
        // get the current selection
        const CommandSystem::SelectionList& selection = GetCommandManager()->GetCurrentSelection();
        const uint32 numSelectedMotions = selection.GetNumSelectedMotions();
        if (numSelectedMotions == 0)
        {
            return;
        }

        // get the motion sets window plugin
        EMStudioPlugin* motionWindowPlugin = EMStudio::GetPluginManager()->FindActivePlugin(MotionSetsWindowPlugin::CLASS_ID);
        if (motionWindowPlugin == nullptr)
        {
            return;
        }

        // Get the selected motion sets.
        AZStd::vector<EMotionFX::MotionSet*> selectedMotionSets;
        MotionSetsWindowPlugin* motionSetsWindowPlugin = static_cast<MotionSetsWindowPlugin*>(motionWindowPlugin);
        motionSetsWindowPlugin->GetManagementWindow()->GetSelectedMotionSets(selectedMotionSets);
        if (selectedMotionSets.empty())
        {
            return;
        }

        // Set the command group name based on the number of motions to add.
        AZStd::string groupName;
        const size_t numSelectedMotionSets = selectedMotionSets.size();
        if (numSelectedMotions > 1)
        {
            groupName = "Add motions in motion sets";
        }
        else
        {
            groupName = "Add motion in motion sets";
        }

        MCore::CommandGroup commandGroup(groupName);

        // add in each selected motion set
        AZStd::string motionName;
        for (uint32 m = 0; m < numSelectedMotionSets; ++m)
        {
            EMotionFX::MotionSet* motionSet = selectedMotionSets[m];

            // Build a list of unique string id values from all motion set entries.
            AZStd::vector<AZStd::string> idStrings;
            motionSet->BuildIdStringList(idStrings);

            // add each selected motion in the motion set
            for (uint32 i = 0; i < numSelectedMotions; ++i)
            {
                // remove the media root folder from the absolute motion filename so that we get the relative one to the media root folder
                motionName = selection.GetMotion(i)->GetFileName();
                EMotionFX::GetEMotionFX().GetFilenameRelativeToMediaRoot(&motionName);

                // construct and call the command for actually adding it
                CommandSystem::AddMotionSetEntry(motionSet->GetID(), "", idStrings, motionName.c_str(), &commandGroup);
            }
        }

        AZStd::string result;
        if (!EMStudio::GetCommandManager()->ExecuteCommandGroup(commandGroup, result))
        {
            AZ_Error("EMotionFX", false, result.c_str());
        }
    }


    void MotionListWindow::keyPressEvent(QKeyEvent* event)
    {
        // delete key
        if (event->key() == Qt::Key_Delete)
        {
            OnRemoveMotionsButtonPressed();
            event->accept();
            return;
        }

        // base class
        QWidget::keyPressEvent(event);
    }


    void MotionListWindow::keyReleaseEvent(QKeyEvent* event)
    {
        // delete key
        if (event->key() == Qt::Key_Delete)
        {
            event->accept();
            return;
        }

        // base class
        QWidget::keyReleaseEvent(event);
    }


    void MotionListWindow::contextMenuEvent(QContextMenuEvent* event)
    {
        // get the current selection
        const CommandSystem::SelectionList& selection = GetCommandManager()->GetCurrentSelection();

        // create the context menu
        QMenu menu(this);

        // add the motion related menus
        if (selection.GetNumSelectedMotions() > 0)
        {
            // get the motion sets window plugin
            EMStudioPlugin* motionWindowPlugin = EMStudio::GetPluginManager()->FindActivePlugin(MotionSetsWindowPlugin::CLASS_ID);
            if (motionWindowPlugin)
            {
                // Get the selected motion sets.
                AZStd::vector<EMotionFX::MotionSet*> selectedMotionSets;
                MotionSetsWindowPlugin* motionSetsWindowPlugin = static_cast<MotionSetsWindowPlugin*>(motionWindowPlugin);
                motionSetsWindowPlugin->GetManagementWindow()->GetSelectedMotionSets(selectedMotionSets);

                // add the menu if at least one motion set is selected
                if (!selectedMotionSets.empty())
                {
                    // add the menu to add in motion sets
                    QAction* addInSelectedMotionSetsAction = menu.addAction("Add To Selected Motion Sets");
                    addInSelectedMotionSetsAction->setIcon(MysticQt::GetMysticQt()->FindIcon("Images/Icons/Plus.png"));
                    connect(addInSelectedMotionSetsAction, &QAction::triggered, this, &MotionListWindow::OnAddMotionsInSelectedMotionSets);

                    menu.addSeparator();
                }
            }

            // add the remove menu
            QAction* removeAction = menu.addAction("Remove Selected Motions");
            removeAction->setIcon(MysticQt::GetMysticQt()->FindIcon("Images/Icons/Minus.png"));
            connect(removeAction, &QAction::triggered, this, &MotionListWindow::OnRemoveMotionsButtonPressed);

            menu.addSeparator();

            // add the save menu
            QAction* saveAction = menu.addAction("Save Selected Motions");
            saveAction->setIcon(MysticQt::GetMysticQt()->FindIcon("/Images/Menu/FileSave.png"));
            connect(saveAction, &QAction::triggered, this, &MotionListWindow::OnSave);
        }

        // show the menu at the given position
        if (menu.isEmpty() == false)
        {
            menu.exec(event->globalPos());
        }
    }


    void MotionListWindow::OnAddMotionsButtonPressed()
    {
        const AZStd::vector<AZStd::string> filenames = GetMainWindow()->GetFileManager()->LoadMotionsFileDialog(this);
        CommandSystem::LoadMotionsCommand(filenames);
    }


    void MotionListWindow::OnClearMotionsButtonPressed()
    {
        // show the save dirty files window before
        if (mMotionWindowPlugin->OnSaveDirtyMotions() == DirtyFileManager::CANCELED)
        {
            return;
        }

        // iterate through the motions and put them into some array
        const uint32 numMotions = EMotionFX::GetMotionManager().GetNumMotions();
        AZStd::vector<EMotionFX::Motion*> motionsToRemove;
        motionsToRemove.reserve(numMotions);

        for (uint32 i = 0; i < numMotions; ++i)
        {
            EMotionFX::Motion* motion = EMotionFX::GetMotionManager().GetMotion(i);
            if (motion->GetIsOwnedByRuntime())
            {
                continue;
            }
            motionsToRemove.push_back(motion);
        }

        // construct the command group and remove the selected motions
        AZStd::vector<EMotionFX::Motion*> failedRemoveMotions;
        CommandSystem::RemoveMotions(motionsToRemove, &failedRemoveMotions);

        // show the window if at least one failed remove motion
        if (!failedRemoveMotions.empty())
        {
            MotionListRemoveMotionsFailedWindow removeMotionsFailedWindow(this, failedRemoveMotions);
            removeMotionsFailedWindow.exec();
        }
    }


    void MotionListWindow::OnRemoveMotionsButtonPressed()
    {
        const CommandSystem::SelectionList& selection = GetCommandManager()->GetCurrentSelection();

        // get the number of selected motions
        const uint32 numMotions = selection.GetNumSelectedMotions();
        if (numMotions == 0)
        {
            return;
        }

        // iterate through the selected motions and put them into some array
        AZStd::vector<EMotionFX::Motion*> motionsToRemove;
        motionsToRemove.reserve(numMotions);
        for (uint32 i = 0; i < numMotions; ++i)
        {
            EMotionFX::Motion* motion = selection.GetMotion(i);

            // in case we modified the motion ask if the user wants to save changes it before removing it
            mMotionWindowPlugin->SaveDirtyMotion(motion, nullptr, true, false);
            motionsToRemove.push_back(motion);
        }

        // find the lowest row selected
        uint32 lowestRowSelected = MCORE_INVALIDINDEX32;
        const QList<QTableWidgetItem*> selectedItems = mMotionTable->selectedItems();
        const int numSelectedItems = selectedItems.size();
        for (int i = 0; i < numSelectedItems; ++i)
        {
            if ((uint32)selectedItems[i]->row() < lowestRowSelected)
            {
                lowestRowSelected = (uint32)selectedItems[i]->row();
            }
        }

        // construct the command group and remove the selected motions
        AZStd::vector<EMotionFX::Motion*> failedRemoveMotions;
        CommandSystem::RemoveMotions(motionsToRemove, &failedRemoveMotions);

        // selected the next row
        if (lowestRowSelected > ((uint32)mMotionTable->rowCount() - 1))
        {
            mMotionTable->selectRow(lowestRowSelected - 1);
        }
        else
        {
            mMotionTable->selectRow(lowestRowSelected);
        }

        // show the window if at least one failed remove motion
        if (!failedRemoveMotions.empty())
        {
            MotionListRemoveMotionsFailedWindow removeMotionsFailedWindow(this, failedRemoveMotions);
            removeMotionsFailedWindow.exec();
        }
    }


    void MotionListWindow::OnStopSelectedMotionsButton()
    {
        mMotionWindowPlugin->StopSelectedMotions();
    }


    void MotionListWindow::OnStopAllMotionsButton()
    {
        // execute the command
        AZStd::string outResult;
        GetCommandManager()->ExecuteCommand("StopAllMotionInstances", outResult);
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // constructor
    MotionTableWidget::MotionTableWidget(MotionWindowPlugin* parentPlugin, QWidget* parent)
        : QTableWidget(parent)
    {
        mPlugin = parentPlugin;

        // enable dragging
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
    }


    // destructor
    MotionTableWidget::~MotionTableWidget()
    {
    }


    // return the mime data
    QMimeData* MotionTableWidget::mimeData(const QList<QTableWidgetItem*> items) const
    {
        MCORE_UNUSED(items);

        // get the current selection list
        const CommandSystem::SelectionList& selectionList = GetCommandManager()->GetCurrentSelection();

        // get the number of selected motions and return directly if there are no motions selected
        AZStd::string textData, command;
        const uint32 numMotions = selectionList.GetNumSelectedMotions();
        for (uint32 i = 0; i < numMotions; ++i)
        {
            EMotionFX::Motion* motion = selectionList.GetMotion(i);

            // construct the drag&drop data string
            command = AZStd::string::format("-window \"MotionWindow\" -motionID %i\n", motion->GetID());
            textData += command;
        }

        // create the data, set the text and return it
        QMimeData* mimeData = new QMimeData();
        mimeData->setText(textData.c_str());
        return mimeData;
    }


    // return the supported mime types
    QStringList MotionTableWidget::mimeTypes() const
    {
        QStringList result;
        result.append("text/plain");
        return result;
    }


    // get the allowed drop actions
    Qt::DropActions MotionTableWidget::supportedDropActions() const
    {
        return Qt::CopyAction;
    }
} // namespace EMStudio

#include <EMotionFX/Tools/EMotionStudio/Plugins/StandardPlugins/Source/MotionWindow/MotionListWindow.moc>