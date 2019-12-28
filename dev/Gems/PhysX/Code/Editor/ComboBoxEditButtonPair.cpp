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

#include <PhysX_precompiled.h>

#include <Editor/ComboBoxEditButtonPair.h>

namespace PhysX
{
    namespace Editor
    {
        ComboBoxEditButtonPair::ComboBoxEditButtonPair(QWidget* parent)
            : QWidget(parent)
        {
            QHBoxLayout* layout = new QHBoxLayout(this);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(0);

            m_comboBox = new QComboBox();
            m_comboBox->installEventFilter(this);

            m_editButton = new QPushButton();
            m_editButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            m_editButton->setFixedSize(QSize(24, 16));
            m_editButton->setToolTip(QString("Edit"));
            m_editButton->setIcon(QIcon(":/PropertyEditor/Resources/edit-asset.png"));

            layout->addWidget(m_comboBox);
            layout->addWidget(m_editButton);
        }

        bool ComboBoxEditButtonPair::eventFilter(QObject *object, QEvent *event)
        {
            return event->type() == QEvent::Wheel;
        }

        QComboBox* ComboBoxEditButtonPair::GetComboBox()
        { 
            return m_comboBox; 
        }

        QPushButton* ComboBoxEditButtonPair::GetEditButton()
        { 
            return m_editButton; 
        }
    }
}