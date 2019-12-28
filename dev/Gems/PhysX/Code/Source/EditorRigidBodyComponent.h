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

#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <AzFramework/Physics/RigidBody.h>

#include <AzCore/Component/TransformBus.h>
#include <PhysX/EditorRigidBodyRequestBus.h>

namespace PhysX
{
    /// Configuration data for EditorPhysXRigidBodyComponent.
    ///
    struct EditorRigidBodyConfiguration
        : public Physics::RigidBodyConfiguration
    {
        AZ_CLASS_ALLOCATOR(EditorRigidBodyConfiguration, AZ::SystemAllocator, 0);
        AZ_RTTI(EditorRigidBodyConfiguration, "{27297024-5A99-4C58-8614-4EF18137CE69}", Physics::RigidBodyConfiguration);

        static void Reflect(AZ::ReflectContext* context);

        bool ShowInertia() const
        {
            return !m_computeInertiaTensor;
        }

        // Debug properties.
        bool m_centerOfMassDebugDraw = false;

        void OnConfigurationChanged();
    };

    /// Class for in-editor PhysX Rigid Body Component.
    ///
    class EditorRigidBodyComponent
        : public AzToolsFramework::Components::EditorComponentBase
        , protected AzFramework::EntityDebugDisplayEventBus::Handler
        , private AZ::TransformNotificationBus::Handler
        , private EditorRigidBodyRequestBus::Handler
    {
    public:
        AZ_EDITOR_COMPONENT(EditorRigidBodyComponent, "{F2478E6B-001A-4006-9D7E-DCB5A6B041DD}", AzToolsFramework::Components::EditorComponentBase);
        static void Reflect(AZ::ReflectContext* context);

        EditorRigidBodyComponent() = default;
        explicit EditorRigidBodyComponent(const EditorRigidBodyConfiguration& configuration);
        ~EditorRigidBodyComponent() = default;

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("PhysXRigidBodyService", 0x1d4c64a8));
        }

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC("PhysXRigidBodyService", 0x1d4c64a8));
            incompatible.push_back(AZ_CRC("PhysicsService", 0xa7350d22));
            incompatible.push_back(AZ_CRC("LegacyCryPhysicsService", 0xbb370351));
        }

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("TransformService", 0x8ee22c50));
        }

        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
        {
            dependent.push_back(AZ_CRC("PhysXColliderService", 0x4ff43f7c));
        }

        // AZ::Component
        void Init() override;
        void Activate() override;
        void Deactivate() override;

        // EditorComponentBase
        void BuildGameEntity(AZ::Entity* gameEntity) override;

        // AzFramework::EntityDebugDisplayEventBus
        void DisplayEntityViewport(
            const AzFramework::ViewportInfo& viewportInfo,
            AzFramework::DebugDisplayRequests& debugDisplay) override;

        // TransformBus
        void OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world) override;

    private:
        EditorRigidBodyConfiguration m_config;
        AZStd::unique_ptr<Physics::RigidBody> m_editorBody;

        // EditorRigidBodyRequestBus
        void RefreshEditorRigidBody() override;

        void CreateEditorWorldRigidBody();
    };
} // namespace PhysX