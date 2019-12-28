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

#include "Bus.h"

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>

#include <AzFramework/TargetManagement/TargetManagementAPI.h>
#include <AzFramework/Entity/EntityContextBus.h>

#include <ScriptCanvas/Core/NodeBus.h>

#include "Messages/Request.h"
#include "Messages/Notify.h"
#include "APIArguments.h"

namespace ScriptCanvas
{
    namespace Debugger
    {


        //! The ScriptCanvas debugger component, this is the runtime debugger code that directly controls the execution
        //! and provides insight into a running ScriptCanvas graph.
        class ServiceComponent 
            : public AZ::Component
            , public Message::RequestVisitor
            , public AzFramework::TmMsgBus::Handler
            , public ExecutionNotificationsBus::Handler
            , public AzFramework::TargetManagerClient::Bus::Handler
        {
           using Lock = AZStd::lock_guard<AZStd::recursive_mutex>;

        public:
            AZ_COMPONENT(ServiceComponent, "{794B1BA5-DE13-46C7-9149-74FFB02CB51B}");

            static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
            static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
            static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);
            static void Reflect(AZ::ReflectContext* reflection);

            ServiceComponent() = default;
            ServiceComponent(const ServiceComponent&) = delete;
            ~ServiceComponent() = default;

            //////////////////////////////////////////////////////////////////////////
            // AZ::ServiceComponent
            void Init() override;
            void Activate() override;
            void Deactivate() override;
            //////////////////////////////////////////////////////////////////////////

            //////////////////////////////////////////////////////////////////////////
            // TmMsgBus
            void OnReceivedMsg(AzFramework::TmMsgPtr msg) override;
            //////////////////////////////////////////////////////////////////////////

            //////////////////////////////////////////////////////////////////////////
            // TargetManagerClient::Bus::Handler
            void TargetLeftNetwork(AzFramework::TargetInfo info) override;
            //////////////////////////////////////////////////////////////////////////

            //////////////////////////////////////////////////////////////////////////
            // ExecutionNotifications
            void GraphActivated(const GraphActivation&) override;
            void GraphDeactivated(const GraphActivation&) override;
            bool IsNodeObserved(const Node&) override;
            bool IsVariableObserved(const Node&, VariableId variableId) override;
            void NodeSignaledOutput(const OutputSignal&) override;
            void NodeSignaledInput(const InputSignal&) override;
            void NodeSignaledDataOuput(const OutputDataSignal&) override;
            void NodeStateUpdated(const NodeStateChange&) override;
            void VariableChanged(const VariableChange&) override;
            void AnnotateNode(const AnnotateNodeSignal&) override;
            //////////////////////////////////////////////////////////////////////////

            bool IsGraphObserved(const AZ::EntityId& entityId, const GraphIdentifier& graphIdentifier) const;
            bool IsAssetObserved(const AZ::Data::AssetId& assetId) const;
            
            //////////////////////////////////////////////////////////////////////////
            // Message processing
            void Visit(Message::AddBreakpointRequest& request) override;
            void Visit(Message::BreakRequest& request) override;
            void Visit(Message::ContinueRequest& request) override;
            void Visit(Message::AddTargetsRequest& request) override;
            void Visit(Message::RemoveTargetsRequest& request) override;
            void Visit(Message::StartLoggingRequest& request) override;
            void Visit(Message::StopLoggingRequest& request) override;
            void Visit(Message::GetAvailableScriptTargets& request) override;
            void Visit(Message::GetActiveEntitiesRequest& request) override;
            void Visit(Message::GetActiveGraphsRequest& request) override;
            void Visit(Message::RemoveBreakpointRequest& request) override;
            void Visit(Message::StepOverRequest& request) override;
            //////////////////////////////////////////////////////////////////////////

        protected:
            template<typename t_SignalType, typename t_MessageType>
            void NodeSignalled(const t_SignalType& nodeSignal)
            {
                if (m_state == SCDebugState_Interactive)
                {
                    SCRIPT_CANVAS_DEBUGGER_TRACE_SERVER("Interactive: %s", nodeSignal.ToString().data());
                    AzFramework::TargetManager::Bus::Broadcast(&AzFramework::TargetManager::SendTmMessage, m_client.m_info, t_MessageType(nodeSignal));
                    Interact();
                }
                else if (m_state == SCDebugState_InteractOnNext)
                {
                    SCRIPT_CANVAS_DEBUGGER_TRACE_SERVER("IterateOnNext: %s", nodeSignal.ToString().data());
                    AzFramework::TargetManager::Bus::Broadcast(&AzFramework::TargetManager::SendTmMessage, m_client.m_info, t_MessageType(nodeSignal));
                    m_state = SCDebugState_Interactive;
                    Interact();
                }
                else if (m_state == SCDebugState_Attached)
                {
                    const Signal& asSignal(nodeSignal);
                    Breakpoint breakpoint(asSignal);

                    if (m_breakpoints.find(breakpoint) != m_breakpoints.end())
                    {
                        SCRIPT_CANVAS_DEBUGGER_TRACE_SERVER("Hit breakpoint: %s", nodeSignal.ToString().data());
                        AzFramework::TargetManager::Bus::Broadcast(&AzFramework::TargetManager::SendTmMessage, m_client.m_info, Message::BreakpointHit(nodeSignal));
                        m_state = SCDebugState_Interactive;
                        Interact();
                    }
                    else if (m_client.m_script.m_logExecution)
                    {
                        SCRIPT_CANVAS_DEBUGGER_TRACE_SERVER("Logging Requested: %s", nodeSignal.ToString().data());
                        AzFramework::TargetManager::Bus::Broadcast(&AzFramework::TargetManager::SendTmMessage, m_client.m_info, t_MessageType(nodeSignal));
                    }
                }
                else
                {
                    // \todo performance error/warning on receiving this callback
                }
            }

            void Connect(Target& target);
            Message::Request* FilterMessage(AzFramework::TmMsgPtr& msg);
            void Interact();
            bool IsAttached() const;
            void ProcessMessages();
            
        private:
            enum eSCDebugState
            {
                SCDebugState_Detached = 0,
                SCDebugState_Attached,
                SCDebugState_Interactive,
                SCDebugState_InteractOnNext,
                SCDebugState_Detaching
            };

            void DisconnectFromClient();

            void RefreshActiveEntityStatus();
            void RefreshActiveGraphStatus();

            AZStd::recursive_mutex m_mutex;
            Target m_self;
            Target m_client;

            AzFramework::EntityContextId m_contextId;

            AZStd::atomic_uint m_state;
            AZStd::unordered_set<Breakpoint> m_breakpoints;

            bool                    m_activeGraphStatusDirty;
            ActiveGraphStatusMap    m_activeGraphs;

            bool                    m_activeEntityStatusDirty;
            ActiveEntityStatusMap   m_activeEntities;

            AZStd::recursive_mutex m_msgMutex;
            AzFramework::TmMsgQueue m_msgQueue;
        };
    } // namespace Debugger
}
