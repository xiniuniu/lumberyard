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

#include <QGraphicsLinearLayout>
#include <QTimer>

#include <Components/Slots/SlotLayoutComponent.h>
#include <GraphCanvas/Components/SceneBus.h>
#include <GraphCanvas/Components/Slots/SlotBus.h>
#include <GraphCanvas/Components/Slots/Data/DataSlotBus.h>
#include <GraphCanvas/Components/StyleBus.h>
#include <GraphCanvas/Components/VisualBus.h>
#include <GraphCanvas/Styling/StyleHelper.h>
#include <Widgets/GraphCanvasLabel.h>
#include <Widgets/NodePropertyDisplayWidget.h>

namespace GraphCanvas
{
    class DataSlotLayoutComponent;
    class DataSlotConnectionPin;
    class GraphCanvasLabel;

    class DataSlotLayout
        : public QGraphicsLinearLayout
        , public SlotNotificationBus::Handler
        , public DataSlotLayoutRequestBus::Handler
        , public DataSlotNotificationBus::Handler
        , public NodeDataSlotRequestBus::Handler
        , public SceneMemberNotificationBus::Handler
        , public StyleNotificationBus::Handler
        , public VisualNotificationBus::Handler
    {
    public:
        AZ_CLASS_ALLOCATOR(DataSlotLayout, AZ::SystemAllocator, 0);

        DataSlotLayout(DataSlotLayoutComponent& owner);
        ~DataSlotLayout();

        void Activate();
        void Deactivate();

        // SceneMemberNotificationBus
        void OnSceneSet(const AZ::EntityId&) override;
        void OnSceneReady() override;
        ////

        // SlotNotificationBus
        void OnRegisteredToNode(const AZ::EntityId& nodeId) override;

        void OnNameChanged(const TranslationKeyedString&) override;
        void OnTooltipChanged(const TranslationKeyedString&) override;
        ////

        // StyleNotificationBus
        void OnStyleChanged() override;
        ////

        // DataSlotLayoutRequestBus
        const DataSlotConnectionPin* GetConnectionPin() const override;

        void UpdateDisplay() override;
        ////

        // DataSlotNotificationBus
        void OnDataSlotTypeChanged(const DataSlotType& dataSlotType) override;
        void OnDisplayTypeChanged(const AZ::Uuid& dataType, const AZStd::vector<AZ::Uuid>& typeIds) override;
        ////

        // NodeDataSlotRequestBus
        void RecreatePropertyDisplay() override;
        ////

    private:

        AZ::EntityId GetSceneId() const;

        void TryAndSetupSlot();

        void CreateDataDisplay();
        void UpdateLayout();
        void UpdateGeometry();

        ConnectionType m_connectionType;

        Styling::StyleHelper m_style;
        DataSlotLayoutComponent& m_owner;

        QGraphicsWidget*                                m_spacer;
        NodePropertyDisplayWidget*                      m_nodePropertyDisplay;
        DataSlotConnectionPin*                          m_slotConnectionPin;
        GraphCanvasLabel*                               m_slotText;

        // track the last seen values of some members to prevent UpdateLayout doing unnecessary work
        struct
        {
            ConnectionType connectionType = CT_Invalid;
            DataSlotConnectionPin* slotConnectionPin = nullptr;
            GraphCanvasLabel* slotText = nullptr;
            NodePropertyDisplayWidget* nodePropertyDisplay = nullptr;
            QGraphicsWidget* spacer = nullptr;
        } m_atLastUpdate;
    };

    //! Lays out the parts of the Data Slot
    class DataSlotLayoutComponent
        : public SlotLayoutComponent
    {
    public:
        AZ_COMPONENT(DataSlotLayoutComponent, "{0DA3CBDA-1C43-4A18-8E01-AEEAA3C81882}");
        static void Reflect(AZ::ReflectContext* context);

        DataSlotLayoutComponent();
        virtual ~DataSlotLayoutComponent() = default;

        void Init();
        void Activate();
        void Deactivate();

    private:
        DataSlotLayout* m_layout;
    };
}