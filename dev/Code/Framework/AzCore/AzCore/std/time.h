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
#ifndef AZSTD_SYSTEM_TIME_H
#define AZSTD_SYSTEM_TIME_H

#include <AzCore/std/base.h>

namespace AZStd
{
    AZStd::sys_time_t GetTimeTicksPerSecond();
    AZStd::sys_time_t GetTimeNowTicks();
    AZStd::sys_time_t GetTimeNowMicroSecond();
    AZStd::sys_time_t GetTimeNowSecond();
    // return time in millisecond since 1970/01/01 00:00:00 UTC.
    AZ::u64           GetTimeUTCMilliSecond();
}

#endif // AZSTD_SYSTEM_TIME_H
#pragma once
