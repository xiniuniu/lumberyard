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

// Description : check vis


#include "StdAfx.h"

#include "terrain.h"
#include "terrain_sector.h"
#include "StatObj.h"
#include "ObjMan.h"
#include "CullBuffer.h"
#include "Ocean.h"
#include "VisAreas.h"
#include "Vegetation.h"
#include "RoadRenderNode.h"
#include <AzCore/std/functional.h>
#include "Environment/OceanEnvironmentBus.h"
#include <Terrain/Bus/LegacyTerrainBus.h>
#include <IVegetationPoolManager.h>

int CTerrain::m_nUnitSize = 2;
float CTerrain::m_fInvUnitSize = 1.0f / 2.0f;
int CTerrain::m_nTerrainSize = 1024;
int CTerrain::m_nSectorSize = 64;
int CTerrain::m_nSectorsTableSize = 16;

CTerrain::CTerrain(const STerrainInfo& terrainInfo)
    : m_RootNode(nullptr)
    , m_nLoadedSectors(0)
    , m_bOceanIsVisible(false)
    , m_fDistanceToSectorWithWater(0)
    , m_pTerrainEf(nullptr)
    , m_UnitToSectorBitShift(0)
    , m_MeterToUnitBitShift(0)
{
    ResetHeightMapCache();

    // load default textures
    m_nWhiteTexId = GetRenderer()->EF_LoadTexture("EngineAssets/Textures/white.dds", FT_DONT_STREAM)->GetTextureID();
    m_nBlackTexId = GetRenderer()->EF_LoadTexture("EngineAssets/Textures/black.dds", FT_DONT_STREAM)->GetTextureID();

    // set params
    terrainInfo.LoadTerrainSettings(m_nUnitSize, m_fInvUnitSize, m_nTerrainSize, m_MeterToUnitBitShift, m_nTerrainSizeDiv, m_nSectorSize, m_nSectorsTableSize, m_UnitToSectorBitShift);

    m_lstSectors.reserve(512); // based on inspection in MemReplay

    m_SurfaceTypes.PreAllocate(SurfaceTile::MaxSurfaceCount, SurfaceTile::MaxSurfaceCount);

    InitHeightfieldPhysics();

    {
        SInputShaderResources Res;
        Res.m_LMaterial.m_Opacity = 1.0f;
        m_pTerrainEf = MakeSystemMaterialFromShader("Terrain", &Res);
    }

    m_pTerrainUpdateDispatcher = new CTerrainUpdateDispatcher();

    LegacyTerrain::CryTerrainRequestBus::Handler::BusConnect();
}

CTerrain::~CTerrain()
{
    INDENT_LOG_DURING_SCOPE(true, "Destroying terrain");

    SAFE_DELETE(m_pTerrainUpdateDispatcher);

    CloseTerrainTextureFile();

    for (int i = 0; i < m_SurfaceTypes.Count(); ++i)
    {
        m_SurfaceTypes[i].lstnVegetationGroups.Reset();
        m_SurfaceTypes[i].pLayerMat = NULL;
    }
    m_SurfaceTypes.Reset();

    SAFE_DELETE(m_RootNode);

    for (int i = 0; i < m_NodePyramid.Count(); i++)
    {
        m_NodePyramid[i].Reset();
    }
    m_NodePyramid.Reset();

    CTerrainNode::ResetStaticData();
    LegacyTerrain::CryTerrainRequestBus::Handler::BusDisconnect();
}

void CTerrain::InitHeightfieldPhysics()
{
    // for phys engine
    primitives::heightfield hf;
    hf.Basis.SetIdentity();
    hf.origin.zero();
    hf.step.x = hf.step.y = (float)CTerrain::GetHeightMapUnitSize();
    hf.size.x = hf.size.y = CTerrain::GetTerrainSize() / CTerrain::GetHeightMapUnitSize();
    hf.stride.set(hf.size.y + 1, 1);
    hf.heightscale = 1.0f;
    hf.typemask = SurfaceWeight::Hole | SurfaceWeight::Undefined;
    hf.typehole = SurfaceWeight::Hole;

    hf.fpGetHeightCallback = GetHeightFromTerrain_Callback;
    hf.fpGetSurfTypeCallback = GetSurfaceTypeFromTerrain_Callback;

    int arrMatMapping[SurfaceTile::MaxSurfaceCount];
    memset(arrMatMapping, 0, sizeof(arrMatMapping));
    for (int i = 0; i < SurfaceTile::MaxSurfaceCount; i++)
    {
        if (_smart_ptr<IMaterial> pMat = m_SurfaceTypes[i].pLayerMat)
        {
            if (pMat->GetSubMtlCount() > 2)
            {
                pMat = pMat->GetSubMtl(2);
            }
            arrMatMapping[i] = pMat->GetSurfaceTypeId();
        }
    }

    // KLUDGE: The 3rd parameter in the following call (nMats) cannot be 128 because it gets assigned to an unsigned:7!
    IPhysicalEntity* pPhysTerrain = GetPhysicalWorld()->SetHeightfieldData(&hf, arrMatMapping, SurfaceWeight::Undefined);
    pe_params_foreign_data pfd;
    pfd.iForeignData = PHYS_FOREIGN_ID_TERRAIN;
    pPhysTerrain->SetParams(&pfd);
}

void CTerrain::SendLegacyTerrainUpdateNotifications(int tileMinX, int tileMinY, int tileMaxX, int tileMaxY)
{
    AZ::u32 numTiles = CTerrain::m_NodePyramid[0].GetSize();
    AZ::u32 tileSize = CTerrain::m_nTerrainSize / (CTerrain::m_nUnitSize * numTiles);
    LegacyTerrain::LegacyTerrainNotificationBus::Broadcast(&LegacyTerrain::LegacyTerrainNotifications::SetNumTiles,
        numTiles, tileSize);

    AZ::u32 clampedMaxX = static_cast<AZ::u32>(tileMaxX);
    AZ::u32 clampedMinX = static_cast<AZ::u32>(AZ::GetMax(0, tileMinX - 1));
    AZ::u32 clampedMaxY = static_cast<AZ::u32>(tileMaxY);
    AZ::u32 clampedMinY = static_cast<AZ::u32>(AZ::GetMax(0, tileMinY - 1));

    // Iterate in reverse order here so that tiles are updated after the neighbours they share vertices with.
    for (AZ::u32 tileX = clampedMaxX; tileX-- > clampedMinX;)
    {
        for (AZ::u32 tileY = clampedMaxY; tileY-- > clampedMinY;)
        {
            CTerrainNode* node = CTerrain::m_NodePyramid[0][tileX][tileY];
            const SurfaceTile& tile = node->GetSurfaceTile();
            float heightMin = tile.GetOffset();
            // Cry terrain uses factors of 100.0f and 0.01f to convert heights between metres and centimetres.
            // The default height resolution is 1cm, unless the range of heights would be too large to fit
            // into a 16 bit integer, in which case a step of an integer number of centimetres is used.
            int heightRange = int(tile.GetRange() * 100.0f);
            int step = heightRange ? (heightRange + UINT16_MAX - 1) / UINT16_MAX : 1;
            float heightScale = step * 0.01f;

            LegacyTerrain::LegacyTerrainNotificationBus::Broadcast(&LegacyTerrain::LegacyTerrainNotifications::UpdateTile,
                tileX, tileY, tile.GetHeightmap(), heightMin, heightScale, tileSize, CTerrain::GetHeightMapUnitSize());
        }
    }
}

void CTerrain::BuildSectorsTree(bool bBuildErrorsTable)
{
    if (m_RootNode)
    {
        return;
    }

    m_NodePyramid.PreAllocate(TERRAIN_NODE_TREE_DEPTH, TERRAIN_NODE_TREE_DEPTH);
    int nCount = 0;
    for (int i = 0; i < TERRAIN_NODE_TREE_DEPTH; i++)
    {
        int nSectors = CTerrain::GetSectorsTableSize() >> i;
        m_NodePyramid[i].Allocate(nSectors);
        nCount += nSectors * nSectors;
    }

    m_SurfaceTypes.PreAllocate(SurfaceTile::MaxSurfaceCount, SurfaceTile::MaxSurfaceCount);

    float fStartTime = GetCurAsyncTimeSec();

    // Log() to use LogPlus() later
    PrintMessage(
        bBuildErrorsTable ?
        "Compiling %d terrain nodes (%.1f MB) " :
        "Constructing %d terrain nodes (%.1f MB) ",
        nCount,
        float(sizeof(CTerrainNode) * nCount) / 1024.f / 1024.f);

    m_RootNode = new CTerrainNode();
    m_RootNode->Init(0, 0, CTerrain::GetTerrainSize(), NULL, bBuildErrorsTable);

    if (Get3DEngine()->IsObjectTreeReady())
    {
        Get3DEngine()->GetObjectTree()->UpdateTerrainNodes();
    }

#if !defined(_RELEASE)
    const int numTilesX = m_NodePyramid[0].GetSize();
    AZ::Debug::TerrainProfiler::GetInstance().SetTotalTilesCount(numTilesX * numTilesX);
#endif

    PrintMessagePlus(" done in %.2f sec", GetCurAsyncTimeSec() - fStartTime);
}

void CTerrain::AddVisSector(CTerrainNode* newsec)
{
    assert(newsec->m_QueuedLOD < m_UnitToSectorBitShift);
    m_lstVisSectors.Add((CTerrainNode*)newsec);
}

void CTerrain::CheckVis(const SRenderingPassInfo& passInfo)
{
    FUNCTION_PROFILER_3DENGINE_LEGACYONLY;
    AZ_TRACE_METHOD();
    AZ_PROFILE_TIMER("TerrainProfiler", TERRAIN_STATISTIC_CHECK_VISIBILITY);

    if (passInfo.IsGeneralPass())
    {
        //Eventually the value if this variable will change once m_RootNode->CheckVis() is called.
        m_fDistanceToSectorWithWater = OCEAN_IS_VERY_FAR_AWAY;
    }

    if (m_RootNode)
    {
        // reopen texture file if needed, texture pack may be randomly closed by editor so automatic reopening used
        if (!m_MacroTexture && m_bEditor)
        {
            OpenTerrainTextureFile(COMPILED_TERRAIN_TEXTURE_FILE_NAME);
        }

        m_RootNode->CheckVis(false, (GetCVars()->e_CoverageBufferTerrain != 0) && (GetCVars()->e_CoverageBuffer != 0), passInfo);
    }

    if (passInfo.IsGeneralPass())
    {
        m_bOceanIsVisible = (int)((m_fDistanceToSectorWithWater != OCEAN_IS_VERY_FAR_AWAY) || !m_lstVisSectors.Count());

        if (m_fDistanceToSectorWithWater < 0)
        {
            m_fDistanceToSectorWithWater = 0;
        }

        if (!m_lstVisSectors.Count())
        {
            m_fDistanceToSectorWithWater = 0;
        }

        const float fOceanWaterLevel = m_pOcean ? m_pOcean->GetWaterLevel() : 0;
        m_fDistanceToSectorWithWater = max(m_fDistanceToSectorWithWater, (passInfo.GetCamera().GetPosition().z - fOceanWaterLevel));
    }
}

void CTerrain::ClearVisSectors()
{
    m_lstVisSectors.Clear();
}

void CTerrain::ClearTextureSets()
{
    AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::Renderer);

    TraverseTree([](CTerrainNode* node)
        {
            node->m_TextureSet = SSectorTextureSet(0);
        });
}

int __cdecl CmpTerrainNodesDistance(const void* v1, const void* v2)
{
    CTerrainNode* p1 = *(CTerrainNode**)v1;
    CTerrainNode* p2 = *(CTerrainNode**)v2;

    float f1 = (p1->m_DistanceToCamera[0]);
    float f2 = (p2->m_DistanceToCamera[0]);
    if (f1 > f2)
    {
        return 1;
    }
    else if (f1 < f2)
    {
        return -1;
    }

    return 0;
}

void CTerrain::ActivateNodeProcObj(CTerrainNode* pNode)
{
    if (m_lstActiveProcObjNodes.Find(pNode) < 0)
    {
        m_lstActiveProcObjNodes.Add(pNode);
    }
}


void CTerrain::RequestTerrainUpdate()
{
    if (m_NodePyramid.Size() > 0)
    {
        SendLegacyTerrainUpdateNotifications(0, 0, m_NodePyramid[0].GetSize(), m_NodePyramid[0].GetSize());
    }
}

void CTerrain::CheckNodesGeomUnload(const SRenderingPassInfo& passInfo)
{
    AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::Renderer);

    if (!Get3DEngine()->m_bShowTerrainSurface || !m_RootNode)
    {
        return;
    }

    for (int n = 0; n < 32; n++)
    {
        static uint32 nOldSectorsX = ~0, nOldSectorsY = ~0, nTreeLevel = ~0;
        const uint32 treeLevel = static_cast<uint32>(m_RootNode->m_nTreeLevel);

        if (nTreeLevel > treeLevel)
        {
            nTreeLevel = treeLevel;
        }

        uint32 nTableSize = CTerrain::GetSectorsTableSize() >> nTreeLevel;
        assert(nTableSize);

        // x/y cycle
        nOldSectorsY++;
        if (nOldSectorsY >= nTableSize)
        {
            nOldSectorsY = 0;
            nOldSectorsX++;
        }

        if (nOldSectorsX >= nTableSize)
        {
            nOldSectorsX = 0;
            nOldSectorsY = 0;
            nTreeLevel++;
        }

        if (nTreeLevel > treeLevel)
        {
            nTreeLevel = 0;
        }

        if (CTerrainNode* pNode = m_NodePyramid[nTreeLevel][nOldSectorsX][nOldSectorsY])
        {
            pNode->CheckNodeGeomUnload(passInfo);
        }
    }
}

void CTerrain::UpdateNodesIncrementaly(const SRenderingPassInfo& passInfo)
{
    FUNCTION_PROFILER_3DENGINE_LEGACYONLY;
    AZ_TRACE_METHOD();
    AZ_PROFILE_TIMER("TerrainProfiler", TERRAIN_STATISTIC_UPDATE_NODES);

    ProcessTextureStreamingRequests(passInfo);

    // process procedural objects
    if (m_lstActiveProcObjNodes.Count())
    {
        LegacyProceduralVegetation::IVegetationPoolManager& vegPoolManager = Get3DEngine()->GetIVegetationPoolManager();

        // make sure distances are correct
        for (int i = 0; i < m_lstActiveProcObjNodes.Count(); i++)
        {
            m_lstActiveProcObjNodes[i]->UpdateDistance(passInfo);
        }

        // sort by distance
        qsort(m_lstActiveProcObjNodes.GetElements(), m_lstActiveProcObjNodes.Count(),
            sizeof(m_lstActiveProcObjNodes[0]), CmpTerrainNodesDistance);

        // release unimportant sectors
        static int nMaxSectors = vegPoolManager.GetMaxSectors();
        while (m_lstActiveProcObjNodes.Count() > (vegPoolManager.IsProceduralVegetationEnabled() ? nMaxSectors : 0))
        {
            m_lstActiveProcObjNodes.Last()->RemoveProcObjects(false);
            m_lstActiveProcObjNodes.DeleteLast();
        }

        const int maxChunksPerSector = vegPoolManager.GetMaxVegetationChunksPerSector();
        while (1)
        {
            // release even more if we are running out of memory
            while (m_lstActiveProcObjNodes.Count())
            {
                int nAll = 0;
                int nUsed = vegPoolManager.GetUsedVegetationChunksCount(nAll);
                if (nAll - nUsed > maxChunksPerSector)// make sure at least X chunks are free and ready to be used in this frame
                {
                    break;
                }

                m_lstActiveProcObjNodes.Last()->RemoveProcObjects(false);
                m_lstActiveProcObjNodes.DeleteLast();

                nMaxSectors = min(nMaxSectors, m_lstActiveProcObjNodes.Count());
            }

            // build most important not ready sector
            bool bAllDone = true;
            for (int i = 0; i < m_lstActiveProcObjNodes.Count(); i++)
            {
                if (m_lstActiveProcObjNodes[i]->CheckUpdateProcObjects(passInfo))
                {
                    bAllDone = false;
                    break;
                }
            }

            if (!Get3DEngine()->IsTerrainSyncLoad() || bAllDone)
            {
                break;
            }
        }

        IF (GetCVars()->e_ProcVegetation == 2, 0)
        {
            for (int i = 0; i < m_lstActiveProcObjNodes.Count(); i++)
            {
                DrawBBox(m_lstActiveProcObjNodes[i]->GetBBoxVirtual(),
                    m_lstActiveProcObjNodes[i]->IsProcObjectsReady() ? Col_Green : Col_Red);
            }
        }
    }
}

void CTerrain::GetTerrainAlignmentMatrix(const Vec3& vPos, const float amount, Matrix33& matrix33)
{
    //All paths from GetTerrainSurfaceNormal() return a normalized vector
    Vec3 vTerrainNormal = GetTerrainSurfaceNormal(vPos, 0.5f);
    vTerrainNormal = LERP(Vec3(0, 0, 1.f), vTerrainNormal, amount);
    vTerrainNormal.Normalize();
    Vec3 vDir = Vec3(-1, 0, 0).Cross(vTerrainNormal);

    matrix33 = matrix33.CreateOrientation(vDir, -vTerrainNormal, 0);
}

void CTerrain::GetMaterials(AZStd::vector<_smart_ptr<IMaterial>>& materials)
{
    TraverseTree([&materials](CTerrainNode* node)
    {
        node->GetMaterials(materials);
    });
}

void CTerrain::GetMemoryUsage(class ICrySizer* pSizer) const
{
    pSizer->AddObject(this, sizeof(*this));

    {
        SIZER_COMPONENT_NAME(pSizer, "NodesTree");
        if (m_RootNode)
        {
            m_RootNode->GetMemoryUsage(pSizer);
        }
    }

    {
        SIZER_COMPONENT_NAME(pSizer, "SecInfoTable");
        pSizer->AddObject(m_NodePyramid);
    }

    pSizer->AddObject(m_lstVisSectors);
    pSizer->AddObject(m_lstActiveTextureNodes);
    pSizer->AddObject(m_lstActiveProcObjNodes);
    pSizer->AddObject(m_pOcean);

    m_MacroTexture->GetMemoryUsage(pSizer);

    {
        SIZER_COMPONENT_NAME(pSizer, "TerrainSectorRenderTemp Data");
        if (m_pTerrainUpdateDispatcher)
        {
            m_pTerrainUpdateDispatcher->GetMemoryUsage(pSizer);
        }
    }

    CTerrainNode::GetStaticMemoryUsage(pSizer);
}

void CTerrain::IntersectWithShadowFrustum(PodArray<CTerrainNode*>* plstResult, ShadowMapFrustum* pFrustum, const SRenderingPassInfo& passInfo)
{
    if (m_RootNode)
    {
        float fHalfGSMBoxSize = 0.5f / (pFrustum->fFrustrumSize * Get3DEngine()->m_fGsmRange);

        m_RootNode->IntersectWithShadowFrustum(false, plstResult, pFrustum, fHalfGSMBoxSize, passInfo);
    }
}

void CTerrain::IntersectWithBox(const AABB& aabbBox, PodArray<CTerrainNode*>* plstResult)
{
    if (m_RootNode)
    {
        m_RootNode->IntersectWithBox(aabbBox, plstResult);
    }
}

void CTerrain::MarkAllSectorsAsUncompiled()
{
    if (m_RootNode)
    {
        m_RootNode->RemoveProcObjects(true);
        Get3DEngine()->GetObjectTree()->MarkAsUncompiled();
    }
}

void CTerrain::GetResourceMemoryUsage(ICrySizer* pSizer, const AABB& crstAABB)
{
    CTerrainNode*   poTerrainNode = FindMinNodeContainingBox(crstAABB);

    if (poTerrainNode)
    {
        poTerrainNode->GetResourceMemoryUsage(pSizer, crstAABB);
    }
}

void CTerrain::TraverseTree(AZStd::function<void(CTerrainNode*)> callback)
{
    if (!m_NodePyramid.Count())
    {
        return;
    }

    for (int nTreeLevel = 0; nTreeLevel < TERRAIN_NODE_TREE_DEPTH; nTreeLevel++)
    {
        for (int x = 0; x < m_NodePyramid[nTreeLevel].GetSize(); x++)
        {
            for (int y = 0; y < m_NodePyramid[nTreeLevel].GetSize(); y++)
            {
                callback(m_NodePyramid[nTreeLevel][x][y]);
            }
        }
    }
}

struct CTerrain__Cmp_Sectors
{
    CTerrain__Cmp_Sectors(const SRenderingPassInfo& state)
        : passInfo(state) {}

    bool __cdecl operator()(CTerrainNode* p1, CTerrainNode* p2)
    {
        int nRecursiveLevel = passInfo.GetRecursiveLevel();

        // if same - give closest sectors higher priority
        if (p1->m_DistanceToCamera[nRecursiveLevel] > p2->m_DistanceToCamera[nRecursiveLevel])
        {
            return false;
        }
        else if (p1->m_DistanceToCamera[nRecursiveLevel] < p2->m_DistanceToCamera[nRecursiveLevel])
        {
            return true;
        }

        return false;
    }
private:
    const SRenderingPassInfo passInfo;
};

void CTerrain::DrawVisibleSectors(const SRenderingPassInfo& passInfo)
{
    FUNCTION_PROFILER_3DENGINE_LEGACYONLY;
    AZ_TRACE_METHOD();
    AZ_PROFILE_TIMER("TerrainProfiler", TERRAIN_STATISTIC_DRAW_VISIBLE_SECTORS);

    if (!passInfo.RenderTerrain() || !Get3DEngine()->m_bShowTerrainSurface)
    {
        return;
    }

    const int visibleSectorsCount = m_lstVisSectors.Count();

#if !defined(_RELEASE)
    AZ::Debug::TerrainProfiler::GetInstance().SetVisibleTilesCount(visibleSectorsCount);
#endif

    // sort to get good texture streaming priority
    if (visibleSectorsCount)
    {
        std::sort(m_lstVisSectors.GetElements(), m_lstVisSectors.GetElements() + visibleSectorsCount, CTerrain__Cmp_Sectors(passInfo));
    }

    for (int i = 0; i < visibleSectorsCount; i++)
    {
        CTerrainNode* pNode = (CTerrainNode*)m_lstVisSectors[i];

        if (!pNode->RenderNodeHeightmap(passInfo))
        {
            m_pTerrainUpdateDispatcher->QueueJob(pNode, passInfo);
        }
    }
}

void CTerrain::UpdateSectorMeshes(const SRenderingPassInfo& passInfo)
{
    AZ_TRACE_METHOD();
    AZ_PROFILE_TIMER("TerrainProfiler", TERRAIN_STATISTIC_UPDATE_SECTOR_MESHES);
    m_pTerrainUpdateDispatcher->SyncAllJobs(false, passInfo);
}


static Vec3 GetTerrainNormal(CTerrain& terrain, int x, int y)
{
    const int   nTerrainSize = CTerrain::GetTerrainSize();
    const int nRange = terrain.GetHeightMapUnitSize();

    float sx;
    if ((x + nRange) < nTerrainSize && x >= nRange)
    {
        sx = terrain.GetZ(x + nRange, y) - terrain.GetZ(x - nRange, y);
    }
    else
    {
        sx = 0;
    }

    float sy;
    if ((y + nRange) < nTerrainSize && y >= nRange)
    {
        sy = terrain.GetZ(x, y + nRange) - terrain.GetZ(x, y - nRange);
    }
    else
    {
        sy = 0;
    }

    Vec3 vNorm(-sx, -sy, nRange * 2.0f);
    return vNorm.GetNormalized();
}

_smart_ptr<IRenderMesh> CTerrain::MakeAreaRenderMesh(const Vec3& vPos, float fRadius,
    _smart_ptr<IMaterial> pMat, const char* szLSourceName,
    Plane* planes)
{
    PodArray<vtx_idx> lstIndices;
    lstIndices.Clear();
    PodArray<vtx_idx> lstClippedIndices;
    lstClippedIndices.Clear();
    PodArray<Vec3> posBuffer;
    posBuffer.Clear();
    PodArray<SVF_P3S_C4B_T2S> vertBuffer;
    vertBuffer.Clear();
    PodArray<SPipTangents> tangBasises;
    tangBasises.Clear();

    Vec3 vPt(vPos);
    int nUnitSize = GetTerrain()->GetHeightMapUnitSize();

    Vec3i vBoxMin = vPos - Vec3(fRadius, fRadius, fRadius);
    Vec3i vBoxMax = vPos + Vec3(fRadius, fRadius, fRadius);

    vBoxMin.x = vBoxMin.x / nUnitSize * nUnitSize;
    vBoxMin.y = vBoxMin.y / nUnitSize * nUnitSize;
    vBoxMin.z = vBoxMin.z / nUnitSize * nUnitSize;

    vBoxMax.x = vBoxMax.x / nUnitSize * nUnitSize + nUnitSize;
    vBoxMax.y = vBoxMax.y / nUnitSize * nUnitSize + nUnitSize;
    vBoxMax.z = vBoxMax.z / nUnitSize * nUnitSize + nUnitSize;

    int nSizeX = (vBoxMax.x - vBoxMin.x) / nUnitSize;
    int nSizeY = (vBoxMax.y - vBoxMin.y) / nUnitSize;

    int nEstimateVerts = nSizeX * nSizeY;
    nEstimateVerts = clamp_tpl(nEstimateVerts, 100, 10000);
    posBuffer.reserve(nEstimateVerts);
    lstIndices.reserve(nEstimateVerts * 6);
    lstClippedIndices.reserve(nEstimateVerts * 6);

    const CTerrain* pTerrain = GetTerrain();
    for (int x = vBoxMin.x; x <= vBoxMax.x; x += nUnitSize)
    {
        for (int y = vBoxMin.y; y <= vBoxMax.y; y += nUnitSize)
        {
            Vec3 vTmp = Vec3((float)x, (float)y, (float)(pTerrain->GetZ(x, y)));
            posBuffer.Add(vTmp);
        }
    }

    for (int x = 0; x < nSizeX; x++)
    {
        for (int y = 0; y < nSizeY; y++)
        {
            vtx_idx id0 = (x + 0) * (nSizeY + 1) + (y + 0);
            vtx_idx id1 = (x + 1) * (nSizeY + 1) + (y + 0);
            vtx_idx id2 = (x + 0) * (nSizeY + 1) + (y + 1);
            vtx_idx id3 = (x + 1) * (nSizeY + 1) + (y + 1);

            assert((int)id3 < posBuffer.Count());

            if (pTerrain->IsHole(vBoxMin.x + x, vBoxMin.y + y))
            {
                continue;
            }


            if (pTerrain->IsMeshQuadFlipped(vBoxMin.x + x, vBoxMin.y + y, nUnitSize))
            {
                lstIndices.Add(id0);
                lstIndices.Add(id1);
                lstIndices.Add(id3);

                lstIndices.Add(id0);
                lstIndices.Add(id3);
                lstIndices.Add(id2);
            }
            else
            {
                lstIndices.Add(id0);
                lstIndices.Add(id1);
                lstIndices.Add(id2);

                lstIndices.Add(id2);
                lstIndices.Add(id1);
                lstIndices.Add(id3);
            }
        }
    }

    // clip triangles
    if (planes)
    {
        CPolygonClipContext clipContext;

        int indexCount = lstIndices.Count();
        for (int i = 0; i < indexCount; i += 3)
        {
            CRoadRenderNode::ClipTriangle(clipContext, posBuffer, lstIndices, lstClippedIndices, i, planes);
        }
    }

    AABB bbox;
    bbox.Reset();
    for (int i = 0, nIndexCount = lstClippedIndices.size(); i < nIndexCount; i++)
    {
        bbox.Add(posBuffer[lstClippedIndices[i]]);
    }

    tangBasises.reserve(posBuffer.size());
    vertBuffer.reserve(posBuffer.size());

    for (int i = 0, nPosCount = posBuffer.size(); i < nPosCount; i++)
    {
        SVF_P3S_C4B_T2S vTmp;
        vTmp.xyz = posBuffer[i] - vPos;
        vTmp.color.dcolor = uint32(-1);
        vTmp.st = Vec2(0, 0);
        vertBuffer.Add(vTmp);

        SPipTangents basis;

        Vec3 vTang = Vec3(0, 1, 0);
        Vec3 vBitang = Vec3(-1, 0, 0);
        Vec3 vNormal = GetTerrainNormal(*this, fastround_positive(posBuffer[i].x), fastround_positive(posBuffer[i].y));

        // Orthonormalize Tangent Frame
        vBitang = -vNormal.Cross(vTang);
        vBitang.Normalize();
        vTang = vNormal.Cross(vBitang);
        vTang.Normalize();

        tangBasises.Add(SPipTangents(vTang, vBitang, -1));
    }

    _smart_ptr<IRenderMesh> pMesh = GetRenderer()->CreateRenderMeshInitialized(
            vertBuffer.GetElements(), vertBuffer.Count(), eVF_P3S_C4B_T2S,
            lstClippedIndices.GetElements(), lstClippedIndices.Count(), prtTriangleList,
            szLSourceName, szLSourceName, eRMT_Static, 1, 0, NULL, NULL, false, true, tangBasises.GetElements());

    float texelAreaDensity = 1.0f;

    pMesh->SetChunk(pMat, 0, vertBuffer.Count(), 0, lstClippedIndices.Count(), texelAreaDensity, eVF_P3S_C4B_T2S);
    pMesh->SetBBox(bbox.min - vPos, bbox.max - vPos);
    if (pMesh->GetChunks()[0].pRE)
    {
        pMesh->GetChunks()[0].pRE->mfUpdateFlags(FCEF_DIRTY);
    }

    return pMesh;
}

bool CTerrain::RenderArea(Vec3 vPos, float fRadius, _smart_ptr<IRenderMesh>& pRenderMesh,
    CRenderObject* pObj, _smart_ptr<IMaterial> pMaterial, const char* szComment, float* pCustomData,
    Plane* planes, const SRenderingPassInfo& passInfo)
{
    if (passInfo.IsRecursivePass())
    {
        return false;
    }

    FUNCTION_PROFILER_3DENGINE;

    bool bREAdded = false;

    if (!pRenderMesh)
    {
        pRenderMesh = MakeAreaRenderMesh(vPos, fRadius, pMaterial, szComment, planes);
    }

    if (pRenderMesh && pRenderMesh->GetIndicesCount())
    {
        pRenderMesh->SetREUserData(pCustomData);
        pRenderMesh->AddRE(pMaterial, pObj, 0, passInfo, EFSLIST_GENERAL, 1, SRendItemSorter::CreateDefaultRendItemSorter());
        bREAdded = true;
    }

    return bREAdded;
}

int CTerrain::GetNodesData(byte*& pData, int& nDataSize, EEndian eEndian, SHotUpdateInfo* pExportInfo)
{
    if (m_RootNode)
    {
        return m_RootNode->GetData(pData, nDataSize, eEndian, pExportInfo);
    }
    return 0;
}
