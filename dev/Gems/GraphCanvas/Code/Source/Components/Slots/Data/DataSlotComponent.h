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

#include <Components/Slots/SlotComponent.h>

#include <GraphCanvas/Components/Slots/Data/DataSlotBus.h>
#include <GraphCanvas/Components/Nodes/NodeBus.h>

namespace GraphCanvas
{
    class DataSlotComponent
        : public SlotComponent
        , public DataSlotRequestBus::Handler        
    {
    public:
        AZ_COMPONENT(DataSlotComponent, "{DB13C73D-2453-44F8-BB38-316C90264B73}", SlotComponent);
        static void Reflect(AZ::ReflectContext* reflectContext);
        
        static AZ::Entity* CreateDataSlot(const AZ::EntityId& nodeId, const DataSlotConfiguration& dataSlotConfiguration);
        
        DataSlotComponent();
        DataSlotComponent(const DataSlotConfiguration& dataSlotConfiguration);
        ~DataSlotComponent();
        
        // Component
        void Init();
        void Activate();
        void Deactivate();
        ////

        // SceneMemberNotifications
        void OnSceneMemberAboutToSerialize(GraphSerialization& sceneSerialization) override;
        ////

        // SlotRequestBus
        void DisplayProposedConnection(const AZ::EntityId& connectionId, const Endpoint& endpoint) override;
        void RemoveProposedConnection(const AZ::EntityId& connectionId, const Endpoint& endpoint) override;

        void AddConnectionId(const AZ::EntityId& connectionId, const Endpoint& endpoint) override;
        void RemoveConnectionId(const AZ::EntityId& connectionId, const Endpoint& endpoint) override;

        void SetNode(const AZ::EntityId& nodeId) override;

        SlotConfiguration* CloneSlotConfiguration() const override;
        ////

        // DataSlotRequestBus
        bool AssignVariable(const AZ::EntityId& variableId) override;

        AZ::EntityId GetVariableId() const override;

        bool ConvertToReference() override;
        bool CanConvertToReference() const override;

        bool ConvertToValue() override;
        bool CanConvertToValue() const override;

        DataSlotType GetDataSlotType() const override;

        AZ::Uuid GetDataTypeId() const override;
        void SetDataTypeId(AZ::Uuid typeId) override;

        const Styling::StyleHelper* GetDataColorPalette() const override;
        
        size_t GetContainedTypesCount() const override;
        AZ::Uuid GetContainedTypeId(size_t index) const override;
        const Styling::StyleHelper* GetContainedTypeColorPalette(size_t index) const override;

        void SetDataAndContainedTypeIds(AZ::Uuid typeId, const AZStd::vector<AZ::Uuid>& typeIds = AZStd::vector<AZ::Uuid>()) override;
        ////

    protected:
        void UpdateDisplay();
        void RestoreDisplay(bool updateDisplay = false);

        // SlotComponent
        void OnFinalizeDisplay() override;
        ////

        void UpdatePropertyDisplayState();

    private:
        DataSlotComponent(const DataSlotComponent&) = delete;
        DataSlotComponent& operator=(const DataSlotComponent&) = delete;
        AZ::Entity* ConstructConnectionEntity(const Endpoint& sourceEndpoint, const Endpoint& targetEndpoint, bool createModelConnection) override;

        bool            m_fixedType;
        DataSlotType    m_dataSlotType;

        AZ::Uuid                m_dataTypeId;
        AZStd::vector<AZ::Uuid> m_containedTypeIds;

        AZ::EntityId    m_variableId;
        AZ::u64         m_copiedVariableId;

        AZ::EntityId    m_displayedConnection;

        // Cached information for the display
        DataSlotType    m_previousDataSlotType;
        AZ::EntityId    m_cachedVariableId;
    };
}