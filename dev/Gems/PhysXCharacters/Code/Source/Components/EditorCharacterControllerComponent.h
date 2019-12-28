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
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <AzFramework/Physics/ShapeConfiguration.h>
#include <API/CharacterController.h>
#include <PhysX/ConfigurationBus.h>

namespace PhysXCharacters
{
    class CharacterControllerConfiguration;

    /// Proxy container for only displaying a specific shape configuration depending on the shapeType selected.
    struct EditorProxyShapeConfig
    {
        AZ_CLASS_ALLOCATOR(EditorProxyShapeConfig, AZ::SystemAllocator, 0);
        AZ_RTTI(EditorProxyShapeConfig, "{0A9F0213-E281-4424-97C5-BAD2D318F496}");
        static void Reflect(AZ::ReflectContext* context);

        EditorProxyShapeConfig() = default;
        EditorProxyShapeConfig(const Physics::ShapeConfiguration& shapeConfiguration);
        virtual ~EditorProxyShapeConfig() = default;

        Physics::ShapeType m_shapeType = Physics::ShapeType::Capsule;
        Physics::CapsuleShapeConfiguration m_capsule;
        Physics::BoxShapeConfiguration m_box;

        bool IsBoxConfig() const;
        bool IsCapsuleConfig() const;
        virtual const Physics::ShapeConfiguration& GetCurrent() const;
    };

    /// Editor component that allows a PhysX character controller to be edited.
    class EditorCharacterControllerComponent
        : public AzToolsFramework::Components::EditorComponentBase
        , protected AzFramework::EntityDebugDisplayEventBus::Handler
        , protected AzToolsFramework::EntitySelectionEvents::Bus::Handler
        , private PhysX::ConfigurationNotificationBus::Handler
    {
    public:
        AZ_EDITOR_COMPONENT(EditorCharacterControllerComponent, "{F361E19D-34C7-4E70-BF1B-909F48305702}",
            AzToolsFramework::Components::EditorComponentBase);
        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("PhysXCharacterControllerService", 0x428de4fa));
        }

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC("PhysXCharacterControllerService", 0x428de4fa));
            incompatible.push_back(AZ_CRC("LegacyCryPhysicsService", 0xbb370351));
        }

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("TransformService", 0x8ee22c50));
        }

        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
        {
        }

        EditorCharacterControllerComponent() = default;

        const CharacterControllerConfiguration& GetCharacterConfiguration() const
        {
            return m_configuration;
        }

        const EditorProxyShapeConfig& GetShapeConfiguration() const
        {
            return m_proxyShapeConfiguration;
        }

    protected:

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AzToolsFramework::EntitySelectionEvents
        void OnSelected() override;
        void OnDeselected() override;

        // AzFramework::EntityDebugDisplayEventBus
        void DisplayEntityViewport(
            const AzFramework::ViewportInfo& viewportInfo,
            AzFramework::DebugDisplayRequests& debugDisplay) override;

        // PhysX::ConfigurationNotificationBus
        virtual void OnConfigurationRefreshed(const PhysX::Configuration& configuration) override;

        // EditorComponentBase
        void BuildGameEntity(AZ::Entity* gameEntity) override;

        // Editor change notification
        AZ::u32 OnControllerConfigChanged();
        AZ::u32 OnShapeConfigChanged();

    private:

        CharacterControllerConfiguration m_configuration;
        EditorProxyShapeConfig m_proxyShapeConfiguration;

        AZStd::vector<AZ::Vector3> m_vertexBuffer;
        AZStd::vector<AZ::u32> m_indexBuffer;
        AZStd::vector<AZ::Vector3> m_lineBuffer;
    };
} // namespace PhysXCharacters


