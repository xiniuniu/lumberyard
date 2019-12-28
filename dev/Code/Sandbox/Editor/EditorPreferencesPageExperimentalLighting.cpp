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
#include "EditorPreferencesPageExperimentalLighting.h"
#include <AzCore/Serialization/EditContext.h>

void CEditorPreferencesPage_ExperimentalLighting::Reflect(AZ::SerializeContext& serialize)
{
    serialize.Class<Options>()
        ->Version(1)
        ->Field("TotalIlluminationEnabled", &Options::m_totalIlluminationEnabled);

    serialize.Class<CEditorPreferencesPage_ExperimentalLighting>()
        ->Version(1)
        ->Field("Options", &CEditorPreferencesPage_ExperimentalLighting::m_options);

    AZ::EditContext* editContext = serialize.GetEditContext();
    if (editContext)
    {
        editContext->Class<Options>("Options", "")
            ->DataElement(AZ::Edit::UIHandlers::CheckBox, &Options::m_totalIlluminationEnabled, "Total Illumination", "Enable Total Illumination");

        editContext->Class<CEditorPreferencesPage_ExperimentalLighting>("Experimental Features Preferences", "Experimental Features Preferences")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly", 0xef428f20))
            ->DataElement(AZ::Edit::UIHandlers::Default, &CEditorPreferencesPage_ExperimentalLighting::m_options, "Options", "Experimental Features Options");
    }
}


CEditorPreferencesPage_ExperimentalLighting::CEditorPreferencesPage_ExperimentalLighting()
{
    InitializeSettings();
}

void CEditorPreferencesPage_ExperimentalLighting::OnApply()
{
    gSettings.sExperimentalFeaturesSettings.bTotalIlluminationEnabled = m_options.m_totalIlluminationEnabled;
}

void CEditorPreferencesPage_ExperimentalLighting::InitializeSettings()
{
    m_options.m_totalIlluminationEnabled = gSettings.sExperimentalFeaturesSettings.bTotalIlluminationEnabled;
}
