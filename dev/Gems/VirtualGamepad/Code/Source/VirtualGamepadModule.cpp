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

#include "VirtualGamepad_precompiled.h"
#include <platform_impl.h>

#include <AzCore/Memory/SystemAllocator.h>

#include "VirtualGamepadButtonComponent.h"
#include "VirtualGamepadSystemComponent.h"
#include "VirtualGamepadThumbStickComponent.h"

#include <IGem.h>

namespace VirtualGamepad
{
    class VirtualGamepadModule
        : public CryHooksModule
    {
    public:
        AZ_RTTI(VirtualGamepadModule, "{0454CF83-A35E-443B-A9BE-858EBE9C908F}", CryHooksModule);
        AZ_CLASS_ALLOCATOR(VirtualGamepadModule, AZ::SystemAllocator, 0);

        VirtualGamepadModule()
            : CryHooksModule()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            m_descriptors.insert(m_descriptors.end(), {
                VirtualGamepadSystemComponent::CreateDescriptor(),
                VirtualGamepadButtonComponent::CreateDescriptor(),
                VirtualGamepadThumbStickComponent::CreateDescriptor(),
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList{
                azrtti_typeid<VirtualGamepadSystemComponent>(),
            };
        }
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(VirtualGamepad_8aa9a66015804d6d9c809ce306e7d913, VirtualGamepad::VirtualGamepadModule)
