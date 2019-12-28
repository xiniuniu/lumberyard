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

#include "StdAfx.h"
#include <platform_impl.h>

#include "DebugDrawSystemComponent.h"
#include "DebugDrawLineComponent.h"
#include "DebugDrawRayComponent.h"
#include "DebugDrawSphereComponent.h"
#include "DebugDrawObbComponent.h"
#include "DebugDrawTextComponent.h"

#ifdef DEBUGDRAW_GEM_EDITOR
#include "EditorDebugDrawLineComponent.h"
#include "EditorDebugDrawRayComponent.h"
#include "EditorDebugDrawSphereComponent.h"
#include "EditorDebugDrawObbComponent.h"
#include "EditorDebugDrawTextComponent.h"
#endif // DEBUGDRAW_GEM_EDITOR

#include <IGem.h>

namespace DebugDraw
{
    class DebugDrawModule
        : public CryHooksModule
    {
    public:
        AZ_RTTI(DebugDrawModule, "{07AC9E51-535C-402D-A2EB-529366ED9985}", CryHooksModule);

        DebugDrawModule()
            : CryHooksModule()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            m_descriptors.insert(m_descriptors.end(), {

                //DebugDrawSystemComponent::CreateDescriptor(),
                DebugDrawLineComponent::CreateDescriptor(),
                DebugDrawRayComponent::CreateDescriptor(),
                DebugDrawSphereComponent::CreateDescriptor(),
                DebugDrawObbComponent::CreateDescriptor(),
                DebugDrawTextComponent::CreateDescriptor(),
                DebugDrawSystemComponent::CreateDescriptor(),

                #ifdef DEBUGDRAW_GEM_EDITOR
                EditorDebugDrawLineComponent::CreateDescriptor(),
                EditorDebugDrawRayComponent::CreateDescriptor(),
                EditorDebugDrawSphereComponent::CreateDescriptor(),
                EditorDebugDrawObbComponent::CreateDescriptor(),
                EditorDebugDrawTextComponent::CreateDescriptor(),
                #endif // DEBUGDRAW_GEM_EDITOR
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList{
                azrtti_typeid<DebugDrawSystemComponent>(),
            };
        }
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(DebugDraw_66239f50bf754354b514c850c8b841fb, DebugDraw::DebugDrawModule)
