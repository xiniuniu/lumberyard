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

#include "FastNoise_precompiled.h"

#if !defined(AZ_MONOLITHIC_BUILD)
#include <platform_impl.h> // must be included once per DLL so things from CryCommon will function
#endif

#include <FastNoiseModule.h>
#include <FastNoiseSystemComponent.h>
#include <FastNoiseGradientComponent.h>

namespace FastNoiseGem
{
    FastNoiseModule::FastNoiseModule()
    {
        m_descriptors.insert(m_descriptors.end(), {
            FastNoiseSystemComponent::CreateDescriptor(),
            FastNoiseGradientComponent::CreateDescriptor(),
        });
    }

    AZ::ComponentTypeList FastNoiseModule::GetRequiredSystemComponents() const
    {
        return AZ::ComponentTypeList{
            azrtti_typeid<FastNoiseSystemComponent>(),
        };
    }
}

#if !defined(FASTNOISE_EDITOR)
// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(FastNoise_c5f23032407f49ca8d8de1733423565c, FastNoiseGem::FastNoiseModule)
#endif