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
#include "precompiled.h"

#include <QPainter>
#include <QGraphicsLayout>
#include <QGraphicsSceneEvent>

#include <Components/Nodes/NodeFrameGraphicsWidget.h>

#include <GraphCanvas/Components/GridBus.h>
#include <GraphCanvas/tools.h>
#include <GraphCanvas/Styling/StyleHelper.h>

namespace GraphCanvas
{
    ////////////////////////////
    // NodeFrameGraphicsWidget
    ////////////////////////////

    NodeFrameGraphicsWidget::NodeFrameGraphicsWidget(const AZ::EntityId& entityKey)
        : RootGraphicsItem(entityKey)
        , m_displayState(NodeFrameDisplayState::None)
    {
        setFlags(ItemIsSelectable | ItemIsFocusable | ItemIsMovable | ItemSendsScenePositionChanges);
        setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
        setData(GraphicsItemName, QStringLiteral("DefaultNodeVisual/%1").arg(static_cast<AZ::u64>(GetEntityId()), 16, 16, QChar('0')));

        setCacheMode(QGraphicsItem::CacheMode::DeviceCoordinateCache);
    }

    void NodeFrameGraphicsWidget::Activate()
    {
        SceneMemberUIRequestBus::Handler::BusConnect(GetEntityId());
        GeometryNotificationBus::Handler::BusConnect(GetEntityId());
        StyleNotificationBus::Handler::BusConnect(GetEntityId());
        NodeNotificationBus::Handler::BusConnect(GetEntityId());
        NodeUIRequestBus::Handler::BusConnect(GetEntityId());
        VisualRequestBus::Handler::BusConnect(GetEntityId());

        OnActivated();
    }

    void NodeFrameGraphicsWidget::Deactivate()
    {
        StyleNotificationBus::Handler::BusDisconnect();
        NodeNotificationBus::Handler::BusDisconnect();
        NodeUIRequestBus::Handler::BusDisconnect();
        VisualRequestBus::Handler::BusDisconnect();
        GeometryNotificationBus::Handler::BusDisconnect();
        SceneMemberUIRequestBus::Handler::BusDisconnect();
    }

    QRectF NodeFrameGraphicsWidget::GetBoundingRect() const
    {
        return boundingRect();
    }

    QGraphicsItem* NodeFrameGraphicsWidget::GetRootGraphicsItem()
    {
        return this;
    }

    QGraphicsLayoutItem* NodeFrameGraphicsWidget::GetRootGraphicsLayoutItem()
    {
        return this;
    }

    void NodeFrameGraphicsWidget::SetSelected(bool selected)
    {
        setSelected(selected);
    }

    bool NodeFrameGraphicsWidget::IsSelected() const
    {
        return isSelected();
    }

    void NodeFrameGraphicsWidget::SetZValue(int zValue)
    {
        setZValue(zValue);
    }

    int NodeFrameGraphicsWidget::GetZValue() const
    {
        return zValue();
    }

    void NodeFrameGraphicsWidget::OnPositionChanged(const AZ::EntityId& entityId, const AZ::Vector2& position)
    {
        setPos(QPointF(position.GetX(), position.GetY()));
    }

    void NodeFrameGraphicsWidget::OnStyleChanged()
    {
        m_style.SetStyle(GetEntityId());

        setOpacity(m_style.GetAttribute(Styling::Attribute::Opacity, 1.0f));

        OnRefreshStyle();
        update();
    }

    QSizeF NodeFrameGraphicsWidget::sizeHint(Qt::SizeHint which, const QSizeF& constraint) const
    {
        QSizeF retVal = QGraphicsWidget::sizeHint(which, constraint);

        if (IsResizedToGrid())
        {
            int width = static_cast<int>(retVal.width());
            int height = static_cast<int>(retVal.height());

            width = GrowToNextStep(width, GetGridXStep());
            height = GrowToNextStep(height, GetGridYStep());

            retVal = QSizeF(width, height);
        }

        return retVal;
    }

    void NodeFrameGraphicsWidget::resizeEvent(QGraphicsSceneResizeEvent* resizeEvent)
    {
        QGraphicsWidget::resizeEvent(resizeEvent);

        GeometryRequestBus::Event(GetEntityId(), &GeometryRequests::SignalBoundsChanged);
    }
    
    void NodeFrameGraphicsWidget::OnDeleteItem()
    {
        AZ::EntityId graphId;
        SceneMemberRequestBus::EventResult(graphId, GetEntityId(), &SceneMemberRequests::GetScene);

        SceneRequestBus::Event(graphId, &SceneRequests::DeleteNodeAndStitchConnections, GetEntityId());
    }

    QGraphicsItem* NodeFrameGraphicsWidget::AsGraphicsItem()
    {
        return this;
    }

    bool NodeFrameGraphicsWidget::Contains(const AZ::Vector2& position) const
    {
        auto local = mapFromScene(QPointF(position.GetX(), position.GetY()));
        return boundingRect().contains(local);
    }

    void NodeFrameGraphicsWidget::SetVisible(bool visible)
    {
        setVisible(visible);
    }

    bool NodeFrameGraphicsWidget::IsVisible() const
    {
        return isVisible();
    }

    void NodeFrameGraphicsWidget::OnNodeActivated()
    {
    }

    void NodeFrameGraphicsWidget::OnAddedToScene(const AZ::EntityId&)
    {
        AZStd::string tooltip;
        NodeRequestBus::EventResult(tooltip, GetEntityId(), &NodeRequests::GetTooltip);
        setToolTip(Tools::qStringFromUtf8(tooltip));
        //TODO setEnabled(node->IsEnabled());

        AZ::Vector2 position;
        GeometryRequestBus::EventResult(position, GetEntityId(), &GeometryRequests::GetPosition);
        setPos(QPointF(position.GetX(), position.GetY()));
    }

    void NodeFrameGraphicsWidget::OnNodeWrapped(const AZ::EntityId& wrappingNode)
    {
        GeometryNotificationBus::Handler::BusDisconnect();
        setFlag(QGraphicsItem::ItemIsMovable, false);

        SetSnapToGridEnabled(false);
        SetResizeToGridEnabled(false);
    }

    void NodeFrameGraphicsWidget::AdjustSize()
    {
        adjustSize();
    }

    void NodeFrameGraphicsWidget::SetSnapToGrid(bool snapToGrid)
    {
        SetSnapToGridEnabled(snapToGrid);
    }

    void NodeFrameGraphicsWidget::SetResizeToGrid(bool resizeToGrid)
    {
        SetResizeToGridEnabled(resizeToGrid);
    }

    void NodeFrameGraphicsWidget::SetGrid(AZ::EntityId gridId)
    {
        AZ::Vector2 gridSize;
        GridRequestBus::EventResult(gridSize, gridId, &GridRequests::GetMinorPitch);

        SetGridSize(gridSize);
    }

    qreal NodeFrameGraphicsWidget::GetCornerRadius() const
    {
        return m_style.GetAttribute(Styling::Attribute::BorderRadius, 5.0);
    }

    int NodeFrameGraphicsWidget::GrowToNextStep(int value, int step) const
    {
        int delta = value % step;        

        if (delta == 0)
        {
            return value;
        }
        else
        {
            return value + (step - delta);
        }
    }

    int NodeFrameGraphicsWidget::RoundToClosestStep(int value, int step) const
    {
        if (step == 1)
        {
            return value;
        }

        int halfStep = step / 2;

        value += halfStep;
        return ShrinkToPreviousStep(value, step);
    }

    int NodeFrameGraphicsWidget::ShrinkToPreviousStep(int value, int step) const
    {
        int absValue = (value%step);

        if (absValue < 0)
        {
            absValue = step + absValue;
        }

        return value - absValue;
    }

    void NodeFrameGraphicsWidget::OnActivated()
    {
    }

    void NodeFrameGraphicsWidget::OnDeactivated()
    {
    }

    void NodeFrameGraphicsWidget::OnRefreshStyle()
    {
    }
}