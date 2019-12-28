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

#include "EditorEntityModelComponent.h"

#include <AzToolsFramework/Entity/EditorEntityModel.h>

#include <AzCore/RTTI/BehaviorContext.h>

namespace AzToolsFramework
{
    namespace Components
    {
        EditorEntityModelComponent::EditorEntityModelComponent() = default;
        EditorEntityModelComponent::~EditorEntityModelComponent() = default;

        void EditorEntityModelComponent::Reflect(AZ::ReflectContext* context)
        {
            if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
            {
                serializeContext->Class<EditorEntityModelComponent, AZ::Component>(); // Empty class
            }

            if (auto behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
            {
                behaviorContext->EBus<EditorEntityInfoRequestBus>("EditorEntityInfoRequestBus")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation)
                    ->Attribute(AZ::Script::Attributes::Category, "Entity")
                    ->Attribute(AZ::Script::Attributes::Module, "editor")
                    ->Event("GetParent", &EditorEntityInfoRequests::GetParent)
                    ->Event("GetChildren", &EditorEntityInfoRequests::GetChildren)
                    ->Event("GetChild", &EditorEntityInfoRequests::GetChild)
                    ->Event("GetChildCount", &EditorEntityInfoRequests::GetChildCount)
                    ->Event("GetChildIndex", &EditorEntityInfoRequests::GetChildIndex)
                    ->Event("GetName", &EditorEntityInfoRequests::GetName)
                    ;

                behaviorContext->EBus<EditorEntityAPIBus>("EditorEntityAPIBus")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation)
                    ->Attribute(AZ::Script::Attributes::Category, "Entity")
                    ->Attribute(AZ::Script::Attributes::Module, "editor")
                    ->Event("SetName", &EditorEntityAPIRequests::SetName)
                    ;
            }
        }

        void EditorEntityModelComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& services)
        {
            services.push_back(AZ_CRC("EditorEntityModelService", 0x9d215543));
        }

        void EditorEntityModelComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& services)
        {
            services.push_back(AZ_CRC("EditorEntityModelService", 0x9d215543));
        }

        void EditorEntityModelComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& services)
        {
            services.push_back(AZ_CRC("EditorEntityContextService", 0x28d93a43));
        }

        void EditorEntityModelComponent::Activate()
        {
            m_entityModel = AZStd::make_unique<EditorEntityModel>();
        }

        void EditorEntityModelComponent::Deactivate()
        {
            m_entityModel.reset();
        }

    } // namespace Components
} // namespace AzToolsFramework
