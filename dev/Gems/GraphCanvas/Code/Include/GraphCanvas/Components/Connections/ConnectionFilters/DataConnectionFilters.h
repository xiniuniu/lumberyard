/*
* All or Portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
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

#include <AzCore/std/containers/unordered_set.h>

#include <GraphCanvas/Components/Connections/ConnectionFilters/ConnectionFilterBus.h>
#include <GraphCanvas/Components/Slots/Data/DataSlotBus.h>
#include <GraphCanvas/Editor/GraphModelBus.h>

namespace GraphCanvas
{    
    class DataSlotTypeFilter
        : public ConnectionFilter
    {
        friend class SlotConnectionFilter;
    public:
        AZ_RTTI(DataSlotTypeFilter, "{D625AE2F-5F71-461E-A553-554402A824BF}", ConnectionFilter);
        AZ_CLASS_ALLOCATOR(DataSlotTypeFilter, AZ::SystemAllocator, 0);

        DataSlotTypeFilter()
        {
        }
        
        bool CanConnectWith(const Endpoint& endpoint) const override
        {
            AZ::EntityId sceneId;
            SceneMemberRequestBus::EventResult(sceneId, GetEntityId(), &SceneMemberRequests::GetScene);            

            DataSlotType sourceType = DataSlotType::Unknown;
            DataSlotType targetType = DataSlotType::Unknown;

            Endpoint sourceEndpoint;
            Endpoint targetEndpoint;

            // We want to look at the connection we are trying to create.
            // Since this runs on the target of the connections.
            // We need to look at this from the perspective of the thing asking us for the connection.
            ConnectionType connectionType = CT_None;
            SlotRequestBus::EventResult(connectionType, endpoint.GetSlotId(), &SlotRequests::GetConnectionType);

            if (connectionType == CT_Input)
            {
                sourceEndpoint.m_slotId = GetEntityId();
                SlotRequestBus::EventResult(sourceEndpoint.m_nodeId, GetEntityId(), &SlotRequests::GetNode);
                
                targetEndpoint = endpoint;
            }
            else if (connectionType == CT_Output)
            {
                sourceEndpoint = endpoint;

                targetEndpoint.m_slotId = GetEntityId();
                SlotRequestBus::EventResult(targetEndpoint.m_nodeId, GetEntityId(), &SlotRequests::GetNode);
            }
            else
            {
                return false;
            }

            DataSlotRequestBus::EventResult(sourceType, sourceEndpoint.GetSlotId(), &DataSlotRequests::GetDataSlotType);
            DataSlotRequestBus::EventResult(targetType, targetEndpoint.GetSlotId(), &DataSlotRequests::GetDataSlotType);

            bool acceptConnection = false;

            if (sourceType == DataSlotType::Variable)
            {
                if (targetType == DataSlotType::Value)
                {
                    DataSlotRequestBus::EventResult(acceptConnection, targetEndpoint.GetSlotId(), &DataSlotRequests::CanConvertToReference);
                }
                else
                {
                    acceptConnection = true;
                }

                if (acceptConnection)
                {
                    AZ::EntityId variableId;
                    DataSlotRequestBus::EventResult(variableId, sourceEndpoint.GetSlotId(), &DataSlotRequests::GetVariableId);

                    GraphModelRequestBus::EventResult(acceptConnection, sceneId, &GraphModelRequests::IsValidVariableAssignment, variableId, targetEndpoint);
                }
            }
            else if (sourceType == DataSlotType::Value)
            {
                if (targetType == DataSlotType::Reference)
                {
                    DataSlotRequestBus::EventResult(acceptConnection, targetEndpoint.GetSlotId(), &DataSlotRequests::CanConvertToValue);
                }
                else if (targetType == DataSlotType::Value)
                {
                    acceptConnection = true;
                }
            }
            else if (sourceType == DataSlotType::Container)
            {
                if (targetType == DataSlotType::Container)
                {
                    acceptConnection = true;
                }

                // A target type of value may represent a context sensitive slot that will mutate according to the container
                if (targetType == DataSlotType::Value)
                {
                    acceptConnection = true;
                }

            }

            return acceptConnection; // HACK, not to submit
        }
    };
}