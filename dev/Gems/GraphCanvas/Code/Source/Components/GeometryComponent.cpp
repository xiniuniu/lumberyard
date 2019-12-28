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
#include <precompiled.h>

#include <QGraphicsSceneMouseEvent>

#include <AzCore/Serialization/EditContext.h>

#include <Components/GeometryComponent.h>

#include <Components/Nodes/NodeComponent.h>
#include <GraphCanvas/tools.h>

namespace GraphCanvas
{
    //////////////////////
    // GeometryComponent
    //////////////////////
    const float GeometryComponent::IS_CLOSE_TOLERANCE = 0.001f;

    bool GeometryComponentVersionConverter(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement)
    {
        if (classElement.GetVersion() <= 3)
        {
            AZ::Crc32 positionId = AZ_CRC("Position", 0x462ce4f5);

            GeometrySaveData saveData;

            AZ::SerializeContext::DataElementNode* dataNode = classElement.FindSubElement(positionId);

            if (dataNode)
            {
                dataNode->GetData(saveData.m_position);
            }
            
            classElement.RemoveElementByName(positionId);
            classElement.AddElementWithData(context, "SaveData", saveData);
        }

        return true;
    }

    void GeometryComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (!serializeContext)
        {
            return;
        }

        serializeContext->Class<GeometrySaveData>()
            ->Version(1)
            ->Field("Position", &GeometrySaveData::m_position)
        ;

        serializeContext->Class<GeometryComponent, AZ::Component>()
            ->Version(4, &GeometryComponentVersionConverter)
            ->Field("SaveData", &GeometryComponent::m_saveData)
        ;
    }

    GeometryComponent::GeometryComponent()
        : m_animating(false)
    {
    }

    GeometryComponent::~GeometryComponent()
    {
        GeometryRequestBus::Handler::BusDisconnect();
    }

    void GeometryComponent::Init()
    {
        GeometryRequestBus::Handler::BusConnect(GetEntityId());
        EntitySaveDataRequestBus::Handler::BusConnect(GetEntityId());
    }

    void GeometryComponent::Activate()
    {
        SceneMemberNotificationBus::Handler::BusConnect(GetEntityId());        
    }

    void GeometryComponent::Deactivate()
    {
        VisualNotificationBus::Handler::BusDisconnect();
        SceneMemberNotificationBus::Handler::BusDisconnect();
    }

    void GeometryComponent::OnSceneSet(const AZ::EntityId& scene)
    {
        VisualNotificationBus::Handler::BusConnect(GetEntityId());

        m_saveData.RegisterIds(GetEntityId(), scene);
    }

    AZ::Vector2 GeometryComponent::GetPosition() const
    {
        return m_saveData.m_position;
    }

    void GeometryComponent::SetPosition(const AZ::Vector2& position)
    {
        if (!position.IsClose(m_saveData.m_position)
            && (!m_animating || !m_animatingPosition.IsClose(position)))
        {
            if (!m_animating)
            {
                m_saveData.m_position = position;
            }
            else
            {
                m_animatingPosition = position;
            }

            GeometryNotificationBus::Event(GetEntityId(), &GeometryNotifications::OnPositionChanged, GetEntityId(), position);

            if (!m_animating)
            {
                m_saveData.SignalDirty();
            }
        }
    }

    void GeometryComponent::SignalBoundsChanged()
    {
        GeometryNotificationBus::Event(GetEntityId(), &GeometryNotifications::OnBoundsChanged);
    }

    void GeometryComponent::OnItemChange(const AZ::EntityId& entityId, QGraphicsItem::GraphicsItemChange change, const QVariant& value)
    {
        AZ_Assert(entityId == GetEntityId(), "EIDs should match");

        switch (change)
        {
        case QGraphicsItem::ItemPositionChange:
        {
            QPointF qt = value.toPointF();
            SetPosition(::AZ::Vector2(qt.x(), qt.y()));
            break;
        }
        default:
            break;
        }
    }

    void GeometryComponent::OnPositionAnimateBegin(const AZ::Vector2& targetPoint)
    {
        // Store the animating position separate from the savedata position
        // so any attempts to save will cause appropriate data to be saved
        // while visually I can lerp cleanly between the values.
        m_animatingPosition = m_saveData.m_position;

        // Set a flag so we know not to signal out tha the save data needs to be dirtied while the element
        // is moving
        m_animating = true;
        m_saveData.m_position = targetPoint;        
        m_saveData.SignalDirty();
    }

    void GeometryComponent::OnPositionAnimateEnd()
    {
        m_animating = false;

        // Force the alignment to wherever we were aiming at.
        AZ::Vector2 position = m_saveData.m_position;

        if (m_saveData.m_position.IsZero())
        {
            m_saveData.m_position = AZ::Vector2(1,1);
        }
        else
        {
            m_saveData.m_position = AZ::Vector2::CreateZero();
        }

        SetPosition(position);
    }

    void GeometryComponent::WriteSaveData(EntitySaveDataContainer& saveDataContainer) const
    {
        GeometrySaveData* saveData = saveDataContainer.FindCreateSaveData<GeometrySaveData>();

        if (saveData)
        {
            (*saveData) = m_saveData;
        }
    }

    void GeometryComponent::ReadSaveData(const EntitySaveDataContainer& saveDataContainer)
    {
        GeometrySaveData* saveData = saveDataContainer.FindSaveDataAs<GeometrySaveData>();

        if (saveData)
        {
            m_saveData = (*saveData);
        }
    }
}