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

#include "SplineComponent.h"

#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <AzToolsFramework/API/ComponentEntitySelectionBus.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/ComponentMode/ComponentModeDelegate.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>
#include <LmbrCentral/Shape/SplineComponentBus.h>

namespace LmbrCentral
{
    /// Editor representation of SplineComponent.
    class EditorSplineComponent
        : public AzToolsFramework::Components::EditorComponentBase
        , private AzToolsFramework::EditorComponentSelectionRequestsBus::Handler
        , private AzToolsFramework::EditorComponentSelectionNotificationsBus::Handler
        , private AzFramework::EntityDebugDisplayEventBus::Handler
        , private AZ::TransformNotificationBus::Handler
        , private SplineComponentRequestBus::Handler
        , private AZ::FixedVerticesRequestBus<AZ::Vector3>::Handler
        , private AZ::VariableVerticesRequestBus<AZ::Vector3>::Handler
    {
    public:
        AZ_EDITOR_COMPONENT(EditorSplineComponent, "{5B29D788-4885-4D56-BD9B-C0C45BE08EC1}", EditorComponentBase);
        static void Reflect(AZ::ReflectContext* context);

        EditorSplineComponent() = default;

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // EditorComponentBase
        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        AZ_DISABLE_COPY_MOVE(EditorSplineComponent)

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        // EditorComponentSelectionRequestsBus
        AZ::Aabb GetEditorSelectionBoundsViewport(
            const AzFramework::ViewportInfo& viewportInfo) override;
        bool EditorSelectionIntersectRayViewport(
            const AzFramework::ViewportInfo& viewportInfo,
            const AZ::Vector3& src, const AZ::Vector3& dir, AZ::VectorFloat& distance) override;
        bool SupportsEditorRayIntersect() override { return true; };
        AZ::u32 GetBoundingBoxDisplayType() override { return NoBoundingBox; }

        // EditorComponentSelectionNotificationsBus::Handler
        void OnAccentTypeChanged(AzToolsFramework::EntityAccentType accent) override { m_accentType = accent; }

        // TransformNotificationBus
        void OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world) override;

        // SplineComponentRequestBus handler
        AZ::SplinePtr GetSpline() override;
        void ChangeSplineType(AZ::u64 splineType) override;
        void SetClosed(bool closed) override;

        // SplineComponentRequestBus/VertexContainerInterface
        bool GetVertex(size_t index, AZ::Vector3& vertex) const override;
        void AddVertex(const AZ::Vector3& vertex) override;
        bool UpdateVertex(size_t index, const AZ::Vector3& vertex) override;
        bool InsertVertex(size_t index, const AZ::Vector3& vertex) override;
        bool RemoveVertex(size_t index) override;
        void SetVertices(const AZStd::vector<AZ::Vector3>& vertices) override;
        void ClearVertices() override;
        size_t Size() const override;
        bool Empty() const override;

        // AzFramework::EntityDebugDisplayEventBus
        void DisplayEntityViewport(
            const AzFramework::ViewportInfo& viewportInfo,
            AzFramework::DebugDisplayRequests& debugDisplay) override;

        void OnChangeSplineType();
        void SplineChanged() const;

        using ComponentModeDelegate = AzToolsFramework::ComponentModeFramework::ComponentModeDelegate;
        ComponentModeDelegate m_componentModeDelegate; ///< Responsible for detecting ComponentMode activation 
                                                       ///< and creating a concrete ComponentMode.

        SplineCommon m_splineCommon; ///< Stores common spline functionality and properties.

        AZ::Transform m_cachedUniformScaleTransform; ///< Stores the current transform of the component with uniform scale.

        AzToolsFramework::EntityAccentType m_accentType = AzToolsFramework::EntityAccentType::None; ///< State of the entity selection in the viewport.
        bool m_visibleInEditor = true; ///< Visible in the editor viewport.
    };
} // namespace LmbrCentral