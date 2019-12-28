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
#include "GameStartup.h"
#include "Core/LegacyGame.h"
#include "Core/EditorGame.h"
#include <IPlayerProfiles.h>
#include <CryLibrary.h>
#include <ITimer.h>

using namespace LegacyGameInterface;

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

GameStartup::GameStartup()
    : m_Framework(nullptr)
    , m_Game(nullptr)
{
}

GameStartup::~GameStartup()
{
    Shutdown();
}

IGameRef GameStartup::Init(SSystemInitParams& startupParams)
{
    IGameRef gameRef = nullptr;
    startupParams.pGameStartup = this;

    CryGameFrameworkBus::BroadcastResult(m_Framework, &CryGameFrameworkRequests::InitFramework, startupParams);
    if (m_Framework)
    {
        ISystem* system = m_Framework->GetISystem();
        startupParams.pSystem = system;

        // register system listeners
        system->GetISystemEventDispatcher()->RegisterListener(this);

        IEntitySystem* entitySystem = system->GetIEntitySystem();
        IComponentFactoryRegistry::RegisterAllComponentFactoryNodes(*entitySystem->GetComponentFactoryRegistry());

        gameRef = Reset();

        if (m_Framework->CompleteInit())
        {
            ISystemEventDispatcher* eventDispatcher = system->GetISystemEventDispatcher();
            eventDispatcher->OnSystemEvent(
                ESYSTEM_EVENT_RANDOM_SEED,
                static_cast<UINT_PTR>(gEnv->pTimer->GetAsyncTime().GetMicroSecondsAsInt64()),
                0
                );

            if (startupParams.bExecuteCommandLine)
            {
                system->ExecuteCommandLine();
            }
        }
        else
        {
            gameRef->Shutdown();
            gameRef = nullptr;
        }
    }

    return gameRef;
}

IGameRef GameStartup::Reset()
{
    static char gameBuffer[sizeof(LegacyGameInterface::LegacyGame)];

    ISystem* system = m_Framework->GetISystem();
    ModuleInitISystem(system, "LegacyGame");

    m_Game = new(reinterpret_cast<void*>(gameBuffer)) LegacyGameInterface::LegacyGame();
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
    CryGameFrameworkBus::Broadcast(&CryGameFrameworkRequests::ShutdownFramework);
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
