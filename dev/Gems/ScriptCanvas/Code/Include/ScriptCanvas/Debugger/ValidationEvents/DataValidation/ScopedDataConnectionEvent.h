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

#include <ScriptCanvas/Debugger/ValidationEvents/DataValidation/DataValidationIds.h>

namespace ScriptCanvas
{
    // Latent Data Connection Event
    // An event that is generated when data is passed outside
    // of a particular execution scope, and will not be available
    // for use when the node executes
    class ScopedDataConnectionEvent
        : public ValidationEvent
    {
    public:
        AZ_CLASS_ALLOCATOR(ScopedDataConnectionEvent, AZ::SystemAllocator, 0);
        AZ_RTTI(ScopedDataConnectionEvent, "{58F76284-987C-4A15-A31B-407475586958}", ValidationEvent);
        
        ScopedDataConnectionEvent(const AZ::EntityId& connectionId)
            : ValidationEvent(ValidationSeverity::Warning)
            , m_connectionId(connectionId)
        {
            SetDescription("Data Connection crosses across execution boundaries, and will not provide data.");
        }

        bool CanAutoFix() const
        {
            return false;
        }
        
        AZStd::string GetIdentifier() const
        {
            return DataValidationIds::ScopedDataConnectionId;
        }
        
        AZ::Crc32 GetIdCrc() const
        {
            return DataValidationIds::ScopedDataConnectionCrc;
        }
        
        const AZ::EntityId& GetConnectionId() const
        {
            return m_connectionId;
        }

        AZStd::string_view GetTooltip() const
        {
            return "Out of Scope Data Connection";
        }
        
    private:
    
        AZ::EntityId m_connectionId;
    };
}