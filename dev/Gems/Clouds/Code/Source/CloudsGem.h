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

#include <AzCore/Module/Module.h>
#include <CrySystemBus.h>

/*!
* \namespace CloudsGem
*/
namespace CloudsGem
{
    /*! The CloudsGem module class coordinates with the application to reflect classes and create system components. */
    class CloudsGemModule
        : public AZ::Module
    {
    public:
        AZ_RTTI(CloudsGemModule, "{4f293a6f-11ab-4cd7-b9db-3017d12b5eda}", Module);

        CloudsGemModule();
        ~CloudsGemModule() override = default;
        AZ::ComponentTypeList GetRequiredSystemComponents() const override;
    };
} // namespace LmbrCentral