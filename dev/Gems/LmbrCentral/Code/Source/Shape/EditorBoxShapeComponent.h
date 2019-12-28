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

#include "BoxShape.h"
#include "EditorBaseShapeComponent.h"

#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <AzToolsFramework/ComponentMode/ComponentModeDelegate.h>
#include <AzToolsFramework/Manipulators/BoxManipulatorRequestBus.h>


namespace LmbrCentral
{
    /// Editor representation of Box Shape Component.
    class EditorBoxShapeComponent
        : public EditorBaseShapeComponent
        , private AzFramework::EntityDebugDisplayEventBus::Handler
        , private AzToolsFramework::BoxManipulatorRequestBus::Handler
    {
    public:
        AZ_EDITOR_COMPONENT(EditorBoxShapeComponent, EditorBoxShapeComponentTypeId, EditorBaseShapeComponent);
        static void Reflect(AZ::ReflectContext* context);

        EditorBoxShapeComponent() = default;

        // AZ::Component
        void Init() override;
        void Activate() override;
        void Deactivate() override;        

    protected:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            EditorBaseShapeComponent::GetProvidedServices(provided);
            provided.push_back(AZ_CRC("BoxShapeService", 0x946a0032));
        }

        // EditorComponentBase
        void BuildGameEntity(AZ::Entity* gameEntity) override;

        // AZ::TransformNotificationBus::Handler
        void OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world) override;

    private:
        AZ_DISABLE_COPY_MOVE(EditorBoxShapeComponent)

        // AzFramework::EntityDebugDisplayEventBus
        void DisplayEntityViewport(
            const AzFramework::ViewportInfo& viewportInfo,
            AzFramework::DebugDisplayRequests& debugDisplay) override;

        // AzToolsFramework::BoxManipulatorRequestBus
        AZ::Vector3 GetDimensions() override;
        void SetDimensions(const AZ::Vector3& dimensions) override;
        AZ::Transform GetCurrentTransform() override;
        AZ::Vector3 GetBoxScale() override;

        void ConfigurationChanged();        

        BoxShape m_boxShape; ///< Stores underlying box representation for this component.
        
        using ComponentModeDelegate = AzToolsFramework::ComponentModeFramework::ComponentModeDelegate;
        ComponentModeDelegate m_componentModeDelegate; /**< Responsible for detecting ComponentMode activation
                                                         *  and creating a concrete ComponentMode.*/
    };
} // namespace LmbrCentral