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

#include <GraphCanvas/Components/Nodes/NodeBus.h>
#include <GraphCanvas/Components/SceneBus.h>

#include "Editor/Include/ScriptCanvas/Bus/NodeIdPair.h"

#include "Editor/GraphCanvas/Components/NodeDescriptors/NodeDescriptorComponent.h"
#include <ScriptEvents/ScriptEventsAssetRef.h>
#include <ScriptCanvas/GraphCanvas/VersionControlledNodeBus.h>


namespace ScriptCanvasEditor
{
    class ScriptEventReceiverEventNodeDescriptorComponent
        : public NodeDescriptorComponent
        , protected GraphCanvas::SceneMemberNotificationBus::Handler
        , protected ScriptEventReceiverEventNodeDescriptorBus::Handler
        , public EBusHandlerEventNodeDescriptorRequestBus::Handler
        , public GraphCanvas::ForcedWrappedNodeRequestBus::Handler
        , public ScriptEventReceiveNodeDescriptorNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(ScriptEventReceiverEventNodeDescriptorComponent, "{EFC9CCCE-32FC-466F-AA29-2EEE1320FF4F}", NodeDescriptorComponent);
        static void Reflect(AZ::ReflectContext* reflectContext);

        ScriptEventReceiverEventNodeDescriptorComponent();
        ScriptEventReceiverEventNodeDescriptorComponent(const AZ::Data::AssetId assetId, const ScriptEvents::Method& methodDefinition);
        ~ScriptEventReceiverEventNodeDescriptorComponent() = default;

        void Activate() override;
        void Deactivate() override;

        // EBusHandlerEventNodeDescriptorRequestBus
        AZStd::string_view GetBusName() const override;
        AZStd::string_view GetEventName() const override;
        ScriptCanvas::EBusEventId GetEventId() const override;
        ////

        // NodeNotificationBus::Handler        
        void OnNodeWrapped(const AZ::EntityId& wrappingNode) override;
        ////

        // SceneMemberNotifications
        void OnSceneMemberAboutToSerialize(GraphCanvas::GraphSerialization& graphSerialization) override;
        void OnSceneMemberDeserialized(const AZ::EntityId& graphId, const GraphCanvas::GraphSerialization& serializationTarget) override;
        ////

        // ScriptEventSenderNodeDescriptorBus::Handler
        const ScriptEvents::Method& GetMethodDefinition() override
        {
            return m_methodDefinition;
        }

        AZStd::string GetEventName() override
        {
            return m_eventName;
        }
        ////

        // ForcedWrappedNodeRequestBus
        AZ::Crc32 GetWrapperType() const override;
        AZ::Crc32 GetIdentifier() const override;

        AZ::EntityId CreateWrapperNode(const AZ::EntityId& sceneId, const AZ::Vector2& nodePosition) override;
        ////

        // ScriptEventReceiveNodeDescriptorNotifications
        void OnScriptEventReloaded(const AZ::Data::Asset<ScriptEvents::ScriptEventsAsset>& asset);
        ////

    protected:

        void OnAddedToGraphCanvasGraph(const GraphCanvas::GraphId& grapphId, const AZ::EntityId& scriptCanvasNodeId) override;

    private:

        void UpdateTitles();

        NodeIdPair    m_ebusWrapper;

        AZ::Data::AssetId m_assetId;
        ScriptEvents::Method m_methodDefinition;
        ScriptCanvas::EBusEventId m_eventId;

        // These are required for reconstructing the node (i.e. copy/paste)
        AZStd::string m_busName;
        AZ::Crc32     m_busId;
        AZStd::string m_eventName;
        AZ::Uuid m_eventPropertyId;
    };
}
