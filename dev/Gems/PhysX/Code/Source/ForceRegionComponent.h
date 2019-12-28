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

#include <Source/ForceRegionForces.h>
#include <Source/ForceRegion.h>

#include <PhysX/ComponentTypeIds.h>

#include <AzCore/Component/Component.h>

#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <AzFramework/Physics/World.h>
#include <AzFramework/Physics/TriggerBus.h>

namespace PhysX
{
    /// ForceRegionComponent
    /// Applies a forces to objects within a region.
    /// Uses a PhysX trigger collider to receive notifications about entities entering and exiting the region. 
    /// A net force will be calculated per entity by summing all the attached forces on each tick.
    class ForceRegionComponent
        : public AZ::Component
        , protected Physics::WorldNotificationBus::Handler
        , protected Physics::TriggerNotificationBus::Handler
        , private AzFramework::EntityDebugDisplayEventBus::Handler
    {
    public:
        AZ_COMPONENT(ForceRegionComponent, ForceRegionComponentTypeId);
        static void Reflect(AZ::ReflectContext* context);

        ForceRegionComponent() = default;
        ForceRegionComponent(const ForceRegion& forceRegion, bool debug);
        ~ForceRegionComponent() = default;

    protected:
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("TransformService", 0x8ee22c50));
            required.push_back(AZ_CRC("PhysXTriggerService", 0x3a117d7b));
        }

        // Component
        void Activate() override;
        void Deactivate() override;

        // Physics::WorldNotificationBus
        void OnPostPhysicsUpdate(float fixedDeltaTime) override;
        int GetPhysicsTickOrder() override;

        // TriggerNotifications
        void OnTriggerEnter(const Physics::TriggerEvent& triggerEvent) override;
        void OnTriggerExit(const Physics::TriggerEvent& triggerEvent) override;

        // EntityDebugDisplayEventBus
        void DisplayEntityViewport(const AzFramework::ViewportInfo& viewportInfo
            , AzFramework::DebugDisplayRequests& debugDisplayRequests) override;

    private:
        AZStd::unordered_set<AZ::EntityId> m_entities; ///< Collection of entity IDs contained within the region.
        ForceRegion m_forceRegion; ///< Calculates the net force.
        bool m_debugForces = false; ///< Draws debug lines for entities in the region
    };
}