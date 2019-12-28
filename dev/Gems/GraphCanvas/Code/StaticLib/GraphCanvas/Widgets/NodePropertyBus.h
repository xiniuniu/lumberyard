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

#include <QColor>

#include <AzCore/Component/EntityId.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/Math/Uuid.h>

namespace GraphCanvas
{
    class NodePropertyDisplay;

    enum class NodePropertyLayoutState
    {
        None,
        Display,
        Editing,
        Disabled
    };
    
    struct NodePropertyConfiguration
    {
    };
    
	//! NodePropertiesRequestBus
	//!
    //! These are requests that will modify the display state of the entire node.
    //! This will handle things like locking editing states in reaction to the mouse cursor, 
	//! or forcing a layout state to a particular state.
    class NodePropertiesRequests
        : public AZ::EBusTraits
    {
    public:
        // BusId here is the node that contains the property.
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AZ::EntityId;
        
        virtual void LockEditState(NodePropertyDisplay* propertyDisplay) = 0;
        virtual void UnlockEditState(NodePropertyDisplay* propertyDisplay) = 0;
        
        virtual void ForceLayoutState(NodePropertyLayoutState layoutState) = 0;
    };
    
    using NodePropertiesRequestBus = AZ::EBus<NodePropertiesRequests>;
    
	//! NodePropertyRequestBus
	//!
    //! Requests that should modify a specific node property display.
    //! Will handle things like changing the display property or disabling the property.
    class NodePropertyRequests
        : public AZ::EBusTraits
    {
    public:
        // BusId is the id that was given to the NodeProeprty in question.
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AZ::EntityId;
        
        virtual void SetNodePropertyDisplay(NodePropertyDisplay* nodePropertyDisplay) = 0;
        virtual NodePropertyDisplay* GetNodePropertyDisplay() const = 0;
        
        virtual void SetDisabled(bool disabled) = 0;
    };

    using NodePropertyRequestBus = AZ::EBus<NodePropertyRequests>;
}