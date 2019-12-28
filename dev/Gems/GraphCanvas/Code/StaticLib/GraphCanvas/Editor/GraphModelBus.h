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

#include <QMimeData>

#include <AzCore/Component/EntityId.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/std/containers/vector.h>

#include <GraphCanvas/Editor/EditorTypes.h>
#include <GraphCanvas/Types/Endpoint.h>

class QMimeData;

namespace GraphCanvas
{    
    class NodePropertyDisplay;
    struct Endpoint;
    
    class GraphSettingsRequests
        : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        using BusIdType = GraphId;
    };
    
    using GraphSettingsRequestBus = AZ::EBus<GraphSettingsRequests>;
    
    class GraphModelRequests
        : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        using BusIdType = GraphId;

        //! Callback for requesting an Undo Point to be posted.
        virtual void RequestUndoPoint() = 0;

        //! Callback for requesting the incrementation of the value of the ignore undo point tracker
        virtual void RequestPushPreventUndoStateUpdate() = 0;

        //! Callback for requesting the decrementation of the value of the ignore undo point tracker
        virtual void RequestPopPreventUndoStateUpdate() = 0;

        //! Request to trigger an undo
        virtual void TriggerUndo() = 0;

        //! Request to trigger a redo
        virtual void TriggerRedo() = 0;

        // Enable the specified nodes
        virtual bool EnableNodes(const AZStd::unordered_set< NodeId >& nodeIds)
        {
            AZ_UNUSED(nodeIds);
            return false;
        }

        // Disables the specified nodes
        virtual bool DisableNodes(const AZStd::unordered_set< NodeId >& nodeIds)
        {
            AZ_UNUSED(nodeIds);
            return false;
        }

        //! Request to create a NodePropertyDisplay class for a particular DataSlot.
        virtual NodePropertyDisplay* CreateDataSlotPropertyDisplay(const AZ::Uuid& dataType, const NodeId& nodeId, const SlotId& slotId) const { return nullptr; }
        virtual NodePropertyDisplay* CreateDataSlotVariablePropertyDisplay(const AZ::Uuid& dataType, const NodeId& nodeId, const SlotId& slotId) const { return nullptr; }
        virtual NodePropertyDisplay* CreatePropertySlotPropertyDisplay(const AZ::Crc32& propertyId, const NodeId& nodeId, const SlotId& slotId) const { return nullptr; }

        //! This is sent when a connection is disconnected.
        virtual void DisconnectConnection(const ConnectionId& connectionId) = 0;

        //! This is sent when attempting to create a given connection.
        virtual bool CreateConnection(const ConnectionId& connectionId, const Endpoint& sourcePoint, const Endpoint& targetPoint) = 0;

        //! This is sent to confirm whether or not a connection can take place.
        virtual bool IsValidConnection(const Endpoint& sourcePoint, const Endpoint& targetPoint) const = 0;

        //! This will return the structure needed to display why a connection could not be created between the specified endpoints.
        virtual ConnectionValidationTooltip GetConnectionValidityTooltip(const Endpoint& sourcePoint, const Endpoint& targetPoint) const
        {
            ConnectionValidationTooltip result;

            result.m_isValid = IsValidConnection(sourcePoint, targetPoint);

            return result;
        }

        //! This is sent to confirm whether or not a variable assignment can take place.
        virtual bool IsValidVariableAssignment(const AZ::EntityId& variableId, const Endpoint& targetPoint) const = 0;

        //! This will return the structure needed to display why a variable could not be assigned to a specific reference inside of GraphCanvas.
        virtual ConnectionValidationTooltip GetVariableAssignmentValidityTooltip(const AZ::EntityId& variableId, const Endpoint& targetPoint) const
        {
            ConnectionValidationTooltip result;

            result.m_isValid = IsValidVariableAssignment(variableId, targetPoint);

            return result;
        }

        //! Get the Display Type name for the given AZ type
        virtual AZStd::string GetDataTypeString(const AZ::Uuid& typeId) = 0;

        // Signals out that the specified elements save data is dirty.
        virtual void OnSaveDataDirtied(const AZ::EntityId& savedElement) = 0;

        // Signals out that the graph was signeld to clean itself up.
        virtual void OnRemoveUnusedNodes() = 0;
        virtual void OnRemoveUnusedElements() = 0;

        virtual void ResetSlotToDefaultValue(const NodeId& nodeId, const SlotId& slotId) = 0;

        virtual void RemoveSlot(const NodeId& nodeId, const SlotId& slotId) = 0;

        virtual bool IsSlotRemovable(const NodeId& nodeId, const SlotId& slotId) const = 0;        

        //////////////////////////////////////
        // Extender Slot Optional Overrides

        // Request an extension to the node for the specified group from the specific Node and ExtenderId.
        //
        // Should return the appropriate slotId for the newly added slots.
        virtual SlotId RequestExtension(const NodeId& nodeId, const ExtenderId& extenderId)
        {
            AZ_UNUSED(nodeId);
            AZ_UNUSED(extenderId);
            return SlotId();
        }
        ////
        
        //////////////////////////////////////
        // Node Wrapper Optional Overrides

        // Returns whether or not the specified wrapper node should accept the given drop
        virtual bool ShouldWrapperAcceptDrop(const NodeId& wrapperNode, const QMimeData* mimeData) const
        {
            AZ_UNUSED(wrapperNode); AZ_UNUSED(mimeData);
            AZ_Error("GraphCanvas", false, "Trying to use Node Wrappers without providing model information. Please implement 'ShouldWrapperAcceptDrop' on the GraphModelRequestBus.");
            return false;
        }

        // Signals out that we want to drop onto the specified wrapper node
        virtual void AddWrapperDropTarget(const NodeId& wrapperNode)
        {
            AZ_UNUSED(wrapperNode);
            AZ_Error("GraphCanvas", false, "Trying to use Node Wrappers without providing model information. Please implement 'AddWrapperDropTarget' on the GraphModelRequestBus.");
        };

        // Signals out that we no longer wish to drop onto the specified wrapper node
        virtual void RemoveWrapperDropTarget(const NodeId& wrapperNode)
        {
            AZ_UNUSED(wrapperNode);
            AZ_Error("GraphCanvas", false, "Trying to use Node Wrappers without providing model information. Please implement 'RemoveWrapperDropTarget' on the GraphModelRequestBus.");
        }
        //////////////////////////////////////
    };

    using GraphModelRequestBus = AZ::EBus<GraphModelRequests>;

    class GraphModelNotifications
        : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        using BusIdType = GraphId;        
    };

    using GraphModelNotificationBus = AZ::EBus<GraphModelNotifications>;
}