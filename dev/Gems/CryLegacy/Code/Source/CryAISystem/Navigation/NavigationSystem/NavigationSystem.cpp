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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#include "CryLegacy_precompiled.h"
#include "NavigationSystem.h"
#include "../MNM/TileGenerator.h"
#include "../MNM/MeshGrid.h"
#include "DebugDrawContext.h"
#include "MNMPathfinder.h"
#include "IStereoRenderer.h" //For IsRenderingToHMD

#include <LmbrCentral/Ai/NavigationSeedBus.h>

#define BAI_NAVIGATION_FILE_VERSION 7
#define MAX_NAME_LENGTH 512
#define BAI_NAVIGATION_GUID_FLAG (1 << 30)


//#pragma optimize("", off)
//#pragma inline_depth(0)

#if !defined(_RELEASE)
#define NAVIGATION_SYSTEM_CONSOLE_AUTOCOMPLETE
#endif

#if defined NAVIGATION_SYSTEM_CONSOLE_AUTOCOMPLETE

#include <IConsole.h>

struct SAgentTypeListAutoComplete
    : public IConsoleArgumentAutoComplete
{
    virtual int GetCount() const
    {
        assert(gAIEnv.pNavigationSystem);
        return static_cast<int>(gAIEnv.pNavigationSystem->GetAgentTypeCount());
    }

    virtual const char* GetValue(int nIndex) const
    {
        NavigationSystem* pNavigationSystem = gAIEnv.pNavigationSystem;
        assert(pNavigationSystem);
        return pNavigationSystem->GetAgentTypeName(pNavigationSystem->GetAgentTypeID(nIndex));
    }
};

SAgentTypeListAutoComplete s_agentTypeListAutoComplete;

#endif


enum
{
    MaxTaskCountPerWorkerThread = 12,
};
enum
{
    MaxVolumeDefCopyCount = 8
};                                  // volume copies for access in other threads

#if NAVIGATION_SYSTEM_PC_ONLY
void GenerateTileJob(MNM::TileGenerator::Params params, volatile uint16* state, MNM::Tile* tile, uint32* hashValue)
{
    AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AI);

    if (*state != NavigationSystem::TileTaskResult::Failed)
    {
        MNM::TileGenerator generator;
        bool result = generator.Generate(params, *tile, hashValue);
        if (result)
        {
            *state = NavigationSystem::TileTaskResult::Completed;
        }
        else if (((params.flags & MNM::TileGenerator::Params::NoHashTest) == 0) && (*hashValue == params.hashValue))
        {
            *state = NavigationSystem::TileTaskResult::NoChanges;
        }
        else
        {
            *state = NavigationSystem::TileTaskResult::Failed;
        }
    }
}
#endif

uint32 NameHash(const char* name)
{
    uint32 len = strlen(name);
    uint32 hash = 0;

    for (uint32 i = 0; i < len; ++i)
    {
        hash += (uint8)(CryStringUtils::toLowerAscii(name[i]));
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    return hash;
}

bool ShouldBeConsideredByVoxelizer(IPhysicalEntity& physicalEntity, uint32& flags)
{
    if (physicalEntity.GetType() == PE_WHEELEDVEHICLE)
    {
        return false;
    }

    bool considerMass = (physicalEntity.GetType() == PE_RIGID);
    if (!considerMass)
    {
        pe_status_pos sp;
        if (physicalEntity.GetStatus(&sp))
        {
            considerMass = (sp.iSimClass == SC_ACTIVE_RIGID) || (sp.iSimClass == SC_SLEEPING_RIGID);
        }
    }

    if (considerMass)
    {
        pe_status_dynamics dyn;
        if (!physicalEntity.GetStatus(&dyn))
        {
            return false;
        }

        if (dyn.mass > 1e-6f)
        {
            return false;
        }
    }

    pe_params_foreign_data  foreignData;
    physicalEntity.GetParams(&foreignData);

    if (foreignData.iForeignFlags & PFF_EXCLUDE_FROM_STATIC)
    {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

NavigationSystem::NavigationSystem(const char* configName)
    : m_configName(configName)
    , m_throughput(0.0f)
    , m_cacheHitRate(0.0f)
    , m_free(0)
    , m_state(Idle)
    , m_meshes(256)               //Same size of meshes, off-mesh and islandConnections elements
    , m_offMeshNavigationManager(256)
    , m_islandConnectionsManager()
    , m_volumes(512)
    , m_worldAABB(AABB::RESET)
    , m_volumeDefCopy(MaxVolumeDefCopyCount, VolumeDefCopy())
    , m_listenersList(10)
    , m_users(10)
    , m_configurationVersion(0)
    , m_isNavigationUpdatePaused(false)
{
    SetupTasks();

    m_worldMonitor = WorldMonitor(functor(*this, &NavigationSystem::WorldChanged));
    StartWorldMonitoring();

    ReloadConfig();

    m_pEditorBackgroundUpdate = new NavigationSystemBackgroundUpdate(*this);

#ifdef NAVIGATION_SYSTEM_CONSOLE_AUTOCOMPLETE
    gEnv->pConsole->RegisterAutoComplete("ai_debugMNMAgentType", &s_agentTypeListAutoComplete);
#endif
}

NavigationSystem::~NavigationSystem()
{
    StopWorldMonitoring();

    Clear();

    SAFE_DELETE(m_pEditorBackgroundUpdate);

#ifdef NAVIGATION_SYSTEM_CONSOLE_AUTOCOMPLETE
    gEnv->pConsole->UnRegisterAutoComplete("ai_debugMNMAgentType");
#endif
}

NavigationAgentTypeID NavigationSystem::CreateAgentType(const char* name, const CreateAgentTypeParams& params)
{
    assert(name);
    AgentTypes::const_iterator it = m_agentTypes.begin();
    AgentTypes::const_iterator end = m_agentTypes.end();

    for (; it != end; ++it)
    {
        const AgentType& agentType = *it;

        if (!agentType.name.compareNoCase(name))
        {
            AIWarning("Trying to create NavigationSystem AgentType with duplicate name '%s'!", name);
            assert(0);
            return NavigationAgentTypeID();
        }
    }

    m_agentTypes.resize(m_agentTypes.size() + 1);
    AgentType& agentType = m_agentTypes.back();

    agentType.name = name;
    agentType.settings.voxelSize = params.voxelSize;
    agentType.settings.radiusVoxelCount = params.radiusVoxelCount;
    agentType.settings.climbableVoxelCount = params.climbableVoxelCount;
    agentType.settings.climbableInclineGradient = params.climbableInclineGradient;
    agentType.settings.climbableStepRatio = params.climbableStepRatio;
    agentType.meshEntityCallback = functor(ShouldBeConsideredByVoxelizer);
    agentType.settings.heightVoxelCount = params.heightVoxelCount;
    agentType.settings.maxWaterDepthVoxelCount = params.maxWaterDepthVoxelCount;

    return NavigationAgentTypeID(m_agentTypes.size());
}

NavigationAgentTypeID NavigationSystem::GetAgentTypeID(const char* name) const
{
    assert(name);

    AgentTypes::const_iterator it = m_agentTypes.begin();
    AgentTypes::const_iterator end = m_agentTypes.end();

    for (; it != end; ++it)
    {
        const AgentType& agentType = *it;

        if (!agentType.name.compareNoCase(name))
        {
            return NavigationAgentTypeID((uint32)(std::distance(m_agentTypes.begin(), it) + 1));
        }
    }

    return NavigationAgentTypeID();
}

NavigationAgentTypeID NavigationSystem::GetAgentTypeID(size_t index) const
{
    if (index  < m_agentTypes.size())
    {
        return NavigationAgentTypeID(index + 1);
    }

    return NavigationAgentTypeID();
}

const char* NavigationSystem::GetAgentTypeName(NavigationAgentTypeID agentTypeID) const
{
    if (agentTypeID && agentTypeID <= m_agentTypes.size())
    {
        return m_agentTypes[agentTypeID - 1].name.c_str();
    }

    return 0;
}

size_t NavigationSystem::GetAgentTypeCount() const
{
    return m_agentTypes.size();
}

bool NavigationSystem::GetAgentTypeProperties(const NavigationAgentTypeID agentTypeID, AgentType& agentTypeProperties) const
{
    if (agentTypeID && agentTypeID <= m_agentTypes.size())
    {
        agentTypeProperties = m_agentTypes[agentTypeID - 1];
        return true;
    }

    return false;
}

inline size_t NearestFactor(size_t n, size_t f)
{
    while (n % f)
    {
        ++f;
    }

    return f;
}

NavigationMeshID NavigationSystem::CreateMesh(const char* name, NavigationAgentTypeID agentTypeID,
    const CreateMeshParams& params)
{
    return CreateMesh(name, agentTypeID, params, NavigationMeshID(0));
}

NavigationMeshID NavigationSystem::CreateMesh(const char* name, NavigationAgentTypeID agentTypeID,
    const CreateMeshParams& params, NavigationMeshID requestedID)
{
    assert(name && agentTypeID && agentTypeID <= m_agentTypes.size());

    if (agentTypeID && (agentTypeID <= m_agentTypes.size()))
    {
        AgentType& agentType = m_agentTypes[agentTypeID - 1];

        MNM::MeshGrid::Params paramsGrid;
        paramsGrid.tileSize = params.tileSize;
        paramsGrid.voxelSize = agentType.settings.voxelSize;
        paramsGrid.tileCount = params.tileCount;

        paramsGrid.voxelSize.x = NearestFactor(paramsGrid.tileSize.x * 1000, (size_t)(paramsGrid.voxelSize.x * 1000)) * 0.001f;
        paramsGrid.voxelSize.y = NearestFactor(paramsGrid.tileSize.y * 1000, (size_t)(paramsGrid.voxelSize.y * 1000)) * 0.001f;
        paramsGrid.voxelSize.z = NearestFactor(paramsGrid.tileSize.z * 1000, (size_t)(paramsGrid.voxelSize.z * 1000)) * 0.001f;

        if (!paramsGrid.voxelSize.IsEquivalent(agentType.settings.voxelSize, 0.0001f))
        {
            AIWarning("VoxelSize (%.3f, %.3f, %.3f) for agent '%s' adjusted to (%.3f, %.3f, %.3f)"
                " - needs to be a factor of TileSize (%d, %d, %d)",
                agentType.settings.voxelSize.x, agentType.settings.voxelSize.y, agentType.settings.voxelSize.z,
                agentType.name.c_str(),
                paramsGrid.voxelSize.x, paramsGrid.voxelSize.y, paramsGrid.voxelSize.z,
                paramsGrid.tileSize.x, paramsGrid.tileSize.y, paramsGrid.tileSize.z);
        }

        NavigationMeshID id = requestedID;
        if (requestedID == NavigationMeshID(0))
        {
            id = NavigationMeshID(m_meshes.insert(NavigationMesh(agentTypeID)));
        }
        else
        {
            m_meshes.insert(requestedID, NavigationMesh(agentTypeID));
        }
        NavigationMesh& mesh = m_meshes[id];
        mesh.grid.Init(paramsGrid);
        mesh.name = name;
        mesh.exclusions = agentType.exclusions;

        agentType.meshes.push_back(AgentType::MeshInfo(id, NameHash(name)));

        m_offMeshNavigationManager.OnNavigationMeshCreated(id);

        return id;
    }

    return NavigationMeshID();
}

void NavigationSystem::DestroyMesh(NavigationMeshID meshID)
{
    if (meshID && m_meshes.validate(meshID))
    {
        NavigationMesh& mesh = m_meshes[meshID];

        for (size_t t = 0; t < m_runningTasks.size(); ++t)
        {
            if (m_results[m_runningTasks[t]].meshID == meshID)
            {
                m_results[m_runningTasks[t]].state = TileTaskResult::Failed;
            }
        }

        for (size_t t = 0; t < m_runningTasks.size(); ++t)
        {
            if (m_results[m_runningTasks[t]].meshID == meshID)
            {
                m_results[m_runningTasks[t]].jobExecutor.WaitForCompletion();
            }
        }

        AgentType& agentType = m_agentTypes[mesh.agentTypeID - 1];
        AgentType::Meshes::iterator it = agentType.meshes.begin();
        AgentType::Meshes::iterator end = agentType.meshes.end();

        for (; it != end; ++it)
        {
            if (it->id == meshID)
            {
                std::swap(*it, agentType.meshes.back());
                agentType.meshes.pop_back();
                break;
            }
        }

        {
            AZStd::lock_guard<AZStd::recursive_mutex> tileQueueGuard(m_tileQueueMutex);
            TileTaskQueue::iterator it = m_tileQueue.begin();
            while (it != m_tileQueue.end())
            {
                const TileTask& task = *it;
                if (task.meshID == meshID)
                {
                    it = m_tileQueue.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        m_meshes.erase(meshID);

        m_offMeshNavigationManager.OnNavigationMeshDestroyed(meshID);

        ComputeWorldAABB();
    }
}

void NavigationSystem::SetMeshEntityCallback(NavigationAgentTypeID agentTypeID, const NavigationMeshEntityCallback& callback)
{
    if (agentTypeID && (agentTypeID <= m_agentTypes.size()))
    {
        AgentType& agentType = m_agentTypes[agentTypeID - 1];

        agentType.meshEntityCallback = callback;
    }
}

void NavigationSystem::AddMeshChangeCallback(NavigationAgentTypeID agentTypeID,
    const NavigationMeshChangeCallback& callback)
{
    if (agentTypeID && (agentTypeID <= m_agentTypes.size()))
    {
        AgentType& agentType = m_agentTypes[agentTypeID - 1];

        stl::push_back_unique(agentType.callbacks, callback);
    }
}

void NavigationSystem::RemoveMeshChangeCallback(NavigationAgentTypeID agentTypeID,
    const NavigationMeshChangeCallback& callback)
{
    if (agentTypeID && (agentTypeID <= m_agentTypes.size()))
    {
        AgentType& agentType = m_agentTypes[agentTypeID - 1];

        stl::find_and_erase(agentType.callbacks, callback);
    }
}

void NavigationSystem::SetMeshBoundaryVolume(NavigationMeshID meshID, NavigationVolumeID volumeID)
{
    if (meshID && m_meshes.validate(meshID))
    {
        NavigationMesh& mesh = m_meshes[meshID];

        if (mesh.boundary)
        {
            ++mesh.version;
        }

        mesh.boundary = volumeID;
        ComputeWorldAABB();
    }
}

NavigationVolumeID NavigationSystem::CreateVolume(Vec3* vertices, size_t vertexCount, float height)
{
    return CreateVolume(vertices, vertexCount, height, NavigationVolumeID(0));
}

NavigationVolumeID NavigationSystem::CreateVolume(Vec3* vertices, size_t vertexCount, float height, NavigationVolumeID requestedID)
{
    NavigationVolumeID id = requestedID;
    if (requestedID == NavigationVolumeID(0))
    {
        id = NavigationVolumeID(m_volumes.insert(NavigationBoundingVolume()));
    }
    else
    {
        m_volumes.insert(requestedID, NavigationBoundingVolume());
    }
    assert(id != 0);
    SetVolume(id, vertices, vertexCount, height);

    return id;
}

void NavigationSystem::DestroyVolume(NavigationVolumeID volumeID)
{
    if (volumeID && m_volumes.validate(volumeID))
    {
        NavigationBoundingVolume& volume = m_volumes[volumeID];

        AgentTypes::iterator it = m_agentTypes.begin();
        AgentTypes::iterator end = m_agentTypes.end();

        for (; it != end; ++it)
        {
            AgentType& agentType = *it;

            AgentType::Meshes::const_iterator mit = agentType.meshes.begin();
            AgentType::Meshes::const_iterator mend = agentType.meshes.end();

            stl::find_and_erase(agentType.exclusions, volumeID);

            for (; mit != mend; ++mit)
            {
                const NavigationMeshID meshID = mit->id;
                NavigationMesh& mesh = m_meshes[meshID];

                if (mesh.boundary == volumeID)
                {
                    ++mesh.version;
                    continue;
                }

                if (stl::find_and_erase(mesh.exclusions, volumeID))
                {
                    QueueMeshUpdate(meshID, volume.aabb);
                    ++mesh.version;
                }
            }
        }

        if (gEnv->IsEditor())
        {
            m_volumesManager.InvalidateID(volumeID);
        }

        m_volumes.erase(volumeID);
    }
}

void NavigationSystem::SetVolume(NavigationVolumeID volumeID, Vec3* vertices, size_t vertexCount, float height)
{
    assert(vertices);

    if (volumeID && m_volumes.validate(volumeID))
    {
        bool recomputeAABB = false;

        NavigationBoundingVolume newVolume;
        AABB aabbNew(AABB::RESET);

        newVolume.vertices.reserve(vertexCount);

        for (size_t i = 0; i < vertexCount; ++i)
        {
            aabbNew.Add(vertices[i]);
            newVolume.vertices.push_back(vertices[i]);
        }

        aabbNew.Add(vertices[0] + Vec3(0.0f, 0.0f, height));

        newVolume.height = height;
        newVolume.aabb = aabbNew;

        NavigationBoundingVolume& volume = m_volumes[volumeID];

        if (!volume.vertices.empty())
        {
            AgentTypes::const_iterator it = m_agentTypes.begin();
            AgentTypes::const_iterator end = m_agentTypes.end();

            for (; it != end; ++it)
            {
                const AgentType& agentType = *it;

                AgentType::Meshes::const_iterator mit = agentType.meshes.begin();
                AgentType::Meshes::const_iterator mend = agentType.meshes.end();

                for (; mit != mend; ++mit)
                {
                    const NavigationMeshID meshID = mit->id;
                    NavigationMesh& mesh = m_meshes[meshID];

                    if (mesh.boundary == volumeID)
                    {
                        ++mesh.version;
                        recomputeAABB = true;

                        QueueDifferenceUpdate(meshID, volume, newVolume);
                    }

                    if (std::find(mesh.exclusions.begin(), mesh.exclusions.end(), volumeID) != mesh.exclusions.end())
                    {
                        QueueMeshUpdate(meshID, volume.aabb);
                        QueueMeshUpdate(meshID, aabbNew);
                        ++mesh.version;
                    }
                }
            }
        }

        if (recomputeAABB)
        {
            ComputeWorldAABB();
        }

        newVolume.Swap(volume);
    }
}

bool NavigationSystem::ValidateVolume(NavigationVolumeID volumeID)
{
    return m_volumes.validate(volumeID);
}

NavigationVolumeID NavigationSystem::GetVolumeID(NavigationMeshID meshID)
{
    // This function is used to retrieve the correct ID of the volume boundary connected to the mesh.
    // After restoring the navigation data it could be that the cached volume id in the Sandbox SapeObject
    // is different from the actual one connected to the shape.
    if (meshID && m_meshes.validate(meshID))
    {
        return m_meshes[meshID].boundary;
    }
    else
    {
        return NavigationVolumeID();
    }
}

void NavigationSystem::SetExclusionVolume(const NavigationAgentTypeID* agentTypeIDs, size_t agentTypeIDCount, NavigationVolumeID volumeID)
{
    if (volumeID && m_volumes.validate(volumeID))
    {
        bool recomputeAABB = false;

        NavigationBoundingVolume& volume = m_volumes[volumeID];

        AgentTypes::iterator it = m_agentTypes.begin();
        AgentTypes::iterator end = m_agentTypes.end();

        for (; it != end; ++it)
        {
            AgentType& agentType = *it;

            stl::find_and_erase(agentType.exclusions, volumeID);

            AgentType::Meshes::const_iterator mit = agentType.meshes.begin();
            AgentType::Meshes::const_iterator mend = agentType.meshes.end();

            for (; mit != mend; ++mit)
            {
                const NavigationMeshID meshID = mit->id;
                NavigationMesh& mesh = m_meshes[meshID];

                if (stl::find_and_erase(mesh.exclusions, volumeID))
                {
                    QueueMeshUpdate(meshID, volume.aabb);

                    ++mesh.version;
                }
            }
        }

        for (size_t i = 0; i < agentTypeIDCount; ++i)
        {
            if (agentTypeIDs[i] && (agentTypeIDs[i] <= m_agentTypes.size()))
            {
                AgentType& agentType = m_agentTypes[agentTypeIDs[i] - 1];

                agentType.exclusions.push_back(volumeID);

                AgentType::Meshes::const_iterator mit = agentType.meshes.begin();
                AgentType::Meshes::const_iterator mend = agentType.meshes.end();

                for (; mit != mend; ++mit)
                {
                    const NavigationMeshID meshID = mit->id;
                    NavigationMesh& mesh = m_meshes[meshID];

                    mesh.exclusions.push_back(volumeID);

                    if (mesh.boundary != volumeID)
                    {
                        QueueMeshUpdate(meshID, volume.aabb);
                    }
                    else
                    {
                        ++mesh.version;
                        mesh.boundary = NavigationVolumeID();
                        recomputeAABB = true;
                    }
                }
            }
        }

        if (recomputeAABB)
        {
            ComputeWorldAABB();
        }
    }
}

/*
void NavigationSystem::AddMeshExclusionVolume(NavigationMeshID meshID, NavigationVolumeID volumeID)
{
    if (meshID && volumeID && m_meshes.validate(meshID) && m_volumes.validate(volumeID))
    {
        NavigationMesh& mesh = m_meshes[meshID];

        if (stl::push_back_unique(mesh.exclusions, volumeID))
            ++mesh.version;
    }
}

void NavigationSystem::RemoveMeshExclusionVolume(NavigationMeshID meshID, NavigationVolumeID volumeID)
{
    if (meshID && volumeID && m_meshes.validate(meshID) && m_volumes.validate(volumeID))
    {
        NavigationMesh& mesh = m_meshes[meshID];

        if (stl::find_and_erase(mesh.exclusions, volumeID))
            ++mesh.version;
    }
}
*/
NavigationMeshID NavigationSystem::GetMeshID(const char* name, NavigationAgentTypeID agentTypeID) const
{
    assert(name && agentTypeID && (agentTypeID <= m_agentTypes.size()));

    if (agentTypeID && (agentTypeID <= m_agentTypes.size()))
    {
        uint32 nameHash = NameHash(name);

        const AgentType& agentType = m_agentTypes[agentTypeID - 1];
        AgentType::Meshes::const_iterator it = agentType.meshes.begin();
        AgentType::Meshes::const_iterator end = agentType.meshes.end();

        for (; it != end; ++it)
        {
            if (it->name == nameHash)
            {
                return it->id;
            }
        }
    }

    return NavigationMeshID();
}

const char* NavigationSystem::GetMeshName(NavigationMeshID meshID) const
{
    if (meshID && m_meshes.validate(meshID))
    {
        const NavigationMesh& mesh = m_meshes[meshID];

        return mesh.name.c_str();
    }

    return 0;
}

void NavigationSystem::SetMeshName(NavigationMeshID meshID, const char* name)
{
    if (meshID && m_meshes.validate(meshID))
    {
        NavigationMesh& mesh = m_meshes[meshID];

        mesh.name = name;

        AgentType& agentType = m_agentTypes[mesh.agentTypeID - 1];
        AgentType::Meshes::iterator it = agentType.meshes.begin();
        AgentType::Meshes::iterator end = agentType.meshes.end();

        for (; it != end; ++it)
        {
            if (it->id == meshID)
            {
                it->name = NameHash(name);
                break;
            }
        }
    }
}

void NavigationSystem::ResetAllNavigationSystemUsers()
{
    for (NavigationSystemUsers::Notifier notifier(m_users); notifier.IsValid(); notifier.Next())
    {
        notifier->Reset();
    }
}

void NavigationSystem::WaitForAllNavigationSystemUsersCompleteTheirReadingAsynchronousTasks()
{
    for (NavigationSystemUsers::Notifier notifier(m_users); notifier.IsValid(); notifier.Next())
    {
        notifier->CompleteRunningTasks();
    }
}

INavigationSystem::WorkingState NavigationSystem::GetState() const
{
    return m_state;
}

void NavigationSystem::UpdateNavigationSystemUsersForSynchronousWritingOperations()
{
    for (NavigationSystemUsers::Notifier notifier(m_users); notifier.IsValid(); notifier.Next())
    {
        notifier->UpdateForSynchronousWritingOperations();
    }
}

void NavigationSystem::UpdateNavigationSystemUsersForSynchronousOrAsynchronousReadingOperations()
{
    for (NavigationSystemUsers::Notifier notifier(m_users); notifier.IsValid(); notifier.Next())
    {
        notifier->UpdateForSynchronousOrAsynchronousReadingOperation();
    }
}

void NavigationSystem::UpdateInternalNavigationSystemData(const bool blocking)
{
#if NAVIGATION_SYSTEM_PC_ONLY
    FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);

    CRY_ASSERT_MESSAGE(m_pEditorBackgroundUpdate->IsRunning() == false, "Background update for editor is still running while the application has the focus!!");

    const bool editorBackgroundThreadRunning = m_pEditorBackgroundUpdate->IsRunning();
    if (editorBackgroundThreadRunning)
    {
        AIError("NavigationSystem - Editor background thread is still running, while the main thread is updating, skipping update");
        return;
    }

    // Prevent multiple updates per frame
    static int lastUpdateFrameID = 0;

    const int frameID = gEnv->pRenderer->GetFrameID();
    const bool doUpdate = (frameID != lastUpdateFrameID) && !(editorBackgroundThreadRunning);
    if (doUpdate)
    {
        lastUpdateFrameID = frameID;

        const float frameTime = gEnv->pTimer->GetFrameTime();

        UpdateMeshes(frameTime, blocking, gAIEnv.CVars.NavigationSystemMT != 0, false);
    }
#endif
}

void NavigationSystem::UpdateInternalSubsystems()
{
    m_offMeshNavigationManager.ProcessQueuedRequests();
}

INavigationSystem::WorkingState NavigationSystem::Update(bool blocking)
{
    // Pre update step. We need to request all our NavigationSystem users
    // to complete all their reading jobs.
    WaitForAllNavigationSystemUsersCompleteTheirReadingAsynchronousTasks();

    // step 1: update all the tasks that may write on the NavigationSystem data
    UpdateInternalNavigationSystemData(blocking);
    UpdateInternalSubsystems();

    // step 2: update all the writing tasks that needs to write some data inside the navigation system
    UpdateNavigationSystemUsersForSynchronousWritingOperations();

    // step 3: update all the tasks that needs to read only the NavigationSystem data
    // Those tasks may or may not be asynchronous tasks and the reading of the navigation data
    // may extend in time after this function is actually executed.
    UpdateNavigationSystemUsersForSynchronousOrAsynchronousReadingOperations();

    return m_state;
}

void NavigationSystem::PauseNavigationUpdate()
{
    m_isNavigationUpdatePaused = true;
    m_pEditorBackgroundUpdate->Pause(true);
}

void NavigationSystem::RestartNavigationUpdate()
{
    m_isNavigationUpdatePaused = false;
    m_pEditorBackgroundUpdate->Pause(false);
}

#if NAVIGATION_SYSTEM_PC_ONLY
void NavigationSystem::UpdateMeshes(const float frameTime, const bool blocking, const bool multiThreaded, const bool bBackground)
{
    if (m_isNavigationUpdatePaused || frameTime == .0f)
    {
        return;
    }

    m_debugDraw.UpdateWorkingProgress(frameTime, m_tileQueue.size());

    if (m_tileQueue.empty() && m_runningTasks.empty())
    {
        if (m_state != Idle)
        {
            // We just finished the processing of the tiles, so before being in Idle
            // we need to recompute the Islands detection
            ComputeIslands();
        }
        m_state = Idle;
        return;
    }

    size_t completed = 0;
    size_t cacheHit = 0;

    while (true)
    {
        if (!m_runningTasks.empty())
        {
            FRAME_PROFILER("Navigation System::UpdateMeshes() - Running Task Processing", gEnv->pSystem, PROFILE_AI);

            RunningTasks::iterator it = m_runningTasks.begin();
            RunningTasks::iterator end = m_runningTasks.end();

            for (; it != end; )
            {
                const size_t resultSlot = *it;
                assert(resultSlot < m_results.size());

                TileTaskResult& result = m_results[resultSlot];
                if (result.state == TileTaskResult::Running)
                {
                    ++it;
                    continue;
                }

                CommitTile(result);

                {
                    FRAME_PROFILER("Navigation System::UpdateMeshes() - Running Task Processing - WaitForJob", gEnv->pSystem, PROFILE_AI);

                    result.jobExecutor.WaitForCompletion();
                }

                ++completed;
                cacheHit += result.state == TileTaskResult::NoChanges;

                MNM::Tile().Swap(result.tile);

                VolumeDefCopy& def = m_volumeDefCopy[result.volumeCopy];
                --def.refCount;

                result.state = TileTaskResult::Running;
                result.next = m_free;

                m_free = resultSlot;

                const bool reachedLastTask = (it == m_runningTasks.end() - 1);

                std::swap(*it, m_runningTasks.back());
                m_runningTasks.pop_back();

                if (reachedLastTask)
                {
                    break;  // prevent the invalidated iterator "it" from getting compared in the for-loop
                }
                end = m_runningTasks.end();
            }
        }

        m_throughput = completed / frameTime;
        m_cacheHitRate = cacheHit / frameTime;

        if (m_tileQueue.empty() && m_runningTasks.empty())
        {
            if (m_state != Idle)
            {
                // We just finished the processing of the tiles, so before being in Idle
                // we need to recompute the Islands detection
                ComputeIslands();
            }

            m_state = Idle;

            return;
        }

        if (!m_tileQueue.empty())
        {
            m_state = Working;

            FRAME_PROFILER("Navigation System::UpdateMeshes() - Job Spawning", gEnv->pSystem, PROFILE_AI);
            const size_t idealMinimumTaskCount = 2;
            const size_t MaxRunningTaskCount = multiThreaded ? m_maxRunningTaskCount : std::min(m_maxRunningTaskCount, idealMinimumTaskCount);

            while (!m_tileQueue.empty() && (m_runningTasks.size() < MaxRunningTaskCount))
            {
                TileTask task;
                {
                    AZStd::lock_guard<AZStd::recursive_mutex> tileQueueGuard(m_tileQueueMutex);
                    auto firstTask = m_tileQueue.begin();
                    task = AZStd::move(*firstTask);
                    m_tileQueue.erase(firstTask);
                }

                const NavigationMesh& mesh = m_meshes[task.meshID];
                const MNM::MeshGrid::Params& paramsGrid = mesh.grid.GetParams();

                m_runningTasks.push_back(m_free);
                CRY_ASSERT_MESSAGE(m_free < m_results.size(), "Index out of array bounds!");
                TileTaskResult& result = m_results[m_free];
                m_free = result.next;

                if (!SpawnJob(result, task.meshID, paramsGrid, task.x, task.y, task.z, multiThreaded))
                {
                    result.state = TileTaskResult::Running;
                    result.next = m_free;

                    m_free = m_runningTasks.back();
                    m_runningTasks.pop_back();
                    break;
                }
            }

            // keep main thread busy too if we're blocking
            if (blocking && !m_tileQueue.empty() && multiThreaded)
            {
                while (!m_tileQueue.empty())
                {
                    TileTask task;
                    {
                        AZStd::lock_guard<AZStd::recursive_mutex> tileQueueGuard(m_tileQueueMutex);
                        auto firstTask = m_tileQueue.begin();
                        task = AZStd::move(*firstTask);
                        m_tileQueue.erase(firstTask);
                    }

                    NavigationMesh& mesh = m_meshes[task.meshID];
                    const MNM::MeshGrid::Params& paramsGrid = mesh.grid.GetParams();

                    TileTaskResult result;
                    if (!SpawnJob(result, task.meshID, paramsGrid, task.x, task.y, task.z, false))
                    {
                        break;
                    }

                    CommitTile(result);
                    break;
                }
            }
        }

        if (blocking && (!m_tileQueue.empty() || !m_runningTasks.empty()))
        {
            continue;
        }

        m_state = m_runningTasks.empty() ? Idle : Working;

        return;
    }
}

void NavigationSystem::SetupGenerator(NavigationMeshID meshID, const MNM::MeshGrid::Params& paramsGrid,
    uint16 x, uint16 y, uint16 z, MNM::TileGenerator::Params& params,
    const MNM::BoundingVolume* boundary, const MNM::BoundingVolume* exclusions,
    size_t exclusionCount)
{
    const NavigationMesh& mesh = m_meshes[meshID];

    params.origin = paramsGrid.origin + Vec3i(x * paramsGrid.tileSize.x, y * paramsGrid.tileSize.y, z * paramsGrid.tileSize.z);
    params.voxelSize = paramsGrid.voxelSize;
    params.sizeX = paramsGrid.tileSize.x;
    params.sizeY = paramsGrid.tileSize.y;
    params.sizeZ = paramsGrid.tileSize.z;
    params.boundary = boundary;
    params.exclusions = exclusions;
    params.exclusionCount = exclusionCount;

    const AgentType& agentType = m_agentTypes[mesh.agentTypeID - 1];

    params.agent.radius = agentType.settings.radiusVoxelCount;
    params.agent.height = agentType.settings.heightVoxelCount;
    params.agent.climbableHeight = agentType.settings.climbableVoxelCount;
    params.agent.maxWaterDepth = agentType.settings.maxWaterDepthVoxelCount;
    params.climbableInclineGradient = agentType.settings.climbableInclineGradient;
    params.climbableStepRatio = agentType.settings.climbableStepRatio;
    params.agent.callback = agentType.meshEntityCallback;

    if (MNM::TileID tileID = mesh.grid.GetTileID(x, y, z))
    {
        params.hashValue = mesh.grid.GetTile(tileID).hashValue;
    }
    else
    {
        params.flags |= MNM::TileGenerator::Params::NoHashTest;
    }
}

bool NavigationSystem::SpawnJob(TileTaskResult& result, NavigationMeshID meshID, const MNM::MeshGrid::Params& paramsGrid,
    uint16 x, uint16 y, uint16 z, bool mt)
{
    result.x = x;
    result.y = y;
    result.z = z;

    result.meshID = meshID;
    result.state = TileTaskResult::Running;
    result.hashValue = 0;

    const NavigationMesh& mesh = m_meshes[meshID];

    size_t firstFree = ~0ul;
    size_t index = ~0ul;

    for (size_t i = 0; i < MaxVolumeDefCopyCount; ++i)
    {
        VolumeDefCopy& def = m_volumeDefCopy[i];
        if ((def.meshID == meshID) && (def.version == mesh.version))
        {
            index = i;
            break;
        }
        else if ((firstFree == ~0ul) && !def.refCount)
        {
            firstFree = i;
        }
    }

    if ((index == ~0ul) && (firstFree == ~0ul))
    {
        return false;
    }

    VolumeDefCopy* def = 0;

    if (index != ~0ul)
    {
        def = &m_volumeDefCopy[index];
    }
    else
    {
        index = firstFree;

        def = &m_volumeDefCopy[index];
        def->meshID = meshID;
        def->version = mesh.version;
        def->boundary = m_volumes[mesh.boundary];
        def->exclusions.clear();
        def->exclusions.reserve(mesh.exclusions.size());

        NavigationMesh::ExclusionVolumes::const_iterator eit = mesh.exclusions.begin();
        NavigationMesh::ExclusionVolumes::const_iterator eend = mesh.exclusions.end();

        for (; eit != eend; ++eit)
        {
            const NavigationVolumeID exclusionVolumeID = *eit;

            // Its okay for an exclusion volume to be invalid temporarily during creation - the user may have only clicked 2 points so far.
            if (m_volumes.validate(exclusionVolumeID))
            {
                def->exclusions.push_back(m_volumes[exclusionVolumeID]);
            }
            else if (m_invalidExclusionVolumes.find(exclusionVolumeID) == m_invalidExclusionVolumes.end())
            {
                m_invalidExclusionVolumes.insert(exclusionVolumeID);
                CryLogAlways("NavigationSystem::SpawnJob(): Skipping non-valid exclusion volume (%d) for mesh '%s'", (uint32)exclusionVolumeID, mesh.name.c_str());
            }
        }
    }

    result.volumeCopy = index;

    ++def->refCount;

    MNM::TileGenerator::Params params;

    SetupGenerator(meshID, paramsGrid, x, y, z, params, &def->boundary,
        def->exclusions.empty() ? 0 : &def->exclusions[0], def->exclusions.size());

    if (mt)
    {
        result.jobExecutor.StartJob([params, &result]()
        {
            GenerateTileJob(params, &result.state, &result.tile, &result.hashValue);
        }); // Legacy JobManager priority: eStreamPriority
    }
    else
    {
        GenerateTileJob(params, &result.state, &result.tile, &result.hashValue);
    }

    if (gAIEnv.CVars.DebugDrawNavigation)
    {
        CDebugDrawContext dc;

        dc->DrawAABB(AABB(params.origin, params.origin + Vec3((float)params.sizeX, (float)params.sizeY, (float)params.sizeZ)),
            IDENTITY, false, Col_Red, eBBD_Faceted);
    }

    return true;
}

void NavigationSystem::CommitTile(TileTaskResult& result)
{
    // The mesh for this tile has been destroyed, it doesn't make sense to commit the tile
    const bool nonValidTile = (m_meshes.validate(result.meshID) == false);
    if (nonValidTile)
    {
        return;
    }

    NavigationMesh& mesh = m_meshes[result.meshID];

    switch (result.state)
    {
    case TileTaskResult::Completed:
    {
        FRAME_PROFILER("Navigation System::CommitTile() - Running Task Processing - ConnectToNetwork", gEnv->pSystem, PROFILE_AI);

        MNM::TileID tileID = mesh.grid.SetTile(result.x, result.y, result.z, result.tile);
        mesh.grid.ConnectToNetwork(tileID);

        m_offMeshNavigationManager.RefreshConnections(result.meshID, tileID);
        gAIEnv.pMNMPathfinder->OnNavigationMeshChanged(result.meshID, tileID);

        const AgentType& agentType = m_agentTypes[mesh.agentTypeID - 1];
        AgentType::Callbacks::const_iterator cit = agentType.callbacks.begin();
        AgentType::Callbacks::const_iterator cend = agentType.callbacks.end();

        for (; cit != cend; ++cit)
        {
            const NavigationMeshChangeCallback& callback = *cit;
            callback(mesh.agentTypeID, result.meshID, tileID);
        }
    }
    break;
    case TileTaskResult::Failed:
    {
        FRAME_PROFILER("Navigation System::CommitTile() - Running Task Processing - ClearTile", gEnv->pSystem, PROFILE_AI);

        if (MNM::TileID tileID = mesh.grid.GetTileID(result.x, result.y, result.z))
        {
            mesh.grid.ClearTile(tileID);

            m_offMeshNavigationManager.RefreshConnections(result.meshID, tileID);
            gAIEnv.pMNMPathfinder->OnNavigationMeshChanged(result.meshID, tileID);

            const AgentType& agentType = m_agentTypes[mesh.agentTypeID - 1];
            AgentType::Callbacks::const_iterator cit = agentType.callbacks.begin();
            AgentType::Callbacks::const_iterator cend = agentType.callbacks.end();

            for (; cit != cend; ++cit)
            {
                const NavigationMeshChangeCallback& callback = *cit;
                callback(mesh.agentTypeID, result.meshID, tileID);
            }
        }
    }
    break;
    case TileTaskResult::NoChanges:
        break;
    default:
        assert(0);
        break;
    }
}
#endif

size_t NavigationSystem::QueueMeshUpdate(NavigationMeshID meshID, const AABB& aabb)
{
    assert(meshID != 0);

    size_t affectedCount = 0;
#if NAVIGATION_SYSTEM_PC_ONLY
    if (meshID && m_meshes.validate(meshID))
    {
        FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);

        NavigationMesh& mesh = m_meshes[meshID];
        MNM::MeshGrid& grid = mesh.grid;

        const MNM::MeshGrid::Params& paramsGrid = grid.GetParams();

        if (aabb.IsEmpty() || !mesh.boundary)
        {
            return 0;
        }

        const AABB& boundary = m_volumes[mesh.boundary].aabb;
        const AgentType& agentType = m_agentTypes[mesh.agentTypeID - 1];

        const float extraH = std::max(paramsGrid.voxelSize.x, paramsGrid.voxelSize.y) * (agentType.settings.radiusVoxelCount + 1);
        const float extraV = paramsGrid.voxelSize.z * (agentType.settings.heightVoxelCount + 1);
        const float extraVM = paramsGrid.voxelSize.z; // tiles above are not directly influenced

        Vec3 bmin(std::max(0.0f, std::max(boundary.min.x, aabb.min.x - extraH) - paramsGrid.origin.x),
            std::max(0.0f, std::max(boundary.min.y, aabb.min.y - extraH) - paramsGrid.origin.y),
            std::max(0.0f, std::max(boundary.min.z, aabb.min.z - extraV) - paramsGrid.origin.z));

        Vec3 bmax(std::max(0.0f, std::min(boundary.max.x, aabb.max.x + extraH) - paramsGrid.origin.x),
            std::max(0.0f, std::min(boundary.max.y, aabb.max.y + extraH) - paramsGrid.origin.y),
            std::max(0.0f, std::min(boundary.max.z, aabb.max.z + extraVM) - paramsGrid.origin.z));

        uint16 xmin = (uint16)(floor_tpl(bmin.x / (float)paramsGrid.tileSize.x));
        uint16 xmax = (uint16)(floor_tpl(bmax.x / (float)paramsGrid.tileSize.x));

        uint16 ymin = (uint16)(floor_tpl(bmin.y / (float)paramsGrid.tileSize.y));
        uint16 ymax = (uint16)(floor_tpl(bmax.y / (float)paramsGrid.tileSize.y));

        uint16 zmin = (uint16)(floor_tpl(bmin.z / (float)paramsGrid.tileSize.z));
        uint16 zmax = (uint16)(floor_tpl(bmax.z / (float)paramsGrid.tileSize.z));

        AZStd::lock_guard<AZStd::recursive_mutex> lockGuard(m_tileQueueMutex);
        for (size_t y = ymin; y <= ymax; ++y)
        {
            for (size_t x = xmin; x <= xmax; ++x)
            {
                for (size_t z = zmin; z <= zmax; ++z)
                {
                    TileTask task;
                    task.meshID = meshID;
                    task.x = (uint16)x;
                    task.y = (uint16)y;
                    task.z = (uint16)z;

                    m_tileQueue.insert(task);

                    ++affectedCount;
                }
            }
        }
    }

#endif

    return affectedCount;
}

void NavigationSystem::ProcessQueuedMeshUpdates()
{
#if NAVIGATION_SYSTEM_PC_ONLY
    do
    {
        UpdateMeshes(0.0333f, false, gAIEnv.CVars.NavigationSystemMT != 0, false);
    } while (m_state == INavigationSystem::Working);
#endif
}

void NavigationSystem::QueueDifferenceUpdate(NavigationMeshID meshID, const NavigationBoundingVolume& oldVolume,
    const NavigationBoundingVolume& newVolume)
{
#if NAVIGATION_SYSTEM_PC_ONLY
    // TODO: implement properly by verifying what didn't change
    // since there will be loads of volume-aabb intersection tests,
    // this should be a new job running in a different thread
    // producing an array of all the tiles that need updating
    // which then gets concatenated into m_tileQueue in the main update

    if (meshID && m_meshes.validate(meshID))
    {
        FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);

        NavigationMesh& mesh = m_meshes[meshID];
        MNM::MeshGrid& grid = mesh.grid;

        const MNM::MeshGrid::Params& paramsGrid = grid.GetParams();

        AABB aabb = oldVolume.aabb;
        aabb.Add(newVolume.aabb);

        const AgentType& agentType = m_agentTypes[mesh.agentTypeID - 1];

        const float extraH = std::max(paramsGrid.voxelSize.x, paramsGrid.voxelSize.y) * (agentType.settings.radiusVoxelCount + 1);
        const float extraV = paramsGrid.voxelSize.z * (agentType.settings.heightVoxelCount + 1);
        const float extraVM = paramsGrid.voxelSize.z; // tiles above are not directly influenced

        Vec3 bmin(std::max(0.0f, (aabb.min.x - extraH) - paramsGrid.origin.x),
            std::max(0.0f, (aabb.min.y - extraH) - paramsGrid.origin.y),
            std::max(0.0f, (aabb.min.z - extraV) - paramsGrid.origin.z));

        Vec3 bmax(std::max(0.0f, (aabb.max.x + extraH) - paramsGrid.origin.x),
            std::max(0.0f, (aabb.max.y + extraH) - paramsGrid.origin.y),
            std::max(0.0f, (aabb.max.z + extraVM) - paramsGrid.origin.z));

        uint16 xmin = (uint16)(floor_tpl(bmin.x / (float)paramsGrid.tileSize.x));
        uint16 xmax = (uint16)(floor_tpl(bmax.x / (float)paramsGrid.tileSize.x));

        uint16 ymin = (uint16)(floor_tpl(bmin.y / (float)paramsGrid.tileSize.y));
        uint16 ymax = (uint16)(floor_tpl(bmax.y / (float)paramsGrid.tileSize.y));

        uint16 zmin = (uint16)(floor_tpl(bmin.z / (float)paramsGrid.tileSize.z));
        uint16 zmax = (uint16)(floor_tpl(bmax.z / (float)paramsGrid.tileSize.z));

        AZStd::lock_guard<AZStd::recursive_mutex> tileQueueGuard(m_tileQueueMutex);
        for (size_t y = ymin; y <= ymax; ++y)
        {
            for (size_t x = xmin; x <= xmax; ++x)
            {
                for (size_t z = zmin; z <= zmax; ++z)
                {
                    TileTask task;
                    task.meshID = meshID;
                    task.x = (uint16)x;
                    task.y = (uint16)y;
                    task.z = (uint16)z;

                    m_tileQueue.insert(task);
                }
            }
        }
    }
#endif
}

void NavigationSystem::WorldChanged(const AABB& aabb)
{
#if NAVIGATION_SYSTEM_PC_ONLY
    if (!aabb.IsEmpty() && Overlap::AABB_AABB(m_worldAABB, aabb))
    {
        AgentTypes::const_iterator it = m_agentTypes.begin();
        AgentTypes::const_iterator end = m_agentTypes.end();

        for (; it != end; ++it)
        {
            const AgentType& agentType = *it;

            AgentType::Meshes::const_iterator mit = agentType.meshes.begin();
            AgentType::Meshes::const_iterator mend = agentType.meshes.end();

            for (; mit != mend; ++mit)
            {
                const NavigationMeshID meshID = mit->id;
                const NavigationMesh& mesh = m_meshes[meshID];

                if (mesh.boundary && Overlap::AABB_AABB(aabb, m_volumes[mesh.boundary].aabb))
                {
                    QueueMeshUpdate(meshID, aabb);
                }
            }
        }
    }
#endif
}

void NavigationSystem::StopAllTasks()
{
    for (size_t t = 0; t < m_runningTasks.size(); ++t)
    {
        m_results[m_runningTasks[t]].state = TileTaskResult::Failed;
    }

    for (size_t t = 0; t < m_runningTasks.size(); ++t)
    {
        m_results[m_runningTasks[t]].jobExecutor.WaitForCompletion();
    }

    for (size_t t = 0; t < m_runningTasks.size(); ++t)
    {
        m_results[m_runningTasks[t]].tile.Destroy();
    }

    m_runningTasks.clear();
}

void NavigationSystem::ComputeIslands()
{
    FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);

    m_islandConnectionsManager.Reset();

    AgentTypes::const_iterator it = m_agentTypes.begin();
    AgentTypes::const_iterator end = m_agentTypes.end();

    for (; it != end; ++it)
    {
        const AgentType& agentType = *it;
        AgentType::Meshes::const_iterator itMesh = agentType.meshes.begin();
        AgentType::Meshes::const_iterator endMesh = agentType.meshes.end();
        for (; itMesh != endMesh; ++itMesh)
        {
            if (itMesh->id && m_meshes.validate(itMesh->id))
            {
                NavigationMeshID meshID = itMesh->id;
                MNM::IslandConnections& islandConnections = m_islandConnectionsManager.GetIslandConnections();
                const MNM::OffMeshNavigation& offMeshNavigation = m_offMeshNavigationManager.GetOffMeshNavigationForMesh(meshID);
                NavigationMesh& mesh = m_meshes[meshID];
                mesh.grid.ComputeStaticIslandsAndConnections(meshID, offMeshNavigation, islandConnections);
            }
        }
    }
}

void NavigationSystem::AddIslandConnectionsBetweenTriangles(const NavigationMeshID& meshID, const MNM::TriangleID startingTriangleID,
    const MNM::TriangleID endingTriangleID)
{
    FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);

    if (m_meshes.validate(meshID))
    {
        NavigationMesh& mesh = m_meshes[meshID];
        MNM::Tile::Triangle startingTriangle, endingTriangle;
        if (mesh.grid.GetTriangle(startingTriangleID, startingTriangle))
        {
            if (mesh.grid.GetTriangle(endingTriangleID, endingTriangle))
            {
                MNM::GlobalIslandID startingIslandID(meshID, startingTriangle.islandID);

                MNM::Tile& tile = mesh.grid.GetTile(MNM::ComputeTileID(startingTriangleID));
                for (uint16 l = 0; l < startingTriangle.linkCount; ++l)
                {
                    const MNM::Tile::Link& link = tile.links[startingTriangle.firstLink + l];
                    if (link.side == MNM::Tile::Link::OffMesh)
                    {
                        MNM::GlobalIslandID endingIslandID(meshID, endingTriangle.islandID);
                        MNM::OffMeshNavigation& offMeshNavigation = m_offMeshNavigationManager.GetOffMeshNavigationForMesh(meshID);
                        MNM::OffMeshNavigation::QueryLinksResult linksResult = offMeshNavigation.GetLinksForTriangle(startingTriangleID, link.triangle);
                        while (MNM::WayTriangleData nextTri = linksResult.GetNextTriangle())
                        {
                            if (nextTri.triangleID == endingTriangleID)
                            {
                                const MNM::OffMeshLink* pLink = offMeshNavigation.GetObjectLinkInfo(nextTri.offMeshLinkID);
                                assert(pLink);
                                MNM::IslandConnections::Link islandLink(nextTri.triangleID, nextTri.offMeshLinkID, endingIslandID, pLink->GetEntityIdForOffMeshLink());
                                MNM::IslandConnections& islandConnections = m_islandConnectionsManager.GetIslandConnections();
                                islandConnections.SetOneWayConnectionBetweenIsland(startingIslandID, islandLink);
                            }
                        }
                    }
                }
            }
        }
    }
}

void NavigationSystem::RemoveAllIslandConnectionsForObject(const NavigationMeshID& meshID, const uint32 objectId)
{
    MNM::IslandConnections& islandConnections = m_islandConnectionsManager.GetIslandConnections();
    islandConnections.RemoveAllIslandConnectionsForObject(meshID, objectId);
}

void NavigationSystem::RemoveIslandsConnectionBetweenTriangles(const NavigationMeshID& meshID, const MNM::TriangleID startingTriangleID,
    const MNM::TriangleID endingTriangleID)
{
    FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);

    if (m_meshes.validate(meshID))
    {
        NavigationMesh& mesh = m_meshes[meshID];
        MNM::Tile::Triangle startingTriangle, endingTriangle;
        if (mesh.grid.GetTriangle(startingTriangleID, startingTriangle))
        {
            if (mesh.grid.GetTriangle(endingTriangleID, endingTriangle))
            {
                MNM::GlobalIslandID startingIslandID(meshID, startingTriangle.islandID);

                MNM::Tile& tile = mesh.grid.GetTile(MNM::ComputeTileID(startingTriangleID));
                for (uint16 l = 0; l < startingTriangle.linkCount; ++l)
                {
                    const MNM::Tile::Link& link = tile.links[startingTriangle.firstLink + l];
                    if (link.side == MNM::Tile::Link::OffMesh)
                    {
                        MNM::GlobalIslandID endingIslandID(meshID, endingTriangle.islandID);
                        MNM::OffMeshNavigation& offMeshNavigation = m_offMeshNavigationManager.GetOffMeshNavigationForMesh(meshID);
                        MNM::OffMeshNavigation::QueryLinksResult linksResult = offMeshNavigation.GetLinksForTriangle(startingTriangleID, link.triangle);
                        while (MNM::WayTriangleData nextTri = linksResult.GetNextTriangle())
                        {
                            if (nextTri.triangleID == endingTriangleID)
                            {
                                const MNM::OffMeshLink* pLink = offMeshNavigation.GetObjectLinkInfo(nextTri.offMeshLinkID);
                                assert(pLink);
                                MNM::IslandConnections::Link islandLink(nextTri.triangleID, nextTri.offMeshLinkID, endingIslandID, pLink->GetEntityIdForOffMeshLink());
                                MNM::IslandConnections& islandConnections = m_islandConnectionsManager.GetIslandConnections();
                                islandConnections.RemoveOneWayConnectionBetweenIsland(startingIslandID, islandLink);
                            }
                        }
                    }
                }
            }
        }
    }
}

#if MNM_USE_EXPORT_INFORMATION
void NavigationSystem::ComputeAccessibility(IAIObject* pIAIObject, NavigationAgentTypeID agentTypeId /* = NavigationAgentTypeID(0) */)
{
    const CAIActor* actor = CastToCAIActorSafe(pIAIObject);
    const Vec3 debugLocation = pIAIObject->GetEntity()->GetPos();   // we're using the IEntity's position (not the IAIObject one's), because the CAIObject one's is always some time behind due to the way its private position is queried via BodyInfo -> StanceState -> eye position
    const NavigationAgentTypeID actorTypeId = actor ? actor->GetNavigationTypeID() : NavigationAgentTypeID(0);
    const NavigationAgentTypeID agentTypeIdForAccessibilityCalculation = agentTypeId ? agentTypeId : actorTypeId;
    NavigationMeshID meshId = GetEnclosingMeshID(agentTypeIdForAccessibilityCalculation, debugLocation);

    if (meshId)
    {
        NavigationMesh& mesh = GetMesh(meshId);
        const MNM::MeshGrid::Params& paramsGrid = mesh.grid.GetParams();
        const MNM::OffMeshNavigation& offMeshNavigation = GetOffMeshNavigationManager()->GetOffMeshNavigationForMesh(meshId);

        const MNM::vector3_t origin = MNM::vector3_t(MNM::real_t(paramsGrid.origin.x), MNM::real_t(paramsGrid.origin.y), MNM::real_t(paramsGrid.origin.z));
        const Vec3& voxelSize = mesh.grid.GetParams().voxelSize;
        const MNM::vector3_t seedLocation(MNM::real_t(debugLocation.x), MNM::real_t(debugLocation.y), MNM::real_t(debugLocation.z));

        const uint16 agentHeightUnits = GetAgentHeightInVoxelUnits(agentTypeIdForAccessibilityCalculation);

        const MNM::real_t verticalRange = MNMUtils::CalculateMinVerticalRange(agentHeightUnits, voxelSize.z);
        const MNM::real_t verticalDownwardRange(verticalRange);

        AgentType agentTypeProperties;
        const bool arePropertiesValid = GetAgentTypeProperties(agentTypeIdForAccessibilityCalculation, agentTypeProperties);
        assert(arePropertiesValid);
        const uint16 minZOffsetMultiplier(2);
        const uint16 zOffsetMultiplier = min(minZOffsetMultiplier, agentTypeProperties.settings.heightVoxelCount);
        const MNM::real_t verticalUpwardRange = arePropertiesValid ? MNM::real_t(zOffsetMultiplier * agentTypeProperties.settings.voxelSize.z) : MNM::real_t(.2f);

        MNM::TriangleID seedTriangleID = mesh.grid.GetTriangleAt(seedLocation - origin, verticalDownwardRange, verticalUpwardRange);

        if (seedTriangleID)
        {
            MNM::MeshGrid::AccessibilityRequest inputRequest(seedTriangleID, offMeshNavigation);
            mesh.grid.ComputeAccessibility(inputRequest);
        }
    }
}

void NavigationSystem::ClearAllAccessibility(uint8 resetValue)
{
    AgentTypes::const_iterator it = m_agentTypes.begin();
    AgentTypes::const_iterator end = m_agentTypes.end();

    for (; it != end; ++it)
    {
        const AgentType& agentType = *it;
        AgentType::Meshes::const_iterator itMesh = agentType.meshes.begin();
        AgentType::Meshes::const_iterator endMesh = agentType.meshes.end();
        for (; itMesh != endMesh; ++itMesh)
        {
            if (itMesh->id && m_meshes.validate(itMesh->id))
            {
                NavigationMesh& mesh = m_meshes[itMesh->id];
                mesh.grid.ResetAccessibility(resetValue);
            }
        }
    }
}
#endif

void NavigationSystem::ComputeAccessibility(const Vec3& seedPos, NavigationAgentTypeID agentTypeId /* = NavigationAgentTypeID(0) */, float range /* = 0.f */, EAccessbilityDir dir /* = AccessibilityAway */)
{
#if MNM_USE_EXPORT_INFORMATION
    NavigationMeshID meshId = GetEnclosingMeshID(agentTypeId, seedPos);
    if (meshId)
    {
        NavigationMesh& mesh = GetMesh(meshId);
        const MNM::MeshGrid::Params& paramsGrid = mesh.grid.GetParams();
        const MNM::OffMeshNavigation& offMeshNavigation = GetOffMeshNavigationManager()->GetOffMeshNavigationForMesh(meshId);

        const MNM::vector3_t origin = MNM::vector3_t(paramsGrid.origin);
        const Vec3& voxelSize = mesh.grid.GetParams().voxelSize;
        const MNM::vector3_t seedLocation(seedPos);

        const uint16 agentHeightUnits = GetAgentHeightInVoxelUnits(agentTypeId);

        const MNM::real_t verticalRange = MNMUtils::CalculateMinVerticalRange(agentHeightUnits, voxelSize.z);
        const MNM::real_t verticalDownwardRange(verticalRange);

        AgentType agentTypeProperties;
        const bool arePropertiesValid = GetAgentTypeProperties(agentTypeId, agentTypeProperties);
        assert(arePropertiesValid);
        const uint16 minZOffsetMultiplier(2);
        const uint16 zOffsetMultiplier = min(minZOffsetMultiplier, agentTypeProperties.settings.heightVoxelCount);
        const MNM::real_t verticalUpwardRange = arePropertiesValid ? MNM::real_t(zOffsetMultiplier * agentTypeProperties.settings.voxelSize.z) : MNM::real_t(.2f);

        MNM::TriangleID seedTriangleID = mesh.grid.GetTriangleAt(seedLocation - origin, verticalDownwardRange, verticalUpwardRange);

        if (seedTriangleID)
        {
            MNM::MeshGrid::AccessibilityRequest inputRequest(seedTriangleID, offMeshNavigation);
            mesh.grid.ComputeAccessibility(inputRequest);
        }
    }
#endif
}

void NavigationSystem::CalculateAccessibility()
{
#if MNM_USE_EXPORT_INFORMATION

    bool isThereAtLeastOneSeedPresent = false;

    ClearAllAccessibility(MNM::MeshGrid::eARNotAccessible);

    // Filtering accessibility with actors
    {
        AutoAIObjectIter itActors(gAIEnv.pAIObjectManager->GetFirstAIObject(OBJFILTER_TYPE, AIOBJECT_ACTOR));

        for (; itActors->GetObject(); itActors->Next())
        {
            ComputeAccessibility(itActors->GetObject());
        }
    }

    // Filtering accessibility with Navigation Seeds
    {
        AutoAIObjectIter itNavSeeds(gAIEnv.pAIObjectManager->GetFirstAIObject(OBJFILTER_TYPE, AIOBJECT_NAV_SEED));

        for (; itNavSeeds->GetObject(); itNavSeeds->Next())
        {
            AgentTypes::const_iterator it = m_agentTypes.begin();
            AgentTypes::const_iterator end = m_agentTypes.end();

            for (; it != end; ++it)
            {
                const AgentType& agentType = *it;
                ComputeAccessibility(itNavSeeds->GetObject(), GetAgentTypeID(it->name));
            }
        }
    }

    // Filtering accessibility with Navigation Seed Components
    {
        using LmbrCentral::NavigationSeedRequestsBus;
        NavigationSeedRequestsBus::Broadcast(&NavigationSeedRequestsBus::Events::RecalculateReachabilityAroundSelf);
    }
#endif
}



bool NavigationSystem::IsInUse() const
{
    return m_meshes.size() != 0;
}


MNM::TileID NavigationSystem::GetTileIdWhereLocationIsAtForMesh(NavigationMeshID meshID, const Vec3& location)
{
    NavigationMesh& mesh = GetMesh(meshID);

    const MNM::real_t range = MNM::real_t(1.0f);
    MNM::TriangleID triangleID = mesh.grid.GetTriangleAt(location, range, range);

    return MNM::ComputeTileID(triangleID);
}

void NavigationSystem::GetTileBoundsForMesh(NavigationMeshID meshID, MNM::TileID tileID, AABB& bounds) const
{
    const NavigationMesh& mesh = GetMesh(meshID);
    const MNM::vector3_t coords = mesh.grid.GetTileContainerCoordinates(tileID);

    const MNM::MeshGrid::Params& params = mesh.grid.GetParams();

    Vec3 minPos((float)params.tileSize.x * coords.x.as_float(), (float)params.tileSize.y * coords.y.as_float(), (float)params.tileSize.z * coords.z.as_float());
    minPos += params.origin;

    bounds = AABB(minPos, minPos + Vec3((float)params.tileSize.x, (float)params.tileSize.y, (float)params.tileSize.z));
}

NavigationMesh& NavigationSystem::GetMesh(const NavigationMeshID& meshID)
{
    return const_cast<NavigationMesh&>(static_cast<const NavigationSystem*>(this)->GetMesh(meshID));
}

const NavigationMesh& NavigationSystem::GetMesh(const NavigationMeshID& meshID) const
{
    if (meshID && m_meshes.validate(meshID))
    {
        return m_meshes[meshID];
    }

    assert(0);
    static NavigationMesh dummy(NavigationAgentTypeID(0));

    return dummy;
}

std::tuple<bool, NavigationMeshID, Vec3> NavigationSystem::RaycastWorld(const Vec3& segP0, const Vec3& segP1) const
{
    // Grab all the candidates whose bounding box we intersect
    // note that since agent type is not specified in this api, we need to raycast through all agent types:
    AgentTypes::const_iterator it = m_agentTypes.begin();
    AgentTypes::const_iterator end = m_agentTypes.end();

    std::vector<uint32> candidates;

    for (; it != end; ++it)
    {
        const AgentType& agentType = *it;
        AgentType::Meshes::const_iterator itMesh = agentType.meshes.begin();
        AgentType::Meshes::const_iterator endMesh = agentType.meshes.end();
        for (; itMesh != endMesh; ++itMesh)
        {
            if (itMesh->id && m_meshes.validate(itMesh->id))
            {
                if (m_volumes[m_meshes.get(itMesh->id).boundary].IntersectLineSeg(segP0, segP1).first)
                {
                    candidates.push_back(itMesh->id);
                }
            }
        }
    }

    std::tuple<bool, float, Vec3> minResult = std::make_tuple(false, FLT_MAX, Vec3(IDENTITY));
    uint32 minMeshID = 0;
    for (auto meshID : candidates)
    {
        // Actual collision with meshes
        auto meshResult = m_meshes.get(meshID).grid.RayCastWorld(segP0, segP1);
        if (std::get<0>(meshResult) && std::get<1>(meshResult) < std::get<1>(minResult))
        {
            minResult = meshResult;
            minMeshID = meshID;
        }
    }

    return std::make_tuple(std::get<0>(minResult), (NavigationMeshID)minMeshID, std::get<2>(minResult));
}

NavigationMeshID NavigationSystem::GetEnclosingMeshID(NavigationAgentTypeID agentTypeID, const Vec3& location) const
{
    if (agentTypeID && agentTypeID <= m_agentTypes.size())
    {
        const AgentType& agentType = m_agentTypes[agentTypeID - 1];

        AgentType::Meshes::const_iterator mit = agentType.meshes.begin();
        AgentType::Meshes::const_iterator mend = agentType.meshes.end();

        for (; mit != mend; ++mit)
        {
            const NavigationMeshID meshID = mit->id;
            const NavigationMesh& mesh = m_meshes[meshID];
            const NavigationVolumeID boundaryID = mesh.boundary;

            if (boundaryID && m_volumes[boundaryID].Contains(location))
            {
                return meshID;
            }
        }
    }

    return NavigationMeshID();
}

bool NavigationSystem::IsLocationInMesh(NavigationMeshID meshID, const Vec3& location) const
{
    if (meshID && m_meshes.validate(meshID))
    {
        const NavigationMesh& mesh = m_meshes[meshID];
        const NavigationVolumeID boundaryID = mesh.boundary;

        return (boundaryID && m_volumes[boundaryID].Contains(location));
    }

    return false;
}

MNM::TriangleID NavigationSystem::GetClosestMeshLocation(NavigationMeshID meshID, const Vec3& location, float vrange,
    float hrange, Vec3* meshLocation, float* distSq) const
{
    if (meshID && m_meshes.validate(meshID))
    {
        MNM::vector3_t loc(MNM::real_t(location.x), MNM::real_t(location.y), MNM::real_t(location.z));
        const NavigationMesh& mesh = m_meshes[meshID];
        MNM::real_t verticalRange(vrange);
        if (const MNM::TriangleID enclosingTriID = mesh.grid.GetTriangleAt(loc, verticalRange, verticalRange))
        {
            if (meshLocation)
            {
                *meshLocation = location;
            }

            if (distSq)
            {
                *distSq = 0.0f;
            }

            return enclosingTriID;
        }
        else
        {
            MNM::real_t dSq;
            MNM::vector3_t closest;

            if (const MNM::TriangleID closestTriID = mesh.grid.GetClosestTriangle(loc, MNM::real_t(vrange), MNM::real_t(hrange), &dSq, &closest))
            {
                if (meshLocation)
                {
                    *meshLocation = closest.GetVec3();
                }

                if (distSq)
                {
                    *distSq = dSq.as_float();
                }

                return closestTriID;
            }
        }
    }

    return MNM::TriangleID(0);
}

bool NavigationSystem::GetGroundLocationInMesh(NavigationMeshID meshID, const Vec3& location,
    float vDownwardRange, float hRange, Vec3* meshLocation) const
{
    if (meshID && m_meshes.validate(meshID))
    {
        MNM::vector3_t loc(MNM::real_t(location.x), MNM::real_t(location.y), MNM::real_t(location.z));
        const NavigationMesh& mesh = m_meshes[meshID];
        MNM::real_t verticalRange(vDownwardRange);
        if (const MNM::TriangleID enclosingTriID = mesh.grid.GetTriangleAt(loc, verticalRange, MNM::real_t(0.05f)))
        {
            MNM::vector3_t v0, v1, v2;
            mesh.grid.GetVertices(enclosingTriID, v0, v1, v2);
            MNM::vector3_t closest = ClosestPtPointTriangle(loc, v0, v1, v2);
            if (meshLocation)
            {
                *meshLocation = closest.GetVec3();
            }

            return true;
        }
        else
        {
            MNM::real_t dSq;
            MNM::vector3_t closest;

            if (const MNM::TriangleID closestTriID = mesh.grid.GetClosestTriangle(loc, verticalRange, MNM::real_t(hRange), &dSq, &closest))
            {
                if (meshLocation)
                {
                    *meshLocation = closest.GetVec3();
                }

                return true;
            }
        }
    }

    return false;
}

bool NavigationSystem::AgentTypeSupportSmartObjectUserClass(NavigationAgentTypeID agentTypeID, const char* smartObjectUserClass) const
{
    if (agentTypeID && agentTypeID <= m_agentTypes.size())
    {
        const AgentType& agentType = m_agentTypes[agentTypeID - 1];
        AgentType::SmartObjectUserClasses::const_iterator cit = agentType.smartObjectUserClasses.begin();
        AgentType::SmartObjectUserClasses::const_iterator end = agentType.smartObjectUserClasses.end();

        for (; cit != end; ++cit)
        {
            if (strcmp((*cit).c_str(), smartObjectUserClass))
            {
                continue;
            }

            return true;
        }
    }

    return false;
}

uint16 NavigationSystem::GetAgentRadiusInVoxelUnits(NavigationAgentTypeID agentTypeID) const
{
    if (agentTypeID && agentTypeID <= m_agentTypes.size())
    {
        return m_agentTypes[agentTypeID - 1].settings.radiusVoxelCount;
    }

    return 0;
}

uint16 NavigationSystem::GetAgentHeightInVoxelUnits(NavigationAgentTypeID agentTypeID) const
{
    if (agentTypeID && agentTypeID <= m_agentTypes.size())
    {
        return m_agentTypes[agentTypeID - 1].settings.heightVoxelCount;
    }

    return 0;
}

bool NavigationSystem::TryGetAgentSettings(const char* agentType, const AgentType::Settings*& agentSettings)const
{
    NavigationAgentTypeID agentTypeId = GetAgentTypeID(agentType);
    if (agentTypeId && agentTypeId <= m_agentTypes.size())
    {
        agentSettings = &m_agentTypes[agentTypeId - 1].settings;
        return true;
    }
    return false;
}

bool NavigationSystem::TryGetAgentRadiusData(const char* agentType, Vec3& voxelSize, uint16& radiusInVoxels) const
{
    const AgentType::Settings* agentSettings = nullptr;
    if (TryGetAgentSettings(agentType, agentSettings))
    {
        voxelSize = agentSettings->voxelSize;
        radiusInVoxels = agentSettings->radiusVoxelCount;
        return true;
    }
    return false;
}

void NavigationSystem::Clear()
{
    StopAllTasks();
    SetupTasks();

    AgentTypes::iterator it = m_agentTypes.begin();
    AgentTypes::iterator end = m_agentTypes.end();

    for (; it != end; ++it)
    {
        AgentType& agentType = *it;
        agentType.meshes.clear();
        agentType.exclusions.clear();
    }

    for (size_t i = 0; i < m_meshes.capacity(); ++i)
    {
        if (!m_meshes.index_free(i))
        {
            DestroyMesh(NavigationMeshID(m_meshes.get_index_id(i)));
        }
    }

    for (size_t i = 0; i < m_volumes.capacity(); ++i)
    {
        if (!m_volumes.index_free(i))
        {
            DestroyVolume(NavigationVolumeID(m_volumes.get_index_id(i)));
        }
    }

    m_worldAABB = AABB::RESET;
    {
        AZStd::lock_guard<AZStd::recursive_mutex> tileQueueGuard(m_tileQueueMutex);
        m_tileQueue.clear();
    }
    m_volumeDefCopy.clear();
    m_volumeDefCopy.resize(MaxVolumeDefCopyCount, VolumeDefCopy());

    m_offMeshNavigationManager.Clear();
    m_islandConnectionsManager.Reset();

#if NAVIGATION_SYSTEM_PC_ONLY
    m_invalidExclusionVolumes.clear();
#endif // NAVIGATION_SYSTEM_PC_ONLY

    ResetAllNavigationSystemUsers();
}

void NavigationSystem::ClearAndNotify()
{
    Clear();
    UpdateAllListener(NavigationCleared);

    //////////////////////////////////////////////////////////////////////////
    //After the 'clear' we need to re-enable and register smart objects again

    m_offMeshNavigationManager.Enable();
    gAIEnv.pSmartObjectManager->SoftReset();
}

bool NavigationSystem::ReloadConfig()
{
    Clear();

    XmlNodeRef rootNode = GetISystem()->LoadXmlFromFile(m_configName.c_str());
    if (!rootNode)
    {
        AIWarning("Failed to open XML file '%s'...", m_configName.c_str());

        return false;
    }

    const char* tagName = rootNode->getTag();

    if (!azstricmp(tagName, "Navigation"))
    {
        rootNode->getAttr("version", m_configurationVersion);

        size_t childCount = rootNode->getChildCount();

        for (size_t i = 0; i < childCount; ++i)
        {
            XmlNodeRef childNode = rootNode->getChild(i);

            if (!azstricmp(childNode->getTag(), "AgentTypes"))
            {
                size_t agentTypeCount = childNode->getChildCount();

                for (size_t at = 0; at < agentTypeCount; ++at)
                {
                    XmlNodeRef agentTypeNode = childNode->getChild(at);

                    if (!agentTypeNode->haveAttr("name"))
                    {
                        AIWarning("Missing 'name' attribute for 'AgentType' tag at line %d while parsing NavigationXML '%s'...",
                            agentTypeNode->getLine(), m_configName.c_str());

                        return false;
                    }

                    const char* name = 0;
                    INavigationSystem::CreateAgentTypeParams params;

                    for (size_t attr = 0; attr < (size_t)agentTypeNode->getNumAttributes(); ++attr)
                    {
                        const char* attrName = 0;
                        const char* attrValue = 0;

                        if (!agentTypeNode->getAttributeByIndex(attr, &attrName, &attrValue))
                        {
                            continue;
                        }

                        bool valid = false;
                        if (!azstricmp(attrName, "name"))
                        {
                            if (attrValue && *attrValue)
                            {
                                valid = true;
                                name = attrValue;
                            }
                        }
                        else if (!azstricmp(attrName, "radius"))
                        {
                            int sradius = 0;
                            if (azsscanf(attrValue, "%d", &sradius) == 1 && sradius > 0)
                            {
                                valid = true;
                                params.radiusVoxelCount = sradius;
                            }
                        }
                        else if (!azstricmp(attrName, "height"))
                        {
                            int sheight = 0;
                            if (azsscanf(attrValue, "%d", &sheight) == 1 && sheight > 0)
                            {
                                valid = true;
                                params.heightVoxelCount = sheight;
                            }
                        }
                        else if (!azstricmp(attrName, "climbableHeight"))
                        {
                            int sclimbableheight = 0;
                            if (azsscanf(attrValue, "%d", &sclimbableheight) == 1 && sclimbableheight >= 0)
                            {
                                valid = true;
                                params.climbableVoxelCount = sclimbableheight;
                            }
                        }
                        else if (!azstricmp(attrName, "climbableInclineGradient"))
                        {
                            float sclimbableinclinegradient = 0.f;
                            if (azsscanf(attrValue, "%f", &sclimbableinclinegradient) == 1)
                            {
                                valid = true;
                                params.climbableInclineGradient = sclimbableinclinegradient;
                            }
                        }
                        else if (!azstricmp(attrName, "climbableStepRatio"))
                        {
                            float sclimbablestepratio = 0.f;
                            if (azsscanf(attrValue, "%f", &sclimbablestepratio) == 1)
                            {
                                valid = true;
                                params.climbableStepRatio = sclimbablestepratio;
                            }
                        }
                        else if (!azstricmp(attrName, "maxWaterDepth"))
                        {
                            int smaxwaterdepth = 0;
                            if (azsscanf(attrValue, "%d", &smaxwaterdepth) == 1 && smaxwaterdepth >= 0)
                            {
                                valid = true;
                                params.maxWaterDepthVoxelCount = smaxwaterdepth;
                            }
                        }
                        else if (!azstricmp(attrName, "voxelSize"))
                        {
                            float x, y, z;
                            int c = azsscanf(attrValue, "%g,%g,%g", &x, &y, &z);

                            valid = (c == 1) || (c == 3);
                            if (c == 1)
                            {
                                params.voxelSize = Vec3(x);
                            }
                            else if (c == 3)
                            {
                                params.voxelSize = Vec3(x, y, z);
                            }
                        }
                        else
                        {
                            AIWarning(
                                "Unknown attribute '%s' for '%s' tag found at line %d while parsing Navigation XML '%s'...",
                                attrName, agentTypeNode->getTag(), agentTypeNode->getLine(), m_configName.c_str());

                            return false;
                        }

                        if (!valid)
                        {
                            AIWarning("Invalid '%s' attribute value for '%s' tag at line %d while parsing NavigationXML '%s'...",
                                attrName, agentTypeNode->getTag(), agentTypeNode->getLine(), m_configName.c_str());

                            return false;
                        }
                    }

                    for (size_t childIdx = 0; childIdx < m_agentTypes.size(); ++childIdx)
                    {
                        const AgentType& agentType = m_agentTypes[childIdx];

                        assert(name);

                        if (!azstricmp(agentType.name.c_str(), name))
                        {
                            AIWarning("AgentType '%s' redefinition at line %d while parsing NavigationXML '%s'...",
                                name, agentTypeNode->getLine(), m_configName.c_str());

                            return false;
                        }
                    }

                    // Make sure the agent height is greater than the climable height
                    // because otherwise it can cause an infinite loop in the
                    // navigation algorithm
                    if (params.heightVoxelCount <= params.climbableVoxelCount)
                    {
                        AIWarning("AgentType '%s' height (%d) must be greater than climbable height (%d).",
                            name, params.heightVoxelCount, params.climbableVoxelCount);
                        return false;
                    }

                    NavigationAgentTypeID agentTypeID = CreateAgentType(name, params);
                    if (!agentTypeID)
                    {
                        return false;
                    }

                    //////////////////////////////////////////////////////////////////////////
                    /// Add supported SO classes for this AgentType/Mesh

                    for (size_t childIdx = 0; childIdx < (size_t)agentTypeNode->getChildCount(); ++childIdx)
                    {
                        XmlNodeRef agentTypeChildNode = agentTypeNode->getChild(childIdx);

                        if (!azstricmp(agentTypeChildNode->getTag(), "SmartObjectUserClasses"))
                        {
                            AgentType& agentType = m_agentTypes[agentTypeID - 1];

                            size_t soClassesCount = agentTypeChildNode->getChildCount();
                            agentType.smartObjectUserClasses.reserve(soClassesCount);

                            for (size_t socIdx = 0; socIdx < soClassesCount; ++socIdx)
                            {
                                XmlNodeRef smartObjectClassNode = agentTypeChildNode->getChild(socIdx);

                                if (!azstricmp(smartObjectClassNode->getTag(), "class") && smartObjectClassNode->haveAttr("name"))
                                {
                                    stl::push_back_unique(agentType.smartObjectUserClasses, smartObjectClassNode->getAttr("name"));
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                AIWarning(
                    "Unexpected tag '%s' found at line %d while parsing Navigation XML '%s'...",
                    childNode->getTag(), childNode->getLine(), m_configName.c_str());

                return false;
            }
        }

        return true;
    }
    else
    {
        AIWarning(
            "Unexpected tag '%s' found at line %d while parsing Navigation XML '%s'...",
            rootNode->getTag(), rootNode->getLine(), m_configName.c_str());
    }

    return false;
}

void NavigationSystem::ComputeWorldAABB()
{
    AgentTypes::const_iterator it = m_agentTypes.begin();
    AgentTypes::const_iterator end = m_agentTypes.end();

    m_worldAABB = AABB(AABB::RESET);

    for (; it != end; ++it)
    {
        const AgentType& agentType = *it;

        AgentType::Meshes::const_iterator mit = agentType.meshes.begin();
        AgentType::Meshes::const_iterator mend = agentType.meshes.end();

        for (; mit != mend; ++mit)
        {
            const NavigationMeshID meshID = mit->id;
            const NavigationMesh& mesh = m_meshes[meshID];

            if (mesh.boundary)
            {
                m_worldAABB.Add(m_volumes[mesh.boundary].aabb);
            }
        }
    }
}

void NavigationSystem::SetupTasks()
{
    // the number of parallel tasks while allowing maximization of tile throughput
    // might also slow down the main thread due to extra time processing them but also because
    // the connection of each tile to network is done on the main thread
    // NOTE ChrisR: capped to half the amount. The execution time of a tile job desperately needs to optimized,
    // it cannot not take more than the frame time because it'll stall the system. No clever priority scheme
    // will ever,ever,ever make that efficient.
    // PeteB: Optimized the tile job time enough to make it viable to do more than one per frame. Added CVar
    //        multiplier to allow people to control it based on the speed of their machine.
    m_maxRunningTaskCount = (AZ::JobContext::GetGlobalContext()->GetJobManager().GetNumWorkerThreads() * 3 / 4) * gAIEnv.CVars.NavGenThreadJobs;
    {
        TileTaskResults temp(m_maxRunningTaskCount);
        m_results.swap(temp);
    }

    for (size_t i = 0; i < m_results.size(); ++i)
    {
        m_results[i].next = i + 1;
    }

    m_runningTasks.reserve(m_maxRunningTaskCount);

    m_free = 0;
}


void NavigationSystem::RegisterArea(const char* shapeName)
{
    m_volumesManager.RegisterArea(shapeName);
}

void NavigationSystem::UnRegisterArea(const char* shapeName)
{
    m_volumesManager.UnRegisterArea(shapeName);
}

bool NavigationSystem::IsAreaPresent(const char* shapeName)
{
    return m_volumesManager.IsAreaPresent(shapeName);
}

NavigationVolumeID NavigationSystem::GetAreaId(const char* shapeName) const
{
    return m_volumesManager.GetAreaID(shapeName);
}

void NavigationSystem::SetAreaId(const char* shapeName, NavigationVolumeID id)
{
    m_volumesManager.SetAreaID(shapeName, id);
}

void NavigationSystem::UpdateAreaNameForId(const NavigationVolumeID id, const char* newShapeName)
{
    m_volumesManager.UpdateNameForAreaID(id, newShapeName);
}

void NavigationSystem::StartWorldMonitoring()
{
    m_worldMonitor.Start();
}

void NavigationSystem::StopWorldMonitoring()
{
    m_worldMonitor.Stop();
}

bool NavigationSystem::GetClosestPointInNavigationMesh(const NavigationAgentTypeID agentID, const Vec3& location, float vrange, float hrange, Vec3* meshLocation, float minIslandArea) const
{
    const NavigationMeshID meshID = GetEnclosingMeshID(agentID, location);
    if (meshID && m_meshes.validate(meshID))
    {
        MNM::vector3_t loc(MNM::real_t(location.x), MNM::real_t(location.y), MNM::real_t(location.z));
        const NavigationMesh& mesh = m_meshes[meshID];
        MNM::real_t verticalRange(vrange);

        //first check vertical range, because if we are over navmesh, we want that one
        if (const MNM::TriangleID enclosingTriID = mesh.grid.GetTriangleAt(loc, verticalRange, verticalRange, minIslandArea))
        {
            MNM::vector3_t v0, v1, v2;
            mesh.grid.GetVertices(enclosingTriID, v0, v1, v2);
            MNM::vector3_t closest = ClosestPtPointTriangle(loc, v0, v1, v2);

            if (meshLocation)
            {
                *meshLocation = closest.GetVec3();
            }

            return true;
        }
        else
        {
            MNM::real_t dSq;
            MNM::vector3_t closest;

            if (const MNM::TriangleID closestTriID = mesh.grid.GetClosestTriangle(loc, MNM::real_t(vrange), MNM::real_t(hrange), &dSq, &closest, minIslandArea))
            {
                if (meshLocation)
                {
                    *meshLocation = closest.GetVec3();
                }

                return true;
            }
        }
    }

    return false;
}

bool NavigationSystem::IsLocationValidInNavigationMesh(const NavigationAgentTypeID agentID, const Vec3& location) const
{
    const NavigationMeshID meshID = GetEnclosingMeshID(agentID, location);
    bool isValid = false;
    const float horizontalRange = 1.0f;
    const float verticalRange = 1.0f;
    if (meshID)
    {
        Vec3 locationInMesh(ZERO);
        float accuracy = .0f;
        const MNM::TriangleID triangleID = GetClosestMeshLocation(meshID, location, verticalRange, horizontalRange, &locationInMesh, &accuracy);
        isValid = (triangleID && accuracy == .0f);
    }
    return isValid;
}

bool NavigationSystem::IsPointReachableFromPosition(const NavigationAgentTypeID agentID, const IEntity* pEntityToTestOffGridLinks, const Vec3& startLocation, const Vec3& endLocation) const
{
    const float horizontalRange = 1.0f;
    const float verticalRange = 1.0f;

    MNM::GlobalIslandID startingIslandID;
    const NavigationMeshID startingMeshID = GetEnclosingMeshID(agentID, startLocation);
    if (startingMeshID)
    {
        const MNM::TriangleID triangleID = GetClosestMeshLocation(startingMeshID, startLocation, verticalRange, horizontalRange, NULL, NULL);
        const NavigationMesh& mesh = m_meshes[startingMeshID];
        const MNM::MeshGrid& grid = mesh.grid;
        MNM::Tile::Triangle triangle;
        if (triangleID && grid.GetTriangle(triangleID, triangle) && (triangle.islandID != MNM::Constants::eStaticIsland_InvalidIslandID))
        {
            startingIslandID = MNM::GlobalIslandID(startingMeshID, triangle.islandID);
        }
    }

    MNM::GlobalIslandID endingIslandID;
    const NavigationMeshID endingMeshID = GetEnclosingMeshID(agentID, endLocation);
    if (endingMeshID)
    {
        const MNM::TriangleID triangleID = GetClosestMeshLocation(endingMeshID, endLocation, verticalRange, horizontalRange, NULL, NULL);
        const NavigationMesh& mesh = m_meshes[endingMeshID];
        const MNM::MeshGrid& grid = mesh.grid;
        MNM::Tile::Triangle triangle;
        if (triangleID && grid.GetTriangle(triangleID, triangle) && (triangle.islandID != MNM::Constants::eStaticIsland_InvalidIslandID))
        {
            endingIslandID = MNM::GlobalIslandID(endingMeshID, triangle.islandID);
        }
    }

    return m_islandConnectionsManager.AreIslandsConnected(pEntityToTestOffGridLinks, startingIslandID, endingIslandID);
}

MNM::GlobalIslandID NavigationSystem::GetGlobalIslandIdAtPosition(const NavigationAgentTypeID agentID, const Vec3& location)
{
    const float horizontalRange = 1.0f;
    const float verticalRange = 1.0f;

    MNM::GlobalIslandID startingIslandID;
    const NavigationMeshID startingMeshID = GetEnclosingMeshID(agentID, location);
    if (startingMeshID)
    {
        const MNM::TriangleID triangleID = GetClosestMeshLocation(startingMeshID, location, verticalRange, horizontalRange, NULL, NULL);
        const NavigationMesh& mesh = m_meshes[startingMeshID];
        const MNM::MeshGrid& grid = mesh.grid;
        MNM::Tile::Triangle triangle;
        if (triangleID && grid.GetTriangle(triangleID, triangle) && (triangle.islandID != MNM::Constants::eStaticIsland_InvalidIslandID))
        {
            startingIslandID = MNM::GlobalIslandID(startingMeshID, triangle.islandID);
        }
    }

    return startingIslandID;
}

bool NavigationSystem::IsLocationContainedWithinTriangleInNavigationMesh(const NavigationAgentTypeID agentID, const Vec3& location, float downRange, float upRange) const
{
    if (const NavigationMeshID meshID = GetEnclosingMeshID(agentID, location))
    {
        if (m_meshes.validate(meshID))
        {
            MNM::vector3_t loc(MNM::real_t(location.x), MNM::real_t(location.y), MNM::real_t(location.z));
            const NavigationMesh& mesh = m_meshes[meshID];
            const MNM::TriangleID enclosingTriID = mesh.grid.GetTriangleAt(loc, MNM::real_t(downRange), MNM::real_t(upRange));
            return enclosingTriID != 0;
        }
    }

    return false;
}

MNM::TriangleID NavigationSystem::GetTriangleIDWhereLocationIsAtForMesh(const NavigationAgentTypeID agentID, const Vec3& location)
{
    NavigationMeshID meshId = GetEnclosingMeshID(agentID, location);
    if (meshId)
    {
        NavigationMesh& mesh = GetMesh(meshId);
        const MNM::MeshGrid::Params& paramsGrid = mesh.grid.GetParams();
        const MNM::OffMeshNavigation& offMeshNavigation = GetOffMeshNavigationManager()->GetOffMeshNavigationForMesh(meshId);

        const Vec3& voxelSize = mesh.grid.GetParams().voxelSize;
        const uint16 agentHeightUnits = GetAgentHeightInVoxelUnits(agentID);

        const MNM::real_t verticalRange = MNMUtils::CalculateMinVerticalRange(agentHeightUnits, voxelSize.z);
        const MNM::real_t verticalDownwardRange(verticalRange);

        AgentType agentTypeProperties;
        const bool arePropertiesValid = GetAgentTypeProperties(agentID, agentTypeProperties);
        assert(arePropertiesValid);
        const uint16 minZOffsetMultiplier(2);
        const uint16 zOffsetMultiplier = min(minZOffsetMultiplier, agentTypeProperties.settings.heightVoxelCount);
        const MNM::real_t verticalUpwardRange = arePropertiesValid ? MNM::real_t(zOffsetMultiplier * agentTypeProperties.settings.voxelSize.z) : MNM::real_t(.2f);

        return mesh.grid.GetTriangleAt(location - paramsGrid.origin, verticalDownwardRange, verticalUpwardRange);
    }

    return MNM::TriangleID(0);
}

size_t NavigationSystem::GetTriangleCenterLocationsInMesh(const NavigationMeshID meshID, const Vec3& location, const AABB& searchAABB, Vec3* centerLocations, size_t maxCenterLocationCount, float minIslandArea) const
{
    if (m_meshes.validate(meshID))
    {
        const MNM::vector3_t min(MNM::real_t(searchAABB.min.x), MNM::real_t(searchAABB.min.y), MNM::real_t(searchAABB.min.z));
        const MNM::vector3_t max(MNM::real_t(searchAABB.max.x), MNM::real_t(searchAABB.max.y), MNM::real_t(searchAABB.max.z));
        const NavigationMesh& mesh = m_meshes[meshID];
        const MNM::aabb_t aabb(min, max);
        const size_t maxTriangleCount = 4096;
        MNM::TriangleID triangleIDs[maxTriangleCount];
        const size_t triangleCount = mesh.grid.GetTriangles(aabb, triangleIDs, maxTriangleCount, minIslandArea);
        MNM::Tile::Triangle triangle;

        if (triangleCount > 0)
        {
            MNM::vector3_t a, b, c;

            size_t i = 0;
            size_t num_tris = 0;
            for (i = 0; i < triangleCount; ++i)
            {
                mesh.grid.GetVertices(triangleIDs[i], a, b, c);
                centerLocations[num_tris] = ((a + b + c) * MNM::real_t(0.33333f)).GetVec3();
                num_tris++;

                if (num_tris == maxCenterLocationCount)
                {
                    return num_tris;
                }
            }

            return num_tris;
        }
    }

    return 0;
}

size_t NavigationSystem::GetTriangleBorders(const NavigationMeshID meshID, const AABB& aabb, Vec3* pBorders, size_t maxBorderCount, float minIslandArea) const
{
    size_t numBorders = 0;

    if (m_meshes.validate(meshID))
    {
        const MNM::vector3_t min(MNM::real_t(aabb.min.x), MNM::real_t(aabb.min.y), MNM::real_t(aabb.min.z));
        const MNM::vector3_t max(MNM::real_t(aabb.max.x), MNM::real_t(aabb.max.y), MNM::real_t(aabb.max.z));
        const NavigationMesh& mesh = m_meshes[meshID];
        const MNM::aabb_t aabb(min, max);
        const size_t maxTriangleCount = 4096;
        MNM::TriangleID triangleIDs[maxTriangleCount];
        const size_t triangleCount = mesh.grid.GetTriangles(aabb, triangleIDs, maxTriangleCount, minIslandArea);
        //MNM::Tile::Triangle triangle;

        if (triangleCount > 0)
        {
            MNM::vector3_t verts[3];

            for (size_t i = 0; i < triangleCount; ++i)
            {
                size_t linkedEdges = 0;
                mesh.grid.GetLinkedEdges(triangleIDs[i], linkedEdges);
                mesh.grid.GetVertices(triangleIDs[i], verts[0], verts[1], verts[2]);

                for (size_t e = 0; e < 3; ++e)
                {
                    if ((linkedEdges & (size_t(1) << e)) == 0)
                    {
                        if (pBorders != NULL)
                        {
                            const Vec3 v0 = verts[e].GetVec3();
                            const Vec3 v1 = verts[(e + 1) % 3].GetVec3();
                            const Vec3 vOther = verts[(e + 2) % 3].GetVec3();

                            const Vec3 edge = Vec3(v0 - v1).GetNormalized();
                            const Vec3 otherEdge = Vec3(v0 - vOther).GetNormalized();
                            const Vec3 up = edge.Cross(otherEdge);
                            const Vec3 out = up.Cross(edge);

                            pBorders[numBorders * 3 + 0] = v0;
                            pBorders[numBorders * 3 + 1] = v1;
                            pBorders[numBorders * 3 + 2] = out;
                        }

                        ++numBorders;

                        if (pBorders != NULL && numBorders == maxBorderCount)
                        {
                            return numBorders;
                        }
                    }
                }
            }
        }
    }

    return numBorders;
}

size_t NavigationSystem::GetTriangleInfo(const NavigationMeshID meshID, const AABB& aabb, Vec3* centerLocations, uint32* islandids, size_t max_count, float minIslandArea) const
{
    if (m_meshes.validate(meshID))
    {
        const MNM::vector3_t min(MNM::real_t(aabb.min.x), MNM::real_t(aabb.min.y), MNM::real_t(aabb.min.z));
        const MNM::vector3_t max(MNM::real_t(aabb.max.x), MNM::real_t(aabb.max.y), MNM::real_t(aabb.max.z));
        const NavigationMesh& mesh = m_meshes[meshID];
        const MNM::aabb_t aabb(min, max);
        const size_t maxTriangleCount = 4096;
        MNM::TriangleID triangleIDs[maxTriangleCount];
        const size_t triangleCount = mesh.grid.GetTriangles(aabb, triangleIDs, maxTriangleCount, minIslandArea);
        MNM::Tile::Triangle triangle;

        if (triangleCount > 0)
        {
            MNM::vector3_t a, b, c;

            size_t i = 0;
            size_t num_tris = 0;
            for (i = 0; i < triangleCount; ++i)
            {
                mesh.grid.GetTriangle(triangleIDs[i], triangle);
                mesh.grid.GetVertices(triangleIDs[i], a, b, c);
                centerLocations[num_tris] = ((a + b + c) * MNM::real_t(0.33333f)).GetVec3();
                islandids[num_tris] = triangle.islandID;
                num_tris++;

                if (num_tris == max_count)
                {
                    return num_tris;
                }
            }

            return num_tris;
        }
    }

    return 0;
}

bool NavigationSystem::ReadFromFile(const char* fileName, bool bAfterExporting)
{
    bool fileLoaded = false;

    m_pEditorBackgroundUpdate->Pause(true);

    CCryFile file;
    if (false != file.Open(fileName, "rb"))
    {
        bool fileVersionCompatible = true;

        uint16 nFileVersion = BAI_NAVIGATION_FILE_VERSION;
        file.ReadType(&nFileVersion);

        //Verify version of exported file in first place
        if (nFileVersion != BAI_NAVIGATION_FILE_VERSION)
        {
            AIWarning("Wrong BAI file version (found %d expected %d)!! Regenerate Navigation data in the editor.",
                nFileVersion, BAI_NAVIGATION_FILE_VERSION);

            fileVersionCompatible = false;
        }
        else
        {
            uint32 nConfigurationVersion = 0;
            file.ReadType(&nConfigurationVersion);

            if (nConfigurationVersion != m_configurationVersion)
            {
                AIWarning("Navigation.xml config version mismatch (found %d expected %d)!! Regenerate Navigation data in the editor.",
                    nConfigurationVersion, m_configurationVersion);

                // In the launcher we still read the navigation data even if the configuration file
                // contains different version than the exported one
                if (gEnv->IsEditor())
                {
                    fileVersionCompatible = false;
                }
            }
        }

        std::vector<Vec3> boundaryVertexBuffer;
        boundaryVertexBuffer.reserve(32);

        if (fileVersionCompatible)
        {
            // Reading areas Names/ID
            uint32 areasCount = 0;
            file.ReadType(&areasCount);
            for (uint32 i = 0; i < areasCount; ++i)
            {
                uint32 areaNameLength = 0;
                char areaName[MAX_NAME_LENGTH];
                file.ReadType(&areaNameLength);
                areaNameLength = std::min(areaNameLength, (uint32)MAX_NAME_LENGTH - 1);
                file.ReadType(areaName, areaNameLength);
                areaName[areaNameLength] = '\0';
                uint32 areaIDUint32 = 0;
                file.ReadType(&areaIDUint32);

                if (gEnv->IsEditor())
                {
                    // if this area has not yet been registered, register it now
                    if (!m_volumesManager.IsAreaPresent(areaName))
                    {
                        m_volumesManager.RegisterArea(areaName);
                    }

                    m_volumesManager.SetAreaID(areaName, NavigationVolumeID(areaIDUint32));
                }
            }

            uint32 agentsCount;
            file.ReadType(&agentsCount);
            for (uint32 i = 0; i < agentsCount; ++i)
            {
                uint32 nameLength;
                file.ReadType(&nameLength);
                char agentName[MAX_NAME_LENGTH];
                nameLength = std::min(nameLength, (uint32)MAX_NAME_LENGTH - 1);
                file.ReadType(agentName, nameLength);
                agentName[nameLength] = '\0';

                // Reading total amount of memory used for the current agent
                uint32 totalAgentMemory = 0;
                file.ReadType(&totalAgentMemory);

                const size_t fileSeekPositionForNextAgent = file.GetPosition() + totalAgentMemory;
                const NavigationAgentTypeID agentTypeID = GetAgentTypeID(agentName);
                if (agentTypeID == 0)
                {
                    AIWarning("The agent '%s' doesn't exist between the ones loaded from the Navigation.xml", agentName);
                    file.Seek(fileSeekPositionForNextAgent, SEEK_SET);
                    continue;
                }
                // ---------------------------------------------
                // Reading navmesh for the different agents type

                uint32 meshesCount = 0;
                file.ReadType(&meshesCount);

                for (uint32 meshCounter = 0; meshCounter < meshesCount; ++meshCounter)
                {
                    // Reading mesh id
                    uint32 meshIDuint32 = 0;
                    file.ReadType(&meshIDuint32);

                    // Reading mesh name
                    uint32 meshNameLength = 0;
                    file.ReadType(&meshNameLength);
                    char meshName[MAX_NAME_LENGTH];
                    meshNameLength = std::min(meshNameLength, (uint32)MAX_NAME_LENGTH - 1);
                    file.ReadType(meshName, meshNameLength);
                    meshName[meshNameLength] = '\0';

                    // Reading the amount of islands in the mesh
                    MNM::StaticIslandID totalIslands = 0;
                    file.ReadType(&totalIslands);

                    // Reading total mesh memory
                    uint32 totalMeshMemory = 0;
                    file.ReadType(&totalMeshMemory);

                    const size_t fileSeekPositionForNextMesh = file.GetPosition() + totalMeshMemory;

                    if (gEnv->IsEditor() && !m_volumesManager.IsAreaPresent(meshName))
                    {
                        file.Seek(fileSeekPositionForNextMesh, SEEK_SET);
                        continue;
                    }

                    // Reading mesh boundary
                    uint32 boundaryIDuint32 = 0;
                    file.ReadType(&boundaryIDuint32);
                    NavigationVolumeID boundaryID = NavigationVolumeID(boundaryIDuint32);

                    //Saving the volume used by the boundary
                    MNM::BoundingVolume volume;
                    file.ReadType(&(volume.height));
                    uint32 totalVertices = 0;
                    file.ReadType(&totalVertices);
                    boundaryVertexBuffer.clear();
                    for (uint32 vertexCounter = 0; vertexCounter < totalVertices; ++vertexCounter)
                    {
                        Vec3 vertex(ZERO);
                        file.ReadType(&vertex);
                        boundaryVertexBuffer.push_back(vertex);
                    }

                    if (!m_volumes.validate(boundaryID))
                    {
                        CreateVolume(&boundaryVertexBuffer[0], totalVertices, volume.height, boundaryID);
                    }

                    // Reading mesh exclusion shapes
                    uint32 exclusionShapesCount = 0;
                    file.ReadType(&exclusionShapesCount);
                    AgentType::ExclusionVolumes exclusions;
                    for (uint32 exclusionsCounter = 0; exclusionsCounter < exclusionShapesCount; ++exclusionsCounter)
                    {
                        uint32 exclusionIDuint32 = 0;
                        file.ReadType(&exclusionIDuint32);
                        NavigationVolumeID exclusionId = NavigationVolumeID(exclusionIDuint32);
                        // Save the exclusion shape with the read ID
                        exclusions.push_back(exclusionId);
                    }

                    // Reading tile count
                    uint32 tilesCount = 0;
                    file.ReadType(&tilesCount);

                    // Reading mesh params
                    MNM::MeshGrid::Params params;
                    file.ReadType(&(params.origin.x));
                    file.ReadType(&(params.origin.y));
                    file.ReadType(&(params.origin.z));
                    file.ReadType(&(params.tileSize.x));
                    file.ReadType(&(params.tileSize.y));
                    file.ReadType(&(params.tileSize.z));
                    file.ReadType(&(params.voxelSize.x));
                    file.ReadType(&(params.voxelSize.y));
                    file.ReadType(&(params.voxelSize.z));
                    file.ReadType(&(params.tileCount));
                    // If we are full reloading the mnm then we also want to create a new grid with the parameters
                    // written in the file

                    CreateMeshParams createParams;
                    createParams.origin = params.origin;
                    createParams.tileSize = params.tileSize;
                    createParams.tileCount = tilesCount;

                    NavigationMeshID newMeshID = NavigationMeshID(meshIDuint32);
                    if (!m_meshes.validate(meshIDuint32))
                    {
                        newMeshID = CreateMesh(meshName, agentTypeID, createParams, newMeshID);
                    }

                    if (newMeshID == 0)
                    {
                        AIWarning("Unable to create mesh '%s'", meshName);
                        file.Seek(fileSeekPositionForNextMesh, SEEK_SET);
                        continue;
                    }

                    if (newMeshID != meshIDuint32)
                    {
                        AIWarning("The restored mesh has a different ID compared to the saved one.");
                    }

                    NavigationMesh& mesh = m_meshes[newMeshID];
                    SetMeshBoundaryVolume(newMeshID, boundaryID);

                    mesh.exclusions = exclusions;
                    mesh.grid.SetTotalIslands(totalIslands);
                    for (uint32 j = 0; j < tilesCount; ++j)
                    {
                        // Reading Tile indexes
                        uint16 x, y, z;
                        uint32 hashValue;
                        file.ReadType(&x);
                        file.ReadType(&y);
                        file.ReadType(&z);
                        file.ReadType(&hashValue);

                        // Reading triangles
                        uint16 triangleCount = 0;
                        file.ReadType(&triangleCount);
                        MNM::Tile::Triangle* pTriangles = NULL;
                        if (triangleCount)
                        {
                            pTriangles = new MNM::Tile::Triangle[triangleCount];
                            file.ReadType(pTriangles, triangleCount);
                        }

                        // Reading Vertices
                        uint16 vertexCount = 0;
                        file.ReadType(&vertexCount);
                        MNM::Tile::Vertex* pVertices = NULL;
                        if (vertexCount)
                        {
                            pVertices = new MNM::Tile::Vertex[vertexCount];
                            file.ReadType(pVertices, vertexCount);
                        }

                        // Reading Links
                        uint16 linkCount;
                        file.ReadType(&linkCount);
                        MNM::Tile::Link* pLinks = NULL;
                        if (linkCount)
                        {
                            pLinks = new MNM::Tile::Link[linkCount];
                            file.ReadType(pLinks, linkCount);
                        }

                        // Reading nodes
                        uint16 nodeCount;
                        file.ReadType(&nodeCount);
                        MNM::Tile::BVNode* pNodes = NULL;
                        if (nodeCount)
                        {
                            pNodes = new MNM::Tile::BVNode[nodeCount];
                            file.ReadType(pNodes, nodeCount);
                        }

                        // Creating and swapping the tile
                        MNM::Tile tile = MNM::Tile();
                        tile.triangleCount = triangleCount;
                        tile.triangles = pTriangles;
                        tile.vertexCount = vertexCount;
                        tile.vertices = pVertices;
                        tile.linkCount = linkCount;
                        tile.links = pLinks;
                        tile.nodeCount = nodeCount;
                        tile.nodes = pNodes;
                        tile.hashValue = hashValue;
                        mesh.grid.SetTile(x, y, z, tile);
                    }
                }
            }

            fileLoaded = true;
        }

        file.Close();
    }

    ENavigationEvent navigationEvent = (bAfterExporting) ? MeshReloadedAfterExporting : MeshReloaded;
    UpdateAllListener(navigationEvent);

    m_offMeshNavigationManager.OnNavigationLoadedComplete();

    m_pEditorBackgroundUpdate->Pause(false);

    return fileLoaded;
}

//////////////////////////////////////////////////////////////////////////
/// From the original tile, it generates triangles and links with no off-mesh information
/// Returns the new number of links (triangle count is the same)
#define DO_FILTERING_CONSISTENCY_CHECK  0

uint16 FilterOffMeshLinksForTile(const MNM::Tile& tile, MNM::Tile::Triangle* pTrianglesBuffer, uint16 trianglesBufferSize, MNM::Tile::Link* pLinksBuffer, uint16 linksBufferSize)
{
    assert(pTrianglesBuffer);
    assert(pLinksBuffer);
    assert(tile.triangleCount <= trianglesBufferSize);
    assert(tile.linkCount <= linksBufferSize);

    uint16 newLinkCount = 0;
    uint16 offMeshLinksCount = 0;

    if (tile.links)
    {
        //Re-adjust link indices for triangles
        for (uint16 t = 0; t < tile.triangleCount; ++t)
        {
            const MNM::Tile::Triangle& triangle = tile.triangles[t];
            pTrianglesBuffer[t] = triangle;

            pTrianglesBuffer[t].firstLink = triangle.firstLink - offMeshLinksCount;

            if ((triangle.linkCount > 0) && (tile.links[triangle.firstLink].side == MNM::Tile::Link::OffMesh))
            {
                pTrianglesBuffer[t].linkCount--;
                offMeshLinksCount++;
            }
        }

        //Now copy links except off-mesh ones
        for (uint16 l = 0; l < tile.linkCount; ++l)
        {
            const MNM::Tile::Link& link = tile.links[l];

            if (link.side != MNM::Tile::Link::OffMesh)
            {
                pLinksBuffer[newLinkCount] = link;
                newLinkCount++;
            }
        }
    }
    else
    {
        //Just copy the triangles as they are
        memcpy(pTrianglesBuffer, tile.triangles, sizeof(MNM::Tile::Triangle) * tile.triangleCount);
    }

#if DO_FILTERING_CONSISTENCY_CHECK
    if (newLinkCount > 0)
    {
        for (uint16 i = 0; i < tile.triangleCount; ++i)
        {
            if ((pTrianglesBuffer[i].firstLink + pTrianglesBuffer[i].linkCount) > newLinkCount)
            {
                DEBUG_BREAK;
            }
        }
    }
#endif

    assert(newLinkCount == (tile.linkCount - offMeshLinksCount));

    return newLinkCount;
}

bool NavigationSystem::SaveToFile(const char* fileName) const PREFAST_SUPPRESS_WARNING(6262)
{
#if NAVIGATION_SYSTEM_PC_ONLY

    m_pEditorBackgroundUpdate->Pause(true);

    CCryFile file;
    if (false != file.Open(fileName, "wb"))
    {
        bool issuedDataCorruptionWarning = false;

        const int maxTriangles = 1024;
        const int maxLinks = maxTriangles * 6;
        MNM::Tile::Triangle triangleBuffer[maxTriangles];
        MNM::Tile::Link     linkBuffer[maxLinks];

        // Saving file data version
        uint16 nFileVersion = BAI_NAVIGATION_FILE_VERSION;
        file.Write(&nFileVersion, sizeof(nFileVersion));
        file.Write(&m_configurationVersion, sizeof(m_configurationVersion));

        // Saving areas Names/ID
        std::vector<string> areas;
        m_volumesManager.GetVolumesNames(areas);
        uint32 areasCount = areas.size();
        file.Write(&areasCount, sizeof(areasCount));
        std::vector<string>::iterator areaIt = areas.begin();
        std::vector<string>::iterator areaEnd = areas.end();
        for (; areaIt != areaEnd; ++areaIt)
        {
            uint32 areaIDUint32 = m_volumesManager.GetAreaID(areaIt->c_str());
            uint32 areaNameLength = areaIt->length();
            areaNameLength = std::min(areaNameLength, (uint32)MAX_NAME_LENGTH - 1);
            file.Write(&areaNameLength, sizeof(areaNameLength));
            file.Write((areaIt->c_str()), sizeof(char) * areaNameLength);
            file.Write(&areaIDUint32, sizeof(areaIDUint32));
        }

        // Saving number of agents
        uint32 agentsCount = static_cast<uint32>(GetAgentTypeCount());
        file.Write(&agentsCount, sizeof(agentsCount));
        std::vector<string> agentsNamesList;

        AgentTypes::const_iterator typeIt = m_agentTypes.begin();
        AgentTypes::const_iterator typeEnd = m_agentTypes.end();

        for (; typeIt != typeEnd; ++typeIt)
        {
            const AgentType& agentType = *typeIt;
            uint32 nameLength = agentType.name.length();
            nameLength = std::min(nameLength, (uint32)MAX_NAME_LENGTH - 1);
            // Saving name length and the name itself
            file.Write(&nameLength, sizeof(nameLength));
            file.Write(agentType.name.c_str(), sizeof(char) * nameLength);

            // Saving the amount of memory this agent is using inside the file to be able to skip it during loading
            uint32 totalAgentMemory = 0;
            size_t totalAgentMemoryPositionInFile = file.GetPosition();
            file.Write(&totalAgentMemory, sizeof(totalAgentMemory));

            AgentType::Meshes::const_iterator mit = agentType.meshes.begin();
            AgentType::Meshes::const_iterator mend = agentType.meshes.end();
            uint32 meshesCount = agentType.meshes.size();
            file.Write(&meshesCount, sizeof(meshesCount));

            for (; mit != mend; ++mit)
            {
                const uint32 meshIDuint32 = mit->id;
                const NavigationMesh& mesh = m_meshes[NavigationMeshID(meshIDuint32)];
                const MNM::BoundingVolume& volume = m_volumes[mesh.boundary];
                const MNM::MeshGrid& grid = mesh.grid;

                // Saving mesh id
                file.Write(&meshIDuint32, sizeof(meshIDuint32));

                // Saving mesh name
                uint32 meshNameLength = mesh.name.length();
                meshNameLength = std::min(meshNameLength, (uint32)MAX_NAME_LENGTH - 1);
                file.Write(&meshNameLength, sizeof(meshNameLength));
                file.Write(mesh.name.c_str(), sizeof(char) * meshNameLength);

                // Saving total islands
                uint32 totalIslands = mesh.grid.GetTotalIslands();
                file.Write(&totalIslands, sizeof(totalIslands));

                uint32 totalMeshMemory = 0;
                size_t totalMeshMemoryPositionInFile = file.GetPosition();
                file.Write(&totalMeshMemory, sizeof(totalMeshMemory));

                // Saving mesh boundary id
                uint32 boundaryIDuint32 = mesh.boundary;
                /*
                    Let's check if this boundary id matches the id of the
                    volume stored in the volumes manager.
                    It's an additional check for the consistency of the
                    saved binary data.
                */
                if (m_volumesManager.GetAreaID(mesh.name.c_str()) != mesh.boundary && !issuedDataCorruptionWarning)
                {
                    issuedDataCorruptionWarning = true;
                    CryMessageBox("Sandbox detected a possible data corruption during the save of the navigation mesh."
                        "Delete the level.pak file, then re-load and re-export to engine to fix"
                        " the binary data associated with the MNM.", "Navigation Save Error", 0);
                }
                file.Write(&(boundaryIDuint32), sizeof(boundaryIDuint32));

                // Saving the volume used by the boundary
                file.Write(&(volume.height), sizeof(volume.height));
                uint32 totalVertices = volume.vertices.size();
                file.Write(&totalVertices, sizeof(totalVertices));
                MNM::BoundingVolume::Boundary::const_iterator vertIt = volume.vertices.begin();
                MNM::BoundingVolume::Boundary::const_iterator vertEnd = volume.vertices.end();
                for (; vertIt != vertEnd; ++vertIt)
                {
                    const Vec3& vertex = *vertIt;
                    file.Write(&vertex, sizeof(vertex));
                }

                // Saving mesh exclusion shapes
                uint32 exclusionShapesCount = mesh.exclusions.size();
                file.Write(&exclusionShapesCount, sizeof(exclusionShapesCount));
                for (uint32 exclusionCounter = 0; exclusionCounter < exclusionShapesCount; ++exclusionCounter)
                {
                    uint32 exclusionIDuint32 = mesh.exclusions[exclusionCounter];
                    file.Write(&(exclusionIDuint32), sizeof(exclusionIDuint32));
                }

                // Saving tiles count
                uint32 tileCount = grid.GetTileCount();
                file.Write(&tileCount, sizeof(tileCount));

                // Saving grid params (Not all of this params are actually important to recreate the mesh but
                // we save all for possible further utilization)
                const MNM::MeshGrid::Params paramsGrid = grid.GetParams();
                file.Write(&(paramsGrid.origin.x), sizeof(paramsGrid.origin.x));
                file.Write(&(paramsGrid.origin.y), sizeof(paramsGrid.origin.y));
                file.Write(&(paramsGrid.origin.z), sizeof(paramsGrid.origin.z));
                file.Write(&(paramsGrid.tileSize.x), sizeof(paramsGrid.tileSize.x));
                file.Write(&(paramsGrid.tileSize.y), sizeof(paramsGrid.tileSize.y));
                file.Write(&(paramsGrid.tileSize.z), sizeof(paramsGrid.tileSize.z));
                file.Write(&(paramsGrid.voxelSize.x), sizeof(paramsGrid.voxelSize.x));
                file.Write(&(paramsGrid.voxelSize.y), sizeof(paramsGrid.voxelSize.y));
                file.Write(&(paramsGrid.voxelSize.z), sizeof(paramsGrid.voxelSize.z));
                file.Write(&(paramsGrid.tileCount), sizeof(paramsGrid.tileCount));

                const AABB& boundary = m_volumes[mesh.boundary].aabb;

                Vec3 bmin(std::max(0.0f, boundary.min.x - paramsGrid.origin.x),
                    std::max(0.0f, boundary.min.y - paramsGrid.origin.y),
                    std::max(0.0f, boundary.min.z - paramsGrid.origin.z));

                Vec3 bmax(std::max(0.0f, boundary.max.x - paramsGrid.origin.x),
                    std::max(0.0f, boundary.max.y - paramsGrid.origin.y),
                    std::max(0.0f, boundary.max.z - paramsGrid.origin.z));

                uint16 xmin = (uint16)(floor_tpl(bmin.x / (float)paramsGrid.tileSize.x));
                uint16 xmax = (uint16)(floor_tpl(bmax.x / (float)paramsGrid.tileSize.x));

                uint16 ymin = (uint16)(floor_tpl(bmin.y / (float)paramsGrid.tileSize.y));
                uint16 ymax = (uint16)(floor_tpl(bmax.y / (float)paramsGrid.tileSize.y));

                uint16 zmin = (uint16)(floor_tpl(bmin.z / (float)paramsGrid.tileSize.z));
                uint16 zmax = (uint16)(floor_tpl(bmax.z / (float)paramsGrid.tileSize.z));


                for (uint16 x = xmin; x < xmax + 1; ++x)
                {
                    for (uint16 y = ymin; y < ymax + 1; ++y)
                    {
                        for (uint16 z = zmin; z < zmax + 1; ++z)
                        {
                            MNM::TileID i = grid.GetTileID(x, y, z);
                            // Skipping tile id that are not used (This should never happen now)
                            if (i == 0)
                            {
                                continue;
                            }

                            // Saving tile indexes
                            file.Write(&x, sizeof(x));
                            file.Write(&y, sizeof(y));
                            file.Write(&z, sizeof(z));
                            const MNM::Tile& tile = grid.GetTile(i);
                            file.Write(&(tile.hashValue), sizeof(tile.hashValue));

                            const uint16 saveLinkCount = FilterOffMeshLinksForTile(tile, triangleBuffer, maxTriangles, linkBuffer, maxLinks);

                            // Saving triangles
                            MNM::Tile::Triangle* pTriangles = tile.triangles;
                            file.Write(&(tile.triangleCount), sizeof(tile.triangleCount));
                            file.Write(triangleBuffer, sizeof(MNM::Tile::Triangle) * tile.triangleCount);

                            // Saving vertices
                            MNM::Tile::Vertex* pVertices = tile.vertices;
                            file.Write(&(tile.vertexCount), sizeof(tile.vertexCount));
                            file.Write(pVertices, sizeof(MNM::Tile::Vertex) * tile.vertexCount);

                            // Saving links
                            MNM::Tile::Link* pLinks = tile.links;
                            file.Write(&(saveLinkCount), sizeof(tile.linkCount));
                            file.Write(linkBuffer, sizeof(MNM::Tile::Link) * saveLinkCount);

                            // Saving nodes
                            MNM::Tile::BVNode* pNodes = tile.nodes;
                            file.Write(&(tile.nodeCount), sizeof(tile.nodeCount));
                            file.Write(pNodes, sizeof(MNM::Tile::BVNode) * tile.nodeCount);
                        }
                    }
                }
                size_t endingMeshDataPosition = file.GetPosition();
                totalMeshMemory = endingMeshDataPosition - totalMeshMemoryPositionInFile - sizeof(totalMeshMemory);
                file.Seek(totalMeshMemoryPositionInFile, SEEK_SET);
                file.Write(&totalMeshMemory, sizeof(totalMeshMemory));
                file.Seek(endingMeshDataPosition, SEEK_SET);
            }

            size_t endingAgentDataPosition = file.GetPosition();
            totalAgentMemory = endingAgentDataPosition - totalAgentMemoryPositionInFile - sizeof(totalAgentMemory);
            file.Seek(totalAgentMemoryPositionInFile, SEEK_SET);
            file.Write(&totalAgentMemory, sizeof(totalAgentMemory));
            file.Seek(endingAgentDataPosition, SEEK_SET);
        }
        file.Close();
    }

    m_pEditorBackgroundUpdate->Pause(false);

#endif
    return true;
}

void NavigationSystem::UpdateAllListener(const ENavigationEvent event)
{
    for (NavigationListeners::Notifier notifier(m_listenersList); notifier.IsValid(); notifier.Next())
    {
        notifier->OnNavigationEvent(event);
    }
}

void NavigationSystem::DebugDraw()
{
    m_debugDraw.DebugDraw(*this);
}

void NavigationSystem::Reset()
{
    ResetAllNavigationSystemUsers();
}

void NavigationSystem::GetMemoryStatistics(ICrySizer* pSizer)
{
#if DEBUG_MNM_ENABLED
    size_t totalMemory = 0;
    for (size_t i = 0; i < m_meshes.capacity(); ++i)
    {
        if (!m_meshes.index_free(i))
        {
            const NavigationMeshID meshID(m_meshes.get_index_id(i));

            const NavigationMesh& mesh = m_meshes[meshID];
            const MNM::OffMeshNavigation& offMeshNavigation = m_offMeshNavigationManager.GetOffMeshNavigationForMesh(meshID);
            const AgentType& agentType = m_agentTypes[mesh.agentTypeID - 1];

            const NavigationMesh::ProfileMemoryStats meshMemStats = mesh.GetMemoryStats(pSizer);
            const MNM::OffMeshNavigation::ProfileMemoryStats offMeshMemStats = offMeshNavigation.GetMemoryStats(pSizer);

            totalMemory = meshMemStats.totalNavigationMeshMemory + offMeshMemStats.totalSize;
        }
    }
#endif
}

void NavigationSystem::SetDebugDisplayAgentType(NavigationAgentTypeID agentTypeID)
{
    m_debugDraw.SetAgentType(agentTypeID);
}

NavigationAgentTypeID NavigationSystem::GetDebugDisplayAgentType() const
{
    return m_debugDraw.GetAgentType();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


#if DEBUG_MNM_ENABLED

#if defined(WIN32) || defined(WIN64)
#include <CryWindows.h>
#endif

// Only needed for the NavigationSystemDebugDraw
#include "Navigation/PathHolder.h"
#include "Navigation/MNM/OffGridLinks.h"

void NavigationSystemDebugDraw::DebugDraw(NavigationSystem& navigationSystem)
{
    const bool validDebugAgent = m_agentTypeID && (m_agentTypeID <= navigationSystem.GetAgentTypeCount());

    if (validDebugAgent)
    {
        static int lastFrameID = 0;
        
        //Early out if the nav mesh is asked to draw twice in the same frame,
        //UNLESS we're in VR; in that case we need to allow it so that it will
        //draw in both eyes. Important since we use the nav mesh for VR teleportation
        const int frameID = gEnv->pRenderer->GetFrameID();
        if (frameID == lastFrameID && !gEnv->pRenderer->GetIStereoRenderer()->IsRenderingToHMD())
        {
            return;
        }

        lastFrameID = frameID;

        MNM::TileID excludeTileID(0);
        DebugDrawSettings settings = GetDebugDrawSettings(navigationSystem);

        if (settings.Valid())
        {
            excludeTileID = DebugDrawTileGeneration(navigationSystem, settings);
        }

        DebugDrawRayCast(navigationSystem, settings);

        DebugDrawPathFinder(navigationSystem, settings);

        DebugDrawClosestPoint(navigationSystem, settings);

        DebugDrawGroundPoint(navigationSystem, settings);

        DebugDrawIslandConnection(navigationSystem, settings);

        DebugDrawNavigationMeshesForSelectedAgent(navigationSystem, excludeTileID);

        m_progress.Draw();

        if (navigationSystem.IsInUse())
        {
            navigationSystem.m_offMeshNavigationManager.UpdateEditorDebugHelpers();
        }
    }

    DebugDrawNavigationSystemState(navigationSystem);

    DebugDrawMemoryStats(navigationSystem);
}

void NavigationSystemDebugDraw::UpdateWorkingProgress(const float frameTime, const size_t queueSize)
{
    m_progress.Update(frameTime, queueSize);
}

MNM::TileID NavigationSystemDebugDraw::DebugDrawTileGeneration(NavigationSystem& navigationSystem, const DebugDrawSettings& settings)
{
    MNM::TileID debugTileID(0);

#if defined(WIN32) || defined(WIN64)
    static MNM::TileGenerator debugGenerator;
    static MNM::TileID tileID(0);
    static bool prevKeyState = false;
    static size_t drawMode = MNM::TileGenerator::DrawNone;

    NavigationMesh& mesh = navigationSystem.m_meshes[settings.meshID];
    const MNM::MeshGrid::Params& paramsGrid = mesh.grid.GetParams();
    const AgentType& agentType = navigationSystem.m_agentTypes[m_agentTypeID - 1];

    bool forceGeneration = settings.forceGeneration;
    size_t selectedX = settings.selectedX;
    size_t selectedY = settings.selectedY;
    size_t selectedZ = settings.selectedZ;

    CDebugDrawContext dc;

    bool keyState = (GetAsyncKeyState(VK_BACK) & 0x8000) != 0;

    if (keyState && keyState != prevKeyState)
    {
        if (((GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0) || ((GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0))
        {
            forceGeneration = true;
        }
        else if (((GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0) || ((GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0))
        {
            --drawMode;
        }
        else
        {
            ++drawMode;
        }

        if (drawMode == MNM::TileGenerator::LastDrawMode)
        {
            drawMode = MNM::TileGenerator::DrawNone;
        }
        else if (drawMode > MNM::TileGenerator::LastDrawMode)
        {
            drawMode = MNM::TileGenerator::LastDrawMode - 1;
        }
    }

    prevKeyState = keyState;

    {
        static size_t selectPrevKeyState = 0;
        size_t selectKeyState = 0;

        selectKeyState |= ((GetAsyncKeyState(VK_UP) & 0x8000) != 0);
        selectKeyState |= 2 * ((GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0);
        selectKeyState |= 4 * ((GetAsyncKeyState(VK_DOWN) & 0x8000) != 0);
        selectKeyState |= 8 * ((GetAsyncKeyState(VK_LEFT) & 0x8000) != 0);
        selectKeyState |= 16 * ((GetAsyncKeyState(VK_ADD) & 0x8000) != 0);
        selectKeyState |= 32 * ((GetAsyncKeyState(VK_SUBTRACT) & 0x8000) != 0);

        int dirX = 0;
        int dirY = 0;
        int dirZ = 0;

        if ((selectKeyState & 1 << 0) && ((selectPrevKeyState & 1 << 0) == 0))
        {
            dirY += 1;
        }
        if ((selectKeyState & 1 << 1) && ((selectPrevKeyState & 1 << 1) == 0))
        {
            dirX += 1;
        }
        if ((selectKeyState & 1 << 2) && ((selectPrevKeyState & 1 << 2) == 0))
        {
            dirY -= 1;
        }
        if ((selectKeyState & 1 << 3) && ((selectPrevKeyState & 1 << 3) == 0))
        {
            dirX -= 1;
        }
        if ((selectKeyState & 1 << 4) && ((selectPrevKeyState & 1 << 4) == 0))
        {
            dirZ += 1;
        }
        if ((selectKeyState & 1 << 5) && ((selectPrevKeyState & 1 << 5) == 0))
        {
            dirZ -= 1;
        }

        if (!mesh.boundary)
        {
            selectedX += dirX;
            selectedY += dirY;
            selectedZ += dirZ;
        }
        else
        {
            Vec3 tileOrigin = paramsGrid.origin +
                Vec3((float)(selectedX + dirX) * paramsGrid.tileSize.x,
                    (float)(selectedY + dirY) * paramsGrid.tileSize.y,
                    (float)(selectedZ + dirZ) * paramsGrid.tileSize.z);

            if (navigationSystem.m_volumes[mesh.boundary].Contains(tileOrigin))
            {
                selectedX += dirX;
                selectedY += dirY;
                selectedZ += dirZ;
            }
        }

        selectPrevKeyState = selectKeyState;
    }

    if (forceGeneration)
    {
        tileID = 0;
        debugGenerator = MNM::TileGenerator();

        MNM::Tile tile;
        MNM::TileGenerator::Params params;

        std::vector<MNM::BoundingVolume> exclusions;
        exclusions.resize(mesh.exclusions.size());

        NavigationMesh::ExclusionVolumes::const_iterator eit = mesh.exclusions.begin();
        NavigationMesh::ExclusionVolumes::const_iterator eend = mesh.exclusions.end();

        for (size_t eindex = 0; eit != eend; ++eit, ++eindex)
        {
            exclusions[eindex] = navigationSystem.m_volumes[*eit];
        }

        navigationSystem.SetupGenerator(settings.meshID, paramsGrid, selectedX, selectedY, selectedZ, params,
            mesh.boundary ? &navigationSystem.m_volumes[mesh.boundary] : 0,
            exclusions.empty() ? 0 : &exclusions[0], exclusions.size());

        params.flags |= MNM::TileGenerator::Params::DebugInfo | MNM::TileGenerator::Params::NoHashTest;

        if (debugGenerator.Generate(params, tile, 0))
        {
            tileID = mesh.grid.SetTile(selectedX, selectedY, selectedZ, tile);

            mesh.grid.ConnectToNetwork(tileID);
        }
        else if (tileID = mesh.grid.GetTileID(selectedX, selectedY, selectedZ))
        {
            mesh.grid.ClearTile(tileID);
        }
    }

    debugGenerator.Draw((MNM::TileGenerator::DrawMode)drawMode);

    const char* drawModeName = "None";

    switch (drawMode)
    {
    default:
    case MNM::TileGenerator::DrawNone:
    case MNM::TileGenerator::LastDrawMode:
        break;

    case MNM::TileGenerator::DrawRawVoxels:
        drawModeName = "Raw Voxels";
        break;

    case MNM::TileGenerator::DrawFlaggedVoxels:
        drawModeName = "Flagged Voxels";
        break;

    case MNM::TileGenerator::DrawDistanceTransform:
        drawModeName = "Distance Transform";
        break;

    case MNM::TileGenerator::DrawSegmentation:
        drawModeName = "Segmentation";
        break;

    case MNM::TileGenerator::DrawNumberedContourVertices:
    case MNM::TileGenerator::DrawContourVertices:
        drawModeName = "Contour Vertices";
        break;

    case MNM::TileGenerator::DrawTracers:
        drawModeName = "Tracers";
        break;
    case MNM::TileGenerator::DrawSimplifiedContours:
        drawModeName = "Simplified Contours";
        break;

    case MNM::TileGenerator::DrawTriangulation:
        drawModeName = "Triangulation";
        break;

    case MNM::TileGenerator::DrawBVTree:
        drawModeName = "BV Tree";
        break;
    }

    dc->Draw2dLabel(10.0f, 5.0f, 1.6f, Col_White, false, "TileID %d - Drawing %s", tileID, drawModeName);

    const MNM::TileGenerator::ProfilerType& profilerInfo = debugGenerator.GetProfiler();

    dc->Draw2dLabel(10.0f, 28.0f, 1.25f, Col_White, false,
        "Total: %.1f - Voxelizer(%.2fK tris): %.1f - Filter: %.1f\n"
        "Contour(%d regs): %.1f - Simplify: %.1f\n"
        "Triangulate(%d vtx/%d tris): %.1f - BVTree(%d nodes): %.1f",
        profilerInfo.GetTotalElapsed().GetMilliSeconds(),
        profilerInfo[MNM::TileGenerator::VoxelizationTriCount] / 1000.0f,
        profilerInfo[MNM::TileGenerator::Voxelization].elapsed.GetMilliSeconds(),
        profilerInfo[MNM::TileGenerator::Filter].elapsed.GetMilliSeconds(),
        profilerInfo[MNM::TileGenerator::RegionCount],
        profilerInfo[MNM::TileGenerator::ContourExtraction].elapsed.GetMilliSeconds(),
        profilerInfo[MNM::TileGenerator::Simplification].elapsed.GetMilliSeconds(),
        profilerInfo[MNM::TileGenerator::VertexCount],
        profilerInfo[MNM::TileGenerator::TriangleCount],
        profilerInfo[MNM::TileGenerator::Triangulation].elapsed.GetMilliSeconds(),
        profilerInfo[MNM::TileGenerator::BVTreeNodeCount],
        profilerInfo[MNM::TileGenerator::BVTreeConstruction].elapsed.GetMilliSeconds()
        );

    dc->Draw2dLabel(10.0f, 84.0f, 1.4f, Col_White, false,
        "Peak Memory: %.2fKB", profilerInfo.GetMemoryPeak() / 1024.0f);

    size_t vertexMemory = profilerInfo[MNM::TileGenerator::VertexMemory].used;
    size_t triangleMemory = profilerInfo[MNM::TileGenerator::TriangleMemory].used;
    size_t bvTreeMemory = profilerInfo[MNM::TileGenerator::BVTreeMemory].used;
    size_t tileMemory = vertexMemory + triangleMemory + bvTreeMemory;

    dc->Draw2dLabel(10.0f, 98.0f, 1.4f, Col_White, false,
        "Tile Memory: %.2fKB (Vtx: %db, Tri: %db, BVT: %db)",
        tileMemory / 1024.0f,
        vertexMemory,
        triangleMemory,
        bvTreeMemory);

    if (drawMode != MNM::TileGenerator::DrawNone)
    {
        debugTileID = mesh.grid.GetTileID(selectedX, selectedY, selectedZ);
    }
#endif

    return debugTileID;
}

void NavigationSystemDebugDraw::DebugDrawRayCast(NavigationSystem& navigationSystem, const DebugDrawSettings& settings)
{
    if (CAIObject* debugObjectStart = gAIEnv.pAIObjectManager->GetAIObjectByName("MNMRayStart"))
    {
        if (CAIObject* debugObjectEnd = gAIEnv.pAIObjectManager->GetAIObjectByName("MNMRayEnd"))
        {
            NavigationMeshID meshID = navigationSystem.GetEnclosingMeshID(m_agentTypeID, debugObjectStart->GetPos());
            IF_UNLIKELY (!meshID)
            {
                return;
            }

            NavigationMesh& mesh = navigationSystem.m_meshes[meshID];
            const MNM::MeshGrid::Params& paramsGrid = mesh.grid.GetParams();
            const MNM::MeshGrid& grid = mesh.grid;

            const MNM::vector3_t origin = MNM::vector3_t(MNM::real_t(paramsGrid.origin.x),
                    MNM::real_t(paramsGrid.origin.y),
                    MNM::real_t(paramsGrid.origin.z));
            const MNM::vector3_t originOffset = origin + MNM::vector3_t(0, 0, MNM::real_t::fraction(725, 10000));

            IRenderAuxGeom* renderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();

            const Vec3 startLoc = debugObjectStart->GetPos();
            const Vec3 endLoc = debugObjectEnd->GetPos();

            const MNM::real_t range = MNM::real_t(1.0f);

            MNM::vector3_t start = MNM::vector3_t(
                    MNM::real_t(startLoc.x), MNM::real_t(startLoc.y), MNM::real_t(startLoc.z)) - origin;
            MNM::vector3_t end = MNM::vector3_t(
                    MNM::real_t(endLoc.x), MNM::real_t(endLoc.y), MNM::real_t(endLoc.z)) - origin;

            MNM::TriangleID triStart = grid.GetTriangleAt(start, range, range);

            if (triStart)
            {
                MNM::vector3_t a, b, c;
                grid.GetVertices(triStart, a, b, c);

                renderAuxGeom->DrawTriangle(
                    (a + originOffset).GetVec3(), ColorB(Col_GreenYellow),
                    (b + originOffset).GetVec3(), ColorB(Col_GreenYellow),
                    (c + originOffset).GetVec3(), ColorB(Col_GreenYellow));
            }

            if (triStart)
            {
                MNM::TriangleID triEnd = grid.GetTriangleAt(end, range, range);

                MNM::MeshGrid::RayCastRequest<512> raycastRequest;

                MNM::MeshGrid::ERayCastResult result = grid.RayCast(start, triStart, end, triEnd, raycastRequest);

                for (size_t i = 0; i < raycastRequest.wayTriCount; ++i)
                {
                    MNM::vector3_t a, b, c;

                    grid.GetVertices(raycastRequest.way[i], a, b, c);

                    renderAuxGeom->DrawTriangle(
                        (a + originOffset).GetVec3(), ColorB(Col_Maroon),
                        (b + originOffset).GetVec3(), ColorB(Col_Maroon),
                        (c + originOffset).GetVec3(), ColorB(Col_Maroon));
                }

                if (triStart)
                {
                    MNM::vector3_t a, b, c;
                    grid.GetVertices(triStart, a, b, c);

                    renderAuxGeom->DrawTriangle(
                        (a + originOffset).GetVec3(), ColorB(Col_GreenYellow),
                        (b + originOffset).GetVec3(), ColorB(Col_GreenYellow),
                        (c + originOffset).GetVec3(), ColorB(Col_GreenYellow));
                }

                if (triEnd)
                {
                    MNM::vector3_t a, b, c;
                    grid.GetVertices(triEnd, a, b, c);

                    renderAuxGeom->DrawTriangle(
                        (a + originOffset).GetVec3(), ColorB(Col_Red),
                        (b + originOffset).GetVec3(), ColorB(Col_Red),
                        (c + originOffset).GetVec3(), ColorB(Col_Red));
                }


                const Vec3 offset(0.0f, 0.0f, 0.085f);

                if (result == MNM::MeshGrid::eRayCastResult_NoHit)
                {
                    renderAuxGeom->DrawLine(startLoc + offset, Col_YellowGreen, endLoc + offset, Col_YellowGreen, 8.0f);
                }
                else
                {
                    const MNM::MeshGrid::RayHit& hit = raycastRequest.hit;
                    Vec3 hitLoc = (result == MNM::MeshGrid::eRayCastResult_Hit) ? startLoc + ((endLoc - startLoc) * hit.distance.as_float()) : startLoc;
                    renderAuxGeom->DrawLine(startLoc + offset, Col_YellowGreen, hitLoc + offset, Col_YellowGreen, 8.0f);
                    renderAuxGeom->DrawLine(hitLoc + offset, Col_Red, endLoc + offset, Col_Red, 8.0f);
                }
            }
        }
    }
}

void NavigationSystemDebugDraw::DebugDrawClosestPoint(NavigationSystem& navigationSystem, const DebugDrawSettings& settings)
{
    if (CAIObject* debugObject = gAIEnv.pAIObjectManager->GetAIObjectByName("MNMClosestPoint"))
    {
        NavigationMeshID meshID = navigationSystem.GetEnclosingMeshID(m_agentTypeID, debugObject->GetPos());
        IF_UNLIKELY (!meshID)
        {
            return;
        }

        NavigationMesh& mesh = navigationSystem.m_meshes[meshID];
        const MNM::MeshGrid& grid = mesh.grid;
        const MNM::MeshGrid::Params& paramsGrid = mesh.grid.GetParams();

        const MNM::vector3_t origin = MNM::vector3_t(MNM::real_t(paramsGrid.origin.x),
                MNM::real_t(paramsGrid.origin.y),
                MNM::real_t(paramsGrid.origin.z));
        const MNM::vector3_t originOffset = origin + MNM::vector3_t(0, 0, MNM::real_t::fraction(725, 10000));

        const Vec3 startLoc = debugObject->GetEntity() ? debugObject->GetEntity()->GetWorldPos() : debugObject->GetPos();
        const MNM::vector3_t fixedPointStartLoc(MNM::real_t(startLoc.x), MNM::real_t(startLoc.y), MNM::real_t(startLoc.z));
        const MNM::real_t range = MNM::real_t(1.0f);

        MNM::real_t distance(.0f);
        MNM::vector3_t closestPosition;
        if (MNM::TriangleID closestTriangle = grid.GetClosestTriangle(fixedPointStartLoc, range, range, &distance, &closestPosition))
        {
            IRenderAuxGeom* renderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
            const Vec3 verticalOffset = Vec3(.0f, .0f, .1f);
            const Vec3 endPos(closestPosition.GetVec3() + origin.GetVec3());
            renderAuxGeom->DrawSphere(endPos + verticalOffset, 0.05f, ColorB(Col_Red));
            renderAuxGeom->DrawSphere(fixedPointStartLoc.GetVec3() + verticalOffset, 0.05f, ColorB(Col_Black));

            CDebugDrawContext dc;
            dc->Draw2dLabel(10.0f, 10.0f, 1.3f, Col_White, false,
                "Distance of the ending result position from the original one: %f", distance.as_float());
        }
    }
}

void NavigationSystemDebugDraw::DebugDrawGroundPoint(NavigationSystem& navigationSystem, const DebugDrawSettings& settings)
{
    if (CAIObject* debugObject = gAIEnv.pAIObjectManager->GetAIObjectByName("MNMGroundPoint"))
    {
        NavigationMeshID meshID = navigationSystem.GetEnclosingMeshID(m_agentTypeID, debugObject->GetPos());
        IF_UNLIKELY (!meshID)
        {
            return;
        }

        NavigationMesh& mesh = navigationSystem.m_meshes[meshID];
        const MNM::MeshGrid& grid = mesh.grid;
        const MNM::MeshGrid::Params& paramsGrid = mesh.grid.GetParams();

        const MNM::vector3_t origin = MNM::vector3_t(MNM::real_t(paramsGrid.origin.x),
                MNM::real_t(paramsGrid.origin.y), MNM::real_t(paramsGrid.origin.z));
        const Vec3 startLoc = debugObject->GetEntity() ? debugObject->GetEntity()->GetWorldPos() : debugObject->GetPos();

        Vec3 closestPosition;
        if (navigationSystem.GetGroundLocationInMesh(meshID, startLoc, 100.0f, 0.25f, &closestPosition))
        {
            IRenderAuxGeom* renderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
            const Vec3 verticalOffset = Vec3(.0f, .0f, .1f);
            const Vec3 endPos(closestPosition + origin.GetVec3());
            renderAuxGeom->DrawSphere(endPos + verticalOffset, 0.05f, ColorB(Col_Red));
            renderAuxGeom->DrawSphere(startLoc, 0.05f, ColorB(Col_Black));
        }
    }
}

void NavigationSystemDebugDraw::DebugDrawPathFinder(NavigationSystem& navigationSystem, const DebugDrawSettings& settings)
{
    if (CAIObject* debugObjectStart = gAIEnv.pAIObjectManager->GetAIObjectByName("MNMPathStart"))
    {
        if (CAIObject* debugObjectEnd = gAIEnv.pAIObjectManager->GetAIObjectByName("MNMPathEnd"))
        {
            NavigationMeshID meshID = navigationSystem.GetEnclosingMeshID(m_agentTypeID, debugObjectStart->GetPos());
            IF_UNLIKELY (!meshID)
            {
                return;
            }

            NavigationMesh& mesh = navigationSystem.m_meshes[meshID];
            const MNM::MeshGrid& grid = mesh.grid;
            const MNM::MeshGrid::Params& paramsGrid = mesh.grid.GetParams();

            const MNM::OffMeshNavigation& offMeshNavigation = navigationSystem.GetOffMeshNavigationManager()->GetOffMeshNavigationForMesh(meshID);

            const MNM::vector3_t origin = MNM::vector3_t(MNM::real_t(paramsGrid.origin.x),
                    MNM::real_t(paramsGrid.origin.y),
                    MNM::real_t(paramsGrid.origin.z));
            const MNM::vector3_t originOffset = origin + MNM::vector3_t(0, 0, MNM::real_t::fraction(725, 10000));

            IRenderAuxGeom* renderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();

            const Vec3 startLoc = debugObjectStart->GetEntity() ? debugObjectStart->GetEntity()->GetWorldPos() : debugObjectStart->GetPos();
            const Vec3 endLoc = debugObjectEnd->GetPos();
            const MNM::vector3_t fixedPointStartLoc(MNM::real_t(startLoc.x), MNM::real_t(startLoc.y), MNM::real_t(startLoc.z));
            const MNM::real_t range = MNM::real_t(1.0f);

            MNM::TriangleID triStart = grid.GetTriangleAt(
                    fixedPointStartLoc - origin, range, range);

            if (triStart)
            {
                MNM::vector3_t a, b, c;
                grid.GetVertices(triStart, a, b, c);

                renderAuxGeom->DrawTriangle(
                    (a + originOffset).GetVec3(), ColorB(Col_GreenYellow),
                    (b + originOffset).GetVec3(), ColorB(Col_GreenYellow),
                    (c + originOffset).GetVec3(), ColorB(Col_GreenYellow));
            }

            MNM::TriangleID triEnd = grid.GetTriangleAt(
                    MNM::vector3_t(MNM::real_t(endLoc.x), MNM::real_t(endLoc.y), MNM::real_t(endLoc.z)) - origin, range, range);

            if (triEnd)
            {
                MNM::vector3_t a, b, c;
                grid.GetVertices(triEnd, a, b, c);

                renderAuxGeom->DrawTriangle(
                    (a + originOffset).GetVec3(), ColorB(Col_MidnightBlue),
                    (b + originOffset).GetVec3(), ColorB(Col_MidnightBlue),
                    (c + originOffset).GetVec3(), ColorB(Col_MidnightBlue));
            }

            CTimeValue timeTotal(0ll);
            CTimeValue stringPullingTotalTime(0ll);
            float totalPathLengthSq = 0;
            if (triStart && triEnd)
            {
                const MNM::vector3_t fixedPointEndLoc(MNM::real_t(endLoc.x), MNM::real_t(endLoc.y), MNM::real_t(endLoc.z));
                const MNM::vector3_t startToEnd = (fixedPointStartLoc - fixedPointEndLoc);
                const MNM::real_t startToEndDist = startToEnd.lenNoOverflow();
                MNM::MeshGrid::WayQueryWorkingSet workingSet;
                workingSet.aStarOpenList.SetFrameTimeQuota(0.0f);
                workingSet.aStarOpenList.SetUpForPathSolving(grid.GetTriangleCount(), triStart, fixedPointStartLoc, startToEndDist);

                CTimeValue timeStart = gEnv->pTimer->GetAsyncTime();

                MNM::DangerousAreasList dangersInfo;
                MNM::DangerAreaConstPtr info;
                const Vec3& cameraPos = gAIEnv.GetDebugRenderer()->GetCameraPos(); // To simulate the player position and evaluate the path generation
                info.reset(new MNM::DangerAreaT<MNM::eWCT_Direction>(cameraPos, 0.0f, gAIEnv.CVars.PathfinderDangerCostForAttentionTarget));
                dangersInfo.push_back(info);
                // This object is used to simulate the explosive threat and debug draw the behavior of the pathfinding
                CAIObject* debugObjectExplosiveThreat = gAIEnv.pAIObjectManager->GetAIObjectByName("MNMPathExplosiveThreat");
                if (debugObjectExplosiveThreat)
                {
                    info.reset(new MNM::DangerAreaT<MNM::eWCT_Range>(debugObjectExplosiveThreat->GetPos(),
                            gAIEnv.CVars.PathfinderExplosiveDangerRadius, gAIEnv.CVars.PathfinderDangerCostForExplosives));
                    dangersInfo.push_back(info);
                }

                const float pathSharingPenalty = .0f;
                const float pathLinkSharingPenalty = .0f;
                MNM::MeshGrid::WayQueryRequest inputParams(debugObjectStart->CastToIAIActor(), triStart, startLoc, triEnd, endLoc,
                    offMeshNavigation, dangersInfo);
                MNM::MeshGrid::WayQueryResult result;
                MNM::WayTriangleData* outputWay = result.GetWayData();

                const bool hasPathfindingFinished = (grid.FindWay(inputParams, workingSet, result) == MNM::MeshGrid::eWQR_Done);

                CTimeValue timeEnd = gEnv->pTimer->GetAsyncTime();
                timeTotal = timeEnd - timeStart;

                assert(hasPathfindingFinished);

                for (size_t i = 0; i < result.GetWaySize(); ++i)
                {
                    if ((outputWay[i].triangleID != triStart) && (outputWay[i].triangleID != triEnd))
                    {
                        MNM::vector3_t a, b, c;

                        grid.GetVertices(outputWay[i].triangleID, a, b, c);

                        renderAuxGeom->DrawTriangle(
                            (a + originOffset).GetVec3(), ColorB(Col_Maroon),
                            (b + originOffset).GetVec3(), ColorB(Col_Maroon),
                            (c + originOffset).GetVec3(), ColorB(Col_Maroon));
                    }
                }

                PathPointDescriptor navPathStart(IAISystem::NAV_UNSET, startLoc);
                PathPointDescriptor navPathEnd(IAISystem::NAV_UNSET, endLoc);

                CPathHolder<PathPointDescriptor> outputPath;
                size_t waySize = result.GetWaySize();
                for (size_t i = 0; i < waySize; ++i)
                {
                    // Using the edge-midpoints of adjacent triangles to build the path.
                    if (i > 0)
                    {
                        Vec3 edgeMidPoint;
                        if (grid.CalculateMidEdge(outputWay[i - 1].triangleID, outputWay[i].triangleID, edgeMidPoint))
                        {
                            outputPath.PushFront(PathPointDescriptor(IAISystem::NAV_UNSET, edgeMidPoint + origin.GetVec3()));
                        }
                    }

                    if (outputWay[i].offMeshLinkID)
                    {
                        // Grab off-mesh link object
                        const MNM::OffMeshNavigation& meshOffMeshNav = gAIEnv.pNavigationSystem->GetOffMeshNavigationManager()->GetOffMeshNavigationForMesh(meshID);
                        const MNM::OffMeshLink* pOffMeshLink = meshOffMeshNav.GetObjectLinkInfo(outputWay[i].offMeshLinkID);
                        assert(pOffMeshLink);

                        if (pOffMeshLink)
                        {
                            const bool isLinkSmartObject = pOffMeshLink->GetLinkType() == MNM::OffMeshLink::eLinkType_SmartObject;
                            IAISystem::ENavigationType type = isLinkSmartObject ? IAISystem::NAV_SMARTOBJECT : IAISystem::NAV_CUSTOM_NAVIGATION;

                            // Add Entry/Exit points
                            PathPointDescriptor pathPoint(type);
                            pathPoint.iTriId = outputWay[i].triangleID;

                            // Cache off-mesh link data on the waypoint
                            pathPoint.offMeshLinkData.meshID = meshID;
                            pathPoint.offMeshLinkData.offMeshLinkID = outputWay[i].offMeshLinkID;

                            pathPoint.vPos = pOffMeshLink->GetEndPosition();
                            pathPoint.iTriId = 0;
                            outputPath.PushFront(pathPoint);

                            pathPoint.vPos = pOffMeshLink->GetStartPosition();
                            pathPoint.iTriId = outputWay[i].triangleID;
                            outputPath.PushFront(pathPoint);
                        }
                    }
                }

                const bool pathFound = (result.GetWaySize() != 0);
                if (pathFound)
                {
                    outputPath.PushBack(navPathEnd);
                    outputPath.PushFront(navPathStart);

                    CTimeValue stringPullingStartTime = gEnv->pTimer->GetAsyncTime();
                    if (gAIEnv.CVars.BeautifyPath)
                    {
                        outputPath.PullPathOnNavigationMesh(meshID, gAIEnv.CVars.PathStringPullingIterations, &outputWay[0], result.GetWaySize());
                    }
                    stringPullingTotalTime = gEnv->pTimer->GetAsyncTime() - stringPullingStartTime;
                    size_t pathSize = outputPath.Size();
                    const Vec3 verticalOffset = Vec3(.0f, .0f, .1f);
                    for (size_t j = 0; pathSize > 0 &&  j < pathSize - 1; ++j)
                    {
                        Vec3 start = outputPath.At(j);
                        Vec3 end = outputPath.At(j + 1);
                        renderAuxGeom->DrawLine(start + verticalOffset, Col_Orange, end + verticalOffset, Col_Black, 4.0f);
                        totalPathLengthSq += Distance::Point_PointSq(start, end);
                    }
                }
            }

            const stack_string predictionName = gAIEnv.CVars.MNMPathfinderPositionInTrianglePredictionType ? "Advanced prediction" : "Triangle Center";

            CDebugDrawContext dc;

            dc->Draw2dLabel(10.0f, 172.0f, 1.3f, Col_White, false,
                "Start: %08x  -  End: %08x - Total Pathfinding time: %.4fms -- Type of prediction for the point inside each triangle: %s", triStart, triEnd, timeTotal.GetMilliSeconds(), predictionName.c_str());
            dc->Draw2dLabel(10.0f, 184.0f, 1.3f, Col_White, false,
                "String pulling operation - Iteration %d  -  Total time: %.4fms -- Total Length: %f", gAIEnv.CVars.PathStringPullingIterations, stringPullingTotalTime.GetMilliSeconds(), sqrt(totalPathLengthSq));
        }
    }
}

void NavigationSystemDebugDraw::DebugDrawIslandConnection(NavigationSystem& navigationSystem, const DebugDrawSettings& settings)
{
    bool isReachable = false;

    if (CAIObject* debugObjectStart = gAIEnv.pAIObjectManager->GetAIObjectByName("MNMIslandStart"))
    {
        if (CAIObject* debugObjectEnd = gAIEnv.pAIObjectManager->GetAIObjectByName("MNMIslandEnd"))
        {
            assert(debugObjectStart->CastToIAIActor());
            IAIPathAgent* pPathAgent = debugObjectStart->CastToIAIActor();
            isReachable = gAIEnv.pNavigationSystem->IsPointReachableFromPosition(m_agentTypeID, pPathAgent->GetPathAgentEntity(), debugObjectStart->GetPos(), debugObjectEnd->GetPos());

            CDebugDrawContext dc;
            dc->Draw2dLabel(10.0f, 250.0f, 1.6f, isReachable ? Col_ForestGreen : Col_VioletRed, false, isReachable ? "The two islands ARE connected" : "The two islands ARE NOT connected");
        }
    }
}

void NavigationSystemDebugDraw::DebugDrawNavigationMeshesForSelectedAgent(NavigationSystem& navigationSystem, MNM::TileID excludeTileID)
{
    AgentType& agentType = navigationSystem.m_agentTypes[m_agentTypeID - 1];
    AgentType::Meshes::const_iterator it = agentType.meshes.begin();
    AgentType::Meshes::const_iterator end = agentType.meshes.end();

    for (; it != end; ++it)
    {
        const NavigationMesh& mesh = navigationSystem.GetMesh(it->id);

        size_t drawFlag = MNM::Tile::DrawTriangles | MNM::Tile::DrawMeshBoundaries;
        if (gAIEnv.CVars.MNMDebugAccessibility)
        {
            drawFlag |= MNM::Tile::DrawAccessibility;
        }

        switch (gAIEnv.CVars.DebugDrawNavigation)
        {
        case 0:
        case 1:
            mesh.grid.Draw(drawFlag, excludeTileID);
            break;
        case 2:
            mesh.grid.Draw(drawFlag | MNM::Tile::DrawInternalLinks, excludeTileID);
            break;
        case 3:
            mesh.grid.Draw(drawFlag | MNM::Tile::DrawInternalLinks |
                MNM::Tile::DrawExternalLinks | MNM::Tile::DrawOffMeshLinks, excludeTileID);
            break;
        case 4:
            mesh.grid.Draw(drawFlag | MNM::Tile::DrawInternalLinks |
                MNM::Tile::DrawExternalLinks | MNM::Tile::DrawOffMeshLinks |
                MNM::Tile::DrawTrianglesId, excludeTileID);
            break;
        case 5:
            mesh.grid.Draw(drawFlag | MNM::Tile::DrawInternalLinks | MNM::Tile::DrawExternalLinks |
                MNM::Tile::DrawOffMeshLinks | MNM::Tile::DrawIslandsId, excludeTileID);
            break;
        default:
            break;
        }
    }
}

void NavigationSystemDebugDraw::DebugDrawNavigationSystemState(NavigationSystem& navigationSystem)
{
    CDebugDrawContext dc;

    if (gAIEnv.CVars.DebugDrawNavigation)
    {
        switch (navigationSystem.m_state)
        {
        case NavigationSystem::Working:
            dc->Draw2dLabel(10.0f, 300.0f, 1.6f, Col_Yellow, false, "Navigation System Working");
            dc->Draw2dLabel(10.0f, 322.0f, 1.2f, Col_White, false, "Processing: %d\nRemaining: %d\nThroughput: %.2f/s\n"
                "Cache Hits: %.2f/s",
                navigationSystem.m_runningTasks.size(), navigationSystem.m_tileQueue.size(), navigationSystem.m_throughput, navigationSystem.m_cacheHitRate);
            break;
        case NavigationSystem::Idle:
            dc->Draw2dLabel(10.0f, 300.0f, 1.6f, Col_ForestGreen, false, "Navigation System Idle");
            break;
        default:
            assert(0);
            break;
        }
    }
}

void NavigationSystemDebugDraw::DebugDrawMemoryStats(NavigationSystem& navigationSystem)
{
    if (gAIEnv.CVars.MNMProfileMemory)
    {
        const float kbInvert = 1.0f / 1024.0f;

        const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        const float grey[4] = {0.65f, 0.65f, 0.65f, 1.0f};

        float posY = 60.f;
        float posX = 30.f;

        size_t totalNavigationSystemMemory = 0;

        ICrySizer* pSizer = gEnv->pSystem->CreateSizer();

        for (size_t i = 0; i < navigationSystem.m_meshes.capacity(); ++i)
        {
            if (!navigationSystem.m_meshes.index_free(i))
            {
                const NavigationMeshID meshID(navigationSystem.m_meshes.get_index_id(i));

                const NavigationMesh& mesh = navigationSystem.m_meshes[meshID];
                const MNM::OffMeshNavigation& offMeshNavigation = navigationSystem.GetOffMeshNavigationManager()->GetOffMeshNavigationForMesh(meshID);
                const AgentType& agentType = navigationSystem.m_agentTypes[mesh.agentTypeID - 1];

                const NavigationMesh::ProfileMemoryStats meshMemStats = mesh.GetMemoryStats(pSizer);
                const MNM::OffMeshNavigation::ProfileMemoryStats offMeshMemStats = offMeshNavigation.GetMemoryStats(pSizer);

                size_t totalMemory = meshMemStats.totalNavigationMeshMemory + offMeshMemStats.totalSize;

                gEnv->pRenderer->Draw2dLabel(posX, posY, 1.3f, white, false, "Mesh: %s Agent: %s - Total Memory %.3f KB : Mesh %.3f KB / Grid %.3f KB / OffMesh %.3f",
                    mesh.name.c_str(), agentType.name.c_str(),
                    totalMemory * kbInvert, meshMemStats.totalNavigationMeshMemory * kbInvert, meshMemStats.gridProfiler.GetMemoryUsage() * kbInvert, offMeshMemStats.totalSize * kbInvert);
                posY += 12.0f;
                gEnv->pRenderer->Draw2dLabel(posX, posY, 1.3f, grey, false, "Tiles [%d] / Vertices [%d] - %.3f KB / Triangles [%d] - %.3f KB / Links [%d] - %.3f KB / BVNodes [%d] - %.3f KB",
                    meshMemStats.gridProfiler[MNM::MeshGrid::TileCount],
                    meshMemStats.gridProfiler[MNM::MeshGrid::VertexCount], meshMemStats.gridProfiler[MNM::MeshGrid::VertexMemory].used * kbInvert,
                    meshMemStats.gridProfiler[MNM::MeshGrid::TriangleCount], meshMemStats.gridProfiler[MNM::MeshGrid::TriangleMemory].used * kbInvert,
                    meshMemStats.gridProfiler[MNM::MeshGrid::LinkCount], meshMemStats.gridProfiler[MNM::MeshGrid::LinkMemory].used * kbInvert,
                    meshMemStats.gridProfiler[MNM::MeshGrid::BVTreeNodeCount], meshMemStats.gridProfiler[MNM::MeshGrid::BVTreeMemory].used * kbInvert
                    );

                posY += 12.0f;
                gEnv->pRenderer->Draw2dLabel(posX, posY, 1.3f, grey, false, "OffMesh Memory : Tile Links %.3f KB / Smart Object Info %.3f KB",
                    offMeshMemStats.offMeshTileLinksMemory * kbInvert,
                    offMeshMemStats.smartObjectInfoMemory * kbInvert
                    );
                posY += 13.0f;

                totalNavigationSystemMemory += totalMemory;
            }
        }


        //TODO: Add Navigation system itself (internal containers and others)

        gEnv->pRenderer->Draw2dLabel(40.0f, 20.0f, 1.5f, white, false, "Navigation System: %.3f KB", totalNavigationSystemMemory * kbInvert);

        pSizer->Release();
    }
}

NavigationSystemDebugDraw::DebugDrawSettings NavigationSystemDebugDraw::GetDebugDrawSettings(NavigationSystem& navigationSystem)
{
    static Vec3 lastLocation(ZERO);

    static size_t selectedX = 0;
    static size_t selectedY = 0;
    static size_t selectedZ = 0;

    DebugDrawSettings settings;
    settings.forceGeneration = false;

    if (CAIObject* pMNMDebugLocator = gAIEnv.pAIObjectManager->GetAIObjectByName("MNMDebugLocator"))
    {
        const Vec3 debugLocation = pMNMDebugLocator->GetPos();

        if ((lastLocation - debugLocation).len2() > 0.0001f)
        {
            settings.meshID = navigationSystem.GetEnclosingMeshID(m_agentTypeID, debugLocation);

            const NavigationMesh& mesh = navigationSystem.GetMesh(settings.meshID);
            const MNM::MeshGrid::Params& params = mesh.grid.GetParams();

            size_t x = (size_t)((debugLocation.x - params.origin.x) / (float)params.tileSize.x);
            size_t y = (size_t)((debugLocation.y - params.origin.y) / (float)params.tileSize.y);
            size_t z = (size_t)((debugLocation.z - params.origin.z) / (float)params.tileSize.z);

            if ((x != selectedX) || (y != selectedY) || (z != selectedZ))
            {
                settings.forceGeneration = true;

                selectedX = x;
                selectedY = y;
                selectedZ = z;
            }

            lastLocation = debugLocation;
        }
    }
    else
    {
        lastLocation.zero();
    }

    settings.selectedX = selectedX;
    settings.selectedY = selectedY;
    settings.selectedZ = selectedZ;


    if (!settings.meshID)
    {
        settings.meshID = navigationSystem.GetEnclosingMeshID(m_agentTypeID, lastLocation);
    }

    return settings;
}

//////////////////////////////////////////////////////////////////////////

void NavigationSystemDebugDraw::NavigationSystemWorkingProgress::Update(const float frameTime, const size_t queueSize)
{
    m_currentQueueSize = queueSize;
    m_initialQueueSize = (queueSize > 0) ? max(m_initialQueueSize, queueSize) : 0;

    const float updateTime = (queueSize > 0) ? frameTime : -2.0f * frameTime;
    m_timeUpdating = clamp_tpl(m_timeUpdating + updateTime, 0.0f, 1.0f);
}

void NavigationSystemDebugDraw::NavigationSystemWorkingProgress::Draw()
{
    const bool draw = (m_timeUpdating > 0.0f);

    if (!draw)
    {
        return;
    }

    BeginDraw();

    const float width = (float)gEnv->pRenderer->GetWidth();
    const float height = (float)gEnv->pRenderer->GetHeight();

    const ColorB backGroundColor(0, 255, 0, CLAMP((int)(0.35f * m_timeUpdating * 255.0f), 0, 255));
    const ColorB progressColor(0, 255, 0, CLAMP((int)(0.8f * m_timeUpdating * 255.0f), 0, 255));

    const float progressFraction = (m_initialQueueSize > 0) ? clamp_tpl(1.0f - ((float)m_currentQueueSize / (float)m_initialQueueSize), 0.0f, 1.0f) : 1.0f;

    const Vec2 progressBarLocation(0.1f, 0.91f);
    const Vec2 progressBarSize(0.2f, 0.025f);

    const float white[4] = { 1.0f, 1.0f, 1.0f, 0.85f * m_timeUpdating };

    gEnv->pRenderer->Draw2dLabel(progressBarLocation.x * width, (progressBarLocation.y * height) - 18.0f, 1.4f, white, false, "Processing Navigation Meshes");

    DrawQuad(progressBarLocation, progressBarSize, backGroundColor);
    DrawQuad(progressBarLocation, Vec2(progressBarSize.x * progressFraction, progressBarSize.y), progressColor);

    EndDraw();
}

void NavigationSystemDebugDraw::NavigationSystemWorkingProgress::BeginDraw()
{
    IRenderAuxGeom* pRenderAux = gEnv->pRenderer->GetIRenderAuxGeom();
    if (pRenderAux)
    {
        m_oldRenderFlags = pRenderAux->GetRenderFlags();

        SAuxGeomRenderFlags newFlags = e_Def3DPublicRenderflags;
        newFlags.SetMode2D3DFlag(e_Mode2D);
        newFlags.SetAlphaBlendMode(e_AlphaBlended);

        pRenderAux->SetRenderFlags(newFlags);
    }
}

void NavigationSystemDebugDraw::NavigationSystemWorkingProgress::EndDraw()
{
    IRenderAuxGeom* pRenderAux = gEnv->pRenderer->GetIRenderAuxGeom();
    if (pRenderAux)
    {
        pRenderAux->SetRenderFlags(m_oldRenderFlags);
    }
}

void NavigationSystemDebugDraw::NavigationSystemWorkingProgress::DrawQuad(const Vec2& origin, const Vec2& size, const ColorB& color)
{
    Vec3 quadVertices[4];
    const vtx_idx auxIndices[6] = {2, 1, 0, 2, 3, 1};

    quadVertices[0] = Vec3(origin.x, origin.y, 1.0f);
    quadVertices[1] = Vec3(origin.x + size.x, origin.y, 1.0f);
    quadVertices[2] = Vec3(origin.x, origin.y + size.y, 1.0f);
    quadVertices[3] = Vec3(origin.x + size.x, origin.y + size.y, 1.0f);

    IRenderAuxGeom* pRenderAux = gEnv->pRenderer->GetIRenderAuxGeom();
    if (pRenderAux)
    {
        pRenderAux->DrawTriangles(quadVertices, 4, auxIndices, 6, color);
    }
}

//////////////////////////////////////////////////////////////////////////

NavigationMesh::ProfileMemoryStats NavigationMesh::GetMemoryStats(ICrySizer* pSizer) const
{
    ProfileMemoryStats memoryStats(grid.GetProfiler());

    size_t initialSize = pSizer->GetTotalSize();
    {
        pSizer->AddObjectSize(this);
        pSizer->AddObject(&grid, memoryStats.gridProfiler.GetMemoryUsage());
        pSizer->AddContainer(exclusions);
        pSizer->AddString(name);

        memoryStats.totalNavigationMeshMemory = pSizer->GetTotalSize() - initialSize;
    }

    return memoryStats;
}

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if NAVIGATION_SYSTEM_EDITOR_BACKGROUND_UPDATE

void NavigationSystemBackgroundUpdate::Thread::Run()
{
    while (!m_requestedStop)
    {
        if (m_navigationSystem.GetState() == INavigationSystem::Working)
        {
            const CTimeValue startedUpdate = gEnv->pTimer->GetAsyncTime();

            m_navigationSystem.UpdateMeshes(0.0333f, false, true, true);

            const CTimeValue lastUpdateTime = gEnv->pTimer->GetAsyncTime() - startedUpdate;

            const unsigned int sleepTime = max(10u, min(0u, 33u - (unsigned int)lastUpdateTime.GetMilliSeconds()));

            CrySleep(sleepTime);
        }
        else
        {
            CrySleep(50);
        }
    }

    Stop();
}

void NavigationSystemBackgroundUpdate::Thread::Cancel()
{
    m_requestedStop = true;

    Base::Cancel();

    WaitForThread();
}

//////////////////////////////////////////////////////////////////////////

bool NavigationSystemBackgroundUpdate::Start()
{
    if (m_pBackgroundThread == NULL)
    {
        m_pBackgroundThread = new Thread(m_navigationSystem);
        m_pBackgroundThread->Start(0, "NavigationSystemBackgroundUpdate");

        return true;
    }

    return false;
}

bool NavigationSystemBackgroundUpdate::Stop()
{
    if (m_pBackgroundThread != NULL)
    {
        m_pBackgroundThread->Cancel();

        delete m_pBackgroundThread;
        m_pBackgroundThread = NULL;

        return true;
    }

    return false;
}

void NavigationSystemBackgroundUpdate::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
    CRY_ASSERT(gEnv->IsEditor());

    if (event == ESYSTEM_EVENT_CHANGE_FOCUS)
    {
        // wparam != 0 is focused, wparam == 0 is not focused
        const bool startBackGroundUpdate = (wparam == 0) && (gAIEnv.CVars.MNMEditorBackgroundUpdate != 0) && (m_navigationSystem.GetState() == INavigationSystem::Working) && !m_paused;

        if (startBackGroundUpdate)
        {
            if (Start())
            {
                CryLog("NavMesh generation background thread started");
            }
        }
        else
        {
            if (Stop())
            {
                CryLog("NavMesh generation background thread stopped");
            }
        }
    }
}

void NavigationSystemBackgroundUpdate::RegisterAsSystemListener()
{
    if (IsEnabled())
    {
        gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this);
    }
}

void NavigationSystemBackgroundUpdate::RemoveAsSystemListener()
{
    if (IsEnabled())
    {
        gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
    }
}

#endif
