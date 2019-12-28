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

// AZCORE_ENABLE_MEMORY_TRACKING is enabled through _WAF_\user_settings.options (or show_option_dialog) and option "enable_memory_tracking"

#if defined(AZCORE_ENABLE_MEMORY_TRACKING)
    #define AZ_MEMORY_PROFILE(...) (__VA_ARGS__)
#else
    #define AZ_MEMORY_PROFILE(...)
#endif
