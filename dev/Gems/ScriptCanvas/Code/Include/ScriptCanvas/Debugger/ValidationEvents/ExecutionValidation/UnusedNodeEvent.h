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

#include <AzCore/Component/EntityId.h>

#include <ScriptCanvas/Debugger/ValidationEvents/ValidationEvent.h>

#include <ScriptCanvas/Debugger/ValidationEvents/ExecutionValidation/ExecutionValidationIds.h>

namespace ScriptCanvas
{
    // UnusedNodeEvent
    // An event that indicated that a node that is not a start point, does not have an execution in.
    // And thus will never execute.
    class UnusedNodeEvent
        : public ValidationEvent
    {
    public:
        AZ_RTTI(UnusedNodeEvent, "{EC6933F8-0D50-49A7-BCA2-BB4B4534AA8C}");
        
        UnusedNodeEvent(const AZ::EntityId& nodeId)
            : ValidationEvent(ValidationSeverity::Warning)
            , m_nodeId(nodeId)
        {
            SetDescription("Node is not marked as an entry point to the graph, and has no incoming connections. Node will not be executed.");
        }
        
        AZStd::string GetIdentifier() const override
        {
            return ExecutionValidationIds::UnusedNodeId;
        }
        
        AZ::Crc32 GetIdCrc() const override
        {
            return AZ_CRC(ExecutionValidationIds::UnusedNodeId);
        }
        
        const AZ::EntityId& GetNodeId() const
        {
            return m_nodeId;
        }

        AZStd::string_view GetTooltip() const override
        {
            return "Unused Node";
        }
        
    private:
    
        AZ::EntityId m_nodeId;
    };
}