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
#pragma once

#include <AzCore/EBus/EBus.h>
#include <AzCore/Component/EntityId.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>

namespace GradientSignal
{
    /**
     * Bus providing gradient sampler data
     */
    class EditorGradientSamplerRequests
        : public AZ::EBusTraits
    {
    public:
        ////////////////////////////////////////////////////////////////////////
        // EBusTraits
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AZ::EntityId;
        ////////////////////////////////////////////////////////////////////////

        virtual AzToolsFramework::EntityIdList GetInboundGradientIds() = 0;
        virtual void SetInboundGradientIds(AzToolsFramework::EntityIdList gradientIds) = 0;
    };

    using EditorGradientSamplerRequestBus = AZ::EBus<EditorGradientSamplerRequests>;

} // namespace GradientSignal
