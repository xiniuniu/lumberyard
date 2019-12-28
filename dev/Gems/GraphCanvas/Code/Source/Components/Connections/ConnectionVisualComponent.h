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

#include <QGraphicsPathItem>
#include <QPainterPath>

#include <AzCore/Component/Component.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/chrono/chrono.h>

#include <GraphCanvas/Components/Connections/ConnectionBus.h>
#include <GraphCanvas/Components/ViewBus.h>
#include <GraphCanvas/Components/VisualBus.h>
#include <GraphCanvas/Editor/AssetEditorBus.h>
#include <GraphCanvas/Widgets/RootGraphicsItem.h>
#include <GraphCanvas/Styling/StyleHelper.h>

class QKeyEvent;

namespace GraphCanvas
{
    class ConnectionGraphicsItem;

    //! The NodeVisual is the QGraphicsItem for a given node, any components that are created
    //! on a Node will all be children QGraphicsItem of this one.
    class ConnectionVisualComponent
        : public AZ::Component
        , public VisualRequestBus::Handler
        , public SceneMemberUIRequestBus::Handler
    {
    public:
        AZ_COMPONENT(ConnectionVisualComponent, "{BF9691F8-7EF8-4A94-9321-2EB877634D22}");
        static void Reflect(AZ::ReflectContext* context);

        ConnectionVisualComponent();
        ~ConnectionVisualComponent() override = default;

        // AZ::Component
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("GraphCanvas_ConnectionVisualService", 0x3877d465));
            provided.push_back(AZ_CRC("GraphCanvas_RootVisualService", 0x9ec46d3b));
            provided.push_back(AZ_CRC("GraphCanvas_VisualService", 0xfbb2c871));
        }

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC("GraphCanvas_ConnectionVisualService", 0x3877d465));
            incompatible.push_back(AZ_CRC("GraphCanvas_RootVisualService", 0x9ec46d3b));
            incompatible.push_back(AZ_CRC("GraphCanvas_VisualService", 0xfbb2c871));
        }

        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
        {
            (void)dependent;
        }

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("GraphCanvas_ConnectionService", 0x7ef98865));
        }

        void Init() override;
        void Activate() override;
        void Deactivate() override;
        ////
        
        // VisualRequestBus
        QGraphicsItem* AsGraphicsItem() override;

        bool Contains(const AZ::Vector2& point) const override;

        void SetVisible(bool visible) override;
        bool IsVisible() const override;
        ////

        // SceneMemberUIRequestBus
        QGraphicsItem* GetRootGraphicsItem() override;
        QGraphicsLayoutItem* GetRootGraphicsLayoutItem() override;

        void SetSelected(bool selected) override;
        bool IsSelected() const override;

        QPainterPath GetOutline() const override;

        void SetZValue(int zValue) override;
        int GetZValue() const override;
        ////

    protected:

        virtual void CreateConnectionVisual();
        
        friend class ConnectionVisualGraphicsItem;
        AZStd::unique_ptr<ConnectionGraphicsItem> m_connectionGraphicsItem;

    private:
        ConnectionVisualComponent(const ConnectionVisualComponent&) = delete;        
    };

    //! The NodeVisual is the QGraphicsItem for a given node, any components that are created
    //! on a Node will all be children QGraphicsItem of this one.
    class ConnectionGraphicsItem
        : public RootGraphicsItem<QGraphicsPathItem>
        , public ConnectionNotificationBus::Handler
        , public ConnectionUIRequestBus::Handler
        , public VisualNotificationBus::MultiHandler
        , public StyleNotificationBus::Handler
        , public AZ::SystemTickBus::Handler
        , public SceneMemberNotificationBus::Handler
        , public AssetEditorSettingsNotificationBus::Handler        
    {
    public:
        AZ_CLASS_ALLOCATOR(ConnectionGraphicsItem, AZ::SystemAllocator, 0);
        
        // Helper function to return the length of a vector
        // (Distance from provided point to the origin)
        static qreal VectorLength(QPointF vectorPoint);

        ConnectionGraphicsItem(const AZ::EntityId& connectionEntityId);
        ~ConnectionGraphicsItem() override = default;

        void Activate();
        void Deactivate();

        void RefreshStyle();
        const Styling::StyleHelper& GetStyle() const;

        void UpdateOffset();

        // RootVisualNotificationsHelper
        QRectF GetBoundingRect() const override;
        ////

        // ConnectionNotificationBus
        void OnSourceSlotIdChanged(const AZ::EntityId& oldSlotId, const AZ::EntityId& newSlotId) override;
        void OnTargetSlotIdChanged(const AZ::EntityId& oldSlotId, const AZ::EntityId& newSlotId) override;

        void OnTooltipChanged(const AZStd::string& tooltip) override;
        ////

        // StyleNotificationBus
        void OnStyleChanged() override;
        ////
        
        // AZ::SystemTickBus
        void OnSystemTick() override;
        ////

        // VisualNotificationBus
        void OnItemChange(const AZ::EntityId& entityId, QGraphicsItem::GraphicsItemChange change, const QVariant& value) override;
        ////

        // ConnectionUIRequestBus
        void UpdateConnectionPath() override;
        ////

        // SceneMemberNotifications
        void OnSceneMemberHidden() override;
        void OnSceneMemberShown() override;

        void OnSceneSet(const GraphId& graphId) override;
        ////

        // AssetEditorSettingsNotifications
        void OnSettingsChanged() override;
        ////

    protected:

        const AZ::EntityId& GetConnectionEntityId() const;

        AZ::EntityId GetSourceSlotEntityId() const;
        AZ::EntityId GetTargetSlotEntityId() const;

        EditorId GetEditorId() const;

        void UpdateCurveStyle();

        virtual Styling::ConnectionCurveType GetCurveStyle() const;

        virtual void UpdatePen();
        virtual void OnActivate();
        virtual void OnDeactivate();
        virtual void OnPathChanged();
        
        // QGraphicsItem
        QPainterPath shape() const override;

        void mousePressEvent(QGraphicsSceneMouseEvent* mouseEvent) override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent* mouseEvent) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent* mouseEvent) override;
        ////

        void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

    private:

        ConnectionGraphicsItem(const ConnectionGraphicsItem&) = delete;

        bool m_trackMove;
        bool m_moveSource;
        
        QPointF m_initialPoint;

        Styling::ConnectionCurveType m_curveType;
        Styling::StyleHelper m_style;
        QPen m_pen;

        AZStd::chrono::milliseconds m_lastUpdate;
        double m_offset;

        AZ::EntityId m_connectionEntityId;        
        EditorId     m_editorId;
    };
}