/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates, or 
* a third party where indicated.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,  
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  
*
*/

#include "StdAfx.h"
#include <platform_impl.h>

#include <AzCore/Memory/SystemAllocator.h>

#include "CloudGemMetricSystemComponent.h"

#include <AzCore/Module/Module.h>

namespace CloudGemMetric
{
    class CloudGemMetricModule
        : public AZ::Module
    {
    public:
        AZ_RTTI(CloudGemMetricModule, "{EF973D14-A8B5-44E3-A630-8792BC94F638}", AZ::Module);
        AZ_CLASS_ALLOCATOR(CloudGemMetricModule, AZ::SystemAllocator, 0);

        CloudGemMetricModule()
            : AZ::Module()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            m_descriptors.insert(m_descriptors.end(), {
                CloudGemMetricSystemComponent::CreateDescriptor(),
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList{
                azrtti_typeid<CloudGemMetricSystemComponent>(),
            };
        }
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(CloudGemMetric_e1bf83aa0ddc49418c6b5689c111cb26, CloudGemMetric::CloudGemMetricModule)
