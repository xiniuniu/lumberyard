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
#include "LmbrCentral_precompiled.h"
#include "DecalComponent.h"
#include "MaterialOwnerRequestBusHandlerImpl.h"
#include <AzCore/Serialization/SerializeContext.h>
#include <MathConversion.h>
#include <I3DEngine.h>
#include <AzCore/RTTI/BehaviorContext.h>

namespace LmbrCentral
{
    void DecalConfiguration::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<DecalConfiguration>()->
                Version(2, &VersionConverter)->
                Field("Visible", &DecalConfiguration::m_visible)->
                Field("ProjectionType", &DecalConfiguration::m_projectionType)->
                Field("Material", &DecalConfiguration::m_material)->
                Field("SortPriority", &DecalConfiguration::m_sortPriority)->
                Field("Depth", &DecalConfiguration::m_depth)->
                Field("Offset", &DecalConfiguration::m_position)->
                Field("Opacity", &DecalConfiguration::m_opacity)->
                Field("Angle Attenuation", &DecalConfiguration::m_angleAttenuation)->
                Field("Deferred", &DecalConfiguration::m_deferred)->
                Field("DeferredString", &DecalConfiguration::m_deferredString)->
                Field("Max View Distance", &DecalConfiguration::m_maxViewDist)->
                Field("View Distance Multiplier", &DecalConfiguration::m_viewDistanceMultiplier)->
                Field("Min Spec", &DecalConfiguration::m_minSpec)
            ;
        }
        AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext)
        {
            behaviorContext->EBus<DecalComponentRequestBus>("DecalComponentRequestBus")
                ->Event("SetVisibility", &DecalComponentRequestBus::Events::SetVisibility)
                ->Event("Show", &DecalComponentRequestBus::Events::Show)
                ->Event("Hide", &DecalComponentRequestBus::Events::Hide)
                ;
        }
    }

    // Private Static
    bool DecalConfiguration::VersionConverter(AZ::SerializeContext& context,
        AZ::SerializeContext::DataElementNode& classElement)
    {
        // conversion from version 1:
        // - Remove Normal
        if (classElement.GetVersion() <= 1)
        {
            classElement.RemoveElementByName(AZ_CRC("Normal", 0x218568e4));
        }

        return true;
    }

    //////////////////////////////////////////////////////////////////////////

    void DecalComponent::Reflect(AZ::ReflectContext* context)
    {
        DecalConfiguration::Reflect(context);

        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<DecalComponent, AZ::Component>()->
                Version(1)->
                Field("DecalConfiguration", &DecalComponent::m_configuration);
            ;
        }
    }

    //////////////////////////////////////////////////////////////////////////

    DecalComponent::DecalComponent()
        : m_decalRenderNode(nullptr)
    {
        m_materialBusHandler = aznew MaterialOwnerRequestBusHandlerImpl;
    }

    DecalComponent::~DecalComponent()
    {
        delete m_materialBusHandler;
    }


    void DecalComponent::Activate()
    {
        AZ::Transform transform = AZ::Transform::CreateIdentity();
        EBUS_EVENT_ID_RESULT(transform, GetEntityId(), AZ::TransformBus, GetWorldTM);

        SDecalProperties decalProperties = m_configuration.GetDecalProperties(transform);

        m_decalRenderNode = static_cast<IDecalRenderNode*>(gEnv->p3DEngine->CreateRenderNode(eERType_Decal));
        if (m_decalRenderNode)
        {
            m_decalRenderNode->SetRndFlags(m_decalRenderNode->GetRndFlags() | ERF_COMPONENT_ENTITY);
            m_decalRenderNode->SetDecalProperties(decalProperties);
            m_decalRenderNode->SetMinSpec(static_cast<int>(decalProperties.m_minSpec));
            m_decalRenderNode->SetMatrix(AZTransformToLYTransform(transform));
            m_decalRenderNode->SetViewDistanceMultiplier(m_configuration.m_viewDistanceMultiplier);

            const int configSpec = gEnv->pSystem->GetConfigSpec(true);
            if (!m_configuration.m_visible || static_cast<AZ::u32>(configSpec) < static_cast<AZ::u32>(m_configuration.m_minSpec))
            {
                Hide();
            }
        }

        m_materialBusHandler->Activate(m_decalRenderNode, m_entity->GetId());

        DecalComponentRequestBus::Handler::BusConnect(GetEntityId());
        RenderNodeRequestBus::Handler::BusConnect(GetEntityId());
        AZ::TransformNotificationBus::Handler::BusConnect(GetEntityId());
        MaterialOwnerRequestBus::Handler::BusConnect(GetEntityId());
    }

    void DecalComponent::Deactivate()
    {
        DecalComponentRequestBus::Handler::BusDisconnect();
        RenderNodeRequestBus::Handler::BusDisconnect();
        AZ::TransformNotificationBus::Handler::BusDisconnect();
        MaterialOwnerRequestBus::Handler::BusDisconnect();

        m_materialBusHandler->Deactivate();

        if (m_decalRenderNode)
        {
            gEnv->p3DEngine->DeleteRenderNode(m_decalRenderNode);
            m_decalRenderNode = nullptr;
        }
    }

    void DecalComponent::OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world)
    {
        SDecalProperties decalProperties = m_configuration.GetDecalProperties(world);
        if (m_decalRenderNode)
        {
            m_decalRenderNode->SetDecalProperties(decalProperties);
            m_decalRenderNode->SetMatrix(AZTransformToLYTransform(world));
        }
    }

    void DecalComponent::Show()
    {
        if (!m_decalRenderNode)
        {
            return;
        }

        m_decalRenderNode->Hide(false);
    }

    void DecalComponent::Hide()
    {
        if (!m_decalRenderNode)
        {
            return;
        }

        m_decalRenderNode->Hide(true);
    }

    void DecalComponent::SetVisibility(bool show)
    {
        if (show)
        {
            Show();
        }
        else
        {
            Hide();
        }
    }

    IRenderNode* DecalComponent::GetRenderNode()
    {
        return m_decalRenderNode;
    }

    float DecalComponent::GetRenderNodeRequestBusOrder() const
    {
        return s_renderNodeRequestBusOrder;
    }


    bool DecalComponent::IsMaterialOwnerReady()
    {
        return m_materialBusHandler->IsMaterialOwnerReady();
    }

    void DecalComponent::SetMaterial(_smart_ptr<IMaterial> material)
    {
        m_materialBusHandler->SetMaterial(material);
    }

    _smart_ptr<IMaterial> DecalComponent::GetMaterial()
    {
        return m_materialBusHandler->GetMaterial();
    }

    void DecalComponent::SetMaterialHandle(const MaterialHandle& materialHandle)
    {
        m_materialBusHandler->SetMaterialHandle(materialHandle);
    }

    MaterialHandle DecalComponent::GetMaterialHandle()
    {
        return m_materialBusHandler->GetMaterialHandle();
    }

    void DecalComponent::SetMaterialParamVector4(const AZStd::string& name, const AZ::Vector4& value, int materialId)
    {
        m_materialBusHandler->SetMaterialParamVector4(name, value, materialId);
    }

    void DecalComponent::SetMaterialParamVector3(const AZStd::string& name, const AZ::Vector3& value, int materialId)
    {
        m_materialBusHandler->SetMaterialParamVector3(name, value, materialId);
    }

    void DecalComponent::SetMaterialParamColor(const AZStd::string& name, const AZ::Color& value, int materialId)
    {
        m_materialBusHandler->SetMaterialParamColor(name, value, materialId);
    }

    void DecalComponent::SetMaterialParamFloat(const AZStd::string& name, float value, int materialId)
    {
        m_materialBusHandler->SetMaterialParamFloat(name, value, materialId);
    }

    AZ::Vector4 DecalComponent::GetMaterialParamVector4(const AZStd::string& name, int materialId)
    {
        return m_materialBusHandler->GetMaterialParamVector4(name, materialId);
    }

    AZ::Vector3 DecalComponent::GetMaterialParamVector3(const AZStd::string& name, int materialId)
    {
        return m_materialBusHandler->GetMaterialParamVector3(name, materialId);
    }

    AZ::Color DecalComponent::GetMaterialParamColor(const AZStd::string& name, int materialId)
    {
        return m_materialBusHandler->GetMaterialParamColor(name, materialId);
    }

    float DecalComponent::GetMaterialParamFloat(const AZStd::string& name, int materialId)
    {
        return m_materialBusHandler->GetMaterialParamFloat(name, materialId);
    }


    const float DecalComponent::s_renderNodeRequestBusOrder = 900.f;
} // namespace LmbrCentral
