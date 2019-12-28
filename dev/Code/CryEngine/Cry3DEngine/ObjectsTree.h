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

#ifndef CRYINCLUDE_CRY3DENGINE_OBJECTSTREE_H
#define CRYINCLUDE_CRY3DENGINE_OBJECTSTREE_H
#pragma once

#define OCTREENODE_RENDER_FLAG_OBJECTS      1
#define OCTREENODE_RENDER_FLAG_OCCLUDERS    2
#define OCTREENODE_RENDER_FLAG_CASTERS      4
#define OCTREENODE_RENDER_FLAG_OBJECTS_ONLY_ENTITIES 8

#define OCTREENODE_CHUNK_VERSION_OLD 3
#define OCTREENODE_CHUNK_VERSION 5

#include <AzCore/Casting/numeric_cast.h>

class CBrush;
class COctreeNode;
template <class T, size_t overAllocBytes>
class PodArray;
struct CLightEntity;
struct ILightSource;
struct IParticleEmitter;
///////////////////////////////////////////////////////////////////////////////
// data to be pushed to occlusion culler
_MS_ALIGN(16) struct SCheckOcclusionJobData
{
    enum JobTypeT
    {
        QUIT, 
        OCTREE_NODE, 
        TERRAIN_NODE, 
#ifdef LY_TERRAIN_RUNTIME
        TERRAIN_SYSTEM_NODE
#endif
    };

    SCheckOcclusionJobData() {}

    static SCheckOcclusionJobData CreateQuitJobData();
    static SCheckOcclusionJobData CreateOctreeJobData(COctreeNode* pOctTreeNode, int nRenderMask, const SRendItemSorter& rendItemSorter, const CCamera* pCam);
    static SCheckOcclusionJobData CreateTerrainJobData(CTerrainNode* pTerrainNode, const AABB& rAABB, float fDistance);
#ifdef LY_TERRAIN_RUNTIME
    static SCheckOcclusionJobData CreateTerrainSystemJobData(IRenderNode* pTerrainNode, const AABB& rAABB, float fDistance);
#endif

    JobTypeT type; // type to indicate with which data the union is filled
    union
    {
        // data for octree nodes
        struct
        {
            COctreeNode*    pOctTreeNode;
            int                     nRenderMask;
        } octTreeData;

        // data for terrain nodes
        struct
        {
            CTerrainNode*   pTerrainNode;
            float                   vAABBMin[3];
            float                   vAABBMax[3];
            float                   fDistance;
        } terrainData;

#ifdef LY_TERRAIN_RUNTIME
        // data for terrain nodes
        struct
        {
            IRenderNode*  pTerrainNode;
            float                   vAABBMin[3];
            float                   vAABBMax[3];
            float                   fDistance;
        } terrainSystemData;
#endif
    };
    // common data
    SRendItemSorter rendItemSorter; // ensure order octree traversal oder even with parallel execution
    const CCamera*  pCam;                       // store camera to handle vis areas correctly
} _ALIGN(16);

///////////////////////////////////////////////////////////////////////////////
inline SCheckOcclusionJobData SCheckOcclusionJobData::CreateQuitJobData()
{
    SCheckOcclusionJobData jobData;
    jobData.type = QUIT;
    return jobData;
}

///////////////////////////////////////////////////////////////////////////////
inline SCheckOcclusionJobData SCheckOcclusionJobData::CreateOctreeJobData(COctreeNode* pOctTreeNode, int nRenderMask, const SRendItemSorter& rendItemSorter, const CCamera* pCam)
{
    SCheckOcclusionJobData jobData;
    jobData.type                    = OCTREE_NODE;
    jobData.octTreeData.pOctTreeNode    = pOctTreeNode;
    jobData.octTreeData.nRenderMask     = nRenderMask;
    jobData.rendItemSorter = rendItemSorter;
    jobData.pCam = pCam;
    return jobData;
}

///////////////////////////////////////////////////////////////////////////////
inline SCheckOcclusionJobData SCheckOcclusionJobData::CreateTerrainJobData(CTerrainNode* pTerrainNode, const AABB& rAABB, float fDistance)
{
    SCheckOcclusionJobData jobData;
    jobData.type                    = TERRAIN_NODE;
    jobData.terrainData.pTerrainNode    = pTerrainNode;
    jobData.terrainData.vAABBMin[0] = rAABB.min.x;
    jobData.terrainData.vAABBMin[1] = rAABB.min.y;
    jobData.terrainData.vAABBMin[2] = rAABB.min.z;
    jobData.terrainData.vAABBMax[0] = rAABB.max.x;
    jobData.terrainData.vAABBMax[1] = rAABB.max.y;
    jobData.terrainData.vAABBMax[2] = rAABB.max.z;
    jobData.terrainData.fDistance = fDistance;
    jobData.rendItemSorter = SRendItemSorter::CreateDefaultRendItemSorter();
    return jobData;
}

///////////////////////////////////////////////////////////////////////////////
#ifdef LY_TERRAIN_RUNTIME
inline SCheckOcclusionJobData SCheckOcclusionJobData::CreateTerrainSystemJobData(IRenderNode* pTerrainNode, const AABB& rAABB, float fDistance)
{
    SCheckOcclusionJobData jobData;
    jobData.type = TERRAIN_SYSTEM_NODE;
    jobData.terrainSystemData.pTerrainNode = pTerrainNode;
    jobData.terrainSystemData.vAABBMin[0] = rAABB.min.x;
    jobData.terrainSystemData.vAABBMin[1] = rAABB.min.y;
    jobData.terrainSystemData.vAABBMin[2] = rAABB.min.z;
    jobData.terrainSystemData.vAABBMax[0] = rAABB.max.x;
    jobData.terrainSystemData.vAABBMax[1] = rAABB.max.y;
    jobData.terrainSystemData.vAABBMax[2] = rAABB.max.z;
    jobData.terrainSystemData.fDistance = fDistance;
    jobData.rendItemSorter = SRendItemSorter::CreateDefaultRendItemSorter();
    return jobData;
}
#endif
///////////////////////////////////////////////////////////////////////////////
// data written by occlusion culler jobs, to control main thread 3dengine side rendering
_MS_ALIGN(16) struct SCheckOcclusionOutput
{
    enum JobTypeT
    {
        VEGETATION, 
        ROAD_DECALS, 
        COMMON, 
        TERRAIN, 
        DEFORMABLE_BRUSH, 
        BRUSH, 
#ifdef LY_TERRAIN_RUNTIME
        TERRAIN_SYSTEM
#endif
    };

    static SCheckOcclusionOutput CreateVegetationOutput(IRenderNode* pObj, const AABB& rObjBox, float fEntDistance, SSectorTextureSet* pTerrainTexInfo, bool bCheckPerObjectOcclusion, const SRendItemSorter& rendItemSorter);
    static SCheckOcclusionOutput CreateDecalsAndRoadsOutput(IRenderNode* pObj, const AABB& rObjBox, float fEntDistance, bool bCheckPerObjectOcclusion, const SRendItemSorter& rendItemSorter);
    static SCheckOcclusionOutput CreateCommonObjectOutput(IRenderNode* pObj, const AABB& rObjBox, float fEntDistance, SSectorTextureSet* pTerrainTexInfo, const SRendItemSorter& rendItemSorter);
    static SCheckOcclusionOutput CreateTerrainOutput(CTerrainNode* pTerrainNode, const SRendItemSorter& rendItemSorter);
    static SCheckOcclusionOutput CreateDeformableBrushOutput(CBrush* pBrush, CRenderObject* pObj, int nLod, const SRendItemSorter& rendItemSorter);
    static SCheckOcclusionOutput CreateBrushOutput(CBrush* pBrush, CRenderObject* pObj, const CLodValue& lodValue, const SRendItemSorter& rendItemSorter);
#ifdef LY_TERRAIN_RUNTIME
    static SCheckOcclusionOutput CreateTerrainSystemOutput(IRenderNode* pTerrainNode, CRenderObject* pObj, const SRendItemSorter& rendItemSorter);
#endif

    JobTypeT            type;
    union
    {
        //VEGETATION,ROAD_DECALS,COMMON Data
        struct
        {
            IRenderNode*                pObj;
            SSectorTextureSet*  pTerrainTexInfo;
            float                               fEntDistance;
            bool                                bCheckPerObjectOcclusion;
        } common;

        //TERRAIN Data
        struct
        {
            CTerrainNode*       pTerrainNode;
        } terrain;

#ifdef LY_TERRAIN_RUNTIME
        //TERRAIN_SYSTEM Data
        struct
        {
            IRenderNode*    pTerrainNode;
            CRenderObject*  pRenderObject;
        } terrain_system;
#endif

        //DEFORMABLE_BRUSH Data
        struct
        {
            CBrush*                 pBrush;
            CRenderObject*  pRenderObject;
            int                         nLod;
        } deformable_brush;

        //BRUSH Data
        struct
        {
            CBrush*                 pBrush;
            CRenderObject*  pRenderObject;
            int16                       nLodA;
            int16                       nLodB;
            uint8                       nDissolveRef;
        } brush;
    };

    AABB                                objBox;
    SRendItemSorter rendItemSorter;
} _ALIGN(16);

///////////////////////////////////////////////////////////////////////////////
inline SCheckOcclusionOutput SCheckOcclusionOutput::CreateVegetationOutput(IRenderNode* pObj, const AABB& rObjBox, float fEntDistance, SSectorTextureSet* pTerrainTexInfo, bool bCheckPerObjectOcclusion, const SRendItemSorter& rendItemSorter)
{
    SCheckOcclusionOutput outputData;
    outputData.type                                         = VEGETATION;
    outputData.rendItemSorter                       = rendItemSorter;
    outputData.objBox                                       = rObjBox;

    outputData.common.pObj                                          = pObj;
    outputData.common.fEntDistance                          = fEntDistance;
    outputData.common.pTerrainTexInfo                       = pTerrainTexInfo;
    outputData.common.bCheckPerObjectOcclusion  = bCheckPerObjectOcclusion;

    return outputData;
}

///////////////////////////////////////////////////////////////////////////////
inline SCheckOcclusionOutput SCheckOcclusionOutput::CreateDecalsAndRoadsOutput(IRenderNode* pObj, const AABB& rObjBox, float fEntDistance, bool bCheckPerObjectOcclusion, const SRendItemSorter& rendItemSorter)
{
    SCheckOcclusionOutput outputData;
    outputData.type                                         = ROAD_DECALS;
    outputData.rendItemSorter                       = rendItemSorter;
    outputData.objBox                                       = rObjBox;

    outputData.common.pObj                                      = pObj;
    outputData.common.pTerrainTexInfo                   = NULL;
    outputData.common.fEntDistance                      = fEntDistance;
    outputData.common.bCheckPerObjectOcclusion = bCheckPerObjectOcclusion;

    return outputData;
}

///////////////////////////////////////////////////////////////////////////////
inline SCheckOcclusionOutput SCheckOcclusionOutput::CreateCommonObjectOutput(IRenderNode* pObj, const AABB& rObjBox, float fEntDistance, SSectorTextureSet* pTerrainTexInfo, const SRendItemSorter& rendItemSorter)
{
    SCheckOcclusionOutput outputData;
    outputData.type                                         = COMMON;
    outputData.rendItemSorter                       = rendItemSorter;
    outputData.objBox                                       = rObjBox;

    outputData.common.pObj                                      = pObj;
    outputData.common.fEntDistance                      = fEntDistance;
    outputData.common.pTerrainTexInfo                   = pTerrainTexInfo;

    return outputData;
}

///////////////////////////////////////////////////////////////////////////////
inline SCheckOcclusionOutput SCheckOcclusionOutput::CreateTerrainOutput(CTerrainNode* pTerrainNode, const SRendItemSorter& rendItemSorter)
{
    SCheckOcclusionOutput outputData;
    outputData.type                                         = TERRAIN;
    outputData.rendItemSorter                       = rendItemSorter;

    outputData.terrain.pTerrainNode         = pTerrainNode;

    return outputData;
}

///////////////////////////////////////////////////////////////////////////////
#ifdef LY_TERRAIN_RUNTIME
inline SCheckOcclusionOutput SCheckOcclusionOutput::CreateTerrainSystemOutput(IRenderNode* pTerrainNode, CRenderObject* pObj, const SRendItemSorter& rendItemSorter)
{
    SCheckOcclusionOutput outputData;
    outputData.type = TERRAIN_SYSTEM;
    outputData.rendItemSorter = rendItemSorter;

    outputData.terrain_system.pTerrainNode = pTerrainNode;

    return outputData;
}
#endif

///////////////////////////////////////////////////////////////////////////////
inline SCheckOcclusionOutput SCheckOcclusionOutput::CreateDeformableBrushOutput(CBrush* pBrush, CRenderObject* pRenderObject, int nLod, const SRendItemSorter& rendItemSorter)
{
    SCheckOcclusionOutput outputData;
    outputData.type                                         = DEFORMABLE_BRUSH;
    outputData.rendItemSorter                       = rendItemSorter;

    outputData.deformable_brush.pBrush                  = pBrush;
    outputData.deformable_brush.nLod                        = nLod;
    outputData.deformable_brush.pRenderObject       = pRenderObject;

    return outputData;
}

///////////////////////////////////////////////////////////////////////////////
inline SCheckOcclusionOutput SCheckOcclusionOutput::CreateBrushOutput(CBrush* pBrush, CRenderObject* pObj, const CLodValue& lodValue, const SRendItemSorter& rendItemSorter)
{
    SCheckOcclusionOutput outputData;
    outputData.type                             = BRUSH;
    outputData.rendItemSorter                   = rendItemSorter;

    outputData.brush.pBrush                     = pBrush;
    outputData.brush.pRenderObject              = pObj;
    outputData.brush.nLodA                      = aznumeric_caster(lodValue.LodA());
    outputData.brush.nLodB                      = aznumeric_caster(lodValue.LodB());
    outputData.brush.nDissolveRef               = lodValue.DissolveRefA();

    return outputData;
}

///////////////////////////////////////////////////////////////////////////////
enum EOcTeeNodeListType
{
    eMain,
    eCasters,
    eSprites_deprecated,
    eLights,
};

template <class T>
struct TDoublyLinkedList
{
    T* m_pFirstNode, * m_pLastNode;

    TDoublyLinkedList()
    {
        m_pFirstNode = m_pLastNode = NULL;
    }

    ~TDoublyLinkedList()
    {
        assert(!m_pFirstNode && !m_pLastNode);
    }

    void insertAfter(T* pAfter, T* pObj)
    {
        pObj->m_pPrev = pAfter;
        pObj->m_pNext = pAfter->m_pNext;
        if (pAfter->m_pNext == NULL)
        {
            m_pLastNode = pObj;
        }
        else
        {
            pAfter->m_pNext->m_pPrev = pObj;
        }
        pAfter->m_pNext = pObj;
    }

    void insertBefore(T* pBefore, T* pObj)
    {
        pObj->m_pPrev = pBefore->m_pPrev;
        pObj->m_pNext = pBefore;
        if (pBefore->m_pPrev == NULL)
        {
            m_pFirstNode = pObj;
        }
        else
        {
            pBefore->m_pPrev->m_pNext = pObj;
        }
        pBefore->m_pPrev    = pObj;
    }

    void insertBeginning(T* pObj)
    {
        if (m_pFirstNode == NULL)
        {
            m_pFirstNode = pObj;
            m_pLastNode  = pObj;
            pObj->m_pPrev = NULL;
            pObj->m_pNext = NULL;
        }
        else
        {
            insertBefore(m_pFirstNode, pObj);
        }
    }

    void insertEnd(T* pObj)
    {
        if (m_pLastNode == NULL)
        {
            insertBeginning(pObj);
        }
        else
        {
            insertAfter(m_pLastNode, pObj);
        }
    }

    void remove(T* pObj)
    {
        if (pObj->m_pPrev == NULL)
        {
            m_pFirstNode = pObj->m_pNext;
        }
        else
        {
            pObj->m_pPrev->m_pNext = pObj->m_pNext;
        }

        if (pObj->m_pNext == NULL)
        {
            m_pLastNode = pObj->m_pPrev;
        }
        else
        {
            pObj->m_pNext->m_pPrev = pObj->m_pPrev;
        }

        pObj->m_pPrev = pObj->m_pNext = NULL;
    }

    bool empty() const { return m_pFirstNode == NULL; }
};

struct SInstancingInfo
{
    SInstancingInfo() { pStatInstGroup = 0; aabb.Reset(); fMinSpriteDistance = 10000.f; bInstancingInUse = 0; }
    StatInstGroup* pStatInstGroup;
    DynArray<CVegetation*> arrInstances;
    stl::aligned_vector<CRenderObject::SInstanceData, 16> arrMats;
    AABB aabb;
    float fMinSpriteDistance;
    bool bInstancingInUse;
};

struct SLayerVisibility
{
    const uint8* pLayerVisibilityMask;
    const uint16* pLayerIdTranslation;
};

struct SOctreeLoadObjectsData
{
    COctreeNode* pNode;
    ptrdiff_t offset;
    size_t size;
    _smart_ptr<IMemoryBlock> pMemBlock;
    byte* pObjPtr;
    byte* pEndObjPtr;
};

class COctreeNode
    : public IOctreeNode
    , Cry3DEngineBase
{
public:

    struct ShadowMapFrustumParams
    {
        CDLight* pLight;
        struct ShadowMapFrustum* pFr;
        PodArray<SPlaneObject>* pShadowHull;
        const SRenderingPassInfo* passInfo;
        Vec3 vCamPos;
        uint32 nRenderNodeFlags;
        bool bSun;
    };

    ~COctreeNode();

    void ResetStaticInstancing();
    void CheckUpdateStaticInstancing();

    bool HasChildNodes();
    int CountChildNodes();
    void InsertObject(IRenderNode* pObj, const AABB& objBox, const float fObjRadiusSqr, const Vec3& vObjCenter);
    bool DeleteObject(IRenderNode* pObj);
    void Render_Object_Nodes(bool bNodeCompletelyInFrustum, int nRenderMask, const SRenderingPassInfo& passInfo, SRendItemSorter& rendItemSorter);

    static void Shutdown();
    void RenderContent(int nRenderMask, const SRenderingPassInfo& passInfo, const SRendItemSorter& rendItemSorter, const CCamera* pCam);
    void RenderContentJobEntry(int nRenderMask, const SRenderingPassInfo& passInfo, SRendItemSorter rendItemSorter, const CCamera* pCam);
    void RenderVegetations(TDoublyLinkedList<IRenderNode>* lstObjects, const CCamera& rCam, int nRenderMask, bool bNodeCompletelyInFrustum, SSectorTextureSet* pTerrainTexInfo, const SRenderingPassInfo& passInfo, SRendItemSorter& rendItemSorter);
    void RenderCommonObjects(TDoublyLinkedList<IRenderNode>* lstObjects, const CCamera& rCam, int nRenderMask, bool bNodeCompletelyInFrustum, SSectorTextureSet* pTerrainTexInfo, const SRenderingPassInfo& passInfo, SRendItemSorter& rendItemSorter);
    void RenderDecalsAndRoads(TDoublyLinkedList<IRenderNode>* lstObjects, const CCamera& rCam, int nRenderMask, bool bNodeCompletelyInFrustum, SSectorTextureSet* pTerrainTexInfo, const SRenderingPassInfo& passInfo, SRendItemSorter& rendItemSorter);
    void RenderBrushes(TDoublyLinkedList<IRenderNode>* lstObjects, const CCamera& rCam, bool bNodeCompletelyInFrustum, SSectorTextureSet* pTerrainTexInfo, const SRenderingPassInfo& passInfo, SRendItemSorter& rendItemSorter);

    void FillShadowCastersList(bool bNodeCompletellyInFrustum, CDLight* pLight, struct ShadowMapFrustum* pFr, PodArray<SPlaneObject>* pShadowHull, uint32 nRenderNodeFlags, const SRenderingPassInfo& passInfo);
    void FillShadowMapCastersList(const ShadowMapFrustumParams& params, bool bNodeCompletellyInFrustum);
    void FillDepthCubemapRenderList(const AABB& cubemapAABB, const SRenderingPassInfo& passInfo, PodArray<struct IShadowCaster*>* objectsList);
    void ActivateObjectsLayer(uint16 nLayerId, bool bActivate, bool bPhys, IGeneralMemoryHeap* pHeap);
    void GetLayerMemoryUsage(uint16 nLayerId, ICrySizer* pSizer, int* pNumBrushes, int* pNumDecals);

    void MarkAsUncompiled(const IRenderNode* pRenderNode = NULL);
    COctreeNode* FindNodeContainingBox(const AABB& objBox);
    void MoveObjectsIntoList(PodArray<SRNInfo>* plstResultEntities, const AABB* pAreaBox, bool bRemoveObjects = false, bool bSkipDecals = false, bool bSkip_ERF_NO_DECALNODE_DECALS = false, bool bSkipDynamicObjects = false, EERType eRNType = eERType_TypesNum);

# if ENGINE_ENABLE_COMPILATION
    int GetData(byte*& pData, int& nDataSize, std::vector<IStatObj*>* pStatObjTable, std::vector<_smart_ptr<IMaterial>>* pMatTable, std::vector<IStatInstGroup*>* pStatInstGroupTable, EEndian eEndian, SHotUpdateInfo* pExportInfo);
# endif

    const AABB& GetObjectsBBox() { return m_objectsBox; }
    AABB GetShadowCastersBox(const AABB* pBBox, const Matrix34* pShadowSpaceTransform);
    void DeleteObjectsByFlag(int nRndFlag);
    void UnregisterEngineObjectsInArea(const SHotUpdateInfo* pExportInfo, PodArray<IRenderNode*>& arrUnregisteredObjects, bool bOnlyEngineObjects);
    uint32 GetLastVisFrameId() { return m_nLastVisFrameId; }
    void GetObjectsByType(PodArray<IRenderNode*>& lstObjects, EERType objType, const AABB* pBBox, ObjectTreeQueryFilterCallback filterCallback = nullptr);
    void GetObjectsByFlags(uint dwFlags, PodArray<IRenderNode*>& lstObjects);

    void GetNearestCubeProbe(float& fMinDistance, int& nMaxPriority, CLightEntity*& pNearestLight, const AABB* pBBox);
    void GetObjects(PodArray<IRenderNode*>& lstObjects, const AABB* pBBox);
    bool GetShadowCastersTimeSliced(IRenderNode* pIgnoreNode, ShadowMapFrustum* pFrustum, int renderNodeExcludeFlags, int& totalRemainingNodes, int nCurLevel, const SRenderingPassInfo& passInfo);
    bool IsObjectTypeInTheBox(EERType objType, const AABB& WSBBox);
    bool CleanUpTree();
    int GetObjectsCount(EOcTeeNodeListType eListType);
    int    SaveObjects(class CMemoryBlock* pMemBlock, std::vector<IStatObj*>* pStatObjTable, std::vector<_smart_ptr<IMaterial>>* pMatTable, std::vector<IStatInstGroup*>* pStatInstGroupTable, EEndian eEndian, const SHotUpdateInfo * pExportInfo);
    int    LoadObjects(byte* pPtr, byte* pEndPtr, std::vector<IStatObj*>* pStatObjTable, std::vector<_smart_ptr<IMaterial>>* pMatTable, EEndian eEndian, int nChunkVersion, const SLayerVisibility* pLayerVisibility);
    static int  GetSingleObjectSize (IRenderNode* pObj, const SHotUpdateInfo* pExportInfo);
    static void SaveSingleObject (byte*& pPtr, int& nDatanSize, IRenderNode* pObj, std::vector<IStatObj*>* pStatObjTable, std::vector<_smart_ptr<IMaterial>>* pMatTable, std::vector<IStatInstGroup*>* pStatInstGroupTable, EEndian eEndian, const SHotUpdateInfo* pExportInfo);
    static void LoadSingleObject(byte*& pPtr, std::vector<IStatObj*>* pStatObjTable, std::vector<_smart_ptr<IMaterial>>* pMatTable, EEndian eEndian, int nChunkVersion, const SLayerVisibility* pLayerVisibility, int nSID);
    bool IsRightNode(const AABB& objBox, const float fObjRadius, float fObjMaxViewDist);
    void GetMemoryUsage(ICrySizer* pSizer) const;

#ifdef LY_TERRAIN_LEGACY_RUNTIME
    void UpdateTerrainNodes(CTerrainNode* pParentNode = 0);
#endif

#ifdef SUPPORT_TERRAIN_AO_PRE_COMPUTATIONS
    bool RayObjectsIntersection2D(Vec3 vStart, Vec3 vEnd, Vec3& vClosestHitPoint, float& fClosestHitDistance, EERType eERType);
#endif

    template <class T>
    int Load_T(T& f, int& nDataSize, std::vector<IStatObj*>* pStatObjTable, std::vector<_smart_ptr<IMaterial>>* pMatTable, EEndian eEndian, AABB* pBox, const SLayerVisibility* pLayerVisibility);
    int Load(AZ::IO::HandleType& fileHandle, int& nDataSize, std::vector<IStatObj*>* pStatObjTable, std::vector<_smart_ptr<IMaterial>>* pMatTable, EEndian eEndian, AABB* pBox, const SLayerVisibility* pLayerVisibility);
    int Load(uint8*& f, int& nDataSize, std::vector<IStatObj*>* pStatObjTable, std::vector<_smart_ptr<IMaterial>>* pMatTable, EEndian eEndian, AABB* pBox, const SLayerVisibility* pLayerVisibility);

    void GenerateStatObjAndMatTables(std::vector<IStatObj*>* pStatObjTable, std::vector<_smart_ptr<IMaterial>>* pMatTable, std::vector<IStatInstGroup*>* pStatInstGroupTable, SHotUpdateInfo* pExportInfo);
    static void ReleaseEmptyNodes();
    static void StaticReset();
    bool IsEmpty();
    bool HasObjects();

    bool UpdateStreamingPriority(PodArray<COctreeNode*>& arrRecursion, float fMinDist, float fMaxDist, bool bFullUpdate, const SObjManPrecacheCamera* pPrecacheCams, size_t nPrecacheCams, const SRenderingPassInfo& passInfo);
    AABB GetNodeBox() const
    {
        return AABB(
            m_vNodeCenter - m_vNodeAxisRadius,
            m_vNodeCenter + m_vNodeAxisRadius);
    }

    void OffsetObjects(const Vec3& offset);
    void SetVisArea(CVisArea* pVisArea);
    void UpdateVisAreaSID(CVisArea* pVisArea, int nSID)
    {
        assert(pVisArea);
        m_nSID = nSID;
    }

    static COctreeNode* Create(int nSID, const AABB& box, struct CVisArea* pVisArea, COctreeNode* pParent = NULL);

protected:
    AABB GetChildBBox(int nChildId);
    void CompileObjects();
    void UpdateStaticInstancing();
    void UpdateObjects(IRenderNode* pObj);
    void CompileCharacter(ICharacterInstance* pChar, unsigned char& nInternalFlags);
    void CompileObjectsBrightness();
    float GetNodeObjectsMaxViewDistance();

    // Check if min spec specified in render node passes current server config spec.
    static bool CheckRenderFlagsMinSpec(uint32 dwRndFlags);

    void LinkObject(IRenderNode* pObj, EERType eERType, bool bPushFront = true);
    void UnlinkObject(IRenderNode* pObj);

    static int Cmp_OctreeNodeSize(const void* v1, const void* v2);

private:
    COctreeNode(int nSID, const AABB& box, struct CVisArea* pVisArea, COctreeNode* pParent);

    float GetNodeRadius2() const { return m_vNodeAxisRadius.Dot(m_vNodeAxisRadius); }
    COctreeNode* FindChildFor(IRenderNode* pObj, const AABB& objBox, const float fObjRadius, const Vec3& vObjCenter);
    bool HasAnyRenderableCandidates(const SRenderingPassInfo& passInfo) const;

    uint32 m_nOccludedFrameId;
    uint32 m_renderFlags;
    uint32 m_errTypesBitField;
    AABB m_objectsBox;
    float m_fObjectsMaxViewDist;
    uint32 m_nLastVisFrameId;

    COctreeNode* m_arrChilds[8];
    TDoublyLinkedList<IRenderNode> m_arrObjects[eRNListType_ListsNum];
    PodArray<SCasterInfo> m_lstCasters;
    Vec3 m_vNodeCenter;
    Vec3 m_vNodeAxisRadius;
    COctreeNode* m_pParent;
    uint32 nFillShadowCastersSkipFrameId;
    float m_fNodeDistance;
    int m_nManageVegetationsFrameId;
    int m_nSID;
    struct CRNTmpData* m_pRNTmpData;

    uint32 m_bHasLights : 1;
    uint32 m_bHasRoads : 1;
    uint32 m_bNodeCompletelyInFrustum : 1;
    uint32 m_fpSunDirX : 7;
    uint32 m_fpSunDirZ : 7;
    uint32 m_fpSunDirYs : 1;
    uint32 m_bStaticInstancingIsDirty : 1;

    struct SNodeInstancingInfo
    {
        SNodeInstancingInfo() { pRNode = 0; nodeMatrix.IsIdentity(); }
        Matrix34 nodeMatrix;
        class CVegetation * pRNode;
    };
    std::map<std::pair<IStatObj*, _smart_ptr<IMaterial>>, PodArray<SNodeInstancingInfo>*> * m_pStaticInstancingInfo;

    static void* m_pRenderContentJobQueue;
public:
    static PodArray<COctreeNode*> m_arrEmptyNodes;
};


#endif // CRYINCLUDE_CRY3DENGINE_OBJECTSTREE_H
