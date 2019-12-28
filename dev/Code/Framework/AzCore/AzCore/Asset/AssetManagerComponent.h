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
#ifndef AZCORE_ASSETDATABASE_COMPONENT_H
#define AZCORE_ASSETDATABASE_COMPONENT_H

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/Crc.h>

namespace AZ
{
    /**
     *
     */
    class AssetManagerComponent
        : public Component
        , public SystemTickBus::Handler
    {
    public:
        AZ_COMPONENT(AssetManagerComponent, "{D5A73BCC-0098-4d1e-8FE4-C86101E374AC}", Component)

        AssetManagerComponent();

    protected:
        //////////////////////////////////////////////////////////////////////////
        // Component base
        void Activate() override;
        void Deactivate() override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // SystemTickBus
        void    OnSystemTick() override;
        //////////////////////////////////////////////////////////////////////////

        /// \ref ComponentDescriptor::GetProvidedServices
        static void GetProvidedServices(ComponentDescriptor::DependencyArrayType& provided);
        /// \ref ComponentDescriptor::GetIncompatibleServices
        static void GetIncompatibleServices(ComponentDescriptor::DependencyArrayType& incompatible);
        /// \ref ComponentDescriptor::GetDependentServices
        static void GetDependentServices(ComponentDescriptor::DependencyArrayType& dependent);
        /// \ref ComponentDescriptor::Reflect
        static void Reflect(ReflectContext* reflection);
    };
}

#endif // AZCORE_ASSETDATABASE_COMPONENT_H
#pragma once
