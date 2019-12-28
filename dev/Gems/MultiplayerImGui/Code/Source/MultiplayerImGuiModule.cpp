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


#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Module/Module.h>

#include <MultiplayerImGuiSystemComponent.h>

namespace MultiplayerDiagnostics
{
    class MultiplayerImGuiModule
        : public AZ::Module
    {
    public:
        AZ_RTTI(MultiplayerImGuiModule, "{5A589AC7-2B74-4030-B1A7-4B2D8E4BAC8E}", AZ::Module);
        AZ_CLASS_ALLOCATOR(MultiplayerImGuiModule, AZ::SystemAllocator, 0);

        MultiplayerImGuiModule()
            : AZ::Module()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            m_descriptors.insert(m_descriptors.end(), {
                MultiplayerImGuiSystemComponent::CreateDescriptor(),
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList{
                azrtti_typeid<MultiplayerImGuiSystemComponent>(),
            };
        }
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(MultiplayerImGui_1ecafbbec4e1406297e56a09fd73646e, MultiplayerDiagnostics::MultiplayerImGuiModule)
