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

#include "ScriptedEntityTweener_precompiled.h"
#include <platform_impl.h>

#include <IGem.h>

#include <ScriptedEntityTweener/ScriptedEntityTweenerBus.h>

#include "ScriptedEntityTweenerSystemComponent.h"

namespace ScriptedEntityTweener
{
    class ScriptedEntityTweenerModule
        : public CryHooksModule
    {
    public:
        AZ_RTTI(ScriptedEntityTweenerModule, "{A6A93611-5E4D-4EB5-BFB9-00031F73F59B}", CryHooksModule);

        ScriptedEntityTweenerModule()
            : CryHooksModule()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            m_descriptors.insert(m_descriptors.end(), {
                ScriptedEntityTweenerSystemComponent::CreateDescriptor(),
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList{
                azrtti_typeid<ScriptedEntityTweenerSystemComponent>(),
            };
        }

        void OnSystemEvent(ESystemEvent systemEvent, UINT_PTR wparam, UINT_PTR lparam)
        {
            CryHooksModule::OnSystemEvent(systemEvent, wparam, lparam);

            switch (systemEvent)
            {
            // In editor mode, ensure ending editor mode clears any in-flight animations
            case ESYSTEM_EVENT_GAME_MODE_SWITCH_END:
            {
                bool inGame = wparam == 1;
                if (!inGame)
                {
                    EBUS_EVENT(ScriptedEntityTweenerBus, Reset);
                }
            } break;
            default:
                break;
            }
        }
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(ScriptedEntityTweener_0d1f5f05559c4a99aaefd30633a0158e, ScriptedEntityTweener::ScriptedEntityTweenerModule)
