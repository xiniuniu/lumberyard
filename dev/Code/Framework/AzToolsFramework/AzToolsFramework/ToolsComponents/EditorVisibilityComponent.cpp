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
#include "StdAfx.h"
#include "EditorVisibilityComponent.h"

#include <AzCore/Serialization/EditContext.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/Viewport/ViewportMessages.h>

namespace AzToolsFramework
{
    namespace Components
    {
        void EditorVisibilityComponent::Reflect(AZ::ReflectContext* context)
        {
            if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
            {
                serializeContext->Class<EditorVisibilityComponent, EditorComponentBase>()
                    ->Field("VisibilityFlag", &EditorVisibilityComponent::m_visibilityFlag)
                    ;

                if (AZ::EditContext* editContext = serializeContext->GetEditContext())
                {
                    editContext->Class<EditorVisibilityComponent>("Visibility", "Edit-time entity visibility")
                        ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                            ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::Hide)
                            ->Attribute(AZ::Edit::Attributes::SliceFlags, AZ::Edit::SliceFlags::NotPushable)
                            ->Attribute(AZ::Edit::Attributes::HideIcon, true);
                }
            }
        }

        void EditorVisibilityComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& services)
        {
            services.push_back(AZ_CRC("EditorVisibilityService", 0x90888caf));
        }

        void EditorVisibilityComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& services)
        {
            services.push_back(AZ_CRC("EditorVisibilityService", 0x90888caf));
        }

        EditorVisibilityComponent::~EditorVisibilityComponent()
        {
            // We disconnect from the bus here because we need to be able to respond even if the entity and component are not active
            // This is a special case for certain EditorComponents only!
            EditorVisibilityRequestBus::Handler::BusDisconnect();
        }

        void EditorVisibilityComponent::Init()
        {
            // Current visibility isn't saved to disk. Initialize it with the visibility flag's value.
            m_currentVisibility = m_visibilityFlag;

            // We connect to the bus here because we need to be able to respond even if the entity and component are not active
            // This is a special case for certain EditorComponents only!
            EditorVisibilityRequestBus::Handler::BusConnect(GetEntityId());
        }

        void EditorVisibilityComponent::Activate()
        {
            EditorComponentBase::Activate();
            EditorVisibilityNotificationBus::Event(
                GetEntityId(), &EditorVisibilityNotifications::OnEntityVisibilityFlagChanged, m_visibilityFlag);
        }

        void EditorVisibilityComponent::Deactivate()
        {
            EditorComponentBase::Deactivate();
        }

        void EditorVisibilityComponent::SetCurrentVisibility(bool visibility)
        {
            if (m_currentVisibility != visibility)
            {
                m_currentVisibility = visibility;

                // notify individual entities connected to this bus
                EditorEntityVisibilityNotificationBus::Event(
                    m_entity->GetId(), &EditorEntityVisibilityNotifications::OnEntityVisibilityChanged, visibility);

                // notify systems connected to this bus of the entity that changed
                EditorContextVisibilityNotificationBus::Event(
                    GetEntityContextId(), &EditorContextVisibilityNotifications::OnEntityVisibilityChanged,
                    m_entity->GetId(), visibility);
            }
        }

        bool EditorVisibilityComponent::GetCurrentVisibility()
        {
            return m_currentVisibility;
        }

        void EditorVisibilityComponent::SetVisibilityFlag(bool flag)
        {
            if (m_visibilityFlag != flag)
            {
                m_visibilityFlag = flag;

                AzToolsFramework::ToolsApplicationRequestBus::Broadcast(
                    &AzToolsFramework::ToolsApplicationRequestBus::Events::AddDirtyEntity, m_entity->GetId());

                // notify individual entities connected to this bus
                EditorEntityVisibilityNotificationBus::Event(
                    m_entity->GetId(), &EditorEntityVisibilityNotifications::OnEntityVisibilityFlagChanged, flag);

                // notify systems connected to this bus of the entity that changed
                EditorContextVisibilityNotificationBus::Event(
                    GetEntityContextId(), &EditorContextVisibilityNotifications::OnEntityVisibilityFlagChanged,
                    m_entity->GetId(), flag);
            }
        }

        bool EditorVisibilityComponent::GetVisibilityFlag()
        {
            return m_visibilityFlag;
        }
    } // namespace Components
} // namespace AzToolsFramework
