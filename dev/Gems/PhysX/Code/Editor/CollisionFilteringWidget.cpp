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

#include <Editor/CollisionFilteringWidget.h>
#include <Editor/CollisionLayersWidget.h>
#include <Editor/CollisionGroupsWidget.h>
#include <Editor/DocumentationLinkWidget.h>
#include <Source/NameConstants.h>
#include <PhysX/ConfigurationBus.h>
#include <QBoxLayout>

namespace PhysX
{
    namespace Editor
    {
        static const char* const s_collisionFilteringLink = "Learn more about <a href=%1>configuring collision filtering.</a>";
        static const char* const s_collisionFilteringAddress = "configuration/collision";

        CollisionFilteringWidget::CollisionFilteringWidget(QWidget* parent)
            : QWidget(parent)
        {
            QVBoxLayout* verticalLayout = new QVBoxLayout(this);
            verticalLayout->setContentsMargins(0, 0, 0, 0);
            verticalLayout->setSpacing(0);

            m_documentationLinkWidget = new DocumentationLinkWidget(s_collisionFilteringLink, (UXNameConstants::GetPhysXDocsRoot() + s_collisionFilteringAddress).c_str());
            m_collisionLayersWidget = new CollisionLayersWidget();
            m_collisionGroupsWidget = new CollisionGroupsWidget();

            m_tabs = new AzQtComponents::SegmentControl();
            m_tabs->addTab(m_collisionLayersWidget, "Layers");
            m_tabs->addTab(m_collisionGroupsWidget, "Groups");
            
            verticalLayout->addWidget(m_documentationLinkWidget);
            verticalLayout->addWidget(m_tabs);

            ConnectSignals();
        }

        void CollisionFilteringWidget::ConnectSignals()
        {
            connect(m_collisionLayersWidget, &CollisionLayersWidget::onValueChanged,
            this, [this](const Physics::CollisionLayers& layers)
            {
                m_layersConfig = layers;
                m_collisionGroupsWidget->SetValue(m_groupsConfig, m_layersConfig);
                emit onConfigurationChanged(m_layersConfig, m_groupsConfig);
            });

            connect(m_collisionGroupsWidget, &CollisionGroupsWidget::onValueChanged,
            this, [this](const Physics::CollisionGroups& groups)
            {
                m_groupsConfig = groups;
                emit onConfigurationChanged(m_layersConfig, m_groupsConfig);
            });
        }

        void CollisionFilteringWidget::SetConfiguration(const Physics::CollisionLayers& layers, const Physics::CollisionGroups& groups)
        {
            m_layersConfig = layers;
            m_groupsConfig = groups;
            m_collisionLayersWidget->SetValue(layers);
            m_collisionGroupsWidget->SetValue(groups, layers);
        }

        void CollisionFilteringWidget::ShowLayersTab()
        {
            int index = m_tabs->indexOf(m_collisionLayersWidget);
            m_tabs->setCurrentIndex(index);
        }

        void CollisionFilteringWidget::ShowGroupsTab()
        {
            int index = m_tabs->indexOf(m_collisionGroupsWidget);
            m_tabs->setCurrentIndex(index);
        }
    }
}

#include <Editor/CollisionFilteringWidget.moc>