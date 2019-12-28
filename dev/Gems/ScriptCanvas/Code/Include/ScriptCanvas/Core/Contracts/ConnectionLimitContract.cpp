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

#include "ConnectionLimitContract.h"
#include <ScriptCanvas/Core/ContractBus.h>
#include <ScriptCanvas/Core/GraphBus.h>
#include <ScriptCanvas/Core/NodeBus.h>
#include <ScriptCanvas/Core/Endpoint.h>
#include <ScriptCanvas/Core/Slot.h>

namespace ScriptCanvas
{
    AZ::Outcome<void, AZStd::string> ConnectionLimitContract::OnEvaluate(const Slot& sourceSlot, const Slot& targetSlot) const
    {
        AZ::EntityId graphId;
        NodeRequestBus::EventResult(graphId, sourceSlot.GetNodeId(), &NodeRequests::GetGraphId);
        
        AZStd::vector<Endpoint> connectedEndpoints;
        GraphRequestBus::EventResult(connectedEndpoints, graphId, &GraphRequests::GetConnectedEndpoints, Endpoint{ sourceSlot.GetNodeId(), sourceSlot.GetId() });

        if (m_limit < 0 || connectedEndpoints.size() < static_cast<AZ::u32>(m_limit))
        {
            return AZ::Success();
        }
        else
        {
            AZStd::string errorMessage = AZStd::string::format
                ( "Connection cannot be created between source slot \"%s\" and target slot \"%s\" as the source slot has a Connection Limit of %d. (%s)"
                , sourceSlot.GetName().data()
                , targetSlot.GetName().data()
                , m_limit
                , RTTI_GetTypeName());

            return AZ::Failure(errorMessage);
        }
    }

    ConnectionLimitContract::ConnectionLimitContract(AZ::s32 limit) 
        : m_limit(AZStd::GetMax(-1, limit)) 
    {}

    void ConnectionLimitContract::SetLimit(AZ::s32 limit) 
    { 
        m_limit = AZStd::GetMax(-1, limit); 
    }

    void ConnectionLimitContract::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<ConnectionLimitContract, Contract>()
                ->Version(0)
                ->Field("limit", &ConnectionLimitContract::m_limit)
                ;
        }
    }
}
