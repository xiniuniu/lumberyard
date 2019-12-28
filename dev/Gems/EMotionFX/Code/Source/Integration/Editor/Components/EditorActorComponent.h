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
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Asset/AssetCommon.h>

#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>
#include <AzToolsFramework/API/ComponentEntitySelectionBus.h>
#include <AzToolsFramework/ToolsComponents/EditorVisibilityBus.h>

#include <Integration/Components/ActorComponent.h>

#include <LmbrCentral/Rendering/RenderBoundsBus.h>

#include <LmbrCentral/Rendering/MaterialOwnerBus.h>

namespace AZ
{
    class ScriptProperty;
}

namespace EMotionFX
{
    namespace Integration
    {
        class ActorRenderNode;

        class EditorActorComponent
            : public AzToolsFramework::Components::EditorComponentBase
            , private AZ::Data::AssetBus::Handler
            , private AZ::TransformNotificationBus::Handler
            , private AZ::TickBus::Handler
            , private LmbrCentral::MeshComponentRequestBus::Handler
            , private LmbrCentral::RenderNodeRequestBus::Handler
            , private LmbrCentral::RenderBoundsRequestBus::Handler
            , private LmbrCentral::MaterialOwnerRequestBus::Handler
            , private ActorComponentRequestBus::Handler
            , private EditorActorComponentRequestBus::Handler
            , private LmbrCentral::AttachmentComponentNotificationBus::Handler
            , private AzToolsFramework::EditorComponentSelectionRequestsBus::Handler
            , private AzToolsFramework::EditorVisibilityNotificationBus::Handler
        {
        public:

            AZ_EDITOR_COMPONENT(EditorActorComponent, "{A863EE1B-8CFD-4EDD-BA0D-1CEC2879AD44}");

            EditorActorComponent();
            ~EditorActorComponent() override;

            // AZ::Component interface implementation
            void Init() override;
            void Activate() override;
            void Deactivate() override;

            // ActorComponentRequestBus::Handler
            EMotionFX::ActorInstance* GetActorInstance() override { return m_actorInstance ? m_actorInstance.get() : nullptr; }
            bool GetRenderCharacter() const override;
            void SetRenderCharacter(bool enable) override;

            // EditorActorComponentRequestBus::Handler
            const AZ::Data::AssetId& GetActorAssetId() override;
            AZ::EntityId GetAttachedToEntityId() const override;

            // EditorVisibilityNotificationBus::Handler
            void OnEntityVisibilityChanged(bool visibility) override;

            //////////////////////////////////////////////////////////////////////////
            // RenderBoundsRequestBus interface implementation
            AZ::Aabb GetWorldBounds() override;
            AZ::Aabb GetLocalBounds() override;

            //////////////////////////////////////////////////////////////////////////
            // LmbrCentral::MeshComponentRequestBus::Handler
            bool GetVisibility() override;
            void SetVisibility(bool isVisible) override;
            void SetMeshAsset(const AZ::Data::AssetId& id) override { (void)id; }
            AZ::Data::Asset<AZ::Data::AssetData> GetMeshAsset() override { return m_actorAsset; }

            // EditorComponentSelectionRequestsBus::Handler
            AZ::Aabb GetEditorSelectionBoundsViewport(
                const AzFramework::ViewportInfo& viewportInfo) override;
            bool EditorSelectionIntersectRayViewport(
                const AzFramework::ViewportInfo& viewportInfo,
                const AZ::Vector3& src, const AZ::Vector3& dir, AZ::VectorFloat& distance) override;
            bool SupportsEditorRayIntersect() override { return true; }

            // RenderNodeRequestBus::Handler
            IRenderNode* GetRenderNode() override;
            float GetRenderNodeRequestBusOrder() const override;
            static const float s_renderNodeRequestBusOrder;

            static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
            {
                ActorComponent::GetProvidedServices(provided);
            }

            static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
            {
                ActorComponent::GetIncompatibleServices(incompatible);
            }

            static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
            {
                ActorComponent::GetDependentServices(dependent);
            }

            static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
            {
                ActorComponent::GetRequiredServices(required);
            }

            static void Reflect(AZ::ReflectContext* context);

        private:
            //vs 2013 build limitation
            //unique_ptr cannot be copied ->class cannot be copied
            EditorActorComponent(const EditorActorComponent&) = delete;

            // Property callbacks.
            AZ::Crc32 OnAssetSelected();
            void OnMaterialChanged();
            void OnMaterialPerActorChanged();
            void OnLODLevelChanged();
            void OnDebugDrawFlagChanged();
            void OnSkinningMethodChanged();
            AZ::Crc32 OnAttachmentTypeChanged();
            AZ::Crc32 OnAttachmentTargetChanged();
            AZ::Crc32 OnAttachmentTargetJointSelect();
            bool AttachmentTargetVisibility();
            bool AttachmentTargetJointVisibility();
            AZStd::string AttachmentJointButtonText();
            void InitializeMaterial(ActorAsset& actorAsset);

            void LaunchAnimationEditor(const AZ::Data::AssetId& assetId, const AZ::Data::AssetType&);

            // AZ::Data::AssetBus::Handler
            void OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset) override;
            void OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset) override;
            void OnAssetError(AZ::Data::Asset<AZ::Data::AssetData> asset) override;

            // AZ::TransformNotificationBus::Handler
            void OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world) override;

            // Called at edit-time when creating the component directly from an asset.
            void SetPrimaryAsset(const AZ::Data::AssetId& assetId) override;

            // AZ::TickBus::Handler
            void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

            // LmbrCentral::AttachmentComponentNotificationBus::Handler
            void OnAttached(AZ::EntityId targetId) override;
            void OnDetached(AZ::EntityId targetId) override;

            // AZ::LmbrCentral::MaterialOwnerRequestBus::Handler
            void SetMaterial(_smart_ptr<IMaterial>) override;
            _smart_ptr<IMaterial> GetMaterial() override;

            void BuildGameEntity(AZ::Entity* gameEntity) override;

            void CreateActorInstance();
            void DestroyActorInstance();

            bool IsValidAttachment(const AZ::EntityId& attachment, const AZ::EntityId& attachTo) const;
 
            AZ::Data::Asset<ActorAsset>         m_actorAsset;               ///< Assigned actor asset.
            AZStd::vector<AZ::EntityId>         m_attachments;              ///< A list of entities that are attached to this entity.
            bool                                m_renderSkeleton;           ///< Toggles rendering of character skeleton.
            bool                                m_renderCharacter;          ///< Toggles rendering of character model.
            bool                                m_renderBounds;             ///< Toggles rendering of the world bounding box.
            bool                                m_entityVisible;            ///< Entity visible from the EditorVisibilityNotificationBus
            SkinningMethod                      m_skinningMethod;           ///< The skinning method for this actor
            AttachmentType                      m_attachmentType;           ///< Attachment type.
            AZ::EntityId                        m_attachmentTarget;         ///< Target entity to attach to, if any.
            AZStd::string                       m_attachmentJointName;      ///< Joint name on target to which to attach (if ActorAttachment).
            AZ::u32                             m_attachmentJointIndex;
            AZ::u32                             m_lodLevel;
            // \todo attachmentTarget node nr

            // Note: LOD work in progress. For now we use one material instead of a list of material, because we don't have the support for LOD with multiple FBXs.
            // We purposely kept a materialList in actorComponent and actorRenderNode for the flexibility in future.
            // At the moment, the materialList stores duplicates of the same material.
            AzFramework::SimpleAssetReference<LmbrCentral::MaterialAsset>   m_materialPerActor;
            ActorAsset::MaterialList            m_materialPerLOD;           ///< Material assignment for each LOD level.

            ActorAsset::ActorInstancePtr        m_actorInstance;            ///< Live actor instance.
            AZStd::unique_ptr<ActorRenderNode>  m_renderNode;               ///< Actor render node.
        };
    } // namespace Integration
} // namespace EMotionFX

