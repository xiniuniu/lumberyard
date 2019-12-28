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
#include <PhysX_precompiled.h>

#include <Source/EditorColliderComponent.h>
#include <Source/EditorForceRegionComponent.h>
#include <Source/ForceRegionComponent.h>
#include <Source/Utils.h>

#include <PhysX/ColliderShapeBus.h>

#include <AZcore/Serialization/EditContext.h>
#include <AZcore/Serialization/SerializeContext.h>

#include <LmbrCentral/Shape/SplineComponentBus.h>

namespace PhysX
{
    void EditorForceRegionComponent::EditorForceProxy::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<EditorForceProxy>()
                ->Version(1)
                ->Field("Type", &EditorForceProxy::m_type)
                ->Field("ForceWorldSpace", &EditorForceProxy::m_forceWorldSpace)
                ->Field("ForceLocalSpace", &EditorForceProxy::m_forceLocalSpace)
                ->Field("ForcePoint", &EditorForceProxy::m_forcePoint)
                ->Field("ForceSplineFollow", &EditorForceProxy::m_forceSplineFollow)
                ->Field("ForceSimpleDrag", &EditorForceProxy::m_forceSimpleDrag)
                ->Field("ForceLinearDamping", &EditorForceProxy::m_forceLinearDamping)
                ;

            if (auto editContext = serializeContext->GetEditContext())
            {
                editContext->Class<EditorForceProxy>(
                    "Forces", "forces")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &EditorForceProxy::m_type, "Force Type", "")
                    ->EnumAttribute(ForceType::WorldSpace, "World Space")
                    ->EnumAttribute(ForceType::LocalSpace, "Local Space")
                    ->EnumAttribute(ForceType::Point, "Point")
                    ->EnumAttribute(ForceType::SplineFollow, "Spline Follow")
                    ->EnumAttribute(ForceType::SimpleDrag, "Simple Drag")
                    ->EnumAttribute(ForceType::LinearDamping, "Linear Damping")
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ::Edit::PropertyRefreshLevels::EntireTree)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &EditorForceProxy::m_forceWorldSpace, "World Space Force", "")
                    ->Attribute(AZ::Edit::Attributes::Visibility, &EditorForceProxy::IsWorldSpaceForce)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &EditorForceProxy::m_forceLocalSpace, "Local Space Force", "")
                    ->Attribute(AZ::Edit::Attributes::Visibility, &EditorForceProxy::IsLocalSpaceForce)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &EditorForceProxy::m_forcePoint, "Point Force", "")
                    ->Attribute(AZ::Edit::Attributes::Visibility, &EditorForceProxy::IsPointForce)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &EditorForceProxy::m_forceSplineFollow, "Spline Follow Force", "")
                    ->Attribute(AZ::Edit::Attributes::Visibility, &EditorForceProxy::IsSplineFollowForce)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &EditorForceProxy::m_forceSimpleDrag, "Simple Drag Force", "")
                    ->Attribute(AZ::Edit::Attributes::Visibility, &EditorForceProxy::IsSimpleDragForce)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &EditorForceProxy::m_forceLinearDamping, "Linear Damping Force", "")
                    ->Attribute(AZ::Edit::Attributes::Visibility, &EditorForceProxy::IsLinearDampingForce)
                    ;
            }
        }
    }

    AZ::Vector3 EditorForceRegionComponent::EditorForceProxy::CalculateForce(const EntityParams& entity, const RegionParams& region) const
    {
        switch (m_type)
        {
        case ForceType::WorldSpace:
            return m_forceWorldSpace.CalculateForce(entity, region);
            break;
        case ForceType::LocalSpace:
            return m_forceLocalSpace.CalculateForce(entity, region);
            break;
        case ForceType::Point:
            return m_forcePoint.CalculateForce(entity, region);
            break;
        case ForceType::SplineFollow:
            return m_forceSplineFollow.CalculateForce(entity, region);
            break;
        case ForceType::SimpleDrag:
            return m_forceSimpleDrag.CalculateForce(entity, region);
            break;
        case ForceType::LinearDamping:
            return m_forceLinearDamping.CalculateForce(entity, region);
            break;
        default:
            return AZ::Vector3::CreateZero();
        }
    }

    bool EditorForceRegionComponent::EditorForceProxy::IsWorldSpaceForce() const
    {
        return m_type == ForceType::WorldSpace;
    }

    bool EditorForceRegionComponent::EditorForceProxy::IsLocalSpaceForce() const
    {
        return m_type == ForceType::LocalSpace;
    }

    bool EditorForceRegionComponent::EditorForceProxy::IsPointForce() const
    {
        return m_type == ForceType::Point;
    }

    bool EditorForceRegionComponent::EditorForceProxy::IsSplineFollowForce() const
    {
        return m_type == ForceType::SplineFollow;
    }

    bool EditorForceRegionComponent::EditorForceProxy::IsSimpleDragForce() const
    {
        return m_type == ForceType::SimpleDrag;
    }

    bool EditorForceRegionComponent::EditorForceProxy::IsLinearDampingForce() const
    {
        return m_type == ForceType::LinearDamping;
    }

    void EditorForceRegionComponent::Reflect(AZ::ReflectContext* context)
    {
        EditorForceRegionComponent::EditorForceProxy::Reflect(context);

        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<EditorForceRegionComponent, EditorComponentBase>()
                ->Field("Visible", &EditorForceRegionComponent::m_visibleInEditor)
                ->Field("DebugForces", &EditorForceRegionComponent::m_debugForces)
                ->Field("Forces", &EditorForceRegionComponent::m_forces)
                ->Field("ForceRegion", &EditorForceRegionComponent::m_forceRegion)
                ->Version(1)
                ;

            if (auto editContext = serializeContext->GetEditContext())
            {
                // EditorForceRegionComponent
                editContext->Class<EditorForceRegionComponent>(
                    "PhysX Force Region", "The force region component is used to apply a physical force on objects within the region")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "PhysX")
                    ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/ForceRegion.png")
                    ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/ForceRegion.png")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game", 0x232b318c))
                    ->Attribute(AZ::Edit::Attributes::HelpPageURL, "https://docs.aws.amazon.com/console/lumberyard/physx/force-region")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->Attribute(AZ::Edit::Attributes::RequiredService, AZ_CRC("PhysXTriggerService", 0x3a117d7b))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &EditorForceRegionComponent::m_visibleInEditor, "Visible", "Always show the component in viewport")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &EditorForceRegionComponent::m_debugForces, "Debug Forces", "Draws debug arrows when an entity enters a force region. This occurs in gameplay mode to show the force direction on an entity.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &EditorForceRegionComponent::m_forces, "Forces", "Forces in force region")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorForceRegionComponent::OnForcesChanged)
                    ;
            }
        }
    }

    void EditorForceRegionComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        m_forceRegion.ClearForces();

        using ForceType = EditorForceRegionComponent::EditorForceProxy::ForceType;
        // Copy edit component's forces to game-time component.
        for (auto& forceProxy : m_forces)
        {
            switch (forceProxy.m_type)
            {
            case ForceType::WorldSpace:
                m_forceRegion.AddAndActivateForce(AZStd::make_unique<ForceWorldSpace>(forceProxy.m_forceWorldSpace));
                break;
            case ForceType::LocalSpace:
                m_forceRegion.AddAndActivateForce(AZStd::make_unique<ForceLocalSpace>(forceProxy.m_forceLocalSpace));
                break;
            case ForceType::Point:
                m_forceRegion.AddAndActivateForce(AZStd::make_unique<ForcePoint>(forceProxy.m_forcePoint));
                break;
            case ForceType::SplineFollow:
                m_forceRegion.AddAndActivateForce(AZStd::make_unique<ForceSplineFollow>(forceProxy.m_forceSplineFollow));
                break;
            case ForceType::SimpleDrag:
                m_forceRegion.AddAndActivateForce(AZStd::make_unique<ForceSimpleDrag>(forceProxy.m_forceSimpleDrag));
                break;
            case ForceType::LinearDamping:
                m_forceRegion.AddAndActivateForce(AZStd::make_unique<ForceLinearDamping>(forceProxy.m_forceLinearDamping));
                break;
            }
        }

        gameEntity->CreateComponent<ForceRegionComponent>(m_forceRegion, m_debugForces);
    }

    void EditorForceRegionComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("ForceRegionService", 0x3c3e4061));
    }

    void EditorForceRegionComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("ForceRegionService", 0x3c3e4061));
    }

    void EditorForceRegionComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC("TransformService", 0x8ee22c50));
        required.push_back(AZ_CRC("PhysXTriggerService", 0x3a117d7b));
    }

    void EditorForceRegionComponent::Activate()
    {
        AZ::EntityId entityId = GetEntityId();
        EditorComponentBase::Activate();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(entityId);
        m_forceRegion.Activate(entityId);

        AZ_Warning("PhysX Force Region"
            , PhysX::Utils::TriggerColliderExists(entityId)
            , "Please ensure collider component marked as trigger exists in entity <%s: %s> with force region."
            , GetEntity()->GetName().c_str()
            , entityId.ToString().c_str());
    }

    void EditorForceRegionComponent::Deactivate()
    {
        m_forceRegion.Deactivate();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        EditorComponentBase::Deactivate();
    }

    void EditorForceRegionComponent::DisplayEntityViewport(const AzFramework::ViewportInfo& viewportInfo
        , AzFramework::DebugDisplayRequests& debugDisplayRequests)
    {
        if (!IsSelected() && !m_visibleInEditor)
        {
            return;
        }

        AZ::Entity* forceRegionEntity = GetEntity();
        if (!forceRegionEntity)
        {
            return;
        }

        // Update AABB cache of collider components if they're outdated or dirty.
        AZ::Aabb aabb;
        ColliderShapeRequestBus::EventResult(aabb
            , GetEntityId()
            , &ColliderShapeRequestBus::Events::GetColliderShapeAabb);

        const AZ::Entity::ComponentArrayType& enabledComponents = forceRegionEntity->GetComponents();
        for (AZ::Component* component : enabledComponents)
        {
            EditorColliderComponent* editorColliderComponent = azrtti_cast<EditorColliderComponent*>(component);
            if (!editorColliderComponent)
            {
                continue;
            }
            const PhysX::EditorProxyShapeConfig& shapeConfig = editorColliderComponent->GetShapeConfiguration();
            AZStd::vector<AZ::Vector3> randomPoints;
            if (shapeConfig.IsBoxConfig())
            {
                AZ::Vector3 dimensions = shapeConfig.m_box.m_dimensions;
                randomPoints = PhysX::ForceRegionUtil::GenerateBoxPoints(dimensions * -0.5f
                    , dimensions * 0.5f);
            }
            else if (shapeConfig.IsCapsuleConfig())
            {
                float height = shapeConfig.m_capsule.m_height;
                float radius = shapeConfig.m_capsule.m_radius;
                randomPoints = ForceRegionUtil::GenerateCylinderPoints(height - radius * 2.0f
                    , radius);
            }
            else if (shapeConfig.IsSphereConfig())
            {
                float radius = shapeConfig.m_sphere.m_radius;
                randomPoints = ForceRegionUtil::GenerateSpherePoints(radius);
            }
            else if (shapeConfig.IsAssetConfig())
            {
                PhysX::MeshColliderComponentRequestsBus::Handler* meshColliderHandler = (PhysX::MeshColliderComponentRequestsBus::Handler*)editorColliderComponent;
                randomPoints = ForceRegionUtil::GenerateMeshPoints(meshColliderHandler->GetMeshAsset()
                    , shapeConfig.m_physicsAsset.m_configuration.m_assetScale);
            }

            PhysX::Utils::ColliderPointsLocalToWorld(randomPoints
                , GetWorldTM()
                , editorColliderComponent->GetColliderConfiguration().m_position
                , editorColliderComponent->GetColliderConfiguration().m_rotation);

            DrawForceArrows(randomPoints, debugDisplayRequests);
        }
    }

    void EditorForceRegionComponent::DrawForceArrows(const AZStd::vector<AZ::Vector3>& arrowPositions
        , AzFramework::DebugDisplayRequests& debugDisplayRequests)
    {
        const AZ::Color color = AZ::Color(0.f, 0.f, 1.f, 1.f);
        debugDisplayRequests.SetColor(color);

        EntityParams entityParams;
        entityParams.m_id.SetInvalid();
        entityParams.m_velocity = AZ::Vector3::CreateZero();
        entityParams.m_mass = 1.f;

        const PhysX::RegionParams regionParams = m_forceRegion.GetRegionParams();

        for (const auto& arrowPosition : arrowPositions)
        {
            entityParams.m_position = arrowPosition;

            auto totalForce = AZ::Vector3::CreateZero();
            for (const auto& force : m_forces)
            {
                totalForce += force.CalculateForce(entityParams, regionParams);
            }

            if (!totalForce.IsZero() && totalForce.IsFinite())
            {
                totalForce.Normalize();
                totalForce *= 0.5f;
                const float arrowHeadScale = 1.5f;
                debugDisplayRequests.DrawArrow(arrowPosition - totalForce
                    , arrowPosition + totalForce
                    , arrowHeadScale);
            }
            else
            {
                const float ballRadius = 0.05f;
                debugDisplayRequests.DrawBall(arrowPosition, ballRadius);
            }
        }
    }

    bool EditorForceRegionComponent::HasSplineFollowForce() const
    {
        for (const auto& force : m_forces)
        {
            if (force.m_type == EditorForceProxy::ForceType::SplineFollow)
            {
                return true;
            }
        }
        return false;
    }

    void EditorForceRegionComponent::OnForcesChanged() const
    {
        if (HasSplineFollowForce())
        {
            AZ::ConstSplinePtr splinePtr = nullptr;
            LmbrCentral::SplineComponentRequestBus::EventResult(splinePtr, GetEntityId()
                , &LmbrCentral::SplineComponentRequestBus::Events::GetSpline);
            AZ::Entity* entity = GetEntity();
            AZ_Warning("PhysX EditorForceRegionComponent"
                , splinePtr!=nullptr
                , "Please add spline shape for force region in entity <%s: %s>."
                , entity->GetName().c_str()
                , entity->GetId().ToString().c_str());
        }
    }
}