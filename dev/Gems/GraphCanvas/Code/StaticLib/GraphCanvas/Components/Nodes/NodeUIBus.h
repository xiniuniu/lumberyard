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

#include <AzCore/Component/EntityId.h>
#include <AzCore/EBus/EBus.h>

namespace GraphCanvas
{
    //! NodeUIRequests
    //! Requests involving the visual state/behavior of a node.
	//! Generally independent of any sort of logical underpinning and more related to the UX
	//! of the node.
    class NodeUIRequests
        : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AZ::EntityId;

        virtual void AdjustSize() = 0;

        virtual void SetSnapToGrid(bool enabled) = 0;
        virtual void SetResizeToGrid(bool enabled) = 0;
        virtual void SetGrid(AZ::EntityId gridId) = 0;

        virtual qreal GetCornerRadius() const = 0;
    };

    using NodeUIRequestBus = AZ::EBus<NodeUIRequests>;
}