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

#include <AzCore/Debug/Profiler.h>
#include "RunningStatisticsManager.h"

namespace AzFramework
{
    namespace Statistics
    {
        /**
         * @brief Specialization useful for data generated with AZ::Debug::FrameProfileComponent
         *
         * Timer based data collection using AZ_PROFILE_TIMER(...), available in
         * AzCore/Debug/Profiler.h can be collected when using AZ::Debug::FrameProfilerComponent
         * and AZ::Debug::FrameProfilerBus. The method PushTimeDataSample(...) is a convenience
         * to convert those Timer registers into a RunningStatistic.
         *
         * 
         */
        class TimeDataStatisticsManager : public RunningStatisticsManager
        {
        public:
            TimeDataStatisticsManager() = default;
            virtual ~TimeDataStatisticsManager() = default;

            //From RunningStatisticsManager
            void RemoveStatistic(const AZStd::string& name) override;

            /**
             * @brief Adds one sample data to a specific running stat by name.
             *
             * This method is specialized to work with ProfilerRegister::TimeData that can be intercepted
             * during AZ::Debug::FrameProfilerBus::OnFrameProfilerData().
             * For each @param registerName a new RunningStat object is created if it doesn't exist.
             *
             * Adds the TimeData as one sample for its RunningStatistic.
             */
            void PushTimeDataSample(const char * registerName, const AZ::Debug::ProfilerRegister::TimeData& timeData);

        protected:
            ///We store here the previous value from the previous timer frame data.
            ///This is necessary because AZ_PROFILER_TIMER is cumulative
            ///and we need the time spent for each call.
            AZStd::vector<AZ::Debug::ProfilerRegister::TimeData> m_previousTimeData;
        };
    } //namespace Statistics
} //namespace AzFramework
