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
#include "ImGuiServerManager.h"
#include <GridMate/Replica/ReplicaChunk.h>
#include <GridMate/Replica/ReplicaChunkDescriptor.h>
#include <GridMate/Replica/RemoteProcedureCall.h>

#if defined(IMGUI_ENABLED)
#include <imgui/imgui.h>
#endif

namespace MultiplayerDiagnostics
{
#if defined(IMGUI_ENABLED)
    static const ImVec4 k_ImGuiTomato = ImVec4(1.0f, 0.4f, 0.3f, 1.0f);
    static const ImVec4 k_ImGuiKhaki  = ImVec4(0.9f, 0.8f, 0.5f, 1.0f);
    static const ImVec4 k_ImGuiCyan   = ImVec4(0.5f, 1.0f, 1.0f, 1.0f);
    static const ImVec4 k_ImGuiDusk   = ImVec4(0.7f, 0.7f, 1.0f, 1.0f);
    static const ImVec4 k_ImGuiWhite  = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

    // --------------------------------------------------------------------------------------------
    template <typename Reporter>
    bool ReplicatedStateTreeNode(const AZStd::string& name, Reporter& report, const ImVec4& color, int depth = 0)
    {
        const int defaultPadAmount = 55;
        const int depthReduction = 3;
        ImGui::PushStyleColor(ImGuiCol_Text, color);

        const bool expanded = ImGui::TreeNode(name.c_str(),
            "%-*s %7.2f kbps %7.2f B Avg. %4zu B Max %10zu B Payload",
            defaultPadAmount - depthReduction * depth,
            name.c_str(),
            report.GetKbitsPerSecond(),
            report.GetAverageBytes(),
            report.GetMaxBytes(),
            report.GetTotalBytes());
        ImGui::PopStyleColor();
        return expanded;
    }

    // --------------------------------------------------------------------------------------------
    void DisplayReplicatedStateReport(AZStd::map<AZStd::string, ComponentReporter>& componentReports, float kbpsWarn, float maxWarn)
    {
        for (auto& componentPair : componentReports)
        {
            ImGui::Separator();
            ComponentReporter& componentReport = componentPair.second;

            if (ReplicatedStateTreeNode(componentPair.first, componentReport, k_ImGuiCyan, 1))
            {
                ImGui::Separator();
                ImGui::Columns(6, "replicated_field_columns");
                ImGui::NextColumn();
                ImGui::Text("kbps");
                ImGui::NextColumn();
                ImGui::Text("Avg. Bytes");
                ImGui::NextColumn();
                ImGui::Text("Min Bytes");
                ImGui::NextColumn();
                ImGui::Text("Max Bytes");
                ImGui::NextColumn();
                ImGui::Text("Total Bytes");
                ImGui::NextColumn();

                auto fieldReports = componentReport.GetFieldReports();
                for (auto& fieldPair : fieldReports)
                {
                    ByteReporter& fieldReport = *fieldPair.second;
                    const float kbitsLastSecond = fieldReport.GetKbitsPerSecond();

                    const ImVec4* textColor = &k_ImGuiWhite;
                    if (fieldReport.GetMaxBytes() > maxWarn)
                    {
                        textColor = &k_ImGuiKhaki;
                    }

                    if (kbitsLastSecond > kbpsWarn)
                    {
                        textColor = &k_ImGuiTomato;
                    }

                    ImGui::PushStyleColor(ImGuiCol_Text, *textColor);

                    ImGui::Text("%s", fieldPair.first.c_str());
                    ImGui::NextColumn();
                    ImGui::Text("%.2f", kbitsLastSecond);
                    ImGui::NextColumn();
                    ImGui::Text("%.2f", fieldReport.GetAverageBytes());
                    ImGui::NextColumn();
                    ImGui::Text("%zu", fieldReport.GetMinBytes());
                    ImGui::NextColumn();
                    ImGui::Text("%zu", fieldReport.GetMaxBytes());
                    ImGui::NextColumn();
                    ImGui::Text("%zu", fieldReport.GetTotalBytes());
                    ImGui::NextColumn();

                    ImGui::PopStyleColor();
                }

                ImGui::Columns(1);
                ImGui::TreePop();
            }
        }
    }
#endif

    ImGuiServerManager::ImGuiServerManager ()
    {
        GridMate::Debug::CarrierDrillerBus::Handler::BusConnect();
    }

    // --------------------------------------------------------------------------------------------
    ImGuiServerManager::~ImGuiServerManager()
    {
        GridMate::Debug::ReplicaDrillerBus::Handler::BusDisconnect();
        GridMate::Debug::CarrierDrillerBus::Handler::BusDisconnect();
    }

    void ImGuiServerManager::OnReceiveReplicaBegin(GridMate::Replica*, const void*, size_t)
    {
        m_currentReceivingEntityReport.Reset();
    }

    void ImGuiServerManager::OnReceiveReplicaEnd(GridMate::Replica* replica)
    {
        m_receivingEntityReports[replica->GetDebugName()].Combine(m_currentReceivingEntityReport);
    }

    void ImGuiServerManager::OnReceiveReplicaChunkEnd(GridMate::ReplicaChunkBase* chunk, AZ::u32 chunkIndex)
    {
        AZ_UNUSED(chunk);
        AZ_UNUSED(chunkIndex);
        m_currentReceivingEntityReport.ReportFragmentEnd();
    }

    void ImGuiServerManager::OnReceiveDataSet(GridMate::ReplicaChunkBase* chunk, AZ::u32 chunkIndex,
        GridMate::DataSetBase* dataSet, GridMate::PeerId, GridMate::PeerId, const void*, size_t len)
    {
        m_currentReceivingEntityReport.ReportField(chunkIndex, chunk->GetDescriptor()->GetChunkName(), chunk->GetDescriptor()->GetDataSetName(chunk, dataSet), len);
    }

    void ImGuiServerManager::OnReceiveRpc (GridMate::ReplicaChunkBase* chunk,
        AZ::u32 chunkIndex,
        GridMate::Internal::RpcRequest* rpc,
        GridMate::PeerId from,
        GridMate::PeerId to,
        const void* data,
        size_t len)
    {
        AZ_UNUSED( from );
        AZ_UNUSED( to );
        AZ_UNUSED( data );

        m_currentReceivingEntityReport.ReportField(chunkIndex, chunk->GetDescriptor()->GetChunkName(), chunk->GetDescriptor()->GetRpcName(chunk, rpc->m_rpc), len);
    }

    void ImGuiServerManager::OnSendReplicaBegin (GridMate::Replica*)
    {
        m_currentSendingEntityReport.Reset();
    }

    void ImGuiServerManager::OnSendReplicaEnd (GridMate::Replica* replica, const void*, size_t)
    {
        m_sendingEntityReports[replica->GetDebugName()].Combine(m_currentSendingEntityReport);
    }

    void ImGuiServerManager::OnSendReplicaChunkEnd (GridMate::ReplicaChunkBase* chunk,
        AZ::u32 chunkIndex,
        const void*,
        size_t)
    {
        AZ_UNUSED(chunk);
        AZ_UNUSED(chunkIndex);
        m_currentSendingEntityReport.ReportFragmentEnd();
    }

    void ImGuiServerManager::OnSendDataSet (GridMate::ReplicaChunkBase* chunk,
        AZ::u32 chunkIndex,
        GridMate::DataSetBase* dataSet,
        GridMate::PeerId,
        GridMate::PeerId,
        const void*,
        size_t len)
    {
        m_currentSendingEntityReport.ReportField(chunkIndex, chunk->GetDescriptor()->GetChunkName(), chunk->GetDescriptor()->GetDataSetName(chunk, dataSet), len);
    }

    void ImGuiServerManager::OnSendRpc (GridMate::ReplicaChunkBase* chunk,
        AZ::u32 chunkIndex,
        GridMate::Internal::RpcRequest* rpc,
        GridMate::PeerId,
        GridMate::PeerId,
        const void*,
        size_t len)
    {
        m_currentSendingEntityReport.ReportField(chunkIndex, chunk->GetDescriptor()->GetChunkName(), chunk->GetDescriptor()->GetRpcName(chunk, rpc->m_rpc), len);
    }

    void ImGuiServerManager::OnIncomingConnection (GridMate::Carrier*, GridMate::ConnectionID)
    {
    }

    void ImGuiServerManager::OnFailedToConnect (GridMate::Carrier*,
        GridMate::ConnectionID,
        GridMate::CarrierDisconnectReason)
    {
        m_lastSecondStats.clear();
    }

    void ImGuiServerManager::OnConnectionEstablished (GridMate::Carrier*, GridMate::ConnectionID)
    {
    }

    void ImGuiServerManager::OnDisconnect (GridMate::Carrier*,
        GridMate::ConnectionID,
        GridMate::CarrierDisconnectReason)
    {
        /*
         * CarrierDrillerBus doesn't provide enough information to correctly keep track of network traffic for all peers.
         * This is a work around until that is fixed to at least not over report the bandwidth amount.
         */
        m_lastSecondStats.clear();
    }

    void ImGuiServerManager::OnDriverError (GridMate::Carrier*,
        GridMate::ConnectionID,
        const GridMate::DriverError&)
    {
        m_lastSecondStats.clear();
    }

    void ImGuiServerManager::OnSecurityError (GridMate::Carrier*,
        GridMate::ConnectionID,
        const GridMate::SecurityError&)
    {
        m_lastSecondStats.clear();
    }

    void ImGuiServerManager::OnUpdateStatistics (const GridMate::string& address,
        const GridMate::TrafficControl::Statistics&,
        const GridMate::TrafficControl::Statistics&,
        const GridMate::TrafficControl::Statistics& effectiveLastSecond,
        const GridMate::TrafficControl::Statistics&)
    {
        m_lastSecondStats[address] = effectiveLastSecond;
    }

    void ImGuiServerManager::OnConnectionStateChanged (GridMate::Carrier*,
        GridMate::ConnectionID,
        GridMate::Carrier::ConnectionStates)
    {
        m_lastSecondStats.clear();
    }

    void ImGuiServerManager::UpdateTrafficStatistics()
    {
#if defined(IMGUI_ENABLED)
        AZ::u32 dataReceived = 0, dataSent = 0;

        for (auto& perConnection : m_lastSecondStats)
        {
            dataReceived += perConnection.second.m_dataReceived;
            dataSent += perConnection.second.m_dataSend;
        }

        if (dataReceived !=0 || dataSent != 0)
        {
            ImGui::Text("Total bandwidth: Sent %u kbps Received %u kbps.", dataSent * 8 / 1000, dataReceived * 8 / 1000);
        }
        else
        {
            ImGui::Text("Total bandwidth: Sent -- kbps Received -- kbps.");
        }
#endif
    }

    // --------------------------------------------------------------------------------------------
    void ImGuiServerManager::OnImGuiUpdate()
    {
#if defined(IMGUI_ENABLED)
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("GridMate"))
            {
                if (m_showServerReportWindow)
                {
                    if (ImGui::MenuItem("Hide Multiplayer Analytics Window"))
                    {
                        m_showServerReportWindow = false;
                    }
                }
                else if (ImGui::MenuItem("Show Multiplayer Analytics Window"))
                {
                    m_showServerReportWindow = true;
                }

                ImGui::End();
            }

            ImGui::EndMainMenuBar();
        }

        if (m_showServerReportWindow)
        {
            if (ImGui::Begin("Multiplayer Analytics", &m_showServerReportWindow))
            {
                // General carrier stats
                UpdateTrafficStatistics();

                if (ImGui::Checkbox("Analyze network traffic", &m_isTrackingMessages))
                {
                    if (m_isTrackingMessages)
                    {
                        GridMate::Debug::ReplicaDrillerBus::Handler::BusConnect();
                    }
                    else
                    {
                        GridMate::Debug::ReplicaDrillerBus::Handler::BusDisconnect();

                        m_currentReceivingEntityReport.Reset();
                        m_receivingEntityReports.clear();

                        m_currentSendingEntityReport.Reset();
                        m_sendingEntityReports.clear();
                    }
                }

                if (m_isTrackingMessages)
                {
                    ImGui::Separator();

                    static ImGuiTextFilter filter;
                    filter.Draw();

                    if (ImGui::CollapsingHeader("Received replicas per type"))
                    {
                        for (auto& entityPair : m_receivingEntityReports)
                        {
                            if (!filter.PassFilter(entityPair.first.c_str()))
                            {
                                continue;
                            }

                            ImGui::Separator();
                            if (ReplicatedStateTreeNode(entityPair.first, entityPair.second, k_ImGuiDusk))
                            {
                                DisplayReplicatedStateReport(entityPair.second.GetComponentReports(), m_replicatedStateKbpsWarn, m_replicatedStateMaxSizeWarn);
                                ImGui::TreePop();
                            }
                        }
                    }

                    if (ImGui::CollapsingHeader("Sent replicas per type"))
                    {
                        for (auto& entityPair : m_sendingEntityReports)
                        {
                            if (!filter.PassFilter(entityPair.first.c_str()))
                            {
                                continue;
                            }

                            ImGui::Separator();
                            if (ReplicatedStateTreeNode(entityPair.first, entityPair.second, k_ImGuiDusk))
                            {
                                DisplayReplicatedStateReport(entityPair.second.GetComponentReports(), m_replicatedStateKbpsWarn, m_replicatedStateMaxSizeWarn);
                                ImGui::TreePop();
                            }
                        }
                    }
                }
            }
            ImGui::End();
        }
#endif
    }
}
