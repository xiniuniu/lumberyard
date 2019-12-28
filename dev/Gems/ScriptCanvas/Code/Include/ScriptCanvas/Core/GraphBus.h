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
#include "Endpoint.h"

#include <AzCore/EBus/EBus.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Outcome/Outcome.h>

namespace ScriptCanvas
{
    struct GraphData;
    class Graph;
    //! These are public graph requests
    class GraphRequests : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AZ::EntityId;

        //! Add a ScriptCanvas Node to the Graph
        virtual bool AddNode(const AZ::EntityId&) = 0;
        //! Remove a ScriptCanvas Node to the Graph
        virtual bool RemoveNode(const AZ::EntityId& nodeId) = 0;

        //! Add a ScriptCanvas Connection to the Graph
        virtual bool AddConnection(const AZ::EntityId&) = 0;
        //! Remove a ScriptCanvas Connection from the Graph
        virtual bool RemoveConnection(const AZ::EntityId& nodeId) = 0;

        //! Add an asset dependency to the Graph
        virtual bool AddDependentAsset(AZ::EntityId nodeId, const AZ::TypeId assetType, const AZ::Data::AssetId assetId) = 0;
        //! Remove an asset dependency from the Graph
        virtual bool RemoveDependentAsset(AZ::EntityId nodeId) = 0;

        virtual AZStd::vector<AZ::EntityId> GetNodes() const = 0;
        virtual AZStd::vector<AZ::EntityId> GetConnections() const = 0;
        virtual AZStd::vector<Endpoint> GetConnectedEndpoints(const Endpoint& firstEndpoint) const = 0;
        virtual bool FindConnection(AZ::Entity*& connectionEntity, const Endpoint& firstEndpoint, const Endpoint& otherEndpoint) const = 0;
        
        //! Retrieves the Entity this Graph component is located on
        //! NOTE: There can be multiple Graph components on the same entity so calling FindComponent may not not return this GraphComponent
        virtual AZ::Entity* GetGraphEntity() const = 0;

        //! Retrieves the Graph Component directly using the BusId
        virtual Graph* GetGraph() = 0;

        virtual bool Connect(const AZ::EntityId& sourceNodeId, const SlotId& sourceSlot, const AZ::EntityId& targetNodeId,const SlotId& targetSlot) = 0;
        virtual bool Disconnect(const AZ::EntityId& sourceNodeId, const SlotId& sourceSlot, const AZ::EntityId& targetNodeId, const SlotId& targetSlot) = 0;

        virtual bool ConnectByEndpoint(const Endpoint& sourceEndpoint, const Endpoint& targetEndpoint) = 0;

        //! Returns whether or not a new connecion can be created between two connections.
        //! This will take into account if the endpoints are already connected
        virtual AZ::Outcome<void, AZStd::string> CanCreateConnectionBetween(const Endpoint& sourceEndpoint, const Endpoint& targetEndpoint) const = 0;

        //! Returns whether or not a connection could exist between the two connections.
        //! Does not take into account if the endpoints are already connected.
        virtual AZ::Outcome<void, AZStd::string> CanConnectionExistBetween(const Endpoint& sourceEndpoint, const Endpoint& targetEndpoint) const = 0;

        virtual bool DisconnectByEndpoint(const Endpoint& sourceEndpoint, const Endpoint& targetEndpoint) = 0;
        virtual bool DisconnectById(const AZ::EntityId& connectionId) = 0;

        //! Copies any Node and Connection Entities that belong to the graph to a GraphSerializableField
        virtual AZStd::unordered_set<AZ::Entity*> CopyItems(const AZStd::unordered_set<AZ::Entity*>& entities) = 0;
        //! Adds any Node and Connection Entities to the graph
        virtual void AddItems(const AZStd::unordered_set<AZ::Entity*>& entities) = 0;
        //! Removes any Node and Connection Entities that belong to the graph 
        virtual void RemoveItems(const AZStd::unordered_set<AZ::Entity*>& entities) = 0;
        //! Retrieves any entities that can be be added to graphs
        virtual AZStd::unordered_set<AZ::Entity*> GetItems() const = 0;

        //! Add item to graph if the item is of the type that can be added to the graph
        virtual bool AddItem(AZ::Entity* itemEntity) = 0;
        //! Remove item if it is on the graph
        virtual bool RemoveItem(AZ::Entity* itemEntity) = 0;
        
        //! Retrieves a pointer the GraphData structure stored on the graph
        virtual GraphData* GetGraphData() = 0;
        virtual const GraphData* GetGraphDataConst() const = 0;

        // Adds nodes and connections in the GraphData structure to the graph
        virtual bool AddGraphData(const GraphData&) = 0;
        // Removes nodes and connections in the GraphData structure from the graph
        virtual void RemoveGraphData(const GraphData&) = 0;

        // Signals wether or not a batch of graph data is being added and some extra steps are needed
        // to maintain data integrity for dynamic nodes
        virtual bool IsBatchAddingGraphData() const = 0;
    };

    using GraphRequestBus = AZ::EBus<GraphRequests>;

    class GraphNotifications : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AZ::EntityId;

        //! Notification when a node is added
        virtual void OnNodeAdded(const AZ::EntityId&) {}

        //! Notification when a node is removed
        virtual void OnNodeRemoved(const AZ::EntityId&) {}

        //! Notification when a connection is added
        virtual void OnConnectionAdded(const AZ::EntityId&) {}

        //! Notification when a connections is removed
        virtual void OnConnectionRemoved(const AZ::EntityId&) {}

        //! Notification when a batch add for a graph begins
        virtual void OnBatchAddBegin() {}

        //! Notification when a batch add for a graph completes
        virtual void OnBatchAddComplete() {};
    };

    using GraphNotificationBus = AZ::EBus<GraphNotifications>;

	class EndpointNotifications : public AZ::EBusTraits
	{
	public:
		static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
		using BusIdType = Endpoint;

		//! Notification when an endpoint has been connected.
		//! \param the target Endpoint. The source Endpoint can be obtained using EndpointNotificationBus::GetCurrentBusId().
		virtual void OnEndpointConnected(const Endpoint& targetEndpoint) {}

		//! Notification when an endpoint has been disconnected.
		//! \param the target Endpoint. The source Endpoint can be obtained using EndpointNotificationBus::GetCurrentBusId().
		virtual void OnEndpointDisconnected(const Endpoint& targetEndpoint) {}
	};

    using EndpointNotificationBus = AZ::EBus<EndpointNotifications>;
}
