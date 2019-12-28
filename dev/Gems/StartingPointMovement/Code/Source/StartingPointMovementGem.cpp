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

#include "StartingPointMovement_precompiled.h"
#include <platform_impl.h>

#include <AzCore/Module/Module.h>

#include <AzFramework/Metrics/MetricsPlainTextNameRegistration.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace StartingPointMovement
{

    // Dummy component to get reflect function
    class StartingPointMovementDummyComponent
        : public AZ::Component
    {
    public:
        AZ_COMPONENT(StartingPointMovementDummyComponent, "{6C9DA3DD-A0B3-4DCB-B77B-E93C4AF89134}");

        static void Reflect(AZ::ReflectContext* context)
        {
            if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
            {
                serializeContext->ClassDeprecate("Event Action Bindings", "{2BB79CFC-7EBC-4EF4-A62E-5D64CB45CDBD}");

                serializeContext->Class<StartingPointMovementDummyComponent, AZ::Component>()
                    ->Version(0)
                    ;
            }
        }

        void Activate() override { }
        void Deactivate() override { }
    };

    class StartingPointMovementModule
        : public AZ::Module
    {
    public:
        AZ_RTTI(StartingPointMovementModule, "{AE406AE3-77AE-4CA6-84AD-842224EE2188}", AZ::Module);

        StartingPointMovementModule()
            : AZ::Module()
        {
            m_descriptors.insert(m_descriptors.end(), {
                StartingPointMovementDummyComponent::CreateDescriptor(),
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
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(StartingPointMovement_73d8779dc28a4123b7c9ed76217464af, StartingPointMovement::StartingPointMovementModule)
