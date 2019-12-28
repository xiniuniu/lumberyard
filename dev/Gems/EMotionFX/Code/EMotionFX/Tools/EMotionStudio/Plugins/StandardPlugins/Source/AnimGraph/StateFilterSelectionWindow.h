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

#include <MCore/Source/StandardHeaders.h>
#include <EMotionFX/Source/AnimGraphStateMachine.h>
#include <EMotionFX/Source/AnimGraphTransitionCondition.h>
#include "../StandardPluginsConfig.h"
#include <QDialog>
#include <QTableWidget>
#include <QTableWidgetItem>

EMFX_FORWARD_DECLARE(AnimGraphNodeGroup);


namespace EMStudio
{
    // forward declarations
    class AnimGraphPlugin;

    class StateFilterSelectionWindow
        : public QDialog
    {
        Q_OBJECT
        MCORE_MEMORYOBJECTCATEGORY(StateFilterSelectionWindow, MCore::MCORE_DEFAULT_ALIGNMENT, MEMCATEGORY_STANDARDPLUGINS_ANIMGRAPH);

    public:
        StateFilterSelectionWindow(QWidget* parent);
        ~StateFilterSelectionWindow();

        void ReInit(EMotionFX::AnimGraphStateMachine* stateMachine, const AZStd::vector<EMotionFX::AnimGraphNodeId>& oldNodeSelection, const AZStd::vector<AZStd::string>& oldGroupSelection);

        const AZStd::vector<EMotionFX::AnimGraphNodeId> GetSelectedNodeIds() const       { return m_selectedNodeIds; }
        const AZStd::vector<AZStd::string>& GetSelectedGroupNames() const                { return mSelectedGroupNames; }

    protected slots:
        void OnSelectionChanged();

    private:
        struct WidgetLookup
        {
            MCORE_MEMORYOBJECTCATEGORY(StateFilterSelectionWindow::WidgetLookup, EMFX_DEFAULT_ALIGNMENT, MEMCATEGORY_STANDARDPLUGINS_ANIMGRAPH);
            QTableWidgetItem*   mWidget;
            AZStd::string       mName;
            bool                mIsGroup;

            WidgetLookup(QTableWidgetItem* widget, const char* name, bool isGroup)
            {
                mWidget     = widget;
                mName       = name;
                mIsGroup    = isGroup;
            }
        };

        EMotionFX::AnimGraphNodeGroup* FindGroupByWidget(QTableWidgetItem* widget) const;
        EMotionFX::AnimGraphNode* FindNodeByWidget(QTableWidgetItem* widget) const;
        void AddRow(uint32 rowIndex, const char* name, bool isGroup, bool isSelected, const QColor& color = QColor(255, 255, 255));

        AZStd::vector<WidgetLookup>         mWidgetTable;
        AZStd::vector<AZStd::string>        mSelectedGroupNames;
        AZStd::vector<EMotionFX::AnimGraphNodeId> m_selectedNodeIds;
        QTableWidget*                       mTableWidget;
        EMotionFX::AnimGraphStateMachine*   m_stateMachine;
    };
} // namespace EMStudio