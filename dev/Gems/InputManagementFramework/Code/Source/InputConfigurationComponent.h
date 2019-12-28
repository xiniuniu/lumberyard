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

#include <AzCore/Component/Component.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzFramework/Components/EditorEntityEvents.h>

// InputManagementFramework Gem includes
#include "Include/InputManagementFramework/InputEventBindings.h"

//CryCommon
#include <PlayerProfileNotificationBus.h>
#include <InputNotificationBus.h>

namespace AZ
{
    class SerializeContext;
}

namespace Input
{
    class InputConfigurationComponent
        : public AZ::Component
        , private AZ::Data::AssetBus::Handler
        , private AZ::InputComponentRequestBus::Handler
        , private AZ::PlayerProfileNotificationBus::Handler
        , private AZ::InputContextNotificationBus::MultiHandler
        , public AzFramework::EditorEntityEvents
    {
    public:

        AZ_COMPONENT(InputConfigurationComponent, "{3106EE2A-4816-433E-B855-D17A6484D5EC}", AzFramework::EditorEntityEvents);
        virtual ~InputConfigurationComponent();
        InputConfigurationComponent() = default;
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void Reflect(AZ::ReflectContext* reflection);

        //////////////////////////////////////////////////////////////////////////
        // AZ::Component
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        /// AzFramework::EditorEntityEvents
        void EditorSetPrimaryAsset(const AZ::Data::AssetId& assetId) override;
        //////////////////////////////////////////////////////////////////////////
    private:
        InputConfigurationComponent(const InputConfigurationComponent&) = delete;

        //////////////////////////////////////////////////////////////////////////
        // AZ::InputComponentRequestBus::Handler
        void SetLocalUserId(AzFramework::LocalUserId localUserId) override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // AZ::PlayerProfileNotificationBus::Handler
        void OnProfileSaving() override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // AZ::InputContextNotificationBus::MultiHandler
        void OnInputContextActivated() override;
        void OnInputContextDeactivated() override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        /// AZ::Data::AssetBus::Handler
        void OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset) override;
        void OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset) override;
        //////////////////////////////////////////////////////////////////////////

        void ActivateBindingsIfAppropriate();

        //////////////////////////////////////////////////////////////////////////
        // Reflected Data
        InputEventBindings m_inputEventBindings;
        AZStd::vector<AZStd::string> m_inputContexts;
        AZ::Data::Asset<InputEventBindingsAsset> m_inputEventBindingsAsset;
        AzFramework::LocalUserId m_localUserId = AzFramework::LocalUserIdAny;

        bool m_isContextActive = false;

        // Unlike the definition of most assets, the input asset requires additional preparation after its loaded
        // in order to actually be prepared to be used.
        bool m_isAssetPrepared = false;
    };
} // namespace Input
