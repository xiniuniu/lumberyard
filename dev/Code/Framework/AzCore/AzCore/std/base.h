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

#include <AzCore/base.h>
#include <AzCore/std/config.h>

#if defined(AZ_HAS_INITIALIZERS_LIST) // Do we need this at a root level?
#   include <initializer_list>
#endif


#define AZSTD_PRINTF printf

namespace AZStd
{
    using ::size_t;
    using ::ptrdiff_t;

    using std::initializer_list;

    using std::nullptr_t;

    using sys_time_t = AZ::s64;
}
