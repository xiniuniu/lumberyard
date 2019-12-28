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
#include "LegacyGameInterface_precompiled.h"
#include "Game/Actor.h"
#include "LegacyGame.h"
#include "IGameFramework.h"
#include "IGameRulesSystem.h"
#include "LegacyGameRules.h"
#include "IPlatformOS.h"
#include <functional>

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

#define REGISTER_FACTORY(host, name, impl, isAI) \
    (host)->RegisterFactory((name), (impl*)0, (isAI), (impl*)0)

namespace LegacyGameInterface
{
    LegacyGame* g_Game = nullptr;

    //////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////

    LegacyGame::LegacyGame()
        : m_clientEntityId(INVALID_ENTITYID)
        , m_gameRules(nullptr)
        , m_gameFramework(nullptr)
        , m_defaultActionMap(nullptr)
    {
        g_Game = this;
        GetISystem()->SetIGame(this);
    }

    LegacyGame::~LegacyGame()
    {
        m_gameFramework->EndGameContext(false);

        // Remove self as listener.
        m_gameFramework->UnregisterListener(this);
        m_gameFramework->GetILevelSystem()->RemoveListener(this);
        gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);

        g_Game = nullptr;
        GetISystem()->SetIGame(nullptr);

        ReleaseActionMaps();
    }

    bool LegacyGame::Init(IGameFramework* framework)
    {
        m_gameFramework = framework;

        // Register the actor class so actors can spawn.
        // #TODO If you create a new actor, make sure to register a factory here.
        REGISTER_FACTORY(m_gameFramework, "Actor", Actor, false);

        // Listen to system events, so we know when levels load/unload, etc.
        gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this);
        m_gameFramework->GetILevelSystem()->AddListener(this);

        // Listen for game framework events (level loaded/unloaded, etc.).
        m_gameFramework->RegisterListener(this, "Game", FRAMEWORKLISTENERPRIORITY_GAME);

        // Load actions maps.
        LoadActionMaps("config/input/actionmaps.xml");

        // Register game rules wrapper.
        REGISTER_FACTORY(framework, "LegacyGameRules", LegacyGameRules, false);
        IGameRulesSystem* pGameRulesSystem = g_Game->GetIGameFramework()->GetIGameRulesSystem();
        pGameRulesSystem->RegisterGameRules("DummyRules", "LegacyGameRules");

        return true;
    }

    bool LegacyGame::CompleteInit()
    {
        return true;
    }

    void LegacyGame::PlayerIdSet(EntityId playerId)
    {
        m_clientEntityId = playerId;
    }

    void LegacyGame::Shutdown()
    {
        this->~LegacyGame();
    }

    void LegacyGame::LoadActionMaps(const char* fileName)
    {
        if (g_Game->GetIGameFramework()->IsGameStarted())
        {
            CryLogAlways("[Profile] Can't change configuration while game is running (yet)");
            return;
        }

        XmlNodeRef rootNode = m_gameFramework->GetISystem()->LoadXmlFromFile(fileName);
        if (rootNode && ReadProfile(rootNode))
        {
            IActionMapManager* actionMapManager = m_gameFramework->GetIActionMapManager();
            actionMapManager->SetLoadFromXMLPath(fileName);
            m_defaultActionMap = actionMapManager->GetActionMap("default");
        }
        else
        {
            CryLogAlways("[Profile] Warning: Could not open configuration file");
        }
    }

    void LegacyGame::ReleaseActionMaps()
    {
        if (m_defaultActionMap && m_gameFramework)
        {
            IActionMapManager* actionMapManager = m_gameFramework->GetIActionMapManager();
            actionMapManager->RemoveActionMap(m_defaultActionMap->GetName());
            m_defaultActionMap = nullptr;
        }
    }

    bool LegacyGame::ReadProfile(const XmlNodeRef& rootNode)
    {
        if (IActionMapManager* actionMapManager = m_gameFramework->GetIActionMapManager())
        {
            actionMapManager->Clear();

            // Load platform information in.
            XmlNodeRef platformsNode = rootNode->findChild("platforms");
            if (!platformsNode)
            {
                CryLogAlways("[Profile] Warning: No platform information specified!");
            }

            const char* platformName = nullptr;
            switch (AZ::g_currentPlatform)
            {
            case AZ::PLATFORM_WINDOWS_32:
            case AZ::PLATFORM_WINDOWS_64:
            case AZ::PLATFORM_APPLE_OSX:
                platformName = "PC";
                break;
#if defined(AZ_EXPAND_FOR_RESTRICTED_PLATFORM) || defined(AZ_TOOLS_EXPAND_FOR_RESTRICTED_PLATFORMS)
#define AZ_RESTRICTED_PLATFORM_EXPANSION(CodeName, CODENAME, codename, PrivateName, PRIVATENAME, privatename, PublicName, PUBLICNAME, publicname, PublicAuxName1, PublicAuxName2, PublicAuxName3)\
            case AZ::PLATFORM_##PUBLICNAME:\
                platformName = #CodeName;\
                break;
#if defined(AZ_EXPAND_FOR_RESTRICTED_PLATFORM)
                AZ_EXPAND_FOR_RESTRICTED_PLATFORM
#else
                AZ_TOOLS_EXPAND_FOR_RESTRICTED_PLATFORMS
#endif
#undef AZ_RESTRICTED_PLATFORM_EXPANSION
#endif
            default:
                platformName = AZ::GetPlatformName(AZ::g_currentPlatform);
                break;
            }

            if (platformsNode && platformName)
            {
                if (XmlNodeRef platform = platformsNode->findChild(platformName))
                {
                    if (strcmp(platform->getAttr("keyboard"), "0"))
                    {
                        actionMapManager->AddInputDeviceMapping(eAID_KeyboardMouse, "keyboard");
                    }

                    if (strcmp(platform->getAttr("xeniapad"), "0"))
                    {
                        actionMapManager->AddInputDeviceMapping(eAID_XeniaPad, "xeniapad");
                    }

                    if (strcmp(platform->getAttr("provopad"), "0"))
                    {
                        actionMapManager->AddInputDeviceMapping(eAID_ProvoPad, "provopad");
                    }

                    if (strcmp(platform->getAttr("androidkey"), "0"))
                    {
                        actionMapManager->AddInputDeviceMapping(eAID_AndroidKey, "androidkey");
                    }
                }
                else
                {
                    GameWarning("ReadProfilePlatform: Failed to find platform, action mappings loading will fail");
                    return false;
                }
            }

            return actionMapManager->LoadFromXML(rootNode);
        }

        return false;
    }

    void LegacyGame::OnActionEvent(const SActionEvent& event)
    {
        switch (event.m_event)
        {
        case eAE_unloadLevel:
            /*!
             * #TODO
             * Add clean up code here.
             */
            break;
        }
    }

} // namespace LegacyGameInterface
