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

#include "SelectionManipulator.h"

#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <AzToolsFramework/Manipulators/ManipulatorView.h>

namespace AzToolsFramework
{
    AZStd::shared_ptr<SelectionManipulator> SelectionManipulator::MakeShared(const AZ::Transform& worldFromLocal)
    {
        return AZStd::shared_ptr<SelectionManipulator>(aznew SelectionManipulator(worldFromLocal));
    }

    SelectionManipulator::SelectionManipulator(const AZ::Transform& worldFromLocal)
        : m_worldFromLocal(worldFromLocal)
    {
        AttachLeftMouseDownImpl();
        AttachRightMouseDownImpl();
    }

    void SelectionManipulator::InstallLeftMouseDownCallback(const MouseActionCallback& onMouseDownCallback)
    {
        m_onLeftMouseDownCallback = onMouseDownCallback;
    }

    void SelectionManipulator::InstallLeftMouseUpCallback(const MouseActionCallback& onMouseUpCallback)
    {
        m_onLeftMouseUpCallback = onMouseUpCallback;
    }

    void SelectionManipulator::InstallRightMouseDownCallback(const MouseActionCallback& onMouseDownCallback)
    {
        m_onRightMouseDownCallback = onMouseDownCallback;
    }

    void SelectionManipulator::InstallRightMouseUpCallback(const MouseActionCallback& onMouseUpCallback)
    {
        m_onRightMouseUpCallback = onMouseUpCallback;
    }

    void SelectionManipulator::OnLeftMouseDownImpl(
        const ViewportInteraction::MouseInteraction& interaction, float /*rayIntersectionDistance*/)
    {
        if (m_onLeftMouseDownCallback)
        {
            m_onLeftMouseDownCallback(interaction);
        }
    }

    void SelectionManipulator::OnLeftMouseUpImpl(const ViewportInteraction::MouseInteraction& interaction)
    {
        if (MouseOver() && m_onLeftMouseUpCallback)
        {
            m_onLeftMouseUpCallback(interaction);
        }
    }

    void SelectionManipulator::OnRightMouseDownImpl(
        const ViewportInteraction::MouseInteraction& interaction, float /*rayIntersectionDistance*/)
    {
        if (m_onRightMouseDownCallback)
        {
            m_onRightMouseDownCallback(interaction);
        }
    }

    void SelectionManipulator::OnRightMouseUpImpl(const ViewportInteraction::MouseInteraction& interaction)
    {
        if (m_onRightMouseUpCallback)
        {
            m_onRightMouseUpCallback(interaction);
        }
    }

    void SelectionManipulator::Draw(
        const ManipulatorManagerState& managerState,
        AzFramework::DebugDisplayRequests& debugDisplay,
        const AzFramework::CameraState& cameraState,
        const ViewportInteraction::MouseInteraction& mouseInteraction)
    {
        m_manipulatorView->Draw(
            GetManipulatorManagerId(), managerState,
            GetManipulatorId(), {
                TransformUniformScale(m_worldFromLocal),
                m_position, MouseOver()
            },
            debugDisplay, cameraState, mouseInteraction);
    }

    void SelectionManipulator::SetBoundsDirtyImpl()
    {
        m_manipulatorView->SetBoundDirty(GetManipulatorManagerId());
    }

    void SelectionManipulator::InvalidateImpl()
    {
        m_manipulatorView->Invalidate(GetManipulatorManagerId());
    }

    void SelectionManipulator::SetView(AZStd::unique_ptr<ManipulatorView>&& view)
    {
        m_manipulatorView = AZStd::move(view);
    }
}
