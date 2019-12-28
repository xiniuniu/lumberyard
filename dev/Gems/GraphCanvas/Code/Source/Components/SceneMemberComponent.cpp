
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
#include "precompiled.h"

#include <AzCore/Serialization/SerializeContext.h>

#include <Components/SceneMemberComponent.h>
#include <Components/PersistentIdComponent.h>

namespace GraphCanvas
{
    /////////////////////////
    // SceneMemberComponent
    /////////////////////////

    void SceneMemberComponent::Reflect(AZ::ReflectContext* reflectContext)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflectContext);

        if (serializeContext)
        {
            serializeContext->Class<SceneMemberComponent, AZ::Component>()
                ->Version(1)
            ;
        }
    }


    SceneMemberComponent::SceneMemberComponent()
    {
    }

    void SceneMemberComponent::Init()
    {
        AZ::EntityBus::Handler::BusConnect(GetEntityId());
    }

    void SceneMemberComponent::Activate()
    {
        SceneMemberRequestBus::Handler::BusConnect(GetEntityId());
    }

    void SceneMemberComponent::Deactivate()
    {
        SceneMemberRequestBus::Handler::BusDisconnect();
        AZ::EntityBus::Handler::BusDisconnect();
    }

    void SceneMemberComponent::SetScene(const AZ::EntityId& sceneId)
    {
        if (m_sceneId != sceneId)
        {
            AZ_Warning("Graph Canvas", !m_sceneId.IsValid(), "Trying to change a SceneMember's scene without removing it from the previous scene.");
            if (m_sceneId.IsValid())
            {
                ClearScene(m_sceneId);
            }

            m_sceneId = sceneId;

            SceneMemberNotificationBus::Event(GetEntityId(), &SceneMemberNotifications::OnSceneSet, sceneId);
        }
    }

    void SceneMemberComponent::ClearScene(const AZ::EntityId& sceneId)
    {
        AZ_Warning("Graph Canvas", m_sceneId == sceneId, "Trying to remove a SceneMember from a scene it is not apart of.");

        if (m_sceneId == sceneId)
        {
            SceneMemberNotificationBus::Event(GetEntityId(), &SceneMemberNotifications::OnRemovedFromScene, sceneId);
            m_sceneId.SetInvalid();
        }
    }

    void SceneMemberComponent::SignalMemberSetupComplete()
    {
        SceneMemberNotificationBus::Event(GetEntityId(), &SceneMemberNotifications::OnMemberSetupComplete);
    }

    AZ::EntityId SceneMemberComponent::GetScene() const
    {
        return m_sceneId;
    }

    bool SceneMemberComponent::LockForExternalMovement(const AZ::EntityId& sceneMemberId)
    {
        // Idea on doing anchoring and the like. Register to the OnSceneMemberPosition changed bus for this id.
        // And then you can just calculate the movement and move yourself by the correct amount.
        if (!m_lockedSceneMember.IsValid())
        {
            m_lockedSceneMember = sceneMemberId;
        }

        return m_lockedSceneMember == sceneMemberId;
    }

    void SceneMemberComponent::UnlockForExternalMovement(const AZ::EntityId& sceneMemberId)
    {
        if (m_lockedSceneMember == sceneMemberId)
        {
            m_lockedSceneMember.SetInvalid();
        }
    }

    void SceneMemberComponent::OnEntityExists(const AZ::EntityId& entityId)
    {
        AZ::EntityBus::Handler::BusDisconnect();

        // Temporary version conversion added in 1.xx to add a PersistentId onto the SceneMembers.
        // Remove after a few revisions with warnings about resaving graphs.
        if (AZ::EntityUtils::FindFirstDerivedComponent<PersistentIdComponent>(GetEntityId()) == nullptr)
        {
            AZ::Entity* selfEntity = GetEntity();
            if (selfEntity)
            {
                selfEntity->CreateComponent<PersistentIdComponent>();
            }
        }
    }
}