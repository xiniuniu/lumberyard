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
#include "ProjectSettingsTool_precompiled.h"

// you must include platform_impl in exactly one of your source files to provide implementations
// of platform functionality such as CryAssert.
#include <platform_impl.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/API/ViewPaneOptions.h>

#include <Include/IPlugin.h>

#include "ProjectSettingsToolWindow.h"
#include "../Editor/LyViewPaneNames.h"


class ProjectSettingsToolPlugin
    : public IPlugin
{
public:
    ProjectSettingsToolPlugin(IEditor* editor)
    {
        AzToolsFramework::ViewPaneOptions options;
        options.showInMenu = false;
        AzToolsFramework::RegisterViewPane<ProjectSettingsTool::ProjectSettingsToolWindow>(LyViewPane::ProjectSettingsTool, LyViewPane::ProjectSettingsTool, options);
    }

    void Release() override
    {
        AzToolsFramework::UnregisterViewPane(LyViewPane::ProjectSettingsTool);
        delete this;
    }

    void ShowAbout() override {}

    const char* GetPluginGUID() override { return "{C5B96A1A-036A-46F9-B7F0-5DF93494F988}"; }
    DWORD GetPluginVersion() override { return 1; }
    const char* GetPluginName() override { return "ProjectSettingsTool"; }
    bool CanExitNow() override { return true; }
    void OnEditorNotify(EEditorNotifyEvent aEventId) override {}
};

PLUGIN_API IPlugin* CreatePluginInstance(PLUGIN_INIT_PARAM* pInitParam)
{
    ISystem* pSystem = pInitParam->pIEditorInterface->GetSystem();
    ModuleInitISystem(pSystem, "ProjectSettingsTool");
    // the above line initializes the gEnv global variable if necessary, and also makes GetIEditor() and other similar functions work correctly.

    return new ProjectSettingsToolPlugin(GetIEditor());
}

#if defined(AZ_PLATFORM_WINDOWS)
HINSTANCE g_hInstance = 0;
BOOL __stdcall DllMain(HINSTANCE hinstDLL, ULONG fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        g_hInstance = hinstDLL;
    }

    return TRUE;
}
#endif