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

#include "EditorToolsApplication.h"

#include <AzToolsFramework/UI/UICore/WidgetHelpers.h>
#include <AzToolsFramework/Thumbnails/ThumbnailerComponent.h>
#include <AzToolsFramework/AssetBrowser/AssetBrowserComponent.h>
#include <AzToolsFramework/MaterialBrowser/MaterialBrowserComponent.h>

#include <ParseEngineConfig.h>
#include <Crates/Crates.h>
#include <Engines/EnginesAPI.h> // For LyzardSDK
#include <QMessageBox>

#include <CryEdit.h>
#include <CryEditDoc.h>
#include <DisplaySettingsPythonFuncs.h>
#include <Material/MaterialPythonFuncs.h>
#include <Modelling/ModellingMode.h>
#include <PythonEditorFuncs.h>
#include <TrackView/TrackViewPythonFuncs.h>
#include <VegetationTool.h>
#include <ViewPane.h>
#ifdef LY_TERRAIN_EDITOR
#include <TerrainModifyTool.h>
#include <Terrain/PythonTerrainFuncs.h>
#include <Terrain/PythonTerrainLayerFuncs.h>
#endif

namespace EditorInternal
{
    bool EditorToolsApplication::OnFailedToFindConfiguration(const char* configFilePath)
    {
        bool overrideAssetRoot = (this->m_assetRoot[0] != '\0');
        const char* sourcePaths[] = { this->m_assetRoot };

        CEngineConfig engineCfg(sourcePaths, overrideAssetRoot ? 1 : 0);

        char msg[4096] = { 0 };
        azsnprintf(msg, sizeof(msg),
            "Application descriptor file not found:\n"
            "%s\n"
            "Generate this file and commit it to source control by running:\n"
            "Bin64\\lmbr.exe projects populate-appdescriptors -projects %s",
            configFilePath, engineCfg.m_gameDLL.c_str());
        QMessageBox::critical(AzToolsFramework::GetActiveWindow(), QObject::tr("File not found"), msg);

        // flag that we failed, so that the application can exit out
        m_StartupAborted = true;

        // indicate that we don't want AzFramework::Application::Start to continue startup
        return false;
    }

    bool EditorToolsApplication::IsStartupAborted() const
    {
        return m_StartupAborted;
    }


    void EditorToolsApplication::RegisterCoreComponents()
    {
        AzToolsFramework::ToolsApplication::RegisterCoreComponents();

        RegisterComponentDescriptor(AZ::CratesHandler::CreateDescriptor());

        // Expose Legacy Python Bindings to Behavior Context for EditorPythonBindings Gem
        RegisterComponentDescriptor(AzToolsFramework::CryEditPythonHandler::CreateDescriptor());
        RegisterComponentDescriptor(AzToolsFramework::CryEditDocFuncsHandler::CreateDescriptor());
        RegisterComponentDescriptor(AzToolsFramework::DisplaySettingsPythonFuncsHandler::CreateDescriptor());
        RegisterComponentDescriptor(AzToolsFramework::MainWindowEditorFuncsHandler::CreateDescriptor());
        RegisterComponentDescriptor(AzToolsFramework::ModellingModeFuncsHandler::CreateDescriptor());
        RegisterComponentDescriptor(AzToolsFramework::ObjectManagerFuncsHandler::CreateDescriptor());
        RegisterComponentDescriptor(AzToolsFramework::MaterialPythonFuncsHandler::CreateDescriptor());
        RegisterComponentDescriptor(AzToolsFramework::PythonEditorFuncsHandler::CreateDescriptor());
        RegisterComponentDescriptor(AzToolsFramework::TrackViewFuncsHandler::CreateDescriptor());
        RegisterComponentDescriptor(AzToolsFramework::VegetationToolFuncsHandler::CreateDescriptor());
        RegisterComponentDescriptor(AzToolsFramework::ViewPanePythonFuncsHandler::CreateDescriptor());
#ifdef LY_TERRAIN_EDITOR
        RegisterComponentDescriptor(AzToolsFramework::TerrainModifyPythonFuncsHandler::CreateDescriptor());
        RegisterComponentDescriptor(AzToolsFramework::TerrainPythonFuncsHandler::CreateDescriptor());
        RegisterComponentDescriptor(AzToolsFramework::TerrainLayerPythonFuncsHandler::CreateDescriptor());
#endif
    }


    AZ::ComponentTypeList EditorToolsApplication::GetRequiredSystemComponents() const
    {
        AZ::ComponentTypeList components = AzToolsFramework::ToolsApplication::GetRequiredSystemComponents();

        components.emplace_back(azrtti_typeid<AZ::CratesHandler>());
        components.emplace_back(azrtti_typeid<AzToolsFramework::Thumbnailer::ThumbnailerComponent>());
        components.emplace_back(azrtti_typeid<AzToolsFramework::AssetBrowser::AssetBrowserComponent>());
        components.emplace_back(azrtti_typeid<AzToolsFramework::MaterialBrowser::MaterialBrowserComponent>());

        return components;
    }


    void EditorToolsApplication::StartCommon(AZ::Entity* systemEntity)
    {
        AzToolsFramework::ToolsApplication::StartCommon(systemEntity);
        if (systemEntity->GetState() != AZ::Entity::ES_ACTIVE)
        {
            m_StartupAborted = true;
            return;
        }

        AZ::ModuleManagerRequestBus::Broadcast(&AZ::ModuleManagerRequestBus::Events::LoadDynamicModule, "LyzardEngines", AZ::ModuleInitializationSteps::ActivateEntity, true);
        AZ::ModuleManagerRequestBus::Broadcast(&AZ::ModuleManagerRequestBus::Events::LoadDynamicModule, "LyzardGems", AZ::ModuleInitializationSteps::ActivateEntity, true);
        AZ::ModuleManagerRequestBus::Broadcast(&AZ::ModuleManagerRequestBus::Events::LoadDynamicModule, "LyzardProjects", AZ::ModuleInitializationSteps::ActivateEntity, true);
    }

    bool EditorToolsApplication::Start(int argc, char* argv[])
    {
        char descriptorPath[AZ_MAX_PATH_LEN] = { 0 };
        char appRootOverride[AZ_MAX_PATH_LEN] = { 0 };
        {
            // CEngineConfig has a default depth of 3 to look for bootstrap.cfg
            // file. When the editor is in an App Bundle then it will be 3 levels
            // deeper. So make the max_depth we are willing to search to be 6, 3
            // for the default + 3 for app bundle location.
            const int max_depth = 6;
            CEngineConfig engineCfg(nullptr, 0, max_depth);

            azstrcat(descriptorPath, AZ_MAX_PATH_LEN, engineCfg.m_gameFolder);
            azstrcat(descriptorPath, AZ_MAX_PATH_LEN, "/Config/Editor.xml");

            AzFramework::Application::StartupParameters params;

            if (!GetOptionalAppRootArg(argc, argv, appRootOverride, AZ_ARRAY_SIZE(appRootOverride)))
            {
                QString currentRoot;
                QDir pathCheck(qApp->applicationDirPath());
                do
                {
                    if (pathCheck.exists(QString("engine.json")))
                    {
                        currentRoot = pathCheck.absolutePath();
                        break;
                    }
                } while (pathCheck.cdUp());
                if (currentRoot.isEmpty())
                {
                    return false;
                }
                azstrncpy(appRootOverride, AZ_ARRAY_SIZE(appRootOverride), currentRoot.toUtf8().data(), currentRoot.length());
            }

            params.m_appRootOverride = appRootOverride;

            // Must be done before creating QApplication, otherwise asserts when we alloc
            AzToolsFramework::ToolsApplication::Start(descriptorPath, params);
            if (IsStartupAborted())
            {
                return false;
            }
        }
        return true;
    }

    bool EditorToolsApplication::GetOptionalAppRootArg(int argc, char* argv[], char destinationRootArgBuffer[], size_t destinationRootArgBufferSize) const
    {
        const char* appRootArg = nullptr;
        bool isAppRootArg = false;
        for (int index = 0; index < argc; index++)
        {
            if (isAppRootArg)
            {
                appRootArg = argv[index];
                isAppRootArg = false;
            }
            else if (azstricmp("--app-root", argv[index]) == 0)
            {
                isAppRootArg = true;
            }
        }
        if (appRootArg)
        {
            azstrncpy(destinationRootArgBuffer, destinationRootArgBufferSize, appRootArg, strlen(appRootArg));
            const char lastChar = destinationRootArgBuffer[strlen(destinationRootArgBuffer) - 1];
            bool needsTrailingPathDelim = (lastChar != AZ_CORRECT_FILESYSTEM_SEPARATOR) && (lastChar != AZ_WRONG_FILESYSTEM_SEPARATOR);
            if (needsTrailingPathDelim)
            {
                azstrncat(destinationRootArgBuffer, destinationRootArgBufferSize, AZ_CORRECT_FILESYSTEM_SEPARATOR_STRING, 1);
            }
            return true;
        }
        else
        {
            return false;
        }
    }
}

