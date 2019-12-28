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

#include <AzCore/std/string/string.h>
#include <AzCore/std/chrono/chrono.h>
#include <AzCore/std/containers/map.h>
#include <AzCore/std/containers/vector.h>

namespace MultiplayerDiagnostics
{
    class ByteReporter
    {
    public:
        ByteReporter() { ByteReporter::Reset(); }
        virtual ~ByteReporter() = default;

        void ReportBytes(size_t byteSize);
        void AggregateBytes(size_t byteSize);
        void ReportAggregateBytes();

        float GetAverageBytes() const;
        size_t GetMaxBytes() const;
        size_t GetMinBytes() const;
        size_t GetTotalBytes() const;
        float GetKbitsPerSecond();

        void Combine(const ByteReporter& other);
        virtual void Reset();

        size_t GetTotalCount() const { return m_count; }

    private:
        size_t m_count;
        size_t m_totalBytes;
        size_t m_totalBytesThisSecond;
        float  m_totalBytesLastSecond;
        size_t m_minBytes;
        size_t m_maxBytes;
        size_t m_aggregateBytes;

        AZStd::chrono::monotonic_clock::time_point m_lastUpdateTime;
    };

    class ComponentReporter : public ByteReporter
    {
    public:
        ComponentReporter() = default;

        void ReportField(const char* fieldName, size_t byteSize);
        void ReportDirtyBits(size_t byteSize) { m_componentDirtyBytes.AggregateBytes(byteSize); }
        void ReportFragmentEnd();

        using Report = AZStd::pair<AZStd::string, ByteReporter*>;
        AZStd::vector<Report> GetFieldReports();
        AZStd::size_t GetTotalDirtyBits() const { return m_componentDirtyBytes.GetTotalBytes(); }
        float GetAvgDirtyBits() const { return m_componentDirtyBytes.GetAverageBytes(); }

        void Combine(const ComponentReporter& other);

    private:
        AZStd::map<AZStd::string, ByteReporter> m_fieldReports;
        ByteReporter m_componentDirtyBytes;
    };

    class EntityReporter : public ByteReporter
    {
    public:
        EntityReporter() = default;

        void ReportField(AZ::u32 index, const char* componentName, const char* fieldName, size_t byteSize);
        void ReportDirtyBits(AZ::u32 index, const char* componentName, size_t byteSize);
        void ReportFragmentEnd();

        void Combine(const EntityReporter& other);
        void Reset() override;

        AZStd::map<AZStd::string, ComponentReporter>& GetComponentReports();
        AZStd::size_t GetTotalDirtyBits() const { return m_gdeDirtyBytes.GetTotalBytes(); }
        float GetAvgDirtyBits() const { return m_gdeDirtyBytes.GetAverageBytes(); }

    private:
        ComponentReporter* m_currentComponentReport = nullptr;
        AZStd::map<AZStd::string, ComponentReporter> m_componentReports;
        ByteReporter m_gdeDirtyBytes;
    };
}
