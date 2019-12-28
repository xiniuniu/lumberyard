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

#include "Core.h"

#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/string/string_view.h>
#include <AzCore/EBus/EBus.h>
#include <ScriptCanvas/Data/Data.h>

#include <ScriptCanvas/Core/SlotConfigurations.h>

namespace ScriptCanvas
{
    struct VariableId;
    class Datum;
    class Slot;

    class NodeRequests : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = ID;

        virtual Slot* GetSlot(const SlotId& slotId) const = 0;
        virtual size_t GetSlotIndex(const SlotId& slotId) const = 0;

        //! Gets all of the slots on the node.
        //! Name is funky to avoid a mismatch with typing with another function
        //! that returns a better version of this information that cannot be used with
        //! EBuses because of references.
        virtual AZStd::vector<const Slot*> GetAllSlots() const = 0;

        //! Retrieves a slot id that matches the supplied name
        //! There can be multiple slots with the same name on a node
        //! Therefore this should only be used when a slot's name is unique within the node
        virtual SlotId GetSlotId(AZStd::string_view slotName) const = 0;

        virtual SlotId FindSlotIdForDescriptor(AZStd::string_view slotName, const SlotDescriptor& descriptor) const = 0;

        //! Retrieves a slot id that matches the supplied name and the supplied slot type
        virtual SlotId GetSlotIdByType(AZStd::string_view slotName, CombinedSlotType slotType) const
        {
            return FindSlotIdForDescriptor(slotName, SlotDescriptor(slotType));
        }

        //! Retrieves all slot ids for slots with the specific name
        virtual AZStd::vector<SlotId> GetSlotIds(AZStd::string_view slotName) const = 0;

        virtual const AZ::EntityId& GetGraphId() const = 0;

        //! Determines whether the slot on this node with the specified slot id can accept values of the specified type
        virtual bool SlotAcceptsType(const SlotId&, const Data::Type&) const = 0;

        //! Gets the input for the given SlotId
        virtual Data::Type GetSlotDataType(const SlotId& slotId) const = 0;

        // Retrieves the variable id which is represents the current variable associated with the specified slot
        virtual VariableId GetSlotVariableId(const SlotId& slotId) const = 0;
        // Sets the variable id parameter as the current variable for the specified slot
        virtual void SetSlotVariableId(const SlotId& slotId, const VariableId& variableId) = 0;
        // Reset the variable id value to the original variable id that was associated with the slot
        // when the slot was created by a call to AddInputDatumSlot().
        // The reset variable Id is not associated Variable Manager and is owned by this node
        virtual void ResetSlotVariableId(const SlotId& slotId) = 0;

        // Updates the slotIndex parameter with offset in the SlotList if the slotId is found within the node
        // returns true if the slot id was found in the SlotList otherwise the slotIndex parameter is not changed
        virtual AZ::Outcome<AZ::s64, AZStd::string> FindSlotIndex(const SlotId& slotId) const = 0;

        virtual bool IsOnPureDataThread(const SlotId& slotId) const = 0;

        virtual AZ::Outcome<void, AZStd::string> IsValidTypeForGroup(const AZ::Crc32& dynamicGroup, const Data::Type& dataType) const = 0;

        virtual void SignalBatchedConnectionManipulationBegin() = 0;
        virtual void SignalBatchedConnectionManipulationEnd() = 0;

        virtual void SetNodeEnabled(bool enabled) = 0;
        virtual bool IsNodeEnabled() const = 0;
    };

    using NodeRequestBus = AZ::EBus<NodeRequests>;

    class LogNotifications : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AZ::EntityId;
        virtual void LogMessage(const AZStd::string& log) {}
    };
    using LogNotificationBus = AZ::EBus<LogNotifications>;
            
    class NodeNotifications
        : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AZ::EntityId;

        virtual void OnInputChanged(const SlotId& /*slotId*/) {}

        //! Events signaled when a slot is added or removed from a node
        virtual void OnSlotAdded(const SlotId& /*slotId*/) {}
        virtual void OnSlotRemoved(const SlotId& /*slotId*/) {}
        virtual void OnSlotRenamed(const SlotId& /*slotId*/, AZStd::string_view /*newName*/) {}

        virtual void OnSlotDisplayTypeChanged(const SlotId& /*slotId*/, const ScriptCanvas::Data::Type& /*slotType*/) {}

        virtual void OnSlotActiveVariableChanged(const SlotId& /*slotId*/, const VariableId& oldVariableId, const VariableId& newVariableId) {}

        virtual void OnNodeDisabled() {};
        virtual void OnNodeEnabled() {};
    };

    using NodeNotificationsBus = AZ::EBus<NodeNotifications>;

    class EditorNodeRequests : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = ID;

        //! Get the object from the specified slot.
        virtual const Datum* GetInput(const SlotId& slotId) const = 0;
        virtual Datum* ModInput(const SlotId& slotId) = 0;
        virtual AZ::EntityId GetGraphEntityId() const = 0;
    };

    using EditorNodeRequestBus = AZ::EBus<EditorNodeRequests>;    
}