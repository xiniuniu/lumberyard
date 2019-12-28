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

#include "Microphone_precompiled.h"
#include <platform_impl.h>

#include "MicrophoneSystemComponent.h"

#include <IGem.h>

namespace Audio
{
    class MicrophoneModule
        : public CryHooksModule
    {
    public:
        AZ_RTTI(MicrophoneModule, "{99939704-566A-42B1-8A7A-567E01C63D9C}", CryHooksModule);

        MicrophoneModule()
            : CryHooksModule()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            m_descriptors.insert(m_descriptors.end(), { MicrophoneSystemComponent::CreateDescriptor() });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList {
                azrtti_typeid<MicrophoneSystemComponent>(),
            };
        }
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(Microphone_e70dd59f02f14ea49e6b38434e86ebf1, Audio::MicrophoneModule)
