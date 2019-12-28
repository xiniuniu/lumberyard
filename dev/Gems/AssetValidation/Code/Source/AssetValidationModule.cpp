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

#include <AssetValidationSystemComponent.h>

#ifdef EDITOR_MODULE
#include <Editor/Source/EditorAssetValidationSystemComponent.h>
#endif

namespace AssetValidation
{
    class AssetValidationModule
        : public AZ::Module
    {
    public:
        AZ_RTTI(AssetValidationModule, "{66A6C65D-7814-4CFF-AF54-B73925FD1188}", AZ::Module);
        AZ_CLASS_ALLOCATOR(AssetValidationModule, AZ::SystemAllocator, 0);

        AssetValidationModule()
            : AZ::Module()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            m_descriptors.insert(m_descriptors.end(), {
                AssetValidationSystemComponent::CreateDescriptor(),
#ifdef EDITOR_MODULE
                EditorAssetValidationSystemComponent::CreateDescriptor(),
#endif

            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList{
                azrtti_typeid<AssetValidationSystemComponent>(),
#ifdef EDITOR_MODULE
                azrtti_typeid<EditorAssetValidationSystemComponent>(),
#endif
            };
        }
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(AssetValidation_5a5c3c10b91d4b4ea8baef474c5b5d49, AssetValidation::AssetValidationModule)
