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

#include "CloudGemFramework_precompiled.h"
#include <platform_impl.h>

#include <CloudGemFrameworkModule.h>

#include "CloudGemFrameworkSystemComponent.h"

#include <CloudGemFramework/HttpClientComponent.h>
#include <MappingsComponent.h>
#include <PlayerIdentityComponent.h>

namespace CloudGemFramework
{
    CloudGemFrameworkModule::CloudGemFrameworkModule()
        : AZ::Module()
    {
        // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
        m_descriptors.insert(m_descriptors.end(), {
            CloudGemFrameworkSystemComponent::CreateDescriptor(),
            HttpClientComponent::CreateDescriptor(),
            CloudCanvasMappingsComponent::CreateDescriptor(),
            CloudCanvasPlayerIdentityComponent::CreateDescriptor(),
        });
    }

    /**
        * Add required SystemComponents to the SystemEntity.
        */
    AZ::ComponentTypeList CloudGemFrameworkModule::GetRequiredSystemComponents() const 
    {
        return AZ::ComponentTypeList{
            azrtti_typeid<CloudGemFrameworkSystemComponent>(),
            azrtti_typeid<CloudCanvasMappingsComponent>(),
            azrtti_typeid<CloudCanvasPlayerIdentityComponent>(),
        };
    }
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
#if !defined(CLOUD_GEM_FRAMEWORK_EDITOR)
AZ_DECLARE_MODULE_CLASS(CloudGemFramework_6fc787a982184217a5a553ca24676cfa, CloudGemFramework::CloudGemFrameworkModule)
#endif
