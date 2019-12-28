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
#include "StdAfx.h"
#include "EditorPreferencesPageViewportMovement.h"


void CEditorPreferencesPage_ViewportMovement::Reflect(AZ::SerializeContext& serialize)
{
    serialize.Class<CameraMovementSettings>()
        ->Version(1)
        ->Field("MoveSpeed", &CameraMovementSettings::m_moveSpeed)
        ->Field("RotateSpeed", &CameraMovementSettings::m_rotateSpeed)
        ->Field("FastMoveSpeed", &CameraMovementSettings::m_fastMoveSpeed)
        ->Field("WheelZoomSpeed", &CameraMovementSettings::m_wheelZoomSpeed)
        ->Field("InvertYAxis", &CameraMovementSettings::m_invertYRotation)
        ->Field("InvertPan", &CameraMovementSettings::m_invertPan);

    serialize.Class<CEditorPreferencesPage_ViewportMovement>()
        ->Version(1)
        ->Field("CameraMovementSettings", &CEditorPreferencesPage_ViewportMovement::m_cameraMovementSettings);


    AZ::EditContext* editContext = serialize.GetEditContext();
    if (editContext)
    {
        editContext->Class<CameraMovementSettings>("Camera Movement Settings", "")
            ->DataElement(AZ::Edit::UIHandlers::SpinBox, &CameraMovementSettings::m_moveSpeed, "Camera Movement Speed", "Camera Movement Speed")
            ->DataElement(AZ::Edit::UIHandlers::SpinBox, &CameraMovementSettings::m_rotateSpeed, "Camera Rotation Speed", "Camera Rotation Speed")
            ->DataElement(AZ::Edit::UIHandlers::SpinBox, &CameraMovementSettings::m_fastMoveSpeed, "Fast Movement Scale", "Fast Movement Scale (holding shift")
            ->DataElement(AZ::Edit::UIHandlers::SpinBox, &CameraMovementSettings::m_wheelZoomSpeed, "Wheel Zoom Speed", "Wheel Zoom Speed")
            ->DataElement(AZ::Edit::UIHandlers::CheckBox, &CameraMovementSettings::m_invertYRotation, "Invert Y Axis", "Invert Y Rotation (holding RMB)")
            ->DataElement(AZ::Edit::UIHandlers::CheckBox, &CameraMovementSettings::m_invertPan, "Invert Pan", "Invert Pan (holding MMB)");

        editContext->Class<CEditorPreferencesPage_ViewportMovement>("Gizmo Movement Preferences", "Gizmo Movement Preferences")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly", 0xef428f20))
            ->DataElement(AZ::Edit::UIHandlers::Default, &CEditorPreferencesPage_ViewportMovement::m_cameraMovementSettings, "Camera Movement Settings", "Camera Movement Settings");
    }
}


CEditorPreferencesPage_ViewportMovement::CEditorPreferencesPage_ViewportMovement()
{
    InitializeSettings();
}

void CEditorPreferencesPage_ViewportMovement::OnApply()
{
    gSettings.cameraMoveSpeed = m_cameraMovementSettings.m_moveSpeed;
    gSettings.cameraRotateSpeed = m_cameraMovementSettings.m_rotateSpeed;
    gSettings.cameraFastMoveSpeed = m_cameraMovementSettings.m_fastMoveSpeed;
    gSettings.wheelZoomSpeed = m_cameraMovementSettings.m_wheelZoomSpeed;
    gSettings.invertYRotation = m_cameraMovementSettings.m_invertYRotation;
    gSettings.invertPan = m_cameraMovementSettings.m_invertPan;
}

void CEditorPreferencesPage_ViewportMovement::InitializeSettings()
{
    m_cameraMovementSettings.m_moveSpeed = gSettings.cameraMoveSpeed;
    m_cameraMovementSettings.m_rotateSpeed = gSettings.cameraRotateSpeed;
    m_cameraMovementSettings.m_fastMoveSpeed = gSettings.cameraFastMoveSpeed;
    m_cameraMovementSettings.m_wheelZoomSpeed = gSettings.wheelZoomSpeed;
    m_cameraMovementSettings.m_invertYRotation = gSettings.invertYRotation;
    m_cameraMovementSettings.m_invertPan = gSettings.invertPan;
}
