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

#include <AzCore/EBus/EBus.h>
#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Outcome/Outcome.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzFramework/Physics/AnimationConfiguration.h>
#include <AzFramework/Physics/Character.h>


namespace EMotionFX
{
    class ActorInstance;

    namespace Integration
    {
        /**
        * EMotion FX attachment type.
        */
        enum class AttachmentType : AZ::u32
        {
            None = 0,           ///< Do not attach to another actor.
            ActorAttachment,    ///< Attach to another actor as a separately animating attachment.
            SkinAttachment,     ///< Attach to another actor as a skinned attachment (using the same skeleton as the attachment target).
        };

        enum class Space : AZ::u32
        {
            LocalSpace,         ///< Relative to the parent.
            ModelSpace,         ///< Relative to the origin of the character.
            WorldSpace          ///< Relative to the world origin.
        };

        /**
        * EMotion FX Actor Component Request Bus
        * Used for making requests to EMotion FX Actor Components.
        */
        class ActorComponentRequests
            : public AZ::ComponentBus
        {
        public:

            /// Retrieve component's actor instance.
            /// \return pointer to actor instance.
            virtual EMotionFX::ActorInstance* GetActorInstance() { return nullptr; }

            /// Find the name index of a given joint by its name.
            /// \param name The name of the join to search for, case insensitive.
            /// \return The joint index, or s_invalidJointIndex if no found.
            virtual size_t GetJointIndexByName(const char* /*name*/) const  { return s_invalidJointIndex; }

            /// Retrieve the local transform (relative to the parent) of a given joint.
            /// \param jointIndex The joint index to get the transform from.
            /// \param Space the space to get the transform in.
            virtual AZ::Transform GetJointTransform(size_t /*jointIndex*/, Space /*space*/) const  { return AZ::Transform::CreateIdentity(); }
            virtual void GetJointTransformComponents(size_t /*jointIndex*/, Space /*space*/, AZ::Vector3& outPosition, AZ::Quaternion& outRotation, AZ::Vector3& outScale) const  { outPosition = AZ::Vector3::CreateZero(); outRotation = AZ::Quaternion::CreateIdentity(); outScale = AZ::Vector3::CreateOne(); }

            virtual Physics::AnimationConfiguration* GetPhysicsConfig() const { return nullptr; }

            /// Attach to the specified entity.
            /// \param targetEntityId - Id of the entity to attach to.
            /// \param attachmentType - Desired type of attachment.
            virtual void AttachToEntity(AZ::EntityId /*targetEntityId*/, AttachmentType /*attachmentType*/) {}

            /// Detach from parent entity, if attached.
            virtual void DetachFromEntity() {}

            /// Enables debug-drawing of the actor's root.
            virtual void DebugDrawRoot(bool /*enable*/) {}

            /// Enables rendering of the actor.
            virtual bool GetRenderCharacter() const = 0;
            virtual void SetRenderCharacter(bool enable) = 0;

            static const size_t s_invalidJointIndex = ~0;
        };

        using ActorComponentRequestBus = AZ::EBus<ActorComponentRequests>;

        /**
        * EMotion FX Actor Component Notification Bus
        * Used for monitoring events from actor components.
        */
        class ActorComponentNotifications
            : public AZ::ComponentBus
        {
        public:

            //////////////////////////////////////////////////////////////////////////
            /**
            * Custom connection policy notifies connecting listeners immediately if actor instance is already created.
            */
            template<class Bus>
            struct AssetConnectionPolicy
                : public AZ::EBusConnectionPolicy<Bus>
            {
                static void Connect(typename Bus::BusPtr& busPtr, typename Bus::Context& context, typename Bus::HandlerNode& handler, const typename Bus::BusIdType& id = 0)
                {
                    AZ::EBusConnectionPolicy<Bus>::Connect(busPtr, context, handler, id);

                    EMotionFX::ActorInstance* instance = nullptr;
                    ActorComponentRequestBus::EventResult(instance, id, &ActorComponentRequestBus::Events::GetActorInstance);
                    if (instance)
                    {
                        handler->OnActorInstanceCreated(instance);
                    }
                }
            };
            template<typename Bus>
            using ConnectionPolicy = AssetConnectionPolicy<Bus>;
            //////////////////////////////////////////////////////////////////////////

            /// Notifies listeners when the component has created an actor instance.
            /// \param actorInstance - pointer to actor instance
            virtual void OnActorInstanceCreated(EMotionFX::ActorInstance* /*actorInstance*/) {};

            /// Notifies listeners when the component is destroying an actor instance.
            /// \param actorInstance - pointer to actor instance
            virtual void OnActorInstanceDestroyed(EMotionFX::ActorInstance* /*actorInstance*/) {};
        };

        using ActorComponentNotificationBus = AZ::EBus<ActorComponentNotifications>;

        /**
        * EMotion FX Editor Actor Component Request Bus
        * Used for making requests to EMotion FX Actor Components.
        */
        class EditorActorComponentRequests
            : public AZ::ComponentBus
        {
        public:
            virtual const AZ::Data::AssetId& GetActorAssetId() = 0;
            virtual AZ::EntityId GetAttachedToEntityId() const = 0;
        };

        using EditorActorComponentRequestBus = AZ::EBus<EditorActorComponentRequests>;
    } //namespace Integration
} // namespace EMotionFX


namespace AZ
{
    AZ_TYPE_INFO_SPECIALIZE(EMotionFX::Integration::Space, "{7606E4DD-B7CB-408B-BD0D-3A95636BB017}");
}
