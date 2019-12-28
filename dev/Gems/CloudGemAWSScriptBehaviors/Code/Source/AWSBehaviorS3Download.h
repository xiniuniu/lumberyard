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

#include "AWSBehaviorBase.h"

#include <CloudGemAWSScriptBehaviors_precompiled.h>
#include <AzCore/EBus/EBus.h>

namespace CloudGemAWSScriptBehaviors
{
    class AWSS3DownloadBehaviorNotifications : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        virtual void OnSuccess(const AZStd::string& resultBody) = 0;
        virtual void OnError(const AZStd::string& errorBody) = 0;
    };

    using AWSBehaviorS3DownloadNotificationsBus = AZ::EBus<AWSS3DownloadBehaviorNotifications>;

    class AWSBehaviorS3DownloadNotificationsBusHandler
        : public AWSBehaviorS3DownloadNotificationsBus::Handler
        , public AZ::BehaviorEBusHandler
    {
    public:
        AZ_EBUS_BEHAVIOR_BINDER(AWSBehaviorS3DownloadNotificationsBusHandler
            , "{CB7E8710-F256-48A6-BC03-D3E3001AEB1E}"
            , AZ::SystemAllocator
            , OnSuccess
            , OnError
        );

        void OnSuccess(const AZStd::string& resultBody) override
        {
            Call(FN_OnSuccess, resultBody);
        }

        void OnError(const AZStd::string& errorBody) override
        {
            Call(FN_OnError, errorBody);
        }
    };


    class AWSBehaviorS3Download : public AWSBehaviorBase
    {
    public:
        AWSBEHAVIOR_DEFINITION(AWSBehaviorS3Download, "{7F4E956C-7463-4236-B320-C992D36A9C6E}")

    private:
        AZStd::string m_bucketName;
        AZStd::string m_keyName;
        AZStd::string m_localFileName;

        void Download();
    };
}
