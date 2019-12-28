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

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Math/Aabb.h>

namespace LmbrCentral
{
    /*!
     * RenderBoundsRequests
     * Messages serviced by components that require custom bounds calculation.
     */
    class RenderBoundsRequests
        : public AZ::ComponentBus
    {
    public:
        /**
        * Returns the axis aligned bounding box in world coordinates
        */
        virtual AZ::Aabb GetWorldBounds() = 0;

        /**
        * Returns the axis aligned bounding box in model coordinates
        */
        virtual AZ::Aabb GetLocalBounds() = 0;
    };

    using RenderBoundsRequestBus = AZ::EBus<RenderBoundsRequests>;

    /*!
     * RenderBoundsNotifications
     * Events dispatched by components with custom bounds.
     */
    class RenderBoundsNotifications
        : public AZ::ComponentBus
    {
    public:

        /**
         * Notifies listeners that bounds were reset.
         */
        virtual void OnRenderBoundsReset() {}
    };

    using RenderBoundsNotificationBus = AZ::EBus<RenderBoundsNotifications>;
} // namespace LmbrCentral
