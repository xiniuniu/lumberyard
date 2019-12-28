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
#include "Maestro_precompiled.h"
#include <platform_impl.h>

#include "Components/SequenceComponent.h"
#include "Components/SequenceAgentComponent.h"
#if defined(MAESTRO_EDITOR)
#include "Components/EditorSequenceComponent.h"
#include "Components/EditorSequenceAgentComponent.h"
#endif
#include "MaestroSystemComponent.h"

#include <IGem.h>

namespace Maestro
{
    class MaestroModule
        : public CryHooksModule
    {
    public:
        AZ_RTTI(MaestroModule, "{ED1C74E6-BB73-4AC5-BD4B-91EFB400BAF4}", CryHooksModule);

        MaestroModule()
            : CryHooksModule()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            m_descriptors.insert(m_descriptors.end(), {
                MaestroAllocatorComponent::CreateDescriptor(),
                MaestroSystemComponent::CreateDescriptor(),
                SequenceComponent::CreateDescriptor(),
                SequenceAgentComponent::CreateDescriptor(),
#if defined(MAESTRO_EDITOR)
                EditorSequenceComponent::CreateDescriptor(),
                EditorSequenceAgentComponent::CreateDescriptor(),
#endif
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList{
                azrtti_typeid<MaestroAllocatorComponent>(),
                azrtti_typeid<MaestroSystemComponent>()
            };
        }
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(Maestro_3b9a978ed6f742a1acb99f74379a342c, Maestro::MaestroModule)
