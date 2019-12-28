/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates, or 
* a third party where indicated.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,  
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  
*
*/

#include "StdAfx.h"
#include "GameStartup.h"
#include "Core/CloudGemSamplesGame.h"
#include "Core/EditorGame.h"
#include <IPlayerProfiles.h>
#include <CryLibrary.h>
#include <ITimer.h>
#include <PakVars.h>

#define DLL_INITFUNC_CREATEGAME "CreateGameFramework"

using namespace LYGame;

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

extern "C"
{
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

GameStartup::GameStartup()
    : m_Framework(nullptr)
    , m_Game(nullptr)
    , m_bExecutedAutoExec(false)
{
}

GameStartup::~GameStartup()
{
    Shutdown();
}

IGameRef GameStartup::Init(SSystemInitParams& startupParams)
{
    IGameRef gameRef = NULL;
    startupParams.pGameStartup = this;

    CryGameFrameworkBus::BroadcastResult(m_Framework, &CryGameFrameworkRequests::InitFramework, startupParams);

    // CloudGemSamples downloads files to user storage - It is a requirement to be able to access them directly
    // through the file system.  In Release mode this is not allowed by default.  See PakVars.h : PakVars()
    //         nPriority  = ePakPriorityPakOnly; // Only read from pak files by default
    // We need to force this to PakFirst priority rather than PakOnly for CloudGemSamples.
    ICVar* pakPriority = gEnv->pConsole->GetCVar("sys_PakPriority");
    if (pakPriority && pakPriority->GetIVal() == ePakPriorityPakOnly) // ePakPriorityPakOnly
    {
        char pakPriorityBuf[5];
        azsprintf(pakPriorityBuf, "%d", ePakPriorityPakFirst);
        pakPriority->ForceSet(pakPriorityBuf);
    }

    if (m_Framework)
    {
        ISystem* system = m_Framework->GetISystem();
        startupParams.pSystem = system;

        // register system listeners
        system->GetISystemEventDispatcher()->RegisterListener(this);

        IComponentFactoryRegistry::RegisterAllComponentFactoryNodes(*system->GetIEntitySystem()->GetComponentFactoryRegistry());

        gameRef = Reset();

        if (m_Framework->CompleteInit())
        {
            GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_RANDOM_SEED, static_cast<UINT_PTR>(gEnv->pTimer->GetAsyncTime().GetMicroSecondsAsInt64()), 0);

            if (startupParams.bExecuteCommandLine)
            {
                system->ExecuteCommandLine();
            }
        }
        else
        {
            gameRef->Shutdown();
            gameRef = NULL;
        }
    }

    return gameRef;
}

IGameRef GameStartup::Reset()
{
    static char gameBuffer[sizeof(LYGame::CloudGemSamplesGame)];

    ISystem* system = m_Framework->GetISystem();
    ModuleInitISystem(system, "CloudGemSamples");

    m_Game = new(reinterpret_cast<void*>(gameBuffer)) LYGame::CloudGemSamplesGame();
    const bool initialized = (m_Game && m_Game->Init(m_Framework));

    return initialized ? &m_Game : nullptr;
}

void GameStartup::Shutdown()
{
    if (m_Game)
    {
        m_Game->Shutdown();
        m_Game = nullptr;
    }
}

void GameStartup::ExecuteAutoExec()
{
    if (m_bExecutedAutoExec)
    {
        return;
    }

    auto profileManager = GetISystem()->GetIGame()->GetIGameFramework()->GetIPlayerProfileManager();
    int userCount = profileManager->GetUserCount();
    if (userCount)
    {
        m_bExecutedAutoExec = true;
        gEnv->pConsole->ExecuteString("exec autoexec.cfg");
    }
}

void GameStartup::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
    switch (event)
    {
    case ESYSTEM_EVENT_RANDOM_SEED:
        cry_random_seed(gEnv->bNoRandomSeed ? 0 : (uint32)wparam);
        break;

    case ESYSTEM_EVENT_CHANGE_FOCUS:
        // 3.8.1 - disable / enable Sticky Keys!
        GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_RANDOM_SEED, (UINT_PTR)gEnv->pTimer->GetAsyncTime().GetMicroSecondsAsInt64(), 0);
        break;

    case ESYSTEM_EVENT_LEVEL_POST_UNLOAD:
        STLALLOCATOR_CLEANUP;
        break;

    case ESYSTEM_EVENT_LEVEL_LOAD_START:
    case ESYSTEM_EVENT_FAST_SHUTDOWN:
    default:
        break;
    }
}

GameStartup* GameStartup::Create()
{
    static char buffer[sizeof(GameStartup)];
    return new(buffer)GameStartup();
}
