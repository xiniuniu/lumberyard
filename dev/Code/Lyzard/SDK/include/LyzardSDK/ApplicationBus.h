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
#include <AzCore/std/string/string.h>

namespace Lyzard
{
    class ApplicationNotifications
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////

        /// Called when the application is preparing to shutdown
        virtual void OnApplicationClosing() { }

        /**
         * Called when the application has prepared for shutdown, and is ensuring all work is complete.
         * Only called by the GUI application, all CLI work is expected to have blocked until return.
         * Return false to buy more time.
         */
        virtual bool IsReadyForShutdown() { return true; }
    };
    using ApplicationNotificationBus = AZ::EBus<ApplicationNotifications>;
}