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
#include "ImGuiGem.h"

namespace ImGui
{
    void ImGuiModule::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
    {
#ifdef IMGUI_ENABLED
        switch (event)
        {
            case ESYSTEM_EVENT_GAME_POST_INIT:
            {
                manager.Initialize();
                lyCommonMenu.Initialize();
                break;
            }
            case ESYSTEM_EVENT_FULL_SHUTDOWN:
            case ESYSTEM_EVENT_FAST_SHUTDOWN:
                manager.Shutdown();
                lyCommonMenu.Shutdown();
                break;
            case ESYSTEM_EVENT_GAME_POST_INIT_DONE:
                // Register CVARS after Init is done
                manager.RegisterImGuiCVARs();
                break;
        }
#endif //IMGUI_ENABLED
    }
}

#if !defined(IMGUI_GEM_EDITOR)
// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(ImGui_bab8807a1bc646b3909f3cc200ffeedf, ImGui::ImGuiModule)
#endif // IMGUI_GEM_EDITOR
