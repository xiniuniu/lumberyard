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

#include "HttpRequestor_precompiled.h"
#include "HttpRequestorSystemComponent.h"
#include <AzCore/Module/Module.h>

namespace HttpRequestor
{
    class HttpRequestorModule
        : public AZ::Module
    {
    public:
        AZ_RTTI(HttpRequestorModule, "{FD411E40-AF83-4F6B-A5A3-F59AB71150BF}", AZ::Module);

        HttpRequestorModule()
            : AZ::Module()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            m_descriptors.insert(m_descriptors.end(), {
                HttpRequestorSystemComponent::CreateDescriptor(),
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList{
                azrtti_typeid<HttpRequestorSystemComponent>(),
            };
        }
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(HttpRequestor_28479e255bde466e91fc34eec808d9c7, HttpRequestor::HttpRequestorModule)
