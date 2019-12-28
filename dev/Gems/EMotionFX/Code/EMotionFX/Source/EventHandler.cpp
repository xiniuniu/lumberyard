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

// include required headers
#include "EventHandler.h"
#include <EMotionFX/Source/Allocators.h>

namespace EMotionFX
{
    AZ_CLASS_ALLOCATOR_IMPL(EventHandler, EventHandlerAllocator, 0)
    AZ_CLASS_ALLOCATOR_IMPL(AnimGraphInstanceEventHandler, AnimGraphEventHandlerAllocator, 0)
    AZ_CLASS_ALLOCATOR_IMPL(MotionInstanceEventHandler, MotionEventHandlerAllocator, 0)

}
