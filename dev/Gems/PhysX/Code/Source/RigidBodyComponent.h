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

#include <PxPhysicsAPI.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzFramework/Physics/RigidBody.h>
#include <AzFramework/Physics/RigidBodyBus.h>
#include <AzFramework/Physics/World.h>
#include <AzFramework/Entity/EntityContextBus.h>

namespace PhysX
{
    class TransformForwardTimeInterpolator;

    /// Component used to register an entity as a dynamic rigid body in the PhysX simulation.
    class RigidBodyComponent
        : public AZ::Component
        , public Physics::RigidBodyRequestBus::Handler
        , public AZ::TickBus::Handler
        , public AzFramework::EntityContextEventBus::Handler
        , protected AZ::TransformNotificationBus::MultiHandler
        , protected Physics::WorldNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(RigidBodyComponent, "{D4E52A70-BDE1-4819-BD3C-93AB3F4F3BE3}");

        static void Reflect(AZ::ReflectContext* context);

        RigidBodyComponent() = default;
        explicit RigidBodyComponent(const Physics::RigidBodyConfiguration& config);
        ~RigidBodyComponent() override = default;

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("PhysXRigidBodyService", 0x1d4c64a8));
        }

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC("PhysXRigidBodyService", 0x1d4c64a8));
            incompatible.push_back(AZ_CRC("PhysicsService", 0xa7350d22));
        }

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("TransformService", 0x8ee22c50));
        }

        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
        {
            dependent.push_back(AZ_CRC("PhysXColliderService", 0x4ff43f7c));
        }

        // RigidBodyRequests
        void EnablePhysics() override;
        void DisablePhysics() override;
        bool IsPhysicsEnabled() const override;

        AZ::Vector3 GetCenterOfMassWorld() const override;
        virtual AZ::Vector3 GetCenterOfMassLocal() const override;

        AZ::Matrix3x3 GetInverseInertiaWorld() const override;
        AZ::Matrix3x3 GetInverseInertiaLocal() const override;

        float GetMass() const override;
        float GetInverseMass() const override;
        void SetMass(float mass) override;
        void SetCenterOfMassOffset(const AZ::Vector3& comOffset) override;

        AZ::Vector3 GetLinearVelocity() const override;
        void SetLinearVelocity(const AZ::Vector3& velocity) override;
        AZ::Vector3 GetAngularVelocity() const override;
        void SetAngularVelocity(const AZ::Vector3& angularVelocity) override;
        AZ::Vector3 GetLinearVelocityAtWorldPoint(const AZ::Vector3& worldPoint) const override;
        void ApplyLinearImpulse(const AZ::Vector3& impulse) override;
        void ApplyLinearImpulseAtWorldPoint(const AZ::Vector3& impulse, const AZ::Vector3& worldPoint) override;
        void ApplyAngularImpulse(const AZ::Vector3& angularImpulse) override;

        float GetLinearDamping() const override;
        void SetLinearDamping(float damping) override;
        float GetAngularDamping() const override;
        void SetAngularDamping(float damping) override;

        bool IsAwake() const override;
        void ForceAsleep() override;
        void ForceAwake() override;

        bool IsKinematic() const override;
        void SetKinematic(bool kinematic) override;
        void SetKinematicTarget(const AZ::Transform& targetPosition) override;

        void SetGravityEnabled(bool enabled) override;
        void SetSimulationEnabled(bool enabled) override;

        float GetSleepThreshold() const override;
        void SetSleepThreshold(float threshold) override;
        AZ::Aabb GetAabb() const override;
        Physics::RigidBody* GetRigidBody() override;

        Physics::RigidBodyConfiguration& GetConfiguration() 
        { 
            return m_configuration; 
        }

        // EntityContextEventBus
        void OnSliceInstantiated(const AZ::Data::AssetId&, const AZ::SliceComponent::SliceInstanceAddress&) override;
        void OnSliceInstantiationFailed(const AZ::Data::AssetId&) override;

    protected:

        // AZ::Component
        void Init() override;
        void Activate() override;
        void Deactivate() override;

        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;
        int GetTickOrder() override;
        void OnPostPhysicsUpdate(float deltaTime) override;
        int GetPhysicsTickOrder() override;

        // TransformNotificationBus
        void OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world) override;

    private:
        void SetupConfiguration();
        void CreatePhysics();

        std::unique_ptr<TransformForwardTimeInterpolator> m_interpolator;

        Physics::RigidBodyConfiguration m_configuration;
        AZStd::unique_ptr<Physics::RigidBody> m_rigidBody;
        Physics::World* m_world = nullptr;

        AZ::Vector3 m_initialScale = AZ::Vector3::CreateOne();
        bool m_staticTransformAtActivation = false; ///< Whether the transform was static when the component last activated.
        bool m_isLastMovementFromKinematicSource = false; ///< True when the source of the movement comes from SetKinematicTarget as opposed to coming from a Transform change
    };

    class TransformForwardTimeInterpolator
    {
    public:
        AZ_RTTI(TransformForwardTimeInterpolator, "{2517631D-9CF3-4F9C-921C-03FB44DE377C}");
        AZ_CLASS_ALLOCATOR(TransformForwardTimeInterpolator, AZ::SystemAllocator, 0);
        virtual ~TransformForwardTimeInterpolator() = default;
        void Reset(const AZ::Vector3& position, const AZ::Quaternion& rotation);
        void SetTarget(const AZ::Vector3& position, const AZ::Quaternion& rotation, float fixedDeltaTime);
        void GetInterpolated(AZ::Vector3& position, AZ::Quaternion& rotation, float realDeltaTime);

    private:
        static const AZ::u32 FloatToIntegralResolution = 1000u;
        AZ::u32 FloatToIntegralTime(float deltaTime) { return static_cast<AZ::u32>(deltaTime * FloatToIntegralResolution) + m_integralTime; }

        AZ::LinearlyInterpolatedSample<AZ::Vector3> m_targetTranslation;
        AZ::LinearlyInterpolatedSample<AZ::Quaternion> m_targetRotation;

        float m_currentRealTime = 0.0f;
        float m_currentFixedTime = 0.0f;
        AZ::u32 m_integralTime = 0;
    };
} // namespace PhysX
