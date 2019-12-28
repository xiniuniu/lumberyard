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

#include <qgraphicswidget.h>
#include <qgraphicssceneevent.h>
#include <QTimer>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/EntityBus.h>
#include <AzCore/std/string/string.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <GraphCanvas/Components/Connections/ConnectionBus.h>
#include <GraphCanvas/Components/GeometryBus.h>
#include <GraphCanvas/Components/SceneBus.h>
#include <GraphCanvas/Utils/StateControllers/StateController.h>

namespace GraphCanvas
{
    class ConnectionEventFilter;

    class ConnectionComponent
        : public AZ::Component
        , public ConnectionRequestBus::Handler
        , public SceneMemberRequestBus::Handler
        , public AzToolsFramework::EditorEvents::Bus::Handler
        , public AZ::TickBus::Handler
    {
    private:
        friend class ConnectionEventFilter;

    protected:

        class ConnectionEndpointAnimator
        {
        public:
            ConnectionEndpointAnimator();

            bool IsAnimating() const;
            void AnimateToEndpoint(const QPointF& startPoint, const Endpoint& endPoint, float timeFrame);

            QPointF GetAnimatedPosition() const;

            bool Tick(float deltaTime);

        private:
            bool m_isAnimating;
            float m_maxTime;
            float m_timer;
            QPointF m_currentPosition;

            QPointF m_startPosition;
            Endpoint m_targetEndpoint;
        };

        enum class DragContext
        {
            Unknown,
            TryConnection,
            MoveSource,
            MoveTarget,
            Connected
        };

        enum class ConnectionMoveResult
        {
            DeleteConnection,
            ConnectionMove,
            NodeCreation
        };

        struct ConnectionCandidate
        {
            Endpoint m_connectableTarget;
            Endpoint m_testedTarget;
        };

    public:
        AZ_COMPONENT(ConnectionComponent, "{14BB1535-3B30-4B1C-8324-D864963FBC76}", AZ::Component);
        static void Reflect(AZ::ReflectContext* context);

        static AZ::Entity* CreateBaseConnectionEntity(const Endpoint& sourceEndpoint, const Endpoint& targetEndpoint, bool createModelConnection, const AZStd::string& selectorClass);
        static AZ::Entity* CreateGeneralConnection(const Endpoint& sourceEndpoint, const Endpoint& targetEndpoint, bool createModelConnection, const AZStd::string& substyle = "");

        ConnectionComponent();
        ConnectionComponent(const Endpoint& sourceEndpoint,const Endpoint& targetEndpoint, bool createModelConnection = true);
        ~ConnectionComponent() override = default;

        // AZ::Component
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("GraphCanvas_ConnectionService", 0x7ef98865));
            provided.push_back(AZ_CRC("GraphCanvas_SceneMemberService", 0xe9759a2d));
        }

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC("GraphCanvas_ConnectionService", 0x7ef98865));
        }

        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
        {
            (void)dependent;
        }

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
        }

        void Activate() override;
        void Deactivate() override;
        ////

        // ConnectionRequestBus
        AZ::EntityId GetSourceSlotId() const override;
        AZ::EntityId GetSourceNodeId() const override;
        Endpoint GetSourceEndpoint() const override;
        QPointF GetSourcePosition() const override;
        void StartSourceMove() override;
        void SnapSourceDisplayTo(const Endpoint& sourceEndpoint) override;
        void AnimateSourceDisplayTo(const Endpoint& sourceEndpoint, float connectionTime) override;

        AZ::EntityId GetTargetSlotId() const override;
        AZ::EntityId GetTargetNodeId() const override;
        Endpoint GetTargetEndpoint() const override;
        QPointF GetTargetPosition() const override;
        void StartTargetMove() override;
        void SnapTargetDisplayTo(const Endpoint& targetEndpoint) override;
        void AnimateTargetDisplayTo(const Endpoint& targetEndpoint, float connectionTime) override;

        bool ContainsEndpoint(const Endpoint& endpoint) const override;

        void ChainProposalCreation(const QPointF& scenePos, const QPoint& screenPos) override;
        ////

        // AzToolsFramework::EditorEvents::Bus
        void OnEscape() override;
        ////

        // SceneMemberRequestBus
        void SetScene(const AZ::EntityId& sceneId) override;
        void ClearScene(const AZ::EntityId& oldSceneId) override;

        void SignalMemberSetupComplete() override;

        AZ::EntityId GetScene() const override;

        bool LockForExternalMovement(const AZ::EntityId& sceneMemberId) override;
        void UnlockForExternalMovement(const AZ::EntityId& sceneMemberId) override;
        ////

        // ConnectionRequestBus
        AZStd::string GetTooltip() const override;
        void SetTooltip(const AZStd::string& tooltip) override;

        AZStd::any* GetUserData() override;
        ////

        // TickBus
        void OnTick(float deltaTime, AZ::ScriptTimePoint timePoint) override;
        ////

    protected:
        ConnectionComponent(const ConnectionComponent&) = delete;
        const ConnectionComponent& operator=(const ConnectionComponent&) = delete;
        
        void FinalizeMove();

        virtual void OnConnectionMoveStart();
        virtual bool OnConnectionMoveCancelled();
        virtual ConnectionMoveResult OnConnectionMoveComplete(const QPointF& scenePos, const QPoint& screenPos);

        void StartMove();
        void StopMove();

        bool UpdateProposal(Endpoint& activePoint, const Endpoint& proposedEndpoint, AZStd::function< void(const AZ::EntityId&, const AZ::EntityId&)> endpointChangedFunctor);

        ConnectionCandidate FindConnectionCandidateAt(const QPointF& scenePos) const;

        void UpdateMovePosition(const QPointF& scenePos);
        void FinalizeMove(const QPointF& scenePos, const QPoint& screenPos, bool chainAddition);

        void DisplayConnectionToolTip(const QPointF& scenePos, const Endpoint& connectionTarget);

        ConnectionValidationTooltip m_validationResult;
        Endpoint m_endpointTooltip;
        ToastId  m_toastId;

        //! The Id of the graph this connection belongs to.
        GraphId m_graphId;

        //! The source endpoint that this connection is from.
        Endpoint m_sourceEndpoint;
        ConnectionEndpointAnimator m_sourceAnimator;

        //! The target endpoint that this connection is to.
        Endpoint m_targetEndpoint;
        ConnectionEndpointAnimator m_targetAnimator;

        //! Information needed to handle the dragging aspect of the connections
        QPointF m_mousePoint;

        DragContext m_dragContext;
        Endpoint m_previousEndPoint;

        AZStd::string m_tooltip;

        ConnectionEventFilter* m_eventFilter;

        AZ::EntityId m_lockingSceneMember;

        //! Store custom data for this connection
        AZStd::any m_userData;

        StateSetter<RootGraphicsItemDisplayState> m_nodeDisplayStateStateSetter;
        StateSetter<RootGraphicsItemDisplayState> m_connectionStateStateSetter;        
    };

    class ConnectionEventFilter
        : public QGraphicsWidget
    {
    public:
        AZ_CLASS_ALLOCATOR(ConnectionEventFilter, AZ::SystemAllocator, 0);
        ConnectionEventFilter(ConnectionComponent& connection)
            : QGraphicsWidget(nullptr)
            , m_connection(connection)
        {
        }

        bool sceneEventFilter(QGraphicsItem*, QEvent* event) override
        {
            switch (event->type())
            {
            case QEvent::GraphicsSceneMouseMove:
            {
                QGraphicsSceneMouseEvent* mouseEvent = static_cast<QGraphicsSceneMouseEvent*>(event);
                m_connection.UpdateMovePosition(mouseEvent->scenePos());
                return true;
            }
            case QEvent::GraphicsSceneMouseRelease:
            {
                QGraphicsSceneMouseEvent* mouseEvent = static_cast<QGraphicsSceneMouseEvent*>(event);

                bool chainAddition = mouseEvent->modifiers() & Qt::KeyboardModifier::ShiftModifier;

                m_connection.FinalizeMove(mouseEvent->scenePos(), mouseEvent->screenPos(), chainAddition);
                return true;
            }
            default:
                break;
            }

            return false;
        }

    private:
        ConnectionComponent& m_connection;
    };
}