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
#include "stdafx.h"

#include "TelemetryEvent.h"

#include "TelemetryBus.h"

namespace Telemetry
{
    TelemetryEvent::TelemetryEvent(const char* eventName)
        : m_eventName(eventName)
    {
    }

    void TelemetryEvent::SetAttribute(const AZStd::string& name, const AZStd::string& value)
    {
        m_attributes[name] = value;
    }

    const AZStd::string& TelemetryEvent::GetAttribute(const AZStd::string& name)
    {
        static AZStd::string k_emptyString;

        AZStd::unordered_map< AZStd::string, AZStd::string >::iterator attributeIter = m_attributes.find(name);

        if (attributeIter != m_attributes.end())
        {
            return attributeIter->second;
        }

        return k_emptyString;
    }

    void TelemetryEvent::SetMetric(const AZStd::string& name, double metric)
    {
        m_metrics[name] = metric;
    }

    double TelemetryEvent::GetMetric(const AZStd::string& name)
    {
        AZStd::unordered_map< AZStd::string, double >::iterator metricIter = m_metrics.find(name);

        if (metricIter != m_metrics.end())
        {
            return metricIter->second;
        }

        return 0.0;
    }

    void TelemetryEvent::Log()
    {
        EBUS_EVENT(TelemetryEventsBus, LogEvent, (*this));
    }

    void TelemetryEvent::ResetEvent()
    {
        m_metrics.clear();
        m_attributes.clear();
    }

    const char* TelemetryEvent::GetEventName() const
    {
        return m_eventName.c_str();
    }

    const TelemetryEvent::AttributesMap& TelemetryEvent::GetAttributes() const
    {
        return m_attributes;
    }

    const TelemetryEvent::MetricsMap& TelemetryEvent::GetMetrics() const
    {
        return m_metrics;
    }
}