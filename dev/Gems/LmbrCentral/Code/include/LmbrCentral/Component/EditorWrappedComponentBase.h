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

#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>
#include <AzToolsFramework/ToolsComponents/EditorVisibilityBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>

namespace LmbrCentral
{
    //! A base class for editor components that need to wrap runtime components, use a configuration object, and respond to visibility toggling
    template<typename TComponent, typename TConfiguration>
    class EditorWrappedComponentBase
        : public AzToolsFramework::Components::EditorComponentBase
        , protected AzToolsFramework::EditorVisibilityNotificationBus::Handler
    {
    public:
        using WrappedComponentType = TComponent;
        using WrappedConfigType = TConfiguration;

        AZ_RTTI((EditorWrappedComponentBase, "{059BC2AF-B086-4D5E-8F6C-2827AB69ED16}", TComponent, TConfiguration), EditorComponentBase);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& services);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& services);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& services);

        //////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        void BuildGameEntity(AZ::Entity* gameEntity) override;

        //////////////////////////////////////////////////////////////////////////
        // AzToolsFramework::EditorVisibilityNotificationBus
        void OnEntityVisibilityChanged(bool visibility) override;

    protected:
        template<typename TDerivedClass, typename TBaseClass>
        static void ReflectSubClass(AZ::ReflectContext* context, unsigned int version = 0, AZ::SerializeContext::VersionConverter versionConverter = nullptr);

        static void Reflect(AZ::ReflectContext* context);

        virtual AZ::u32 ConfigurationChanged();
        
        TComponent m_component;
        TConfiguration m_configuration;
        bool m_visible = true;
    };

} // namespace LmbrCentral

#include "EditorWrappedComponentBase.inl"