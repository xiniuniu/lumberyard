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
#include "EditorPreferencesPageViewportDebug.h"
#include <AzCore/Serialization/EditContext.h>

void CEditorPreferencesPage_ViewportDebug::Reflect(AZ::SerializeContext& serialize)
{
    serialize.Class<Profiling>()
        ->Version(1)
        ->Field("ShowMeshStatsOnMouseOver", &Profiling::m_showMeshStatsOnMouseOver);

    serialize.Class<Warnings>()
        ->Version(1)
        ->Field("WarningIconsDrawDistance", &Warnings::m_warningIconsDrawDistance)
        ->Field("ShowScaleWarnings", &Warnings::m_showScaleWarnings)
        ->Field("ShowRotationWarnings", &Warnings::m_showRotationWarnings);

    serialize.Class<CEditorPreferencesPage_ViewportDebug>()
        ->Version(1)
        ->Field("Profiling", &CEditorPreferencesPage_ViewportDebug::m_profiling)
        ->Field("Warnings", &CEditorPreferencesPage_ViewportDebug::m_warnings);


    AZ::EditContext* editContext = serialize.GetEditContext();
    if (editContext)
    {
        editContext->Class<Profiling>("Profiling", "Profiling")
            ->DataElement(AZ::Edit::UIHandlers::CheckBox, &Profiling::m_showMeshStatsOnMouseOver, "Show Mesh Statistics", "Show Mesh Statistics on Mouse Over");

        editContext->Class<Warnings>("Viewport Warning Settings", "")
            ->DataElement(AZ::Edit::UIHandlers::SpinBox, &Warnings::m_warningIconsDrawDistance, "Warning Icons Draw Distance", "Warning Icons Draw Distance")
            ->DataElement(AZ::Edit::UIHandlers::CheckBox, &Warnings::m_showScaleWarnings, "Show Scale Warnings", "Show Scale Warnings")
            ->DataElement(AZ::Edit::UIHandlers::CheckBox, &Warnings::m_showRotationWarnings, "Show Rotation Warnings", "Show Rotation Warnings");

        editContext->Class<CEditorPreferencesPage_ViewportDebug>("Viewport Debug Preferences", "Viewport Debug Preferences")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly", 0xef428f20))
            ->DataElement(AZ::Edit::UIHandlers::Default, &CEditorPreferencesPage_ViewportDebug::m_profiling, "Profiling", "Profiling")
            ->DataElement(AZ::Edit::UIHandlers::Default, &CEditorPreferencesPage_ViewportDebug::m_warnings, "Viewport Warning Settings", "Viewport Warning Settings");
    }
}


CEditorPreferencesPage_ViewportDebug::CEditorPreferencesPage_ViewportDebug()
{
    InitializeSettings();
}

void CEditorPreferencesPage_ViewportDebug::OnApply()
{
    gSettings.viewports.bShowMeshStatsOnMouseOver = m_profiling.m_showMeshStatsOnMouseOver;
    gSettings.viewports.bShowRotationWarnings = m_warnings.m_showRotationWarnings;
    gSettings.viewports.bShowScaleWarnings = m_warnings.m_showScaleWarnings;
    gSettings.viewports.fWarningIconsDrawDistance = m_warnings.m_warningIconsDrawDistance;
}

void CEditorPreferencesPage_ViewportDebug::InitializeSettings()
{
    m_profiling.m_showMeshStatsOnMouseOver = gSettings.viewports.bShowMeshStatsOnMouseOver;
    m_warnings.m_showRotationWarnings = gSettings.viewports.bShowRotationWarnings;
    m_warnings.m_showScaleWarnings = gSettings.viewports.bShowScaleWarnings;
    m_warnings.m_warningIconsDrawDistance = gSettings.viewports.fWarningIconsDrawDistance;
}
