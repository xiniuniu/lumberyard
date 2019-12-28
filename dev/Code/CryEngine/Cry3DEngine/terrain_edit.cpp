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

// Description : add/remove static objects, modify hmap (used by editor)


#include "StdAfx.h"

#include "terrain.h"
#include "terrain_sector.h"
#include "Ocean.h"
#include "ObjMan.h"
#include "3dEngine.h"
#include "RoadRenderNode.h"
#include "MergedMeshRenderNode.h"
#include "IEntitySystem.h"
#include <MathConversion.h>


//////////////////////////////////////////////////////////////////////////
#define GET_Z_VAL(_x, _y) heightmap[(_x) * nTerrainSize + (_y)]

void CTerrain::BuildErrorsTableForArea(
    float* pLodErrors, int nMaxLods,
    int X1, int Y1, int X2, int Y2,
    const float* heightmap,
    int weightmapSize,
    const SurfaceWeight* weightmap)
{
    memset(pLodErrors, 0, nMaxLods * sizeof(pLodErrors[0]));
    int nSectorSize = CTerrain::GetSectorSize() / CTerrain::GetHeightMapUnitSize();
    int nTerrainSize = CTerrain::GetTerrainSize() / CTerrain::GetHeightMapUnitSize();

    bool bSectorHasHoles = false;
    bool bSectorHasMesh = false;

    {
        int nLodUnitSize = 1;
        int x1 = max(0, X1 - nLodUnitSize);
        int x2 = min(nTerrainSize - nLodUnitSize, X2 + nLodUnitSize);
        int y1 = max(0, Y1 - nLodUnitSize);
        int y2 = min(nTerrainSize - nLodUnitSize, Y2 + nLodUnitSize);

        for (int X = x1; X < x2; X += nLodUnitSize)
        {
            for (int Y = y1; Y < y2; Y += nLodUnitSize)
            {
                int nSurfX = (X - X1);
                int nSurfY = (Y - Y1);
                if (nSurfX >= 0 && nSurfY >= 0 && nSurfX < weightmapSize && nSurfY < weightmapSize && weightmap)
                {
                    int nSurfCell = nSurfX * weightmapSize + nSurfY;
                    assert(nSurfCell >= 0 && nSurfCell < weightmapSize * weightmapSize);
                    if (weightmap[nSurfCell].Ids[0] == SurfaceWeight::Hole)
                    {
                        bSectorHasHoles = true;
                    }
                    else
                    {
                        bSectorHasMesh = true;
                    }
                }
            }
        }
    }

    bool bHasHoleEdges = (bSectorHasHoles && bSectorHasMesh);

    //
    // Loops through all the LOD's above 0 and computes the maximum difference in meters
    // between LOD N and LOD 0.
    //
    for (int nLod = 1; nLod < nMaxLods; nLod++)
    {
        float fMaxDiff = 0;

        // Holes can arbitrarily affect the maximum difference based on the given CVar.
        if (bHasHoleEdges)
        {
            fMaxDiff = max(fMaxDiff, GetFloatCVar(e_TerrainLodRatioHolesMin));
        }

        int nLodUnitSize = (1 << nLod);
        assert(nLodUnitSize <= nSectorSize);

        int x1 = max(0, X1 - nLodUnitSize);
        int x2 = min(nTerrainSize - nLodUnitSize, X2 + nLodUnitSize);
        int y1 = max(0, Y1 - nLodUnitSize);
        int y2 = min(nTerrainSize - nLodUnitSize, Y2 + nLodUnitSize);

        for (int X = x1; X < x2; X += nLodUnitSize)
        {
            for (int Y = y1; Y < y2; Y += nLodUnitSize)
            {
                //
                // This is doing a bilinear filter of the Z value along each unit in the height grid within the
                // LOD quad. The maximum Z difference is stored as the error for this LOD.
                //
                for (int x = 1; x < nLodUnitSize; x++)
                {
                    float u = (float)x / (float)nLodUnitSize;

                    float z1 = (1.f - u) * GET_Z_VAL(X + 0, Y + 0) + (u) * GET_Z_VAL(X + nLodUnitSize, Y + 0);
                    float z2 = (1.f - u) * GET_Z_VAL(X + 0, Y + nLodUnitSize) + (u) * GET_Z_VAL(X + nLodUnitSize, Y + nLodUnitSize);

                    for (int y = 1; y < nLodUnitSize; y++)
                    {
                        // skip map borders
                        int nBorder = (nSectorSize >> 2);
                        if ((X + x) < nBorder || (X + x) > (nTerrainSize - nBorder) ||
                            (Y + y) < nBorder || (Y + y) > (nTerrainSize - nBorder))
                        {
                            continue;
                        }

                        float v = (float)y / nLodUnitSize;
                        float fInterpolatedZ = (1.f - v) * z1 + v * z2;
                        float fRealZ = GET_Z_VAL(X + x, Y + y);
                        float fDiff = fabs_tpl(fRealZ - fInterpolatedZ);

                        if (fDiff > fMaxDiff)
                        {
                            fMaxDiff = fDiff;
                        }
                    }
                }
            }
        }
        // note: values in m_arrGeomErrors table may be non incremental - this is correct
        pLodErrors[nLod] = fMaxDiff;
    }
}


static float GetHeightClamped(const float* heightmap, int heightmapSize, int x, int y)
{
    return heightmap[CLAMP(x, 0, heightmapSize - 1) * heightmapSize + CLAMP(y, 0, heightmapSize - 1)];
}

static float GetHeight(const float* heightmap, int heightmapSize, int x, int y)
{
    return heightmap[x * heightmapSize + y];
}

void CTerrain::SetTerrainElevation(int offsetX, int offsetY, int areaSize, const float* heightmap, int weightmapSize, const SurfaceWeight* weightmap)
{
    LOADING_TIME_PROFILE_SECTION;
    FUNCTION_PROFILER_3DENGINE;

    const float StartTime = GetCurAsyncTimeSec();
    const Meter MetersPerUnit = CTerrain::GetHeightMapUnitSize();
    const Unit HeightmapSize = CTerrain::GetTerrainSize() / MetersPerUnit;

    ResetHeightMapCache();
    InitHeightfieldPhysics();

    // everything is in units in this function
    assert(offsetX == ((offsetX >> m_UnitToSectorBitShift) << m_UnitToSectorBitShift));
    assert(offsetY == ((offsetY >> m_UnitToSectorBitShift) << m_UnitToSectorBitShift));
    assert(areaSize == ((areaSize >> m_UnitToSectorBitShift) << m_UnitToSectorBitShift));
    assert(areaSize == ((areaSize >> m_UnitToSectorBitShift) << m_UnitToSectorBitShift));
    assert (offsetX >= 0 && offsetY >= 0 && offsetX + areaSize <= HeightmapSize && offsetY + areaSize <= HeightmapSize);

    BuildSectorsTree(false);

    // Shift units down to sectors. Find min and max extents of the input quad.
    using Sector = int;
    const Sector SectorMinX = offsetX >> m_UnitToSectorBitShift;
    const Sector SectorMinY = offsetY >> m_UnitToSectorBitShift;
    const Sector SectorMaxX = (offsetX + areaSize) >> m_UnitToSectorBitShift;
    const Sector SectorMaxY = (offsetY + areaSize) >> m_UnitToSectorBitShift;
    // TODO : figure out if the water level should be used to calculate the Terrain node's AABB.
    float fOceanLevel = WATER_LEVEL_UNKNOWN;
    if (OceanToggle::IsActive())
    {
        fOceanLevel = OceanRequest::GetOceanLevel();
    }
    else
    {
        fOceanLevel = m_pOcean ? m_pOcean->GetWaterLevel() : WATER_LEVEL_UNKNOWN;
    }
    for (Sector sectorX = SectorMinX; sectorX < SectorMaxX; sectorX++)
    {
        for (Sector sectorY = SectorMinY; sectorY < SectorMaxY; sectorY++)
        {
            CTerrainNode* leafNode = m_NodePyramid[0][sectorX][sectorY];

            const Unit x1 = (sectorX << m_UnitToSectorBitShift);
            const Unit y1 = (sectorY << m_UnitToSectorBitShift);
            const Unit x2 = ((sectorX + 1) << m_UnitToSectorBitShift);
            const Unit y2 = ((sectorY + 1) << m_UnitToSectorBitShift);

            // find min/max
            float heightMin = GetHeight(heightmap, HeightmapSize, x1, y1);
            float heightMax = heightMin;

            for (Unit x = x1; x <= x2; x++)
            {
                for (Unit y = y1; y <= y2; y++)
                {
                    float z = GetHeightClamped(heightmap, HeightmapSize, x, y);
                    heightMax = max(heightMax, z);
                    heightMin = min(heightMin, z);
                }
            }

            leafNode->m_LocalAABB.min.Set((float)(x1 * MetersPerUnit), (float)(y1 * MetersPerUnit), heightMin);
            leafNode->m_LocalAABB.max.Set((float)(x2 * MetersPerUnit), (float)(y2 * MetersPerUnit), max(heightMax + TerrainConstants::coloredVegetationMaxSafeHeight, fOceanLevel));

            // Build height-based error metrics for sector.
            {
                if (!leafNode->m_ZErrorFromBaseLOD)
                {
                    leafNode->m_ZErrorFromBaseLOD = new float[m_UnitToSectorBitShift];
                }

                BuildErrorsTableForArea(
                    leafNode->m_ZErrorFromBaseLOD,
                    m_UnitToSectorBitShift,
                    x1, y1, x2, y2,
                    heightmap,
                    weightmapSize,
                    weightmap);

                assert(leafNode->m_ZErrorFromBaseLOD[0] == 0);
            }

            // Assign height and surface ids to the system memory buffer.
            {
                SurfaceTile& tile = leafNode->m_SurfaceTile;

                const Unit SectorSize = SectorSize_Units();
                if (tile.GetSize() != SectorSize)
                {
                    tile.ResetMaps(SectorSize);
                }
                tile.SetRange(heightMin, heightMax - heightMin);

                for (Unit x = x1; x <= x2; ++x)
                {
                    for (Unit y = y1; y <= y2; ++y)
                    {
                        // Index relative to this sector
                        int indexLocal = (x - x1) * SectorSize + (y - y1);
                        tile.SetHeightByIndex(indexLocal, GetHeightClamped(heightmap, HeightmapSize, x, y));

                        // Index relative to union of all sector tiles in the parent loop.
                        {
                            // height values can index +1 off the end, but surface ids can't.
                            int indexGlobal = (x - offsetX) * weightmapSize + (y - offsetY);
                            tile.SetWeightByIndex(indexLocal, weightmap[indexGlobal]);
                        }
                    }
                }
            }

            leafNode->PropagateChangesToRoot();
        }
    }

    if (GetCurAsyncTimeSec() - StartTime > 1)
    {
        PrintMessage("CTerrain::SetTerrainElevation took %.2f sec", GetCurAsyncTimeSec() - StartTime);
    }

    if (Get3DEngine()->IsObjectTreeReady())
    {
        Get3DEngine()->GetObjectTree()->UpdateTerrainNodes();
    }

    // update roads
    if (Get3DEngine()->IsObjectTreeReady() && m_bEditor)
    {
        PodArray<IRenderNode*> lstRoads;

        AABB aabb(
            Vec3(
                (MeterF)(offsetX) * (MeterF)MetersPerUnit,
                (MeterF)(offsetY) * (MeterF)MetersPerUnit,
                0.f),
            Vec3(
                (MeterF)(offsetX) * (MeterF)MetersPerUnit + (MeterF)areaSize * (MeterF)MetersPerUnit,
                (MeterF)(offsetY) * (MeterF)MetersPerUnit + (MeterF)areaSize * (MeterF)MetersPerUnit,
                1024.f));

        Get3DEngine()->GetObjectTree()->GetObjectsByType(lstRoads, eERType_Road, &aabb);
        for (int i = 0; i < lstRoads.Count(); i++)
        {
            CRoadRenderNode* pRoad = (CRoadRenderNode*)lstRoads[i];
            pRoad->OnTerrainChanged();
        }
    }

    SendLegacyTerrainUpdateNotifications(SectorMinX, SectorMinY, SectorMaxX, SectorMaxY);
}

void CTerrain::ResetTerrainVertBuffers()
{
    if (m_RootNode)
    {
        m_RootNode->ReleaseHeightMapGeometry(true, nullptr);
    }
}