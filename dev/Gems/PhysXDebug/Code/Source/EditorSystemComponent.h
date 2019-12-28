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
#include <AzToolsFramework/Entity/EditorEntityContextBus.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>
#include <PhysX/ConfigurationBus.h>
#include <IEditor.h>

namespace PhysXDebug
{
    class EditorSystemComponent
        : public AzToolsFramework::Components::EditorComponentBase
        , protected AzToolsFramework::EditorEntityContextNotificationBus::Handler
        , public CrySystemEventBus::Handler
        , private AzToolsFramework::EditorEvents::Bus::Handler
        , private IEditorNotifyListener
        , private PhysX::ConfigurationNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(EditorSystemComponent, "{E6F88D74-5758-453E-8FE0-2FB5E5E53890}");

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("PhysXDebugEditorService", 0xe3dde7d8));
        }

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("PhysXService", 0x75beae2d));
        }

    protected:
        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // CrySystemEvents
        void OnCrySystemShutdown(ISystem&) override;

        // AzToolsFramework::EditorEvents
        void NotifyRegisterViews() override;
    private:
        // EditorEntityContextNotificationBus interface implementation
        void OnStartPlayInEditorBegin() override;
        void OnStopPlayInEditor() override;

        // IEditorNotifyListener interface implementation
        void OnEditorNotifyEvent(EEditorNotifyEvent event) override;

        // PhysX::ConfigurationNotificationBus
        void OnConfigurationRefreshed(const PhysX::Configuration& configuration) override;

        /// Initially connect to the PhysX Visualization debugger based on the current PhysX configuration.
        void AutoConnectPVD();

        /// Register for Cry Editor events.
        void RegisterForEditorEvents();

        /// Unregister for Cry Editor events.
        void UnregisterForEditorEvents();

    };
}
