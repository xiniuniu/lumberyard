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
#include <AzCore/Component/Component.h>
#include <AzFramework/Physics/CharacterBus.h>
#include <AzFramework/Physics/SystemBus.h>
#include <API/CharacterController.h>
#include <AzCore/Component/TransformBus.h>
#include <PhysXCharacters/CharacterControllerBus.h>

namespace PhysXCharacters
{
    class CharacterControllerConfiguration;
    class CharacterControllerCosmeticReplica;

    /// Component used to physically represent characters for basic interactions with the physical world, for example to
    /// prevent walking through walls or falling through terrain.
    class CharacterControllerComponent
        : public AZ::Component
        , public Physics::CharacterRequestBus::Handler
        , public AZ::TransformNotificationBus::Handler
        , public CharacterControllerRequestBus::Handler
    {
    public:
        AZ_COMPONENT(CharacterControllerComponent, "{BCBD8448-2FFC-450D-B82F-7C297D2F0C8C}");

        static void Reflect(AZ::ReflectContext* context);

        CharacterControllerComponent();
        CharacterControllerComponent(AZStd::unique_ptr<Physics::CharacterConfiguration> characterConfig,
            AZStd::unique_ptr<Physics::ShapeConfiguration> shapeConfig);
        ~CharacterControllerComponent();

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("PhysXCharacterControllerService", 0x428de4fa));
        }

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC("PhysXCharacterControllerService", 0x428de4fa));
        }

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("TransformService", 0x8ee22c50));
        }

        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
        {
            dependent.push_back(AZ_CRC("PhysXColliderService", 0x4ff43f7c));
        }

        Physics::CharacterConfiguration& GetCharacterConfiguration()
        {
            return *m_characterConfig;
        }

    protected:
        // AZ::Component
        void Init() override;
        void Activate() override;
        void Deactivate() override;

        // Physics::CharacterRequestBus
        AZ::Vector3 GetBasePosition() const override;
        void SetBasePosition(const AZ::Vector3& position) override;
        AZ::Vector3 GetCenterPosition() const override;
        float GetStepHeight() const override;
        void SetStepHeight(float stepHeight) override;
        AZ::Vector3 GetUpDirection() const override;
        void SetUpDirection(const AZ::Vector3& upDirection) override;
        float GetSlopeLimitDegrees() const override;
        void SetSlopeLimitDegrees(float slopeLimitDegrees) override;
        AZ::Vector3 GetVelocity() const override;
        AZ::Vector3 TryRelativeMove(const AZ::Vector3& deltaPosition, float deltaTime) override;
        bool IsPresent() const override { return true; }

        // CharacterControllerRequestBus
        void Resize(float height) override;
        float GetHeight() override;
        void SetHeight(float height);
        float GetRadius() override;
        void SetRadius(float radius) override;
        float GetHalfSideExtent() override;
        void SetHalfSideExtent(float halfSideExtent) override;
        float GetHalfForwardExtent() override;
        void SetHalfForwardExtent(float halfForwardExtent) override;

        // TransformNotificationBus
        void OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world) override;

    private:
        void AttachColliders(Physics::Character& character);
        bool ValidateDirectlyControlled();

        AZStd::unique_ptr<Physics::CharacterConfiguration> m_characterConfig;
        AZStd::unique_ptr<Physics::ShapeConfiguration> m_shapeConfig;
        AZStd::unique_ptr<Physics::Character> m_controller;
    };
} // namespace PhysXCharacters
