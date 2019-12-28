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
#include "stdafx.h"

#include "EditorCommon.h"
#include "AlignToolbarSection.h"

ModeToolbar::ModeToolbar(EditorWindow* parent)
    : QToolBar("Mode Toolbar", parent)
    , m_group(nullptr)
    , m_previousAction(nullptr)
    , m_alignToolbarSection(new AlignToolbarSection)
{
    setObjectName("ModeToolbar");    // needed to save state
    setFloatable(false);

    AddModes(parent);

    m_alignToolbarSection->AddButtons(this);

    parent->addToolBar(this);
}

void ModeToolbar::SetCheckedItem(int index)
{
    if (m_group)
    {
        for (auto action : m_group->actions())
        {
            if (action->data().toInt() == index)
            {
                m_previousAction->setChecked(false);
                m_previousAction = action;
                m_previousAction->setChecked(true);
                return;
            }
        }
    }
}

void ModeToolbar::AddModes(EditorWindow* parent)
{
    m_group = new QActionGroup(this);

    int i = 0;
    for (const auto& m : ViewportInteraction::InteractionMode())
    {
        int key = (Qt::Key_1 + i++);


        int mode = static_cast<int>(m);

        QString nodeName = ViewportHelpers::InteractionModeToString(mode);

        QString iconImageDefault = QString(":/Icons/Mode%1Default.png").arg(nodeName);
        QString iconImageHover = QString(":/Icons/Mode%1Hover.png").arg(nodeName);
        QString iconImageDisabled = QString(":/Icons/Mode%1Disabled.png").arg(nodeName);

        QIcon icon(iconImageDefault);
        icon.addPixmap(QPixmap(iconImageHover), QIcon::Active);
        icon.addPixmap(QPixmap(iconImageDisabled), QIcon::Disabled);

        QAction* action = new QAction(icon,
                (QString("%1 (%2)").arg(ViewportHelpers::InteractionModeToString(mode), QString(static_cast<char>(key)))),
                this);

        action->setData(mode);
        action->setShortcut(QKeySequence(key));
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        action->setCheckable(true);   // Give it the behavior of a toggle button.
        QObject::connect(action,
            &QAction::triggered,
            this,
            [ this, parent, action ](bool checked)
            {
                if (m_previousAction == action)
                {
                    // Nothing to do.
                    return;
                }

                parent->GetViewport()->GetViewportInteraction()->SetMode((ViewportInteraction::InteractionMode)action->data().toInt());

                m_previousAction = action;
            });
        m_group->addAction(action);
    }

    // Give it the behavior of radio buttons.
    m_group->setExclusive(true);

    // Set the first action as the default.
    m_previousAction = m_group->actions().constFirst();
    m_previousAction->setChecked(true);

    addActions(m_group->actions());
}

#include <ModeToolbar.moc>
