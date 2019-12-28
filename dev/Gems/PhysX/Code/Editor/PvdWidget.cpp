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

#include <PhysX_precompiled.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzToolsFramework/UI/PropertyEditor/ReflectedPropertyEditor.hxx>
#include <AzToolsFramework/UI/PropertyEditor/InstanceDataHierarchy.h>
#include <QBoxLayout>
#include <Editor/PvdWidget.h>
#include <Editor/DocumentationLinkWidget.h>
#include <Source/NameConstants.h>

namespace PhysX
{
    namespace Editor
    {
        static const char* const s_pvdDocumentationLink = "Learn more about the <a href=%0>PhysX Visual Debugger (PVD).</a>";
        static const char* const s_pvdDocumentationAddress = "configuration/debugger";

        PvdWidget::PvdWidget(QWidget* parent)
            : QWidget(parent)
        {
            CreatePropertyEditor(this);
        }

        void PvdWidget::SetValue(const PhysX::Settings& settings)
        {
            m_settings = settings;

            blockSignals(true);
            m_propertyEditor->ClearInstances();
            m_propertyEditor->AddInstance(&m_settings);
            m_propertyEditor->InvalidateAll();
            blockSignals(false);
        }

        void PvdWidget::CreatePropertyEditor(QWidget* parent)
        {
            QVBoxLayout* verticalLayout = new QVBoxLayout(parent);
            verticalLayout->setContentsMargins(0, 0, 0, 0);
            verticalLayout->setSpacing(0);

            m_documentationLinkWidget = new DocumentationLinkWidget(s_pvdDocumentationLink, (UXNameConstants::GetPhysXDocsRoot() + s_pvdDocumentationAddress).c_str());

            AZ::SerializeContext* m_serializeContext;
            AZ::ComponentApplicationBus::BroadcastResult(m_serializeContext, &AZ::ComponentApplicationRequests::GetSerializeContext);
            AZ_Assert(m_serializeContext, "Failed to retrieve serialize context.");

            static const int propertyLabelWidth = 250;
            m_propertyEditor = new AzToolsFramework::ReflectedPropertyEditor(parent);
            m_propertyEditor->Setup(m_serializeContext, this, true, propertyLabelWidth);
            m_propertyEditor->show();
            m_propertyEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

            verticalLayout->addWidget(m_documentationLinkWidget);
            verticalLayout->addWidget(m_propertyEditor);
        }

        void PvdWidget::BeforePropertyModified(AzToolsFramework::InstanceDataNode* /*node*/)
        {

        }

        void PvdWidget::AfterPropertyModified(AzToolsFramework::InstanceDataNode* /*node*/)
        {
            emit onValueChanged(m_settings);
        }

        void PvdWidget::SetPropertyEditingActive(AzToolsFramework::InstanceDataNode* /*node*/)
        {

        }

        void PvdWidget::SetPropertyEditingComplete(AzToolsFramework::InstanceDataNode* /*node*/)
        {
            emit onValueChanged(m_settings);
        }

        void PvdWidget::SealUndoStack()
        {

        }
    } // Editor
} // PhysX

#include <Editor/PvdWidget.moc>