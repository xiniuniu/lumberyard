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

#include "Core.h"
#include "ExecutionNotificationsBus.h"

namespace ScriptCanvas
{
    void ReflectExecutionBusArguments(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            NamedVariabledId::Reflect(context);
            NamedNodeId::Reflect(context);
            NamedSlotId::Reflect(context);

            serializeContext->Class<GraphIdentifier>()

                ->Version(0)
                ->Field("uniqueIdentifier", &GraphIdentifier::m_componentId)
                ->Field("assetId", &GraphIdentifier::m_assetId)
                ;

            serializeContext->Class<ActiveGraphStatus>()
                ->Version(0)
                ->Field("IsObserved", &ActiveGraphStatus::m_isObserved)
                ;

            serializeContext->Class<ActiveEntityStatus>()
                ->Version(0)
                ->Field("NamedEntityId", &ActiveEntityStatus::m_namedEntityId)
                ->Field("ActiveGraphs", &ActiveEntityStatus::m_activeGraphs)
                ;

            serializeContext->Class<GraphInfo>()
                ->Version(0)
                ->Field("runtimeEntityId", &GraphInfo::m_runtimeEntity)
                ->Field("graphIdentifier", &GraphInfo::m_graphIdentifier)
                ;

            serializeContext->Class<DatumValue>()
                ->Version(1)
                ->Field("behaviorContextObjectType", &DatumValue::m_behaviorContextObjectType)
                ->Field("value", &DatumValue::m_value)
                ;

            serializeContext->Class<LoggableEvent>()
                ->Version(0)
                ;

            serializeContext->Class<Signal, GraphInfo>()
                ->Version(1)
                ->Field("endpoint", &Signal::m_endpoint)
                ->Field("data", &Signal::m_data)
                ->Field("nodeType", &Signal::m_nodeType)
                ;

            serializeContext->Class<ActivationInfo, GraphInfo>()
                ->Version(1)
                ->Field("entityIsObserved", &ActivationInfo::m_entityIsObserved)
                ->Field("variableValues", &ActivationInfo::m_variableValues)
                ;

            Breakpoint::Reflect(context);

            serializeContext->Class<GraphInfoEventBase, GraphInfo, LoggableEvent>()
                ->Version(0)
                ->Field("Timestamp", &GraphInfoEventBase::m_timestamp)
            ;

            serializeContext->Class<ExecutionThreadBeginning, GraphInfo, LoggableEvent>()
                ->Version(0)
                ->Field("entityNodeId", &ExecutionThreadBeginning::m_nodeId)
                ->Field("Timestamp", &ExecutionThreadBeginning::m_timestamp)
                ;

            ExecutionThreadEnd::Reflect(context);
            GraphActivation::Reflect(context);
            GraphDeactivation::Reflect(context);
            InputSignal::Reflect(context);

            serializeContext->Class<NodeStateChange, GraphInfoEventBase>()
                ->Version(0)
                ;

            serializeContext->Class<AnnotateNodeSignal, GraphInfoEventBase>()
                ->Version(0)
                ->Field("AnnotationLevel", &AnnotateNodeSignal::m_annotationLevel)
                ->Field("Annotation", &AnnotateNodeSignal::m_annotation)
                ->Field("AssetNodeId", &AnnotateNodeSignal::m_assetNodeId)
                ;

            serializeContext->Class<OutputDataSignal, GraphInfoEventBase>()
                ->Version(1)
                ->Field("Endpoint", &OutputDataSignal::m_endpoint)
                ->Field("DatumValue", &OutputDataSignal::m_outputValue)
                ->Field("NodeType", &OutputDataSignal::m_nodeType)
            ;

            OutputSignal::Reflect(context);
            VariableChange::Reflect(context);
        }
    }
        
    ActivationInfo::ActivationInfo(const GraphInfo& info)
        : GraphInfo(info)
    {}

    ActivationInfo::ActivationInfo(const GraphInfo& info, const VariableValues& variableValues)
        : GraphInfo(info)
        , m_variableValues(variableValues)
    {}

    AZStd::string ActivationInfo::ToString() const
    {
        return AZStd::string::format("Entity: %s, Graph: %s, Variables: %s", m_runtimeEntity.ToString().data(), GraphInfo::ToString().data()), ScriptCanvas::ToString(m_variableValues).data();
    }

    DatumValue DatumValue::Create(const VariableDatumBase& value)
    {
        if (value.GetData().GetType().GetType() == Data::eType::BehaviorContextObject)
        {
            return DatumValue(value.GetData().GetType().GetAZType(), AZStd::string::format("(%p) %s", value.GetData().GetAsDanger(), value.GetData().ToString().data()));
        }
        else
        {
            return DatumValue(value);
        }
    }

    AZStd::string DatumValue::ToString() const
    {
        if (m_behaviorContextObjectType.IsNull())
        {
            return Data::GetBehaviorClassName(m_behaviorContextObjectType);
        }
        else
        {
            return Data::GetName(m_value.GetData().GetType());
        }
    }

    ExecutionThreadBeginning::ExecutionThreadBeginning()
    {}

    LoggableEvent* ExecutionThreadBeginning::Duplicate() const
    {
        return aznew ExecutionThreadBeginning(*this);
    }

    Timestamp ExecutionThreadBeginning::GetTimestamp() const
    {
        return m_timestamp;
    }

    void ExecutionThreadBeginning::SetTimestamp(Timestamp timestamp)
    {
        m_timestamp = timestamp;
    }

    void ExecutionThreadBeginning::Visit(LoggableEventVisitor& visitor)
    {
        visitor.Visit(*this);
    }

    AZStd::string ExecutionThreadBeginning::ToString() const
    {
        return AZStd::string::format("ExecutionThreadBeginning: %s, %s", m_nodeId.ToString().data(), GraphInfo::ToString().data());
    }

    bool GraphIdentifier::operator==(const GraphIdentifier& other) const
    {
        return m_assetId == other.m_assetId && m_componentId == other.m_componentId;
    }

    AZStd::string GraphIdentifier::ToString() const
    {
        return AZStd::string::format("Asset: %s, ComponentId: %llu", m_assetId.ToString<AZStd::string>().data(), m_componentId);
    }

    bool GraphInfo::operator==(const GraphInfo& other) const
    {
        return m_runtimeEntity == other.m_runtimeEntity
            && m_graphIdentifier == other.m_graphIdentifier;
    }

    AZStd::string GraphInfo::ToString() const
    {
        return AZStd::string::format("Entity: %s, %s", m_runtimeEntity.ToString().data(), m_graphIdentifier.ToString().data());
    }

    NodeStateChange::NodeStateChange()
    {}

    LoggableEvent* NodeStateChange::Duplicate() const
    {
        return aznew NodeStateChange(*this);
    }

    AZStd::string NodeStateChange::ToString() const
    {
        // \todo I think....this should get...cut...it's not actually a script canvas level feature
        return "NodeStateChange";
    }

    void NodeStateChange::Visit(LoggableEventVisitor& visitor)
    {
        visitor.Visit(*this);
    }

    GraphInfoEventBase::GraphInfoEventBase()
        : m_timestamp(AZStd::GetTimeUTCMilliSecond())
    {
    }

    GraphInfoEventBase::GraphInfoEventBase(const GraphInfo& graphInfo)
        : GraphInfo(graphInfo)
        , m_timestamp(AZStd::GetTimeUTCMilliSecond())
    {
    }

    Timestamp GraphInfoEventBase::GetTimestamp() const
    {
        return m_timestamp;
    }

    void GraphInfoEventBase::SetTimestamp(Timestamp timestamp)
    {
        m_timestamp = timestamp;
    }

    AnnotateNodeSignal::AnnotateNodeSignal()
        : m_annotationLevel(AnnotationLevel::Info)
        
    {
    }

    AnnotateNodeSignal::AnnotateNodeSignal(const GraphInfo& graphInfo, AnnotationLevel annotationLevel, AZStd::string_view annotation, const AZ::NamedEntityId& assetId)
        : GraphInfoEventBase(graphInfo)
        , m_annotationLevel(annotationLevel)
        , m_annotation(annotation)
        , m_assetNodeId(assetId)
    {
    }

    LoggableEvent* AnnotateNodeSignal::Duplicate() const
    {
        return aznew AnnotateNodeSignal((*this));
    }

    AZStd::string AnnotateNodeSignal::ToString() const
    {
        AZStd::string_view annotationLevel;

        switch (m_annotationLevel)
        {
        case AnnotationLevel::Info:
            annotationLevel = "Info";
            break;
        case AnnotationLevel::Warning:
            annotationLevel = "Warning";
            break;
        case AnnotationLevel::Error:
            annotationLevel = "Error";
            break;
        default:
            break;
        }

        return AZStd::string::format("%s - %s - %s", m_assetNodeId.ToString().c_str(), annotationLevel.data(), m_annotation.c_str());
    }

    void AnnotateNodeSignal::Visit(LoggableEventVisitor& visitor)
    {
        visitor.Visit(*this);
    }

    OutputDataSignal::OutputDataSignal(const GraphInfo& graphInfo, const NodeTypeIdentifier& nodeType, const NamedEndpoint& namedEndpoint, const DatumValue& value)
        : GraphInfoEventBase(graphInfo)
        , m_nodeType(nodeType)
        , m_endpoint(namedEndpoint)
        , m_outputValue(value)
    {
    }

    LoggableEvent* OutputDataSignal::Duplicate() const
    {
        return aznew OutputDataSignal((*this));
    }

    AZStd::string OutputDataSignal::ToString() const
    {
        return AZStd::string::format("Data (%s) pushed from (%s::%s)", m_outputValue.ToString().c_str(), m_endpoint.GetNodeName().c_str(), m_endpoint.GetSlotName().c_str());
    }

    void OutputDataSignal::Visit(LoggableEventVisitor& visitor)
    {
        visitor.Visit((*this));
    }

    bool Signal::operator==(const Signal& other) const
    {
        return m_runtimeEntity == other.m_runtimeEntity
            && m_graphIdentifier == other.m_graphIdentifier
            && m_endpoint == other.m_endpoint;
    }    

    AZStd::string Signal::ToString() const
    {
        return AZStd::string::format("Graph: %s, Node: %s:%s, Slot: %s:%s, Input: %s"
            , GraphInfo::ToString().data()
            , m_endpoint.GetNodeId().ToString().data()
            , m_endpoint.GetNodeName().data()
            , m_endpoint.GetSlotId().ToString().data()
            , m_endpoint.GetSlotName().data()
            , ScriptCanvas::ToString(m_data).data());
    }

    AZStd::string ToString(const SlotDataMap& map)
    {
        AZStd::string result;

        for (const auto& iter : map)
        {
            result += iter.first.ToString();
            result += ":";
            result += iter.first.m_name;
            result += " = ";
            result += iter.second.m_value.GetData().ToString();
            result += ", ";
        }
        
        return result;
    }

    AZStd::string ToString(const VariableValues& variableValues)
    {
        AZStd::string result;
     
        for (const auto& variableEntry : variableValues)
        {
            // <type> name = value, 
            result += "<";
            result += variableEntry.second.second.ToString();
            result += "> ";
            result += variableEntry.second.first;
            result += " = ";
            result += variableEntry.second.second.m_value.GetData().ToString();
            result += ", ";
        }
        
        return result;
    }
}
