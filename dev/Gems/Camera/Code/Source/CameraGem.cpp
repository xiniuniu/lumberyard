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
#include "Camera_precompiled.h"
#include <platform_impl.h>

#include <AzCore/Module/Module.h>
#include <IGem.h>

#include "CameraComponent.h"

#if defined(CAMERA_EDITOR)
#include "CameraEditorSystemComponent.h"
#include "EditorCameraComponent.h"
#endif // CAMERA_EDITOR

#include <AzFramework/Metrics/MetricsPlainTextNameRegistration.h>

namespace Camera
{
    class CameraModule
        : public CryHooksModule
    {
    public:
        AZ_RTTI(CameraModule, "{C2E72B0D-BCEF-452A-9BFA-03833015258C}", AZ::Module);

        CameraModule()
            : CryHooksModule()
        {
            m_descriptors.insert(m_descriptors.end(), {
                Camera::CameraComponent::CreateDescriptor(),

#if defined(CAMERA_EDITOR)
                CameraEditorSystemComponent::CreateDescriptor(),
                EditorCameraComponent::CreateDescriptor(),
#endif // CAMERA_EDITOR
            });




            // This is an internal Amazon gem, so register it's components for metrics tracking, otherwise the name of the component won't get sent back.
            // IF YOU ARE A THIRDPARTY WRITING A GEM, DO NOT REGISTER YOUR COMPONENTS WITH EditorMetricsComponentRegistrationBus
            AZStd::vector<AZ::Uuid> typeIds;
            typeIds.reserve(m_descriptors.size());
            for (AZ::ComponentDescriptor* descriptor : m_descriptors)
            {
                typeIds.emplace_back(descriptor->GetUuid());
            }
            EBUS_EVENT(AzFramework::MetricsPlainTextNameRegistrationBus, RegisterForNameSending, typeIds);
        }

        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList {
#if defined(CAMERA_EDITOR)
                azrtti_typeid<CameraEditorSystemComponent>(),
#endif // CAMERA_EDITOR
            };
        }
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(Camera_f910686b6725452fbfc4671f95f733c6, Camera::CameraModule)
