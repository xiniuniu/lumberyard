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
#include "PhysXDebug_precompiled.h"
#include <platform_impl.h>
#include <AzCore/Memory/SystemAllocator.h>
#include "SystemComponent.h"

#ifdef PHYSXDEBUG_GEM_EDITOR
#include "EditorSystemComponent.h"
#endif
#include <IGem.h>

namespace PhysXDebug
{
    class PhysXDebugModule
        : public CryHooksModule
    {
    public:
        AZ_RTTI(PhysXDebugModule, "{7C9CB91D-D7D7-4362-9FE8-E4D61B6A5113}", CryHooksModule);
        AZ_CLASS_ALLOCATOR(PhysXDebugModule, AZ::SystemAllocator, 0);

        PhysXDebugModule()
            : CryHooksModule()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            m_descriptors.insert(m_descriptors.end(), {
                SystemComponent::CreateDescriptor(),
#ifdef PHYSXDEBUG_GEM_EDITOR
                EditorSystemComponent::CreateDescriptor()
#endif
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList{
                azrtti_typeid<SystemComponent>(),
#ifdef PHYSXDEBUG_GEM_EDITOR
                azrtti_typeid<EditorSystemComponent>(),
#endif
            };
        }
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(PhysXDebug_516145e2d9904b13813f1b54605e26a6, PhysXDebug::PhysXDebugModule)
