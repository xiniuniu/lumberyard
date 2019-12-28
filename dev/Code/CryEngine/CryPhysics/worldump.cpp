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

#include "StdAfx.h"

#ifdef PHYSWORLD_SERIALIZATION
#include "vector"
#include "map"

#include "bvtree.h"
#include "geometry.h"
#include "overlapchecks.h"
#include "raybv.h"
#include "raygeom.h"
#include "aabbtree.h"
#include "obbtree.h"
#include "singleboxtree.h"
#include "trimesh.h"
#include "boxgeom.h"
#include "cylindergeom.h"
#include "capsulegeom.h"
#include "spheregeom.h"
#include "heightfieldbv.h"
#include "heightfieldgeom.h"
#include "geoman.h"
#include "singleboxtree.h"
#include "cylindergeom.h"
#include "spheregeom.h"
#include "rigidbody.h"
#include "physicalplaceholder.h"
#include "physicalentity.h"
#include "rigidentity.h"
#include "particleentity.h"
#include "livingentity.h"
#include "wheeledvehicleentity.h"
#include "articulatedentity.h"
#include "ropeentity.h"
#include "softentity.h"
#include "tetrlattice.h"
#include "physicalworld.h"



enum fieldTypes
{
    ft_int = 0, ft_uint, ft_short, ft_ushort, ft_uint64, ft_float, ft_vector, ft_quaternion, ft_entityptr, ft_matrix33, ft_proc
};
static char* g_strFormats[2][10] = {
    { "%i", "%X", "%hd", "%hu", "%I64X", "%.8g", "(%.8g %.8g %.8g)", "(%f (%f %f %f))", "%d", "(%f %f %f)(%f %f %f)(%f %f %f)" },
    { "%i", "%X", "%hd", "%hu", "%I64X", "%g", "(%g %g %g)", "(%f (%f %f %f))", "%d", "(%f %f %f)(%f %f %f)(%f %f %f)" }
};
static char g_strTabs[17] = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";

struct parse_item;
struct parse_context;
struct Serializer;
typedef std::map<string, parse_item*> parse_map;
typedef std::vector<parse_item*> parse_list;
typedef int (Serializer::* ParseProc)(parse_context&, char*, size_t);

unsigned short* g_hfData;
unsigned char* g_hfFlags;
int g_hfMats[256], g_nHfMats;
Vec2i g_hfStride;
float g_getHeightCallback(int ix, int iy) { return g_hfData[g_hfStride * Vec2i(ix, iy)] * (1.0f / 256) - 128.0f; }
unsigned char g_getSurfTypeCallback(int ix, int iy) { return g_hfFlags[g_hfStride * Vec2i(ix, iy)]; };

union parse_data
{
    void* ptr;
    ParseProc func;
};

struct parse_item
{
    char* name;
    int itype;
    parse_data data;
    int idx;
};

struct parse_context
{
    FILE* f;
    CPhysicalWorld* pWorld;
    Serializer* pSerializer, * pSerializerStack[16];
    void* pobj, * pObjStack[16];
    int iStackPos;
    int bSaving;
    CTetrLattice** pLattices;
    int nLattices, nLatticesAlloc;
    CTriMesh** pGeoms;
    int nGeoms, nGeomsAlloc;

    parse_context()
    {
        pLattices = 0;
        nLattices = nLatticesAlloc = 0;
        pGeoms = 0;
        nGeoms = nGeomsAlloc = 0;
    }
    ~parse_context()
    {
        int i;
        for (i = 0; i < nLattices; i++)
        {
            pLattices[i]->m_flags &= 0xFF;
        }
        if (pLattices)
        {
            delete[] pLattices;
        }
        for (i = 0; i < nGeoms; i++)
        {
            pGeoms[i]->m_nSubtracts = 0;
        }
        if (pGeoms)
        {
            delete[] pGeoms;
        }
    }

    void PushState()
    {
        if (iStackPos < sizeof(pSerializerStack) / sizeof(pSerializerStack[0]))
        {
            pSerializerStack[++iStackPos] = pSerializer;
            pObjStack[iStackPos] = pobj;
        }
    }
    void PopState()
    {
        if (iStackPos >= 0)
        {
            pSerializer = pSerializerStack[iStackPos];
            pobj = pObjStack[iStackPos--];
        }
    }
};


#define DECLARE_MEMBER(strname, type, member) {                          \
        parse_item* pitem;                                               \
        parse_map::iterator itor = map.find(strname);                    \
        if (itor != map.end()) {                                         \
            pitem = itor->second; list.erase(list.begin() + pitem->idx); \
        } else{ pitem = new parse_item; }                                \
        pitem->itype = type; pitem->name = strname;                      \
        pitem->data.ptr = (void*)((char*)&trg.member - (char*)&trg);     \
        parse_list::iterator litor = list.insert(list.end(), pitem);     \
        pitem->idx = litor - list.begin(); map[strname] = pitem;         \
}
#define DECLARE_PROC(strname, proc) {                                    \
        parse_item* pitem;                                               \
        parse_map::iterator itor = map.find(strname);                    \
        if (itor != map.end()) {                                         \
            pitem = itor->second; list.erase(list.begin() + pitem->idx); \
        } else{ pitem = new parse_item; }                                \
        pitem->itype = ft_proc; pitem->name = strname;                   \
        pitem->data.func = (ParseProc)proc;                              \
        parse_list::iterator litor = list.insert(list.end(), pitem);     \
        pitem->idx = litor - list.begin(); map[strname] = pitem;         \
}
#define DEFINE_MEMBER_PROC(type, proc, member)                          \
    int proc(parse_context & ctx, char* str, size_t strSize) {          \
        if (ctx.bSaving) {                                              \
            sprintf_s(str, strSize, "%d", ((type*)ctx.pobj)->member); } \
        else {                                                          \
            int i; sscanf_s(str, "%d", &i);                               \
            ((type*)ctx.pobj)->member = i;                              \
        } return 0;                                                     \
    }

struct Serializer
{
    parse_map map;
    parse_list list;

    ~Serializer()
    {
        for (unsigned int i = 0; i < list.size(); i++)
        {
            delete list[i];
        }
    }

    virtual void Serialize(parse_context& ctx)
    {
        char str[1024], * ptr0, * ptr1;
        if (ctx.bSaving)
        {
            CPhysicalPlaceholder* ppc;
            for (unsigned int i = 0; i < list.size(); i++)
            {
                if (!strcmp(list[i]->name, "end"))
                {
                    continue;
                }
                ptr0 = str + sprintf_s(str, "%.*s%s ", ctx.iStackPos + 1, g_strTabs, list[i]->name);
                void* pdata = (char*)ctx.pobj + (int)list[i]->data.ptr;
                switch (list[i]->itype)
                {
                case ft_int:
                    sprintf_s(ptr0, sizeof(str) - (ptr0 - str), g_strFormats[0][list[i]->itype], *(int*)pdata);
                    break;
                case ft_uint:
                    sprintf_s(ptr0, sizeof(str) - (ptr0 - str), g_strFormats[0][list[i]->itype], *(unsigned int*)pdata);
                    break;
                case ft_short:
                    sprintf_s(ptr0, sizeof(str) - (ptr0 - str), g_strFormats[0][list[i]->itype], *(short*)pdata);
                    break;
                case ft_ushort:
                    sprintf_s(ptr0, sizeof(str) - (ptr0 - str), g_strFormats[0][list[i]->itype], *(unsigned short*)pdata);
                    break;
                case ft_uint64:
                    sprintf_s(ptr0, sizeof(str) - (ptr0 - str), g_strFormats[0][list[i]->itype], *(long long*)pdata);
                    break;
                case ft_float:
                    if (is_unused(*(float*)pdata))
                    {
                        continue;
                    }
                    sprintf_s(ptr0, sizeof(str) - (ptr0 - str), g_strFormats[0][list[i]->itype], *(float*)pdata);
                    break;
                case ft_vector:
                {
                    Vec3* v = (Vec3*)pdata;
                    if (is_unused(*v))
                    {
                        continue;
                    }
                    sprintf_s(ptr0, sizeof(str) - (ptr0 - str), g_strFormats[0][list[i]->itype], v->x, v->y, v->z);
                } break;
                case ft_quaternion:
                {
                    quaternionf* q = (quaternionf*)pdata;
                    if (is_unused(*q))
                    {
                        continue;
                    }
                    sprintf_s(ptr0, sizeof(str) - (ptr0 - str), g_strFormats[0][list[i]->itype], q->w, q->v.x, q->v.y, q->v.z);
                }   break;
                case ft_matrix33:
                {
                    Matrix33& R = *(Matrix33*)pdata;
                    sprintf_s(ptr0, sizeof(str) - (ptr0 - str), g_strFormats[0][list[i]->itype], R.m00, R.m01, R.m02, R.m10, R.m11, R.m12, R.m20, R.m21, R.m22);
                }   break;
                case ft_entityptr:
                    ppc = *(CPhysicalPlaceholder**)((char*)ctx.pobj + (int)list[i]->data.ptr);
                    ppc ? ltoa(ppc->m_id, ptr0, 10) : strcpy(ptr0, "NULL");
                    break;
                case ft_proc:
                    if ((this->*list[i]->data.func)(ctx, ptr0, sizeof(str) - (ptr0 - str)))
                    {
                        continue;
                    }
                }
                strcat(ptr0, "\n");
                fputs(str, ctx.f);
            }
            ctx.PopState();
            fprintf(ctx.f, "%.*send\n", ctx.iStackPos + 1, g_strTabs);
        }
        else
        {
            parse_map::const_iterator itor;
            parse_item* pitem;

            do
            {
                if (!fgets(str, sizeof(str), ctx.f))
                {
                    break;
                }
                for (ptr0 = str; * ptr0 && isspace(*ptr0); ptr0++)
                {
                    ;
                }
                for (ptr1 = ptr0; * ptr1 && !isspace(*ptr1); ptr1++)
                {
                    ;
                }
                if (*ptr1)
                {
                    for (; * ptr1 && isspace(*ptr1); ptr1++)
                    {
                        * ptr1 = 0;
                    }
                }
                itor = ctx.pSerializer->map.find(ptr0);
                if (itor != ctx.pSerializer->map.end())
                {
                    pitem = itor->second;
                    void* pdata = (char*)ctx.pobj + (int)pitem->data.ptr;
                    switch (pitem->itype)
                    {
                    case ft_int:
                    case ft_uint:
                    case ft_short:
                    case ft_ushort:
                    case ft_uint64:
                    case ft_float:
                        sscanf_s(ptr1, g_strFormats[1][pitem->itype], pdata);
                        break;
                    case ft_vector:
                    {
                        Vec3* v = (Vec3*)pdata;
                        sscanf_s(ptr1, g_strFormats[1][pitem->itype], &v->x, &v->y, &v->z);
                    } break;
                    case ft_quaternion:
                    {
                        quaternionf* q = (quaternionf*)pdata;
                        sscanf_s(ptr1, g_strFormats[1][pitem->itype], &q->w, &q->v.x, &q->v.y, &q->v.z);
                    }   break;
                    case ft_matrix33:
                    {
                        Matrix33& R = *(Matrix33*)pdata;
                        sscanf_s(ptr1, g_strFormats[1][pitem->itype], &R.m00, &R.m01, &R.m02, &R.m10, &R.m11, &R.m12, &R.m20, &R.m21, &R.m22);
                    } break;
                    case ft_entityptr:
                        *(IPhysicalEntity**)pdata = *ptr1 != 'N' ? ctx.pWorld->GetPhysicalEntityById(atol(ptr1)) : 0;
                        break;
                    case ft_proc:
                        (this->*pitem->data.func)(ctx, ptr1, sizeof(str) - (ptr1 - str));
                    }
                }
                if (!strcmp(ptr0, "end"))
                {
                    ctx.PopState();
                    break;
                }
            }   while (true);
        }
    }
};


struct CMeshSerializer
    : Serializer
{
    CMeshSerializer()
    {
        CTriMesh trg;
        DECLARE_MEMBER("numVertices", ft_int, m_nVertices)
        DECLARE_MEMBER("numTris", ft_int, m_nTris)
        DECLARE_MEMBER("flags", ft_uint, m_flags)
        DECLARE_PROC("vtx", &CMeshSerializer::SerializeVtx)
        DECLARE_PROC("tri", &CMeshSerializer::SerializeTri)
        DECLARE_PROC("minTrisPerNode", &CMeshSerializer::SerializeMinTrisPerNode)
        DECLARE_PROC("maxTrisPerNode", &CMeshSerializer::SerializeMaxTrisPerNode)
        DECLARE_PROC("end", &CMeshSerializer::Finalize)
    }

    int SerializeVtx(parse_context& ctx, char* str, size_t)
    {
        CTriMesh* pMesh = (CTriMesh*)ctx.pobj;
        int i;
        if (ctx.bSaving)
        {
            for (i = 0; i < pMesh->m_nVertices; i++)
            {
                fprintf(ctx.f, "%.*svtx %d (%.8g %.8g %.8g)\n", ctx.iStackPos + 1, g_strTabs, i, pMesh->m_pVertices[i].x,
                    pMesh->m_pVertices[i].y, pMesh->m_pVertices[i].z);
            }
        }
        else
        {
            Vec3 v;
            sscanf_s(str, "%d (%g %g %g", &i, &v.x, &v.y, &v.z);
            if (!pMesh->m_pVertices.data)
            {
                pMesh->m_pVertices.data = new Vec3[pMesh->m_nVertices];
            }
            pMesh->m_pVertices[i] = v;
        }
        return ctx.bSaving;
    }

    int SerializeTri(parse_context& ctx, char* str, size_t)
    {
        CTriMesh* pMesh = (CTriMesh*)ctx.pobj;
        int i;
        if (ctx.bSaving)
        {
            for (i = 0; i < pMesh->m_nTris; i++)
            {
                fprintf(ctx.f, "%.*stri %d (%d %d %d) %s\n", ctx.iStackPos + 1, g_strTabs, i, pMesh->m_pIndices[i * 3],
                    pMesh->m_pIndices[i * 3 + 1], pMesh->m_pIndices[i * 3 + 2],
                    pMesh->m_pIds ? ltoa(pMesh->m_pIds[i], str, 10) : "");
            }
        }
        else
        {
            int idx[3], idmat;
            if (sscanf_s(str, "%d (%d %d %d) %d", &i, idx, idx + 1, idx + 2, &idmat) > 4 && !pMesh->m_pIds)
            {
                pMesh->m_pIds = new char[pMesh->m_nTris];
            }
            if (!pMesh->m_pIndices)
            {
                pMesh->m_pIndices = new index_t[pMesh->m_nTris * 3];
            }
            pMesh->m_pIndices[i * 3] = idx[0];
            pMesh->m_pIndices[i * 3 + 1] = idx[1];
            pMesh->m_pIndices[i * 3 + 2] = idx[2];
        }
        return ctx.bSaving;
    }

    int SerializeMinTrisPerNode(parse_context& ctx, char* str, size_t strSize)
    {
        CTriMesh* pMesh = (CTriMesh*)ctx.pobj;
        if (ctx.bSaving)
        {
            int n = 0;
            if (pMesh->m_flags & mesh_AABB)
            {
                n = ((CAABBTree*)pMesh->m_pTree)->m_nMinTrisPerNode;
            }
            else if (pMesh->m_flags & mesh_OBB)
            {
                n = ((COBBTree*)pMesh->m_pTree)->m_nMinTrisPerNode;
            }
            sprintf_s(str, strSize, "%d", n);
        }
        else
        {
            pMesh->m_bConvex[0] = atol(str);
        }
        return 0;
    }

    int SerializeMaxTrisPerNode(parse_context& ctx, char* str, size_t strSize)
    {
        CTriMesh* pMesh = (CTriMesh*)ctx.pobj;
        if (ctx.bSaving)
        {
            int n = 0;
            if (pMesh->m_flags & mesh_AABB)
            {
                n = ((CAABBTree*)pMesh->m_pTree)->m_nMaxTrisPerNode;
            }
            else if (pMesh->m_flags & mesh_OBB)
            {
                n = ((COBBTree*)pMesh->m_pTree)->m_nMaxTrisPerNode;
            }
            sprintf_s(str, strSize, "%d", n);
        }
        else
        {
            pMesh->m_bConvex[1] = atol(str);
        }
        return 0;
    }

    int Finalize(parse_context& ctx, char* str, size_t)
    {
        if (!ctx.bSaving)
        {
            CTriMesh* pMesh = (CTriMesh*)ctx.pobj;
            pMesh->CreateTriMesh(pMesh->m_pVertices, strided_pointer<unsigned short>((unsigned short*)pMesh->m_pIndices, sizeof(pMesh->m_pIndices[0])), pMesh->m_pIds,
                0, pMesh->m_nTris, pMesh->m_flags | mesh_shared_vtx | mesh_shared_idx, pMesh->m_bConvex[0], pMesh->m_bConvex[1]);
            pMesh->m_flags &= ~(mesh_shared_vtx | mesh_shared_idx);
        }
        return 0;
    }
};


struct CBoxSerializer
    : Serializer
{
    CBoxSerializer()
    {
        CBoxGeom trg;
        DECLARE_MEMBER("Oriented", ft_int, GetBox().bOriented)
        DECLARE_MEMBER("BasisX", ft_vector, GetBox().Basis.m00)
        DECLARE_MEMBER("BasisY", ft_vector, GetBox().Basis.m10)
        DECLARE_MEMBER("BasisZ", ft_vector, GetBox().Basis.m20)
        DECLARE_MEMBER("Center", ft_vector, GetBox().center)
        DECLARE_MEMBER("Size", ft_vector, GetBox().size)
        DECLARE_PROC("end", &CBoxSerializer::Finalize)
    }

    int Finalize(parse_context& ctx, char* str, size_t)
    {
        CBoxGeom* pbox = (CBoxGeom*)ctx.pobj;
        if (!ctx.bSaving)
        {
            box bx = pbox->GetBox();
            pbox->CreateBox(&bx);
        }
        return 0;
    }
};


struct CCylinderSerializer
    : Serializer
{
    CCylinderSerializer()
    {
        CCylinderGeom trg;
        DECLARE_MEMBER("Center", ft_vector, m_cyl.center)
        DECLARE_MEMBER("Axis", ft_vector, m_cyl.axis)
        DECLARE_MEMBER("Radius", ft_float, m_cyl.r)
        DECLARE_MEMBER("HalfHeight", ft_float, m_cyl.hh)
        DECLARE_PROC("end", &CCylinderSerializer::Finalize)
        bCapsule = 0;
    }
    int bCapsule;

    int Finalize(parse_context& ctx, char* str, size_t)
    {
        if (!ctx.bSaving)
        {
            if (!bCapsule)
            {
                CCylinderGeom* pcyl = (CCylinderGeom*)ctx.pobj;
                pcyl->CreateCylinder(&pcyl->m_cyl);
            }
            else
            {
                CCapsuleGeom* pcyl = (CCapsuleGeom*)ctx.pobj;
                pcyl->CreateCapsule((capsule*)&pcyl->m_cyl);
            }
        }
        return 0;
    }
};


struct CSphereSerializer
    : Serializer
{
    CSphereSerializer()
    {
        CSphereGeom trg;
        DECLARE_MEMBER("Center", ft_vector, m_sphere.center)
        DECLARE_MEMBER("Radius", ft_float, m_sphere.r)
        DECLARE_PROC("end", &CSphereSerializer::Finalize)
    }

    int Finalize(parse_context& ctx, char* str, size_t)
    {
        CSphereGeom* psph = (CSphereGeom*)ctx.pobj;
        if (!ctx.bSaving)
        {
            psph->CreateSphere(&psph->m_sphere);
        }
        return 0;
    }
};


inline char hex2chr(int h) { return h + '0' + (9 - h >> 31 & 'A' - '0' - 10); }
inline int chr2hex(char c) { return c - '0' - (9 + '0' - c >> 31 & 'A' - '0' - 10); }

struct CHeightfieldSerializer
    : Serializer
{
    CHeightfieldSerializer()
    {
        CHeightfield trg;
        DECLARE_MEMBER("BasisX", ft_vector, m_hf.Basis.m00)
        DECLARE_MEMBER("BasisY", ft_vector, m_hf.Basis.m10)
        DECLARE_MEMBER("BasisZ", ft_vector, m_hf.Basis.m20)
        DECLARE_MEMBER("Oriented", ft_int, m_hf.bOriented)
        DECLARE_MEMBER("Origin", ft_vector, m_hf.origin)
        DECLARE_MEMBER("StepX", ft_float, m_hf.step.x)
        DECLARE_MEMBER("StepY", ft_float, m_hf.step.y)
        DECLARE_MEMBER("SizeX", ft_int, m_hf.size.x)
        DECLARE_MEMBER("SizeY", ft_int, m_hf.size.y)
        DECLARE_MEMBER("StrideX", ft_int, m_hf.stride.x)
        DECLARE_MEMBER("StrideY", ft_int, m_hf.stride.y)
        DECLARE_MEMBER("HeightScale", ft_float, m_hf.heightscale)
        DECLARE_MEMBER("TypeMask", ft_ushort, m_hf.typemask)
        DECLARE_MEMBER("TypeHole", ft_int, m_hf.typehole)
        DECLARE_PROC("MatMapping", &CHeightfieldSerializer::SerializeMatMapping)
        DECLARE_PROC("Data", &CHeightfieldSerializer::SerializeData)
    }

    int SerializeMatMapping(parse_context& ctx, char* str, size_t strSize)
    {
        int i, nMats;
        if (ctx.bSaving)
        {
            geom* pgeom = ctx.pWorld->m_pHeightfield[0]->m_parts;
            nMats = pgeom->pMatMapping ? pgeom->nMats : 0;
            char* ptr = str;
            ptr += sprintf_s(str, strSize, "%d ", nMats);
            for (i = 0; i < nMats; i++)
            {
                ptr += sprintf_s(ptr, strSize - (ptr - str), "%d ", pgeom->pMatMapping[i]);
            }
        }
        else
        {
            sscanf_s(str, "%d", &g_nHfMats);
            if (g_nHfMats)
            {
                for (i = 0; i < g_nHfMats; i++)
                {
                    for (; *str && !isdigit(*str); str++)
                    {
                        ;
                    }
                    for (; *str && !isspace(*str); str++)
                    {
                        ;
                    }
                    sscanf_s(str, "%d", g_hfMats + i);
                }
            }
        } return 0;
    }

    int SerializeData(parse_context& ctx, char* str, size_t)
    {
        CHeightfield* phf = (CHeightfield*)ctx.pobj;
        int i, j, idx;
        unsigned short val;
        char buf[65536], * ptr;

        if (ctx.bSaving)
        {
            fprintf(ctx.f, "%.*sData\n", ctx.iStackPos + 1, g_strTabs);
            for (j = 0; j <= phf->m_hf.size.y; j++)
            {
                for (i = 0, ptr = buf; i < ctx.iStackPos + 2; i++)
                {
                    *ptr++ = '\t';
                }
                for (i = 0; i <= phf->m_hf.size.x; i++, *ptr++ = ' ')
                {
                    val = FtoI(min(255.9f, phf->m_hf.fpGetHeightCallback(i, j) + 128.0f) * 256.0f);
                    for (idx = 3; idx >= 0; idx--)
                    {
                        *ptr++ = hex2chr(val >> idx * 4 & 15);
                    }
                    *ptr++ = '.';
                    val = phf->m_hf.fpGetSurfTypeCallback(i, j);
                    *ptr++ = hex2chr(val & 15);
                    *ptr++ = hex2chr(val >> 4 & 15);
                }
                strcpy(ptr, "\n");
                fputs(buf, ctx.f);
            }
        }
        else
        {
            g_hfData = new unsigned short[(phf->m_hf.size.x + 1) * (phf->m_hf.size.y + 1)];
            g_hfFlags = new unsigned char[(phf->m_hf.size.x + 1) * (phf->m_hf.size.y + 1)];
            g_hfStride = phf->m_hf.stride;
            for (j = 0; j <= phf->m_hf.size.y; j++)
            {
                fgets(buf, sizeof(buf), ctx.f);
                for (i = 0, ptr = buf; i <= phf->m_hf.size.x; i++)
                {
                    for (; *ptr && isspace(*ptr); ptr++)
                    {
                        ;
                    }
                    if (!strncmp(ptr, "end", 3))
                    {
                        fseek(ctx.f, -(int)strlen(buf) - 1, SEEK_CUR);
                        goto hfend;
                    }
                    for (idx = 3, val = 0; idx >= 0; idx--)
                    {
                        val += chr2hex(*ptr++) << idx * 4;
                    }
                    g_hfData[i * phf->m_hf.stride.x + j * phf->m_hf.stride.y] = val;
                    for (idx = 1, val = 0, ptr++; idx >= 0; idx--)
                    {
                        val += chr2hex(*ptr++) << idx * 4;
                    }
                    g_hfFlags[i * phf->m_hf.stride.x + j * phf->m_hf.stride.y] = val;
                }
            }
hfend:
            phf->m_hf.fpGetHeightCallback = g_getHeightCallback;
            phf->m_hf.fpGetSurfTypeCallback = g_getSurfTypeCallback;
            phf->CreateHeightfield(&phf->m_hf);
        }
        return 1;
    }
};


struct CTetrahedronSerializer
    : Serializer
{
    CTetrahedronSerializer()
    {
        STetrahedron trg;
        DECLARE_MEMBER("flags", ft_uint, flags)
        DECLARE_MEMBER("M", ft_float, M)
        DECLARE_MEMBER("Vinv", ft_float, Vinv)
        DECLARE_PROC("Iinv", &CTetrahedronSerializer::SerializeIinv)
        DECLARE_PROC("vtx", &CTetrahedronSerializer::SerializeVtx)
        DECLARE_PROC("buddies", &CTetrahedronSerializer::SerializeBuddies)
        DECLARE_PROC("fracFace", &CTetrahedronSerializer::SerializeFaces)
    }

    int SerializeIinv(parse_context& ctx, char* str, size_t)
    {
        STetrahedron* ptet = (STetrahedron*)ctx.pobj;
        if (ctx.bSaving)
        {
            fprintf(ctx.f, "%.*sIinv %f %f %f %f %f %f %f %f %f\n", ctx.iStackPos + 1, g_strTabs, ptet->Iinv(0, 0), ptet->Iinv(0, 1), ptet->Iinv(0, 2),
                ptet->Iinv(1, 0), ptet->Iinv(1, 1), ptet->Iinv(1, 2), ptet->Iinv(2, 0), ptet->Iinv(2, 1), ptet->Iinv(2, 2));
        }
        else
        {
            sscanf_s(str, "%f %f %f %f %f %f %f %f %f", &ptet->Iinv(0, 0), &ptet->Iinv(0, 1), &ptet->Iinv(0, 2),
                &ptet->Iinv(1, 0), &ptet->Iinv(1, 1), &ptet->Iinv(1, 2), &ptet->Iinv(2, 0), &ptet->Iinv(2, 1), &ptet->Iinv(2, 2));
        }
        return ctx.bSaving;
    }

    int SerializeVtx(parse_context& ctx, char* str, size_t)
    {
        STetrahedron* ptet = (STetrahedron*)ctx.pobj;
        if (ctx.bSaving)
        {
            fprintf(ctx.f, "%.*svtx %d %d %d %d\n", ctx.iStackPos + 1, g_strTabs, ptet->ivtx[0], ptet->ivtx[1], ptet->ivtx[2], ptet->ivtx[3]);
        }
        else
        {
            sscanf_s(str, "%d %d %d %d", &ptet->ivtx[0], &ptet->ivtx[1], &ptet->ivtx[2], &ptet->ivtx[3]);
        }
        return ctx.bSaving;
    }

    int SerializeBuddies(parse_context& ctx, char* str, size_t)
    {
        STetrahedron* ptet = (STetrahedron*)ctx.pobj;
        if (ctx.bSaving)
        {
            fprintf(ctx.f, "%.*sbuddies %d %d %d %d\n", ctx.iStackPos + 1, g_strTabs, ptet->ibuddy[0], ptet->ibuddy[1], ptet->ibuddy[2], ptet->ibuddy[3]);
        }
        else
        {
            sscanf_s(str, "%d %d %d %d", &ptet->ibuddy[0], &ptet->ibuddy[1], &ptet->ibuddy[2], &ptet->ibuddy[3]);
        }
        return ctx.bSaving;
    }

    int SerializeFaces(parse_context& ctx, char* str, size_t)
    {
        STetrahedron* ptet = (STetrahedron*)ctx.pobj;
        if (ctx.bSaving)
        {
            fprintf(ctx.f, "%.*sfracFace %f %f %f %f\n", ctx.iStackPos + 1, g_strTabs, ptet->fracFace[0], ptet->fracFace[1], ptet->fracFace[2], ptet->fracFace[2]);
        }
        else
        {
            sscanf_s(str, "%f %f %f %f", &ptet->fracFace[0], &ptet->fracFace[1], &ptet->fracFace[2], &ptet->fracFace[3]);
        }
        return ctx.bSaving;
    }
};


struct CTetrLatticeSerializer
    : Serializer
{
    CTetrahedronSerializer* pTetrSerializer;

    CTetrLatticeSerializer(CTetrahedronSerializer* trs)
        : pTetrSerializer(trs)
    {
        CTetrLattice trg(0);
        DECLARE_MEMBER("numVtx", ft_int, m_nVtx)
        DECLARE_PROC("vtx", &CTetrLatticeSerializer::SerializeVtx)  // both vtx and flags
        DECLARE_MEMBER("numTetr", ft_int, m_nTetr)
        DECLARE_PROC("Tetr", &CTetrLatticeSerializer::SerializeTets)
        DECLARE_MEMBER("maxCracks", ft_int, m_nMaxCracks)
        DECLARE_MEMBER("idmat", ft_int, m_idmat)
        DECLARE_MEMBER("maxForcePush", ft_float, m_maxForcePush)
        DECLARE_MEMBER("maxForcePull", ft_float, m_maxForcePull)
        DECLARE_MEMBER("maxForceShift", ft_float, m_maxForceShift)
        DECLARE_MEMBER("maxTorqueTwist", ft_float, m_maxTorqueTwist)
        DECLARE_MEMBER("maxTorqueBend", ft_float, m_maxTorqueBend)
        DECLARE_MEMBER("crackWeaken", ft_float, m_crackWeaken)
        DECLARE_MEMBER("density", ft_float, m_density)
        DECLARE_MEMBER("numRemovedTets", ft_int, m_nRemovedTets)
    }

    int SerializeVtx(parse_context& ctx, char* str, size_t)
    {
        CTetrLattice* pLattice = (CTetrLattice*)ctx.pobj;
        int i;
        if (ctx.bSaving)
        {
            for (i = 0; i < pLattice->m_nVtx; i++)
            {
                fprintf(ctx.f, "%.*svtx %d (%.8g %.8g %.8g) %X\n", ctx.iStackPos + 1, g_strTabs, i, pLattice->m_pVtx[i].x, pLattice->m_pVtx[i].y,
                    pLattice->m_pVtx[i].z, pLattice->m_pVtxFlags[i]);
            }
        }
        else
        {
            Vec3 v;
            int flags;
            if (!pLattice->m_pVtx)
            {
                pLattice->m_pVtx = new Vec3[pLattice->m_nVtx];
            }
            if (!pLattice->m_pVtxFlags)
            {
                pLattice->m_pVtxFlags = new int[pLattice->m_nVtx];
            }
            sscanf_s(str, "%d (%g %g %g) %X", &i, &v.x, &v.y, &v.z, &flags);
            pLattice->m_pVtx[i] = v;
            pLattice->m_pVtxFlags[i] = flags;
        }
        return ctx.bSaving;
    }

    int SerializeTets(parse_context& ctx, char* str, size_t)
    {
        CTetrLattice* pLattice = (CTetrLattice*)ctx.pobj;
        int i;
        if (ctx.bSaving)
        {
            for (i = 0; i < pLattice->m_nTetr; i++)
            {
                fprintf(ctx.f, "%.*sTetr %d\n", ctx.iStackPos + 1, g_strTabs, i);
                ctx.PushState();
                ctx.pobj = pLattice->m_pTetr + i;
                (ctx.pSerializer = pTetrSerializer)->Serialize(ctx);
            }
        }
        else
        {
            if (!pLattice->m_pTetr)
            {
                pLattice->m_pTetr = new STetrahedron[pLattice->m_nTetr];
            }
            i = atol(str);
            ctx.PushState();
            ctx.pobj = pLattice->m_pTetr + i;
            (ctx.pSerializer = pTetrSerializer)->Serialize(ctx);
            pLattice->m_pTetr[i].Minv = 1 / pLattice->m_pTetr[i].M;
            pLattice->m_pTetr[i].idx = -1;
        }
        return ctx.bSaving;
    }
};


struct CPhysGeometrySerializer
    : Serializer
{
    CMeshSerializer* pMeshSerializer;
    CHeightfieldSerializer* pHeightfieldSerializer;
    CBoxSerializer* pBoxSerializer;
    CCylinderSerializer* pCylinderSerializer;
    CSphereSerializer* pSphereSerializer;

    CPhysGeometrySerializer(CMeshSerializer* ms, CHeightfieldSerializer* hfs, CBoxSerializer* bs, CCylinderSerializer* cs, CSphereSerializer* ss)
        : pMeshSerializer(ms)
        , pHeightfieldSerializer(hfs)
        , pBoxSerializer(bs)
        , pCylinderSerializer(cs)
        , pSphereSerializer(ss)
    {
        phys_geometry trg;
        DECLARE_PROC("Geometry", &CPhysGeometrySerializer::SerializeGeometry)
        DECLARE_MEMBER("volume", ft_float, V)
        DECLARE_MEMBER("Ibody", ft_vector, Ibody)
        DECLARE_MEMBER("origin", ft_vector, origin)
        DECLARE_MEMBER("rotation", ft_quaternion, q)
        DECLARE_MEMBER("matid", ft_int, surface_idx)
    }

    int SerializeGeometryPtr(parse_context& ctx, char* str, size_t, IGeometry*& pGeom)
    {
        Serializer* pSerializer;
        if (ctx.bSaving)
        {
            char* name;
            if (!pGeom)
            {
                return 1;
            }
            switch (pGeom->GetType())
            {
            case GEOM_TRIMESH:
                pSerializer = pMeshSerializer;
                name = "Mesh";
                break;
            case GEOM_HEIGHTFIELD:
                pSerializer = pHeightfieldSerializer;
                name = "Heightfield";
                break;
            case GEOM_BOX:
                pSerializer = pBoxSerializer;
                name = "Box";
                break;
            case GEOM_CYLINDER:
                pSerializer = pCylinderSerializer;
                pCylinderSerializer->bCapsule = 0;
                name = "Cylinder";
                break;
            case GEOM_CAPSULE:
                pSerializer = pCylinderSerializer;
                pCylinderSerializer->bCapsule = 1;
                name = "Capsule";
                break;
            case GEOM_SPHERE:
                pSerializer = pSphereSerializer;
                name = "Sphere";
                break;
            default:
                return 1;
            }
            fprintf(ctx.f, "%.*sGeometry (type %d : %s)\n", ctx.iStackPos + 1, g_strTabs, pGeom->GetType(), name);
            ctx.PushState();
            ctx.pSerializer = pSerializer;
            ctx.pobj = pGeom;
            ctx.pSerializer->Serialize(ctx);
        }
        else
        {
            for (; *str && !isdigit(*str); str++)
            {
                ;
            }
            switch (atol(str))
            {
            case GEOM_TRIMESH:
                pGeom = new CTriMesh;
                pSerializer = pMeshSerializer;
                ((CTriMesh*)pGeom)->m_bConvex[0] = 2;
                ((CTriMesh*)pGeom)->m_bConvex[1] = 4;
                break;
            case GEOM_HEIGHTFIELD:
                pGeom = new CHeightfield;
                pSerializer = pHeightfieldSerializer;
                break;
            case GEOM_BOX:
                pGeom = new CBoxGeom;
                pSerializer = pBoxSerializer;
                break;
            case GEOM_CYLINDER:
                pGeom = new CCylinderGeom;
                pSerializer = pCylinderSerializer;
                pCylinderSerializer->bCapsule = 0;
                break;
            case GEOM_CAPSULE:
                pGeom = new CCapsuleGeom;
                pSerializer = pCylinderSerializer;
                pCylinderSerializer->bCapsule = 1;
                break;
            case GEOM_SPHERE:
                pGeom = new CSphereGeom;
                pSerializer = pSphereSerializer;
                break;
            default:
                return 0;
            }
            ctx.PushState();
            ctx.pSerializer = pSerializer;
            ctx.pobj = pGeom;
            ctx.pSerializer->Serialize(ctx);
        }
        return ctx.bSaving;
    }
    int SerializeGeometry(parse_context& ctx, char* str, size_t strSize) { return SerializeGeometryPtr(ctx, str, strSize, ((phys_geometry*)ctx.pobj)->pGeom); }
};


struct CExplosionShapeSerializer
    : Serializer
{
    CMeshSerializer* pMeshSerializer;

    CExplosionShapeSerializer(CMeshSerializer* ms)
        : pMeshSerializer(ms)
    {
        SExplosionShape trg;
        DECLARE_MEMBER("id", ft_int, id)
        DECLARE_MEMBER("Size", ft_float, size)
        DECLARE_MEMBER("Idmat", ft_int, idmat)
        DECLARE_MEMBER("Probability", ft_float, probability)
        DECLARE_PROC("Geometry", &CExplosionShapeSerializer::SerializeGeometry)
    }

    int SerializeGeometry(parse_context& ctx, char* str, size_t)
    {
        SExplosionShape* pShape = (SExplosionShape*)ctx.pobj;
        int i;
        if (ctx.bSaving)
        {
            int bFullSave = 0;
            if (((CTriMesh*)pShape->pGeom)->m_nSubtracts == 0)
            {
                ((CTriMesh*)pShape->pGeom)->m_nSubtracts = ctx.nGeoms + 1;
                if (ctx.nGeoms == ctx.nGeomsAlloc)
                {
                    ReallocateList(ctx.pGeoms, ctx.nGeoms, ctx.nGeomsAlloc += 32);
                }
                ctx.pGeoms[ctx.nGeoms++] = (CTriMesh*)pShape->pGeom;
                bFullSave = 1;
            }
            if (bFullSave)
            {
                fprintf(ctx.f, "%.*sGeometry %d\n", ctx.iStackPos + 1, g_strTabs, ((CTriMesh*)pShape->pGeom)->m_nSubtracts);
                ctx.PushState();
                ctx.pobj = pShape->pGeom;
                (ctx.pSerializer = pMeshSerializer)->Serialize(ctx);
                return 1;
            }
            else
            {
                ltoa(((CTriMesh*)pShape->pGeom)->m_nSubtracts, str, 10);
            }
        }
        else
        {
            i = atol(str) - 1;
            if (i == ctx.nGeoms)
            {
                if (ctx.nGeoms == ctx.nGeomsAlloc)
                {
                    ReallocateList(ctx.pGeoms, ctx.nGeoms, ctx.nGeomsAlloc += 32);
                }
                ctx.PushState();
                ctx.nGeoms++;
                ctx.pobj = ctx.pGeoms[i] = new CTriMesh();
                (ctx.pSerializer = pMeshSerializer)->Serialize(ctx);
            }
            pShape->pGeom = ctx.pGeoms[i];
        }
        return 0;
    }
};


struct CCrackSerializer
    : Serializer
{
    CMeshSerializer* pMeshSerializer;

    CCrackSerializer(CMeshSerializer* ms)
        : pMeshSerializer(ms)
    {
        SCrackGeom trg;
        DECLARE_MEMBER("id", ft_int, id)
        DECLARE_MEMBER("idmat", ft_int, idmat)
        DECLARE_MEMBER("pt0", ft_vector, pt0)
        DECLARE_PROC("Rc", &CCrackSerializer::SerializeRc)
        DECLARE_MEMBER("rmaxEdge", ft_float, rmaxedge)
        DECLARE_MEMBER("ry3", ft_float, ry3)
        DECLARE_MEMBER("pt3x", ft_float, pt3.x)
        DECLARE_MEMBER("pt3y", ft_float, pt3.y)
        DECLARE_PROC("Geometry", &CCrackSerializer::SerializeGeometry)
    }

    int SerializeRc(parse_context& ctx, char* str, size_t)
    {
        SCrackGeom* pcrk = (SCrackGeom*)ctx.pobj;
        if (ctx.bSaving)
        {
            fprintf(ctx.f, "%.*sRc %f %f %f %f %f %f %f %f %f\n", ctx.iStackPos + 1, g_strTabs, pcrk->Rc(0, 0), pcrk->Rc(0, 1), pcrk->Rc(0, 2),
                pcrk->Rc(1, 0), pcrk->Rc(1, 1), pcrk->Rc(1, 2), pcrk->Rc(2, 0), pcrk->Rc(2, 1), pcrk->Rc(2, 2));
        }
        else
        {
            sscanf_s(str, "%f %f %f %f %f %f %f %f %f", &pcrk->Rc(0, 0), &pcrk->Rc(0, 1), &pcrk->Rc(0, 2),
                &pcrk->Rc(1, 0), &pcrk->Rc(1, 1), &pcrk->Rc(1, 2), &pcrk->Rc(2, 0), &pcrk->Rc(2, 1), &pcrk->Rc(2, 2));
        }
        return ctx.bSaving;
    }

    int SerializeGeometry(parse_context& ctx, char* str, size_t)
    {
        SCrackGeom* pcrk = (SCrackGeom*)ctx.pobj;
        int i;
        if (ctx.bSaving)
        {
            int bFullSave = 0;
            if (((CTriMesh*)pcrk->pGeom)->m_nSubtracts == 0)
            {
                ((CTriMesh*)pcrk->pGeom)->m_nSubtracts = ctx.nGeoms + 1;
                if (ctx.nGeoms == ctx.nGeomsAlloc)
                {
                    ReallocateList(ctx.pGeoms, ctx.nGeoms, ctx.nGeomsAlloc += 32);
                }
                ctx.pGeoms[ctx.nGeoms++] = pcrk->pGeom;
                bFullSave = 1;
            }
            if (bFullSave)
            {
                fprintf(ctx.f, "%.*sGeometry %d\n", ctx.iStackPos + 1, g_strTabs, ((CTriMesh*)pcrk->pGeom)->m_nSubtracts);
                ctx.PushState();
                ctx.pobj = pcrk->pGeom;
                (ctx.pSerializer = pMeshSerializer)->Serialize(ctx);
                return 1;
            }
            else
            {
                ltoa(((CTriMesh*)pcrk->pGeom)->m_nSubtracts, str, 10);
            }
        }
        else
        {
            i = atol(str) - 1;
            if (i == ctx.nGeoms)
            {
                if (ctx.nGeoms == ctx.nGeomsAlloc)
                {
                    ReallocateList(ctx.pGeoms, ctx.nGeoms, ctx.nGeomsAlloc += 32);
                }
                ctx.PushState();
                ctx.nGeoms++;
                ctx.pobj = ctx.pGeoms[i] = new CTriMesh();
                (ctx.pSerializer = pMeshSerializer)->Serialize(ctx);
            }
            pcrk->pGeom = ctx.pGeoms[i];
        }
        return 0;
    }
};


struct CGeomanSerializer
    : Serializer
{
    CPhysGeometrySerializer* pPhysGeometrySerializer;
    CExplosionShapeSerializer* pExplSerializer;
    CCrackSerializer* pCrackSerializer;

    CGeomanSerializer(CPhysGeometrySerializer* pgs, CExplosionShapeSerializer* ess, CCrackSerializer* cs)
        : pPhysGeometrySerializer(pgs)
        , pExplSerializer(ess)
        , pCrackSerializer(cs)
    {
        CPhysicalWorld trg(0);
        DECLARE_PROC("PhysGeometry", &CGeomanSerializer::SerializeGeometry)
        DECLARE_MEMBER("numExplosionShapes", ft_int, m_nExpl)
        DECLARE_PROC("ExplosionShape", &CGeomanSerializer::SerializeExplosionShapes)
        DECLARE_MEMBER("kCrackScale", ft_float, m_kCrackScale)
        DECLARE_MEMBER("kCrackSkew", ft_float, m_kCrackSkew)
        DECLARE_MEMBER("numCracks", ft_int, m_nCracks)
        DECLARE_PROC("CrackGeom", &CGeomanSerializer::SerializeCracks)
        DECLARE_PROC("end", &CGeomanSerializer::Finalize)
    }

    int SerializeGeometry(parse_context& ctx, char* str, size_t)
    {
        CPhysicalWorld* pgeoman = (CPhysicalWorld*)ctx.pobj;
        int i, j;

        if (ctx.bSaving)
        {
            for (i = 0; i < pgeoman->m_nGeomChunks; i++)
            {
                for (j = 0; j < GEOM_CHUNK_SZ; j++)
                {
                    if (pgeoman->m_pGeoms[i][j].pGeom)
                    {
                        fprintf(ctx.f, "%.*sPhysGeometry %d\n", ctx.iStackPos + 1, g_strTabs, i * GEOM_CHUNK_SZ + j);
                        ctx.PushState();
                        ctx.pobj = pgeoman->m_pGeoms[i] + j;
                        (ctx.pSerializer = pPhysGeometrySerializer)->Serialize(ctx);
                    }
                }
            }
        }
        else
        {
            ctx.PushState();
            i = atol(str);
            j = i & GEOM_CHUNK_SZ - 1;
            i /= GEOM_CHUNK_SZ;
            while (pgeoman->m_nGeomChunks < i + 1)
            {
                phys_geometry** t = pgeoman->m_pGeoms;
                pgeoman->m_pGeoms = new phys_geometry*[pgeoman->m_nGeomChunks + 1];
                if (pgeoman->m_nGeomChunks)
                {
                    memcpy(pgeoman->m_pGeoms, t, sizeof(phys_geometry*) * pgeoman->m_nGeomChunks);
                    delete[] t;
                }
                memset(pgeoman->m_pGeoms[pgeoman->m_nGeomChunks++] = new phys_geometry[GEOM_CHUNK_SZ], 0, sizeof(phys_geometry) * GEOM_CHUNK_SZ);
            }
            ctx.pobj = pgeoman->m_pGeoms[i] + j;
            (ctx.pSerializer = pPhysGeometrySerializer)->Serialize(ctx);
        }
        return ctx.bSaving;
    }

    int SerializeExplosionShapes(parse_context& ctx, char* str, size_t)
    {
        CPhysicalWorld* pWorld = (CPhysicalWorld*)ctx.pobj;
        int i;

        if (ctx.bSaving)
        {
            for (i = 0; i < pWorld->m_nExpl; i++)
            {
                fprintf(ctx.f, "%.*sExplosionShape %d\n", ctx.iStackPos + 1, g_strTabs, i);
                ctx.PushState();
                ctx.pobj = pWorld->m_pExpl + i;
                (ctx.pSerializer = pExplSerializer)->Serialize(ctx);
            }
        }
        else
        {
            i = atol(str);
            ctx.PushState();
            if (!pWorld->m_pExpl)
            {
                pWorld->m_pExpl = new SExplosionShape[pWorld->m_nExpl];
            }
            ctx.pobj = pWorld->m_pExpl + i;
            (ctx.pSerializer = pExplSerializer)->Serialize(ctx);
        }
        return ctx.bSaving;
    }

    int SerializeCracks(parse_context& ctx, char* str, size_t)
    {
        CPhysicalWorld* pWorld = (CPhysicalWorld*)ctx.pobj;
        int i;

        if (ctx.bSaving)
        {
            for (i = 0; i < pWorld->m_nCracks; i++)
            {
                fprintf(ctx.f, "%.*sCrackGeom %d\n", ctx.iStackPos + 1, g_strTabs, i);
                ctx.PushState();
                ctx.pobj = pWorld->m_pCracks + i;
                (ctx.pSerializer = pCrackSerializer)->Serialize(ctx);
            }
        }
        else
        {
            i = atol(str);
            ctx.PushState();
            if (!pWorld->m_pCracks)
            {
                pWorld->m_pCracks = new SCrackGeom[pWorld->m_nCracks];
                pWorld->m_idCrack = 0;
            }
            ctx.pobj = pWorld->m_pCracks + i;
            (ctx.pSerializer = pCrackSerializer)->Serialize(ctx);
            pWorld->m_idCrack = max(pWorld->m_idCrack, pWorld->m_pCracks[i].id + 1);
        }
        return ctx.bSaving;
    }

    int Finalize(parse_context& ctx, char* str, size_t)
    {
        CPhysicalWorld* pWorld = (CPhysicalWorld*)ctx.pobj;
        if (!ctx.bSaving)
        {
            int i, j;
            for (i = j = 0; i < pWorld->m_nExpl; i++)
            {
                pWorld->m_pExpl[i].rsize = 1 / pWorld->m_pExpl[i].size;
                if (i > 0 && pWorld->m_pExpl[i].idmat != pWorld->m_pExpl[i - 1].idmat)
                {
                    j = i;
                }
                pWorld->m_pExpl[i].iFirstByMat = j;
                pWorld->m_pExpl[i].nSameMat = i - j + 1;
            }
            for (i = pWorld->m_nExpl - 1; i >= 0; i--)
            {
                if (i == pWorld->m_nExpl - 1 || pWorld->m_pExpl[i].idmat != pWorld->m_pExpl[i + 1].idmat)
                {
                    j = pWorld->m_pExpl[i].nSameMat;
                }
                pWorld->m_pExpl[i].nSameMat = j;
            }
        }
        return 0;
    }
};


struct CPhysicalPlaceholderSerializer
    : Serializer
{
    CPhysicalPlaceholderSerializer()
    {
        CPhysicalPlaceholder trg;
        DECLARE_MEMBER("BBoxMin", ft_vector, m_BBox[0])
        DECLARE_MEMBER("BBoxMax", ft_vector, m_BBox[1])
        DECLARE_PROC("Id", &CPhysicalPlaceholderSerializer::SerializeId)
        DECLARE_PROC("SimClass", &CPhysicalPlaceholderSerializer::SerializeSimClass)
        DECLARE_PROC("end", &CPhysicalPlaceholderSerializer::Finalize)
    }
    DEFINE_MEMBER_PROC(CPhysicalPlaceholder, SerializeSimClass, m_iSimClass)

    int SerializeId(parse_context& ctx, char* str, size_t strSize)
    {
        if (ctx.bSaving)
        {
            sprintf_s(str, strSize, "%d", ((CPhysicalPlaceholder*)ctx.pobj)->m_id);
        }
        else
        {
            int id;
            sscanf_s(str, "%d", &id);
            ((CPhysicalPlaceholder*)ctx.pobj)->m_id = -1;
            ctx.pWorld->SetPhysicalEntityId((CPhysicalPlaceholder*)ctx.pobj, id);
        }
        return 0;
    }
    int Finalize(parse_context& ctx, char* str, size_t)
    {
        if (!ctx.bSaving)
        {
            CPhysicalPlaceholder* ppc = (CPhysicalPlaceholder*)ctx.pobj;
            AtomicAdd(&ctx.pWorld->m_lockGrid, -ctx.pWorld->RepositionEntity(ppc));
        }
        return 0;
    }
};


struct CPhysicalEntityPartSerializer
    : Serializer
{
    CTetrLatticeSerializer* pLatticeSerializer;

    CPhysicalEntityPartSerializer(CTetrLatticeSerializer* tls)
        : pLatticeSerializer(tls)
    {
        geom trg;
        DECLARE_PROC("PhysGeom", &CPhysicalEntityPartSerializer::SerializeGeom)
        DECLARE_PROC("PhysGeomProxy", &CPhysicalEntityPartSerializer::SerializeGeomProxy)
        DECLARE_MEMBER("id", ft_int, id)
        DECLARE_MEMBER("pos", ft_vector, pos)
        DECLARE_MEMBER("quat", ft_quaternion, q)
        DECLARE_MEMBER("scale", ft_float, scale)
        DECLARE_MEMBER("mass", ft_float, mass)
        DECLARE_PROC("surface_idx", &CPhysicalEntityPartSerializer::SerializeSurfaceIdx)
        DECLARE_MEMBER("flags", ft_uint, flags)
        DECLARE_MEMBER("flagsCollider", ft_uint, flagsCollider)
        DECLARE_MEMBER("maxdim", ft_float, maxdim)
        DECLARE_MEMBER("minContactDist", ft_float, minContactDist)
        DECLARE_MEMBER("BBoxMin", ft_vector, BBox[0])
        DECLARE_MEMBER("BBoxMax", ft_vector, BBox[1])
        DECLARE_PROC("idmatBreakable", &CPhysicalEntityPartSerializer::SerializeIdmatBreakable)
        DECLARE_PROC("MatMapping", &CPhysicalEntityPartSerializer::SerializeMatMapping)
        DECLARE_PROC("Lattice", &CPhysicalEntityPartSerializer::SerializeLattice)
    }
    DEFINE_MEMBER_PROC(geom, SerializeSurfaceIdx, surface_idx)
    DEFINE_MEMBER_PROC(geom, SerializeIdmatBreakable, idmatBreakable)

    int SerializeMatMapping(parse_context& ctx, char* str, size_t strSize)
    {
        geom* pgeom = (geom*)ctx.pobj;
        int i, nMats;
        if (ctx.bSaving)
        {
            nMats = pgeom->pMatMapping ? pgeom->nMats : 0;
            char* ptr = str;
            ptr += sprintf_s(str, strSize, "%d ", nMats);
            for (i = 0; i < nMats; i++)
            {
                ptr += sprintf_s(ptr, strSize - (ptr - str), "%d ", pgeom->pMatMapping[i]);
            }
        }
        else
        {
            sscanf_s(str, "%d", &nMats);
            if (nMats)
            {
                pgeom->pMatMapping = new int[pgeom->nMats = nMats];
                for (i = 0; i < nMats; i++)
                {
                    for (; *str && !isdigit(*str); str++)
                    {
                        ;
                    }
                    for (; *str && !isspace(*str); str++)
                    {
                        ;
                    }
                    sscanf_s(str, "%d", pgeom->pMatMapping + i);
                }
            }
            else
            {
                pgeom->pMatMapping = 0;
            }
        } return 0;
    }
    int SerializeGeom(parse_context& ctx, char* str, size_t strSize)
    {
        int res = SerializeGeomPtr(ctx, str, strSize, ((geom*)ctx.pobj)->pPhysGeom);
        ((geom*)ctx.pobj)->pPhysGeomProxy = ((geom*)ctx.pobj)->pPhysGeom;
        return res;
    }
    int SerializeGeomProxy(parse_context& ctx, char* str, size_t strSize)
    {
        if (ctx.bSaving && ((geom*)ctx.pobj)->pPhysGeomProxy == ((geom*)ctx.pobj)->pPhysGeom)
        {
            return 1;
        }
        return SerializeGeomPtr(ctx, str, strSize, ((geom*)ctx.pobj)->pPhysGeomProxy);
    }
    int SerializeGeomPtr(parse_context& ctx, char* str, size_t strSize, phys_geometry*& pPhysGeom)
    {
        int i, j;
        if (ctx.bSaving)
        {
            for (i = 0; i < ctx.pWorld->m_nGeomChunks && (unsigned int)(j = pPhysGeom - ctx.pWorld->m_pGeoms[i]) >= GEOM_CHUNK_SZ; i++)
            {
                ;
            }
            if (i == ctx.pWorld->m_nGeomChunks)
            {
                return 1;
            }
            ltoa(i * GEOM_CHUNK_SZ + j, str, 10);
        }
        else
        {
            i = atol(str);
            j = i & GEOM_CHUNK_SZ - 1;
            i /= GEOM_CHUNK_SZ;
            pPhysGeom = ctx.pWorld->m_pGeoms[i] + j;
            pPhysGeom->nRefCount++;
        }
        return 0;
    }
    int SerializeLattice(parse_context& ctx, char* str, size_t)
    {
        geom* ppart = (geom*)ctx.pobj;
        int i;
        if (ctx.bSaving)
        {
            if (!ppart->pLattice)
            {
                strcpy(str, "none");
            }
            else
            {
                int bFullLat = 0;
                if ((ppart->pLattice->m_flags >> 8) == 0)
                {
                    ppart->pLattice->m_flags |= ctx.nLattices + 1 << 8;
                    if (ctx.nLattices == ctx.nLatticesAlloc)
                    {
                        ReallocateList(ctx.pLattices, ctx.nLattices, ctx.nLatticesAlloc += 32);
                    }
                    ctx.pLattices[ctx.nLattices++] = ppart->pLattice;
                    bFullLat = 1;
                }
                if (bFullLat)
                {
                    fprintf(ctx.f, "%.*sLattice %d\n", ctx.iStackPos + 1, g_strTabs, ppart->pLattice->m_flags >> 8);
                    ctx.PushState();
                    ctx.pobj = ppart->pLattice;
                    (ctx.pSerializer = pLatticeSerializer)->Serialize(ctx);
                    return 1;
                }
                else
                {
                    ltoa(ppart->pLattice->m_flags >> 8, str, 10);
                }
            }
        }
        else
        {
            if (!strncmp(str, "none", 4))
            {
                ppart->pLattice = 0;
            }
            else
            {
                i = atol(str) - 1;
                if (i >= ctx.nLattices)
                {
                    if (ctx.nLattices == ctx.nLatticesAlloc)
                    {
                        ReallocateList(ctx.pLattices, ctx.nLattices, ctx.nLatticesAlloc += 32);
                    }
                    ctx.PushState();
                    ctx.pobj = ctx.pLattices[i] = new CTetrLattice(ctx.pWorld);
                    (ctx.pSerializer = pLatticeSerializer)->Serialize(ctx);
                    ctx.nLattices = i + 1;
                }
                ppart->pLattice = ctx.pLattices[i];
            }
        }
        return 0;
    }
};


struct CPhysicalJointSerializerWrite
    : Serializer
{
    CPhysicalJointSerializerWrite()
    {
        SStructuralJoint trg;
        DECLARE_MEMBER("ipart0", ft_int, ipart[0])
        DECLARE_MEMBER("ipart1", ft_int, ipart[1])
        DECLARE_MEMBER("pt", ft_vector, pt)
        DECLARE_MEMBER("n", ft_vector, n)
        DECLARE_MEMBER("maxForcePush", ft_float, maxForcePush)
        DECLARE_MEMBER("maxForcePull", ft_float, maxForcePull)
        DECLARE_MEMBER("maxForceShift", ft_float, maxForceShift)
        DECLARE_MEMBER("maxTorqueBend", ft_float, maxTorqueBend)
        DECLARE_MEMBER("maxTorqueTwist", ft_float, maxTorqueTwist)
        DECLARE_MEMBER("bBreakable", ft_int, bBreakable)
        DECLARE_MEMBER("szHelper", ft_float, size)
        DECLARE_MEMBER("axisx", ft_vector, axisx)
        DECLARE_MEMBER("limitConstr", ft_float, limitConstr)
        DECLARE_MEMBER("dampingConstr", ft_float, dampingConstr)
    }
};

struct CPhysicalJointSerializerRead
    : Serializer
{
    CPhysicalJointSerializerRead()
    {
        pe_params_structural_joint trg;
        DECLARE_MEMBER("ipart0", ft_int, partid[0])
        DECLARE_MEMBER("ipart1", ft_int, partid[1])
        DECLARE_MEMBER("pt", ft_vector, pt)
        DECLARE_MEMBER("n", ft_vector, n)
        DECLARE_MEMBER("maxForcePush", ft_float, maxForcePush)
        DECLARE_MEMBER("maxForcePull", ft_float, maxForcePull)
        DECLARE_MEMBER("maxForceShift", ft_float, maxForceShift)
        DECLARE_MEMBER("maxTorqueBend", ft_float, maxTorqueBend)
        DECLARE_MEMBER("maxTorqueTwist", ft_float, maxTorqueTwist)
        DECLARE_MEMBER("bBreakable", ft_int, bBreakable)
        DECLARE_MEMBER("szHelper", ft_float, szSensor)
        DECLARE_MEMBER("axisx", ft_vector, axisx)
        DECLARE_MEMBER("limitConstr", ft_float, limitConstraint)
        DECLARE_MEMBER("dampingConstr", ft_float, dampingConstraint)
    }
};


struct CPhysicalEntitySerializer
    : CPhysicalPlaceholderSerializer
{
    CPhysicalEntityPartSerializer* pPartSerializer;
    CPhysicalJointSerializerRead* pJointSerializerRead;
    CPhysicalJointSerializerWrite* pJointSerializerWrite;

    CPhysicalEntitySerializer(CPhysicalEntityPartSerializer* peps, CPhysicalJointSerializerRead* pjsr, CPhysicalJointSerializerWrite* pjsw)
        : pPartSerializer(peps)
        , pJointSerializerRead(pjsr)
        , pJointSerializerWrite(pjsw)
    {
        CPhysicalEntity trg(0);
        DECLARE_MEMBER("flags", ft_uint, m_flags)
        DECLARE_MEMBER("pos", ft_vector, m_pos)
        DECLARE_MEMBER("quat", ft_quaternion, m_qrot)
        DECLARE_MEMBER("timeIdle", ft_float, m_timeIdle)
        DECLARE_MEMBER("maxTimeIdle", ft_float, m_maxTimeIdle)
        DECLARE_MEMBER("timeIdle", ft_float, m_timeIdle)
        DECLARE_MEMBER("timeIdle", ft_float, m_timeIdle)
        DECLARE_MEMBER("numParts", ft_int, m_nParts)
        DECLARE_MEMBER("LastIdx", ft_int, m_iLastIdx)
        DECLARE_MEMBER("collisionClass", ft_uint, m_collisionClass.type)
        DECLARE_MEMBER("collisionClassIgnore", ft_uint, m_collisionClass.ignore)
        DECLARE_PROC("Part", &CPhysicalEntitySerializer::SerializePart)
        DECLARE_PROC("StructuralJoint", &CPhysicalEntitySerializer::SerializeJoint)
    }

    int SerializePart(parse_context& ctx, char* str, size_t)
    {
        CPhysicalEntity* pent = (CPhysicalEntity*)ctx.pobj;
        int i, j;
        if (ctx.bSaving)
        {
            for (i = 0; i < pent->m_nParts; i++)
            {
                fprintf(ctx.f, "%.*sPart %d\n", ctx.iStackPos + 1, g_strTabs, i);
                ctx.PushState();
                ctx.pobj = pent->m_parts + i;
                (ctx.pSerializer = pPartSerializer)->Serialize(ctx);
            }
        }
        else
        {
            i = atol(str);
            j = pent->m_nPartsAlloc;
            if ((pent->m_nParts = max(pent->m_nParts, i + 1)) > pent->m_nPartsAlloc)
            {
                geom* pparts = pent->m_parts;
                memcpy(pent->m_parts = new geom[pent->m_nPartsAlloc = (pent->m_nParts - 1 & ~3) + 4], pparts, sizeof(geom) * j);
                for (int k = 0; k < pent->m_nParts; k++)
                {
                    if (pent->m_parts[k].pNewCoords == (coord_block_BBox*)&pparts[k].pos)
                    {
                        pent->m_parts[k].pNewCoords = (coord_block_BBox*)&pent->m_parts[k].pos;
                    }
                }
                if (pparts != &pent->m_defpart)
                {
                    delete[] pparts;
                }
            }
#if defined(AZ_RESTRICTED_PLATFORM)
    #if defined(AZ_PLATFORM_XENIA)
        #include "Xenia/worldump_cpp_xenia.inl"
    #elif defined(AZ_PLATFORM_PROVO)
        #include "Provo/worldump_cpp_provo.inl"
    #elif defined(AZ_PLATFORM_SALEM)
        #include "Salem/worldump_cpp_salem.inl"
    #endif
#endif
            
            ctx.PushState();
            ctx.pobj = pent->m_parts + i;
            pent->m_parts[i].pPlaceholder = 0;
            pent->m_parts[i].pNewCoords = (coord_block_BBox*)&pent->m_parts[i].pos;
            (ctx.pSerializer = pPartSerializer)->Serialize(ctx);
        }
        return ctx.bSaving;
    }
    int SerializeJoint(parse_context& ctx, char* str, size_t)
    {
        CPhysicalEntity* pent = (CPhysicalEntity*)ctx.pobj;
        if (ctx.bSaving)
        {
            for (int i = 0; i < (pent->m_pStructure ? pent->m_pStructure->nJoints : 0); i++)
            {
                fprintf(ctx.f, "%.*sStructuralJoint %d\n", ctx.iStackPos + 1, g_strTabs, pent->m_pStructure->pJoints[i].id);
                ctx.PushState();
                ctx.pobj = pent->m_pStructure->pParts + i;
                (ctx.pSerializer = pJointSerializerWrite)->Serialize(ctx);
            }
        }
        else
        {
            pe_params_structural_joint psj;
            psj.id = atol(str);
            ctx.PushState();
            ctx.pobj = &psj;
            (ctx.pSerializer = pJointSerializerRead)->Serialize(ctx);
            for (int i = 0; i < 2; i++)
            {
                if (psj.partid[i] >= 0)
                {
                    psj.partid[i] = pent->m_parts[psj.partid[i]].id;
                }
            }
            pent->SetParams(&psj);
        }
        return ctx.bSaving;
    }
};


struct CRigidBodySerializer
    : Serializer
{
    CRigidBodySerializer()
    {
        RigidBody trg;
        DECLARE_MEMBER("pos", ft_vector, pos)
        DECLARE_MEMBER("quat", ft_quaternion, q)
        DECLARE_MEMBER("v", ft_vector, v)
        DECLARE_MEMBER("w", ft_vector, w)
        DECLARE_MEMBER("Mass", ft_float, M)
        DECLARE_MEMBER("Volume", ft_float, V)
        DECLARE_MEMBER("IBody", ft_vector, Ibody)
        DECLARE_MEMBER("QuatRel", ft_quaternion, qfb)
        DECLARE_MEMBER("OffsRel", ft_vector, offsfb)
        DECLARE_PROC("end", &CRigidBodySerializer::Finalize)
    }

    int Finalize(parse_context& ctx, char* str, size_t)
    {
        RigidBody* pbody = (RigidBody*)ctx.pobj;
        if (!ctx.bSaving)
        {
            pbody->P = pbody->v * pbody->M;
            pbody->L = pbody->q * (pbody->Ibody * (!pbody->q * pbody->w));
            if (pbody->M > 0)
            {
                pbody->Minv = 1 / pbody->M;
                (pbody->Ibody_inv = pbody->Ibody).invert();
            }
            pbody->UpdateState();
        }
        return 0;
    }
};

struct CConstraintSerializer
    : Serializer
{
    CConstraintSerializer()
    {
        pe_action_add_constraint trg;
        DECLARE_MEMBER("pBuddy", ft_entityptr, pBuddy)
        DECLARE_MEMBER("pt0", ft_vector, pt[0])
        DECLARE_MEMBER("pt1", ft_vector, pt[1])
        DECLARE_MEMBER("partid0", ft_int, partid[0])
        DECLARE_MEMBER("partid1", ft_int, partid[1])
        DECLARE_MEMBER("qframe0", ft_quaternion, qframe[0])
        DECLARE_MEMBER("qframe1", ft_quaternion, qframe[1])
        DECLARE_MEMBER("xmin", ft_float, xlimits[0])
        DECLARE_MEMBER("xmax", ft_float, xlimits[1])
        DECLARE_MEMBER("yzmin", ft_float, yzlimits[0])
        DECLARE_MEMBER("yzmax", ft_float, yzlimits[1])
        DECLARE_MEMBER("flags", ft_uint, flags)
        DECLARE_MEMBER("damping", ft_float, damping)
        DECLARE_MEMBER("sensorRadius", ft_float, sensorRadius)
        DECLARE_MEMBER("maxPullForce", ft_float, maxPullForce)
        DECLARE_MEMBER("maxBendTorque", ft_float, maxBendTorque)
    }
};

struct CRigidEntitySerializer
    : CPhysicalEntitySerializer
{
    CRigidBodySerializer* pRigidBodySerializer;
    CConstraintSerializer* pConstraintSerializer;

    CRigidEntitySerializer(CPhysicalEntityPartSerializer* peps, CPhysicalJointSerializerRead* pjsr, CPhysicalJointSerializerWrite* pjsw, CRigidBodySerializer* rbs, CConstraintSerializer* cs)
        : CPhysicalEntitySerializer(peps, pjsr, pjsw)
        , pRigidBodySerializer(rbs)
        , pConstraintSerializer(cs)
    {
        CRigidEntity trg(0);
        DECLARE_PROC("Body", &CRigidEntitySerializer::SerializeBody)
        DECLARE_PROC("CollisionCulling", &CRigidEntitySerializer::SerializeCollisionCulling)
        DECLARE_PROC("CanSweep", &CRigidEntitySerializer::SerializeCanSweep)
        DECLARE_MEMBER("Gravity", ft_vector, m_gravity)
        DECLARE_MEMBER("GravityFreefall", ft_vector, m_gravityFreefall)
        DECLARE_MEMBER("Emin", ft_float, m_Emin)
        DECLARE_MEMBER("MaxStep", ft_float, m_maxAllowedStep)
        DECLARE_PROC("Awake", &CRigidEntitySerializer::SerializeAwake)
        DECLARE_MEMBER("Damping", ft_float, m_damping)
        DECLARE_MEMBER("DampingFreefall", ft_float, m_dampingFreefall)
        DECLARE_MEMBER("Maxw", ft_float, m_maxw)
        DECLARE_MEMBER("collTypes", ft_int, m_collTypes)
        DECLARE_PROC("Constraint", &CRigidEntitySerializer::SerializeConstraints)
        DECLARE_PROC("end", &CRigidEntitySerializer::Finalize)
    }
    DEFINE_MEMBER_PROC(CRigidEntity, SerializeCollisionCulling, m_bCollisionCulling)
    DEFINE_MEMBER_PROC(CRigidEntity, SerializeCanSweep, m_bCanSweep)
    DEFINE_MEMBER_PROC(CRigidEntity, SerializeAwake, m_bAwake)

    int SerializeBody(parse_context& ctx, char* str, size_t)
    {
        CRigidEntity* pent = (CRigidEntity*)ctx.pobj;
        if (ctx.bSaving)
        {
            fprintf(ctx.f, "%.*sBody\n", ctx.iStackPos + 1, g_strTabs);
            ctx.PushState();
            ctx.pobj = &pent->m_body;
            (ctx.pSerializer = pRigidBodySerializer)->Serialize(ctx);
        }
        else
        {
            ctx.PushState();
            ctx.pobj = &pent->m_body;
            (ctx.pSerializer = pRigidBodySerializer)->Serialize(ctx);
        }
        return ctx.bSaving;
    }
    int SerializeConstraints(parse_context& ctx, char* str, size_t)
    {
        CRigidEntity* pent = (CRigidEntity*)ctx.pobj;
        masktype mask = 0;
        for (int i = 0; i < pent->m_nColliders; mask |= pent->m_pColliderConstraints[i++])
        {
            ;
        }
        if (ctx.bSaving)
        {
            for (int i = 0; i < NMASKBITS && getmask(i) <= mask; )
            {
                if (pent->m_pConstraintInfos[i].flags & constraint_rope)
                {
                    i++;
                    continue;
                }
                pe_action_add_constraint aac;
                i += pent->ExtractConstraintInfo(i, mask, aac);
                fprintf(ctx.f, "%.*sConstraint %d\n", ctx.iStackPos + 1, g_strTabs, aac.id);
                ctx.PushState();
                ctx.pobj = &aac;
                (ctx.pSerializer = pConstraintSerializer)->Serialize(ctx);
            }
        }
        else
        {
            pe_action_add_constraint aac;
            aac.id = atol(str);
            ctx.PushState();
            ctx.pobj = &aac;
            (ctx.pSerializer = pConstraintSerializer)->Serialize(ctx);
            pent->Action(&aac);
        }
        return ctx.bSaving;
    }

    int Finalize(parse_context& ctx, char* str, size_t)
    {
        CRigidEntity* pent = (CRigidEntity*)ctx.pobj;
        if (!ctx.bSaving)
        {
            pent->m_pNewCoords->pos = pent->m_pos;
            pent->m_pNewCoords->q = pent->m_qrot;
            pent->m_prevPos = pent->m_body.pos;
            pent->m_prevq = pent->m_body.q;
            AtomicAdd(&ctx.pWorld->m_lockGrid, -ctx.pWorld->RepositionEntity(pent));
        }
        return 0;
    }
};


struct CSuspSerializer
    : Serializer
{
    CSuspSerializer()
    {
        suspension_point trg;
        DECLARE_PROC("Driving", &CSuspSerializer::SerializeDriving)
        DECLARE_MEMBER("Axle", ft_int, iAxle)
        DECLARE_MEMBER("Pt", ft_vector, pt)
        DECLARE_MEMBER("FullLen", ft_float, fullen)
        DECLARE_MEMBER("Stiffness", ft_float, kStiffness)
        DECLARE_MEMBER("StiffnessWeight", ft_float, kStiffnessWeight)
        DECLARE_MEMBER("Damping", ft_float, kDamping)
        DECLARE_MEMBER("Damping0", ft_float, kDamping0)
        DECLARE_MEMBER("Len0", ft_float, len0)
        DECLARE_MEMBER("Mpt", ft_float, Mpt)
        DECLARE_MEMBER("Quat0", ft_quaternion, q0)
        DECLARE_MEMBER("Pos0", ft_vector, pos0)
        DECLARE_MEMBER("Ptc0", ft_vector, ptc0)
        DECLARE_MEMBER("Iinv", ft_float, Iinv)
        DECLARE_MEMBER("minFriction", ft_float, minFriction)
        DECLARE_MEMBER("maxFriction", ft_float, maxFriction)
        DECLARE_MEMBER("Flags0", ft_uint, flags0)
        DECLARE_MEMBER("FlagsCollider0", ft_uint, flagsCollider0)
        DECLARE_PROC("CanBrake", &CSuspSerializer::SerializeCanBrake)
        DECLARE_PROC("CanSteer", &CSuspSerializer::SerializeCanSteer)
        DECLARE_MEMBER("iBuddy", ft_int, iBuddy)
        DECLARE_MEMBER("r", ft_float, r)
        DECLARE_MEMBER("Width", ft_float, width)
        DECLARE_PROC("end", &CSuspSerializer::Finalize)
    }
    DEFINE_MEMBER_PROC(suspension_point, SerializeDriving, bDriving)
    DEFINE_MEMBER_PROC(suspension_point, SerializeCanBrake, bCanBrake)
    DEFINE_MEMBER_PROC(suspension_point, SerializeCanSteer, bCanSteer)

    int Finalize(parse_context& ctx, char* str, size_t)
    {
        suspension_point* psusp = (suspension_point*)ctx.pobj;
        if (!ctx.bSaving)
        {
            psusp->curlen = psusp->len0;
            psusp->rinv = 1 / psusp->r;
            psusp->steer = psusp->rot = psusp->w = psusp->wa = psusp->T = psusp->prevw = psusp->prevTdt = 0;
            psusp->bSlip = psusp->bSlipPull = psusp->bContact = 0;
        }
        return 0;
    }
};


struct CWheeledVehicleEntitySerializer
    : CRigidEntitySerializer
{
    CSuspSerializer* pSuspSerializer;

    CWheeledVehicleEntitySerializer(CPhysicalEntityPartSerializer* peps, CPhysicalJointSerializerRead* pjsr, CPhysicalJointSerializerWrite* pjsw, CRigidBodySerializer* rbs,
        CConstraintSerializer* cs, CSuspSerializer* ss)
        : CRigidEntitySerializer(peps, pjsr, pjsw, rbs, cs)
        , pSuspSerializer(ss)
    {
        CWheeledVehicleEntity trg(0);
        DECLARE_MEMBER("EnginePower", ft_float, m_enginePower)
        DECLARE_MEMBER("maxSteer", ft_float, m_maxSteer)
        DECLARE_MEMBER("EngineMaxw", ft_float, m_engineMaxw)
        DECLARE_MEMBER("EngineMinw", ft_float, m_engineMinw)
        DECLARE_MEMBER("EngineIdlew", ft_float, m_engineIdlew)
        DECLARE_MEMBER("EngineShiftUpw", ft_float, m_engineShiftUpw)
        DECLARE_MEMBER("EngineShiftDownw", ft_float, m_engineShiftDownw)
        DECLARE_MEMBER("GearDirSwitchw", ft_float, m_gearDirSwitchw)
        DECLARE_MEMBER("EngineStartw", ft_float, m_engineStartw)
        DECLARE_MEMBER("AxleFriction", ft_float, m_axleFriction)
        DECLARE_MEMBER("BrakeTorque", ft_float, m_brakeTorque)
        DECLARE_MEMBER("ClutchSpeed", ft_float, m_clutchSpeed)
        DECLARE_MEMBER("maxBrakingFriction", ft_float, m_maxBrakingFriction)
        DECLARE_MEMBER("fDynFriction", ft_float, m_kDynFriction)
        DECLARE_MEMBER("SlipThreshold", ft_float, m_slipThreshold)
        DECLARE_MEMBER("kStabilizer", ft_float, m_kStabilizer)
        DECLARE_MEMBER("ackermanOffset", ft_float, m_ackermanOffset)
        DECLARE_MEMBER("numGears", ft_int, m_nGears)
        DECLARE_PROC("Gears", &CWheeledVehicleEntitySerializer::SerializeGears)
        //DECLARE_MEMBER("kSteerToTrack", ft_float, m_kSteerToTrack)
        DECLARE_MEMBER("nHullParts", ft_int, m_nHullParts)
        //DECLARE_MEMBER("IntegrationType", ft_int, m_iIntegrationType)
        DECLARE_MEMBER("EminRigid", ft_float, m_EminRigid)
        DECLARE_MEMBER("EminVehicle", ft_float, m_EminVehicle)
        DECLARE_MEMBER("maxStepVehicle", ft_float, m_maxAllowedStepVehicle)
        DECLARE_MEMBER("maxStepRigid", ft_float, m_maxAllowedStepRigid)
        DECLARE_MEMBER("DampingVehicle", ft_float, m_dampingVehicle)
        DECLARE_PROC("Wheel", &CWheeledVehicleEntitySerializer::SerializeWheel)
    }

    int SerializeGears(parse_context& ctx, char* str, size_t strSize)
    {
        CWheeledVehicleEntity* pent = (CWheeledVehicleEntity*)ctx.pobj;
        int i;
        for (i = 0; i < pent->m_nGears; i++)
        {
            if (ctx.bSaving)
            {
                str += sprintf_s(str, strSize, "%.8g ", pent->m_gears[i]);
            }
            else
            {
                for (; *str && isspace(*str); str++)
                {
                    ;
                }
                pent->m_gears[i] = atof(str);
                for (; *str && !isspace(*str); str++)
                {
                    ;
                }
            }
        }
        return 0;
    }

    int SerializeWheel(parse_context& ctx, char* str, size_t strSize)
    {
        CWheeledVehicleEntity* pent = (CWheeledVehicleEntity*)ctx.pobj;
        int i;
        if (ctx.bSaving)
        {
            for (i = 0; i < pent->m_nParts - pent->m_nHullParts; i++)
            {
                fprintf(ctx.f, "%.*sWheel %d\n", ctx.iStackPos + 1, g_strTabs, i);
                ctx.PushState();
                ctx.pobj = pent->m_susp + i;
                (ctx.pSerializer = pSuspSerializer)->Serialize(ctx);
            }
        }
        else
        {
            i = atol(str);
            ctx.PushState();
            ctx.pobj = pent->m_susp + i;
            pent->m_parts[i + pent->m_nHullParts].pNewCoords = (coord_block_BBox*)&pent->m_susp[i].pos;
            pent->m_susp[i].pos = pent->m_parts[i + pent->m_nHullParts].pos;
            pent->m_susp[i].q = pent->m_parts[i + pent->m_nHullParts].q;
            pent->m_susp[i].scale = pent->m_parts[i + pent->m_nHullParts].scale;
            (ctx.pSerializer = pSuspSerializer)->Serialize(ctx);
        }
        return ctx.bSaving;
    }
};


struct CJointSerializer
    : Serializer
{
    CRigidBodySerializer* pRigidBodySerializer;

    CJointSerializer(CRigidBodySerializer* rbs)
        : pRigidBodySerializer(rbs)
    {
        ae_joint trg;
        DECLARE_MEMBER("Angles", ft_vector, q)
        DECLARE_MEMBER("AnglesExt", ft_vector, qext)
        DECLARE_MEMBER("Quat", ft_quaternion, quat)
        DECLARE_MEMBER("Angles0", ft_vector, q0)
        DECLARE_MEMBER("Flags", ft_uint, flags)
        DECLARE_MEMBER("QuatRel", ft_quaternion, quat0)
        DECLARE_MEMBER("LimitsMin", ft_vector, limits[0])
        DECLARE_MEMBER("LimitsMax", ft_vector, limits[1])
        DECLARE_MEMBER("Bounciness", ft_vector, bounciness)
        DECLARE_MEMBER("Stiffness", ft_vector, ks)
        DECLARE_MEMBER("Damping", ft_vector, kd)
        DECLARE_MEMBER("qDashpot", ft_vector, qdashpot)
        DECLARE_MEMBER("kDashpot", ft_vector, kdashpot)
        DECLARE_MEMBER("PivotParent", ft_vector, pivot[0])
        DECLARE_MEMBER("PivotChild", ft_vector, pivot[1])
        DECLARE_MEMBER("StartPart", ft_int, iStartPart)
        DECLARE_MEMBER("numParts", ft_int, nParts)
        DECLARE_MEMBER("Parent", ft_int, iParent)
        DECLARE_MEMBER("numChildren", ft_int, nChildren)
        DECLARE_MEMBER("numTotChildren", ft_int, nChildrenTree)
        DECLARE_MEMBER("Level", ft_int, iLevel)
        DECLARE_MEMBER("SelfCollMask", ft_uint64, selfCollMask)
        DECLARE_MEMBER("Quat0Changed", ft_int, bQuat0Changed)
        DECLARE_MEMBER("idBody", ft_int, idbody)
        DECLARE_PROC("Body", &CJointSerializer::SerializeBody)
    }

    int SerializeBody(parse_context& ctx, char* str, size_t)
    {
        ae_joint* pjoint = (ae_joint*)ctx.pobj;
        if (ctx.bSaving)
        {
            fprintf(ctx.f, "%.*sBody\n", ctx.iStackPos + 1, g_strTabs);
            ctx.PushState();
            ctx.pobj = &pjoint->body;
            (ctx.pSerializer = pRigidBodySerializer)->Serialize(ctx);
        }
        else
        {
            ctx.PushState();
            ctx.pobj = &pjoint->body;
            (ctx.pSerializer = pRigidBodySerializer)->Serialize(ctx);
        }
        return ctx.bSaving;
    }
};


struct CAEPartInfoSerializer
    : Serializer
{
    CAEPartInfoSerializer()
    {
        ae_part_info trg;
        DECLARE_MEMBER("Quat0", ft_quaternion, q0)
        DECLARE_MEMBER("Pos0", ft_vector, pos0)
        DECLARE_MEMBER("Joint", ft_int, iJoint)
        DECLARE_MEMBER("idBody", ft_int, idbody)
    }
};


struct CArticulatedEntitySerializer
    : CRigidEntitySerializer
{
    CJointSerializer* pJointSerializer;
    CAEPartInfoSerializer* pPartInfoSerializer;

    CArticulatedEntitySerializer(CPhysicalEntityPartSerializer* peps, CPhysicalJointSerializerRead* pjsr, CPhysicalJointSerializerWrite* pjsw, CRigidBodySerializer* rbs,
        CConstraintSerializer* cs, CJointSerializer* js, CAEPartInfoSerializer* aepis)
        : CRigidEntitySerializer(peps, pjsr, pjsw, rbs, cs)
        , pJointSerializer(js)
        , pPartInfoSerializer(aepis)
    {
        CArticulatedEntity trg(0);
        DECLARE_MEMBER("numJoints", ft_int, m_nJoints)
        DECLARE_PROC("Joint", &CArticulatedEntitySerializer::SerializeJoint)
        DECLARE_PROC("PartInfo", &CArticulatedEntitySerializer::SerializePartInfo)
        DECLARE_MEMBER("OffsPivot", ft_vector, m_offsPivot)
        DECLARE_MEMBER("ScaleBounceResponse", ft_float, m_scaleBounceResponse)
        DECLARE_MEMBER("Grounded", ft_int, m_bGrounded)
        DECLARE_MEMBER("InheritVel", ft_int, m_bInheritVel)
        DECLARE_PROC("Host", &CArticulatedEntitySerializer::SerializeHost)
        DECLARE_MEMBER("HostPivot", ft_vector, m_posHostPivot)
        DECLARE_MEMBER("CheckCollisions", ft_int, m_bCheckCollisions)
        DECLARE_MEMBER("CollisionResponse", ft_int, m_bCollisionResp)
        DECLARE_MEMBER("ExertImpulse", ft_int, m_bExertImpulse)
        DECLARE_MEMBER("SimType", ft_int, m_iSimType)
        DECLARE_MEMBER("LyingSymType", ft_int, m_iSimTypeLyingMode)
        DECLARE_MEMBER("numCollLying", ft_int, m_nCollLyingMode)
        DECLARE_MEMBER("LyingGravity", ft_vector, m_gravityLyingMode)
        DECLARE_MEMBER("LyingDamping", ft_float, m_dampingLyingMode)
        DECLARE_MEMBER("LyingEmin", ft_float, m_EminLyingMode)
        DECLARE_PROC("end", &CArticulatedEntitySerializer::Finalize)
    }

    int SerializePartInfo(parse_context& ctx, char* str, size_t)
    {
        CArticulatedEntity* pent = (CArticulatedEntity*)ctx.pobj;
        int i;
        if (ctx.bSaving)
        {
            for (i = 0; i < pent->m_nParts; i++)
            {
                fprintf(ctx.f, "%.*sPartInfo %d\n", ctx.iStackPos + 1, g_strTabs, i);
                ctx.PushState();
                ctx.pobj = pent->m_infos + i;
                (ctx.pSerializer = pPartInfoSerializer)->Serialize(ctx);
            }
        }
        else
        {
            i = atol(str);
            ctx.PushState();
            if (i == 0)
            {
                if (pent->m_infos)
                {
                    delete[] pent->m_infos;
                }
                pent->m_infos = new ae_part_info[pent->m_nParts];
                for (int j = 0; j < pent->m_nParts; j++)
                {
                    pent->m_parts[j].pNewCoords = (coord_block_BBox*)&pent->m_infos[j].pos;
                    pent->m_infos[j].pos = pent->m_parts[j].pos;
                    pent->m_infos[j].q = pent->m_parts[j].q;
                    pent->m_infos[j].scale = pent->m_parts[j].scale;
                }
            }
            ctx.pobj = pent->m_infos + i;
            (ctx.pSerializer = pPartInfoSerializer)->Serialize(ctx);
        }
        return ctx.bSaving;
    }

    int SerializeJoint(parse_context& ctx, char* str, size_t)
    {
        CArticulatedEntity* pent = (CArticulatedEntity*)ctx.pobj;
        int i, j;
        if (ctx.bSaving)
        {
            for (i = 0; i < pent->m_nJoints; i++)
            {
                fprintf(ctx.f, "%.*sJoint %d\n", ctx.iStackPos + 1, g_strTabs, i);
                ctx.PushState();
                ctx.pobj = pent->m_joints + i;
                (ctx.pSerializer = pJointSerializer)->Serialize(ctx);
            }
        }
        else
        {
            i = atol(str);
            if (i >= pent->m_nJointsAlloc)
            {
                ae_joint* pJoints = pent->m_joints;
                j = pent->m_nJointsAlloc;
                memcpy(pent->m_joints = new ae_joint[pent->m_nJointsAlloc = (i & ~3) + 4], pJoints, sizeof(ae_joint) * j);
                if (pJoints)
                {
                    delete[] pJoints;
                }
            }
            ctx.PushState();
            ctx.pobj = pent->m_joints + i;
            (ctx.pSerializer = pJointSerializer)->Serialize(ctx);
        }
        return ctx.bSaving;
    }

    int SerializeHost(parse_context& ctx, char* str, size_t)
    {
        CArticulatedEntity* pent = (CArticulatedEntity*)ctx.pobj;
        if (ctx.bSaving)
        {
            ltoa(ctx.pWorld->GetPhysicalEntityId(pent->m_pHost) + 1, str, 10);
        }
        else
        {
            int id = atoi(str);
            pent->m_pHost = id ? (CPhysicalEntity*)ctx.pWorld->GetPhysicalEntityById(id - 1) : 0;
        }
        return 0;
    }

    int Finalize(parse_context& ctx, char* str, size_t)
    {
        CArticulatedEntity* pent = (CArticulatedEntity*)ctx.pobj;
        if (!ctx.bSaving)
        {
            pent->m_pNewCoords->pos = pent->m_pos;
            pent->m_pNewCoords->q = pent->m_qrot;
            pent->m_posPivot = pent->m_pos + pent->m_offsPivot;
            AtomicAdd(&ctx.pWorld->m_lockGrid, -ctx.pWorld->RepositionEntity(pent));
        }
        return 0;
    }
};


struct CLivingEntitySerializer
    : CPhysicalEntitySerializer
{
    CLivingEntitySerializer(CPhysicalEntityPartSerializer* peps)
        : CPhysicalEntitySerializer(peps, 0, 0)
    {
        CLivingEntity trg(0);
        DECLARE_MEMBER("Gravity", ft_vector, m_gravity)
        DECLARE_MEMBER("Inertia", ft_float, m_kInertia)
        DECLARE_MEMBER("InertiaAccel", ft_float, m_kInertiaAccel)
        DECLARE_MEMBER("AirControl", ft_float, m_kAirControl)
        DECLARE_MEMBER("AirResistance", ft_float, m_kAirResistance)
        DECLARE_MEMBER("hCyl", ft_float, m_hCyl)
        DECLARE_MEMBER("hEye", ft_float, m_hEye)
        DECLARE_MEMBER("hPivot", ft_float, m_hPivot)
        DECLARE_MEMBER("hHead", ft_float, m_hHead)
        DECLARE_MEMBER("Size", ft_vector, m_size)
        DECLARE_MEMBER("NodSpeed", ft_float, m_nodSpeed)
        DECLARE_MEMBER("Mass", ft_float, m_mass)
        DECLARE_MEMBER("MassInv", ft_float, m_massinv)
        DECLARE_MEMBER("SlopeSlide", ft_float, m_slopeSlide)
        DECLARE_MEMBER("SlopeClimb", ft_float, m_slopeClimb)
        DECLARE_MEMBER("SlopeJump", ft_float, m_slopeJump)
        DECLARE_MEMBER("SlopeFall", ft_float, m_slopeFall)
        DECLARE_MEMBER("MaxVelGround", ft_float, m_maxVelGround)
        DECLARE_PROC("Active", &CLivingEntitySerializer::SerializeActive)
        DECLARE_PROC("Swimming", &CLivingEntitySerializer::SerializeSwimming)
        DECLARE_PROC("end", &CLivingEntitySerializer::Finalize)
    }
    DEFINE_MEMBER_PROC(CLivingEntity, SerializeActive, m_bActive)
    DEFINE_MEMBER_PROC(CLivingEntity, SerializeSwimming, m_bSwimming)

    int Finalize(parse_context& ctx, char* str, size_t)
    {
        CLivingEntity* pent = (CLivingEntity*)ctx.pobj;
        if (!ctx.bSaving)
        {
            cylinder dim;
            dim.r = pent->m_size.x;
            dim.hh = pent->m_size.z;
            dim.center.zero();
            dim.axis.Set(0, 0, 1);
            pent->m_pCylinderGeom->CreateCylinder(&dim);
            pent->m_pNewCoords->pos = pent->m_pos;
            pent->m_pNewCoords->q = pent->m_qrot;
            AtomicAdd(&ctx.pWorld->m_lockGrid, -ctx.pWorld->RepositionEntity(pent));
        }
        return 0;
    }
};


struct CRopeVtxSerializer
    : Serializer
{
    CRopeVtxSerializer()
    {
        rope_vtx trg;
        DECLARE_MEMBER("pt", ft_vector, pt)
        DECLARE_MEMBER("vel", ft_vector, vel)
        DECLARE_MEMBER("dir", ft_vector, dir)
        DECLARE_MEMBER("ncontact", ft_vector, ncontact)
        DECLARE_MEMBER("vcontact", ft_vector, vcontact)
        DECLARE_MEMBER("ContactEnt", ft_entityptr, pContactEnt)
        DECLARE_MEMBER("ContactPart", ft_int, iContactPart)
    }
};

struct CRopeSegSerializer
    : Serializer
{
    CRopeSegSerializer()
    {
        rope_segment trg;
        DECLARE_MEMBER("pt", ft_vector, pt)
        DECLARE_MEMBER("vel", ft_vector, vel)
        DECLARE_MEMBER("dir", ft_vector, dir)
        DECLARE_MEMBER("ncontact", ft_vector, ncontact)
        DECLARE_MEMBER("vcontact", ft_vector, vcontact)
        DECLARE_MEMBER("ContactEnt", ft_entityptr, pContactEnt)
        DECLARE_MEMBER("ContactPart", ft_int, iContactPart)
        DECLARE_MEMBER("ptdst", ft_vector, ptdst)
        DECLARE_MEMBER("tcontact", ft_float, tcontact)
        DECLARE_MEMBER("iVtx0", ft_int, iVtx0)
        DECLARE_PROC("end", &CRopeSegSerializer::Finalize)
    }
    int Finalize(parse_context& ctx, char* str, size_t)
    {
        rope_segment* pseg = (rope_segment*)ctx.pobj;
        if (!ctx.bSaving)
        {
            pseg->pt0 = pseg->pt;
        }
        return 0;
    }
};

struct CRopeEntitySerializer
    : CPhysicalEntitySerializer
{
    CRopeVtxSerializer* pVtxSerializer;
    CRopeSegSerializer* pSegSerializer;

    CRopeEntitySerializer(CPhysicalEntityPartSerializer* peps, CRopeVtxSerializer* prvs, CRopeSegSerializer* prss)
        : CPhysicalEntitySerializer(peps, 0, 0)
        , pVtxSerializer(prvs)
        , pSegSerializer(prss)
    {
        CRopeEntity trg(0);
        DECLARE_MEMBER("numSegs", ft_int, m_nSegs)
        DECLARE_MEMBER("numVtx", ft_int, m_nVtx)
        DECLARE_MEMBER("MaxSubVtx", ft_int, m_nMaxSubVtx)
        DECLARE_PROC("Segment", &CRopeEntitySerializer::SerializeSegment)
        DECLARE_PROC("Vertex", &CRopeEntitySerializer::SerializeVertex)
        DECLARE_MEMBER("Gravity", ft_vector, m_gravity)
        DECLARE_MEMBER("Gravity0", ft_vector, m_gravity0)
        DECLARE_MEMBER("Damping", ft_float, m_damping)
        DECLARE_MEMBER("MaxStep", ft_float, m_maxAllowedStep)
        DECLARE_MEMBER("Emin", ft_float, m_Emin)
        DECLARE_MEMBER("Length", ft_float, m_length)
        DECLARE_MEMBER("Mass", ft_float, m_mass)
        DECLARE_MEMBER("Thickness", ft_float, m_collDist)
        DECLARE_MEMBER("collTypes", ft_int, m_collTypes)
        DECLARE_MEMBER("SurfaceIdx", ft_int, m_surface_idx)
        DECLARE_MEMBER("Friction", ft_float, m_friction)
        DECLARE_MEMBER("FrictionPull", ft_float, m_frictionPull)
        DECLARE_MEMBER("Stiffness", ft_float, m_stiffness)
        DECLARE_MEMBER("StiffnessAnim", ft_float, m_stiffnessAnim)
        DECLARE_MEMBER("DampingAnim", ft_float, m_dampingAnim)
        DECLARE_MEMBER("StiffnessDecayAnim", ft_float, m_stiffnessDecayAnim)
        DECLARE_MEMBER("TargetPoseActive", ft_int, m_bTargetPoseActive)
        DECLARE_MEMBER("Wind", ft_vector, m_wind)
        DECLARE_MEMBER("AirResistance", ft_float, m_airResistance)
        DECLARE_MEMBER("WindVariance", ft_float, m_windVariance)
        DECLARE_MEMBER("WaterResistance", ft_float, m_waterResistance)
        DECLARE_MEMBER("rDensity", ft_float, m_rdensity)
        DECLARE_MEMBER("JointLimit", ft_float, m_jointLimit)
        DECLARE_MEMBER("SensorSize", ft_float, m_szSensor)
        DECLARE_MEMBER("MaxForce", ft_float, m_maxForce)
        DECLARE_MEMBER("FlagsCollider", ft_int, m_flagsCollider)
        DECLARE_MEMBER("EntTied0", ft_entityptr, m_pTiedTo[0]);
        DECLARE_MEMBER("EntTied1", ft_entityptr, m_pTiedTo[1]);
        DECLARE_MEMBER("ptTied0", ft_vector, m_ptTiedLoc[0])
        DECLARE_MEMBER("ptTied1", ft_vector, m_ptTiedLoc[1])
        DECLARE_MEMBER("TiedPart0", ft_int, m_iTiedPart[0])
        DECLARE_MEMBER("TiedPart1", ft_int, m_iTiedPart[1])
        DECLARE_MEMBER("Strained", ft_int, m_bStrained)
        DECLARE_PROC("end", &CRopeEntitySerializer::Finalize)
    }

    int SerializeSegment(parse_context& ctx, char* str, size_t)
    {
        CRopeEntity* pent = (CRopeEntity*)ctx.pobj;
        int i;
        if (ctx.bSaving)
        {
            for (i = 0; i <= pent->m_nSegs; i++)
            {
                fprintf(ctx.f, "%.*sSegment %d\n", ctx.iStackPos + 1, g_strTabs, i);
                ctx.PushState();
                ctx.pobj = pent->m_segs + i;
                (ctx.pSerializer = pSegSerializer)->Serialize(ctx);
            }
        }
        else
        {
            i = atol(str);
            if (!pent->m_segs)
            {
                memset(pent->m_segs = new rope_segment[pent->m_nSegs + 1], 0, (pent->m_nSegs + 1) * sizeof(rope_segment));
            }
            ctx.PushState();
            ctx.pobj = pent->m_segs + i;
            (ctx.pSerializer = pSegSerializer)->Serialize(ctx);
        }
        return ctx.bSaving;
    }

    int SerializeVertex(parse_context& ctx, char* str, size_t)
    {
        CRopeEntity* pent = (CRopeEntity*)ctx.pobj;
        int i;
        if (ctx.bSaving)
        {
            for (i = 0; i < pent->m_nVtx; i++)
            {
                fprintf(ctx.f, "%.*sVertex %d\n", ctx.iStackPos + 1, g_strTabs, i);
                ctx.PushState();
                ctx.pobj = pent->m_vtx + i;
                (ctx.pSerializer = pVtxSerializer)->Serialize(ctx);
            }
        }
        else
        {
            i = atol(str);
            if (!pent->m_vtx)
            {
                memset(pent->m_vtx = new rope_vtx[pent->m_nVtx], 0, (pent->m_nVtx) * sizeof(rope_vtx));
                pent->m_idx = new int[pent->m_nMaxSubVtx + 2];
            }
            ctx.PushState();
            ctx.pobj = pent->m_vtx + i;
            (ctx.pSerializer = pVtxSerializer)->Serialize(ctx);
        }
        return ctx.bSaving;
    }

    int Finalize(parse_context& ctx, char* str, size_t)
    {
        CRopeEntity* pent = (CRopeEntity*)ctx.pobj;
        if (!ctx.bSaving)
        {
            pent->m_pNewCoords->pos = pent->m_pos;
            pent->m_pNewCoords->q = pent->m_qrot;
            AtomicAdd(&ctx.pWorld->m_lockGrid, -ctx.pWorld->RepositionEntity(pent));

            pe_params_rope pr;
            for (int i = 0; i < 2; i++)
            {
                if (pr.pEntTiedTo[i] = pent->m_pTiedTo[i])
                {
                    pr.idPartTiedTo[i] = pent->m_pTiedTo[i]->m_parts[pent->m_iTiedPart[i]].id;
                    Vec3 pos;
                    quaternionf q;
                    float scale;
                    pent->m_pTiedTo[i]->GetLocTransform(pent->m_iTiedPart[i], pos, q, scale);
                    pr.ptTiedTo[i] = q * pent->m_ptTiedLoc[i] + pos;
                    pent->m_pTiedTo[0] = 0;
                }
            }
            pent->SetParams(&pr);
        }
        return 0;
    }
};


struct CSoftVtxSerializer
    : Serializer
{
    CSoftVtxSerializer()
    {
        se_vertex trg;
        trg.pContactEnt = 0;
        DECLARE_MEMBER("pos", ft_vector, pos)
        DECLARE_MEMBER("vel", ft_vector, vel)
        DECLARE_MEMBER("mass", ft_float, mass)
        DECLARE_MEMBER("massinv", ft_float, massinv)
        DECLARE_MEMBER("idx", ft_int, idx)
        DECLARE_MEMBER("idx0", ft_int, idx0)
        DECLARE_MEMBER("angle0", ft_float, angle0)
        DECLARE_PROC("bAttached", &CSoftVtxSerializer::SerializeAttached)
        DECLARE_MEMBER("ptAttach", ft_vector, ptAttach)
        DECLARE_MEMBER("posorg", ft_vector, posorg)
        DECLARE_MEMBER("iSorted", ft_int, iSorted)
        DECLARE_PROC("iContactPart", &CSoftVtxSerializer::SerializeContactPart)
        DECLARE_MEMBER("pContactEnt", ft_entityptr, pContactEnt)
    }
    DEFINE_MEMBER_PROC(se_vertex, SerializeAttached, bAttached)
    DEFINE_MEMBER_PROC(se_vertex, SerializeContactPart, iContactPart)
};

struct CSoftEdgeSerializer
    : Serializer
{
    CSoftEdgeSerializer()
    {
        se_edge trg;
        DECLARE_MEMBER("len0", ft_float, len0)
        DECLARE_MEMBER("kmass", ft_float, kmass)
    }
};

struct CSoftEntitySerializer
    : CPhysicalEntitySerializer
{
    CSoftVtxSerializer* pVtxSerializer;
    CSoftEdgeSerializer* pEdgeSerializer;

    CSoftEntitySerializer(CPhysicalEntityPartSerializer* peps, CSoftVtxSerializer* psvs, CSoftEdgeSerializer* pses)
        : CPhysicalEntitySerializer(peps, 0, 0)
        , pVtxSerializer(psvs)
        , pEdgeSerializer(pses)
    {
        CSoftEntity trg(0);
        DECLARE_PROC("Part", &CSoftEntitySerializer::SerializePart)
        DECLARE_MEMBER("nAttachedVtx", ft_int, m_nAttachedVtx)
        DECLARE_MEMBER("nConnectedVtx", ft_int, m_nConnectedVtx)
        DECLARE_MEMBER("offs0", ft_vector, m_offs0)
        DECLARE_MEMBER("gravity", ft_vector, m_gravity)
        DECLARE_MEMBER("Emin", ft_float, m_Emin)
        DECLARE_MEMBER("maxAllowedStep", ft_float, m_maxAllowedStep)
        DECLARE_MEMBER("damping", ft_float, m_damping)
        DECLARE_MEMBER("accuracy", ft_float, m_accuracy)
        DECLARE_MEMBER("nMaxIters", ft_int, m_nMaxIters)
        DECLARE_MEMBER("maxMove", ft_float, m_maxMove)
        DECLARE_MEMBER("maxAllowedDist", ft_float, m_maxAllowedDist)
        DECLARE_MEMBER("thickness", ft_float, m_thickness)
        DECLARE_MEMBER("ks", ft_float, m_ks)
        DECLARE_MEMBER("maxSafeStep", ft_float, m_maxSafeStep)
        DECLARE_MEMBER("density", ft_float, m_density)
        DECLARE_MEMBER("coverage", ft_float, m_coverage)
        DECLARE_MEMBER("friction", ft_float, m_friction)
        DECLARE_MEMBER("impulseScale", ft_float, m_impulseScale)
        DECLARE_MEMBER("explosionScale", ft_float, m_explosionScale)
        DECLARE_MEMBER("collImpulseScale", ft_float, m_collImpulseScale)
        DECLARE_MEMBER("maxCollImpulse", ft_float, m_maxCollImpulse)
        DECLARE_MEMBER("collTypes", ft_int, m_collTypes)
        DECLARE_MEMBER("massDecay", ft_float, m_massDecay)
        DECLARE_MEMBER("kShapeStiffnessNorm", ft_float, m_kShapeStiffnessNorm)
        DECLARE_MEMBER("kShapeStiffnessTang", ft_float, m_kShapeStiffnessTang)
        DECLARE_MEMBER("vtxvol", ft_float, m_vtxvol)
        DECLARE_MEMBER("stiffnessAnim", ft_float, m_stiffnessAnim)
        DECLARE_MEMBER("stiffnessDecayAnim", ft_float, m_stiffnessDecayAnim)
        DECLARE_MEMBER("dampingAnim", ft_float, m_dampingAnim)
        DECLARE_MEMBER("maxDistAnim", ft_float, m_maxDistAnim)
        DECLARE_MEMBER("kRigid", ft_float, m_kRigid)
        DECLARE_MEMBER("maxLevelDenom", ft_float, m_maxLevelDenom)
        DECLARE_MEMBER("waterResistance", ft_float, m_waterResistance)
        DECLARE_MEMBER("airResistance", ft_float, m_airResistance)
        DECLARE_MEMBER("wind", ft_vector, m_wind)
        DECLARE_MEMBER("windVariance", ft_float, m_windVariance)
        DECLARE_PROC("Vertex", &CSoftEntitySerializer::SerializeVertex)
        DECLARE_PROC("Edge", &CSoftEntitySerializer::SerializeEdge)
        DECLARE_PROC("end", &CSoftEntitySerializer::Finalize)
    }

    int SerializePart(parse_context& ctx, char* str, size_t strSize)
    {
        CPhysicalEntitySerializer::SerializePart(ctx, str, strSize);
        CSoftEntity* pent = (CSoftEntity*)ctx.pobj;
        if (!ctx.bSaving && pent->m_nParts == 1)
        {
            int id = pent->m_parts[0].id;
            phys_geometry* pgeom = pent->m_parts[0].pPhysGeom;
            pe_geomparams gp;
            gp.pos =  pent->m_parts[0].pos;
            gp.q = pent->m_parts[0].q;
            gp.scale = pent->m_parts[0].scale;
            gp.mass = pent->m_parts[0].mass;
            ++pgeom->nRefCount;
            pent->RemoveGeometry(id);
            pent->AddGeometry(pgeom, &gp, id);
            pgeom->nRefCount--;
        }
        return ctx.bSaving;
    }

    int SerializeVertex(parse_context& ctx, char* str, size_t)
    {
        CSoftEntity* pent = (CSoftEntity*)ctx.pobj;
        if (ctx.bSaving)
        {
            for (int i = 0; i < pent->m_nVtx; i++)
            {
                fprintf(ctx.f, "%.*sVertex %d\n", ctx.iStackPos + 1, g_strTabs, i);
                ctx.PushState();
                ctx.pobj = pent->m_vtx + i;
                (ctx.pSerializer = pVtxSerializer)->Serialize(ctx);
            }
        }
        else if (pent->m_nParts && pent->m_nVtx)
        {
            ctx.PushState();
            ctx.pobj = pent->m_vtx + atol(str);
            (ctx.pSerializer = pVtxSerializer)->Serialize(ctx);
        }
        return ctx.bSaving;
    }
    int SerializeEdge(parse_context& ctx, char* str, size_t)
    {
        CSoftEntity* pent = (CSoftEntity*)ctx.pobj;
        if (ctx.bSaving)
        {
            for (int i = 0; i < pent->m_nEdges; i++)
            {
                fprintf(ctx.f, "%.*sEdge %d\n", ctx.iStackPos + 1, g_strTabs, i);
                ctx.PushState();
                ctx.pobj = pent->m_edges + i;
                (ctx.pSerializer = pEdgeSerializer)->Serialize(ctx);
            }
        }
        else if (pent->m_nParts && pent->m_nEdges)
        {
            ctx.PushState();
            ctx.pobj = pent->m_edges + atol(str);
            (ctx.pSerializer = pEdgeSerializer)->Serialize(ctx);
        }
        return ctx.bSaving;
    }

    int Finalize(parse_context& ctx, char* str, size_t)
    {
        CSoftEntity* pent = (CSoftEntity*)ctx.pobj;
        if (!ctx.bSaving)
        {
            pent->m_pNewCoords->pos = pent->m_pos;
            pent->m_pNewCoords->q = pent->m_qrot;
            AtomicAdd(&ctx.pWorld->m_lockGrid, -ctx.pWorld->RepositionEntity(pent));
        }
        return 0;
    }
};

int IsAreaNonWater(CPhysArea* pArea);
int IsAreaGravity(CPhysArea* pArea);

struct CAreaSerializer
    : CPhysicalPlaceholderSerializer
{
    CAreaSerializer(CPhysGeometrySerializer* pgs)
        : pGeomSerializer(pgs)
    {
        CPhysArea trg(0);
        DECLARE_MEMBER("offset", ft_vector, m_offset)
        DECLARE_MEMBER("R", ft_matrix33, m_R)
        DECLARE_MEMBER("scale", ft_float, m_scale)
        DECLARE_MEMBER("offset0", ft_vector, m_offset0)
        DECLARE_MEMBER("R0", ft_matrix33, m_R0)
        DECLARE_PROC("Geometry", &CAreaSerializer::SerializeGeometry)
        DECLARE_MEMBER("npt", ft_int, m_npt)
        DECLARE_PROC("pt2d", &CAreaSerializer::SerializePoints)
        DECLARE_MEMBER("zlim0", ft_float, m_zlim[0])
        DECLARE_MEMBER("zlim1", ft_float, m_zlim[1])
        DECLARE_PROC("flow", &CAreaSerializer::SerializeFlows)
        DECLARE_MEMBER("size0", ft_vector, m_size0)
        DECLARE_PROC("pt3d", &CAreaSerializer::SerializeSpline)
        DECLARE_MEMBER("V", ft_float, m_V)
        DECLARE_MEMBER("accuracyV", ft_float, m_accuracyV)
        DECLARE_MEMBER("bConvexBorder", ft_int, m_bConvexBorder)
        DECLARE_MEMBER("sleepVel", ft_float, m_sleepVel)
        DECLARE_MEMBER("borderPad", ft_float, m_borderPad)
        DECLARE_MEMBER("szCell", ft_float, m_szCell)
        DECLARE_MEMBER("waveSpeed", ft_float, m_waveSpeed)
        DECLARE_MEMBER("waveTimeStep", ft_float, m_waveTimeStep)
        DECLARE_MEMBER("waveDamping", ft_float, m_waveDamping)
        DECLARE_MEMBER("sizeReserve", ft_float, m_sizeReserve)
        DECLARE_MEMBER("minObjV", ft_float, m_minObjV)
        DECLARE_MEMBER("waterDensity", ft_float, m_pb.waterDensity)
        DECLARE_MEMBER("waterDamping", ft_float, m_pb.waterDamping)
        DECLARE_MEMBER("waterResistance", ft_float, m_pb.waterResistance)
        DECLARE_MEMBER("waterFlow", ft_vector, m_pb.waterFlow)
        DECLARE_MEMBER("waterPlaneOrigin", ft_vector, m_pb.waterPlane.origin)
        DECLARE_MEMBER("waterPlaneN", ft_vector, m_pb.waterPlane.n)
        DECLARE_MEMBER("iMedium", ft_int, m_pb.iMedium)
        DECLARE_MEMBER("gravity", ft_vector, m_gravity)
        DECLARE_MEMBER("rsize", ft_vector, m_rsize)
        DECLARE_MEMBER("bUniform", ft_int, m_bUniform)
        DECLARE_MEMBER("falloff0", ft_float, m_falloff0)
        DECLARE_MEMBER("damping", ft_float, m_damping)
        DECLARE_MEMBER("bUseCallback", ft_int, m_bUseCallback)
        DECLARE_PROC("end", &CAreaSerializer::Finalize)
    }
    CPhysGeometrySerializer* pGeomSerializer;

    int SerializeGeometry(parse_context& ctx, char* str, size_t strSize) { return pGeomSerializer->SerializeGeometryPtr(ctx, str, strSize, ((CPhysArea*)ctx.pobj)->m_pGeom); }
    int SerializePoints(parse_context& ctx, char* str, size_t)
    {
        CPhysArea* pArea = (CPhysArea*)ctx.pobj;
        if (ctx.bSaving)
        {
            for (int i = 0; i < (pArea->m_pt ? pArea->m_npt : 0); i++)
            {
                fprintf(ctx.f, "%.*spt2d %d (%.8g %.8g)\n", ctx.iStackPos + 1, g_strTabs, i, pArea->m_pt[i].x, pArea->m_pt[i].y);
            }
        }
        else
        {
            Vec2 pt;
            int i;
            sscanf_s(str, "%d (%g %g", &i, &pt.x, &pt.y);
            if (!pArea->m_pt)
            {
                pArea->m_pt = new Vec2[pArea->m_npt + 1];
            }
            pArea->m_pt[i] = pt;
            if (i == pArea->m_npt - 1)
            {
                pArea->ProcessBorder();
            }
        }
        return ctx.bSaving;
    }
    int SerializePt3d(parse_context& ctx, char* str, size_t, const char* tag, int npt, Vec3*& pt3d)
    {
        if (ctx.bSaving)
        {
            for (int i = 0; i < npt; i++)
            {
                fprintf(ctx.f, "%.*s%s %d (%.8g %.8g %.8g)\n", ctx.iStackPos + 1, g_strTabs, tag, i, pt3d[i].x, pt3d[i].y, pt3d[i].z);
            }
        }
        else
        {
            Vec3 pt;
            int i;
            sscanf_s(str, "%d (%g %g %g", &i, &pt.x, &pt.y, &pt.z);
            if (!pt3d)
            {
                pt3d = new Vec3[npt];
            }
            pt3d[i] = pt;
        }
        return ctx.bSaving;
    }
    int SerializeSpline(parse_context& ctx, char* str, size_t strSize)
    {
        CPhysArea* pArea = (CPhysArea*)ctx.pobj;
        return SerializePt3d(ctx, str, strSize, "pt3d", pArea->m_ptSpline || !ctx.bSaving ? pArea->m_npt : 0, pArea->m_ptSpline);
    }
    int SerializeFlows(parse_context& ctx, char* str, size_t strSize)
    {
        CPhysArea* pArea = (CPhysArea*)ctx.pobj;
        return SerializePt3d(ctx, str, strSize, "flow", pArea->m_pFlows || !ctx.bSaving ? pArea->m_npt : 0, pArea->m_pFlows);
    }
    int Finalize(parse_context& ctx, char* str, size_t)
    {
        CPhysArea* pArea = (CPhysArea*)ctx.pobj;
        if (!ctx.bSaving)
        {
            pArea->m_rscale = 1.0f / pArea->m_scale;
            pArea->m_rfalloff0 = pArea->m_falloff0 < 1.0f ? 1.0f / (1 - pArea->m_falloff0) : 0;
            pArea->m_next = ctx.pWorld->m_pGlobalArea->m_next;
            ctx.pWorld->m_pGlobalArea->m_next = pArea;
            ctx.pWorld->RepositionArea(pArea);
            pArea->SetParams(&pArea->m_pb);
            if (max(pArea->m_V, pArea->m_szCell) > 0.0f)
            {
                ctx.pWorld->ActivateArea(pArea);
            }
            ctx.pWorld->m_numNonWaterAreas += IsAreaNonWater(pArea);
            ctx.pWorld->m_numGravityAreas += IsAreaGravity(pArea);
        }
        return 0;
    }
};


struct CEntityGridSerializer
    : Serializer
{
    CEntityGridSerializer()
    {
        CPhysicalWorld trg(0);
#if !defined(GRID_AXIS_STANDARD)
        DECLARE_MEMBER("iAxisZ", ft_int, m_iEntAxisz)
#else
        DECLARE_PROC("iAxisZ", &CEntityGridSerializer::SerializeAxisZ)
#endif
        DECLARE_MEMBER("CellsX", ft_int, m_entgrid.size.x)
        DECLARE_MEMBER("CellsY", ft_int, m_entgrid.size.y)
        DECLARE_MEMBER("StepX", ft_float, m_entgrid.step.x)
        DECLARE_MEMBER("StepY", ft_float, m_entgrid.step.y)
        DECLARE_MEMBER("Origin", ft_vector, m_entgrid.origin)
        DECLARE_PROC("end", &CEntityGridSerializer::Finalize)
    }
    int SerializeAxisZ(parse_context& ctx, char* str, size_t strSize)
    {
        if (ctx.bSaving)
        {
            sprintf_s(str, strSize, "2");
        }
        return 0;
    }

    int Finalize(parse_context& ctx, char* str, size_t)
    {
        CPhysicalWorld* pWorld = (CPhysicalWorld*)ctx.pobj;
        if (!ctx.bSaving)
        {
            pWorld->SetupEntityGrid(pWorld->m_iEntAxisz, pWorld->m_entgrid.origin, pWorld->m_entgrid.size.x,
                pWorld->m_entgrid.size.y, pWorld->m_entgrid.step.x, pWorld->m_entgrid.step.y);
        }
        return 0;
    }
};


struct CPhysVarsSerializer
    : Serializer
{
    CPhysVarsSerializer()
    {
        PhysicsVars trg;
        DECLARE_MEMBER("nMaxStackSizeMC", ft_int, nMaxStackSizeMC)
        DECLARE_MEMBER("maxMassRatioMC", ft_float, maxMassRatioMC)
        DECLARE_MEMBER("nMaxMCiters", ft_int, nMaxMCiters)
        DECLARE_MEMBER("nMaxMCitersHopeless", ft_int, nMaxMCitersHopeless)
        DECLARE_MEMBER("accuracyMC", ft_float, accuracyMC)
        DECLARE_MEMBER("accuracyLCPCG", ft_float, accuracyLCPCG)
        DECLARE_MEMBER("nMaxContacts", ft_int, nMaxContacts)
        DECLARE_MEMBER("nMaxPlaneContacts", ft_int, nMaxPlaneContacts)
        DECLARE_MEMBER("nMaxPlaneContactsDistress", ft_int, nMaxPlaneContactsDistress)
        DECLARE_MEMBER("nMaxLCPCGsubiters", ft_int, nMaxLCPCGsubiters)
        DECLARE_MEMBER("nMaxLCPCGsubitersFinal", ft_int, nMaxLCPCGsubitersFinal)
        DECLARE_MEMBER("nMaxLCPCGmicroiters", ft_int, nMaxLCPCGmicroiters)
        DECLARE_MEMBER("nMaxLCPCGmicroitersFinal", ft_int, nMaxLCPCGmicroitersFinal)
        DECLARE_MEMBER("nMaxLCPCGiters", ft_int, nMaxLCPCGiters)
        DECLARE_MEMBER("minLCPCGimprovement", ft_float, minLCPCGimprovement)
        DECLARE_MEMBER("nMaxLCPCGFruitlessIters", ft_int, nMaxLCPCGFruitlessIters)
        DECLARE_MEMBER("accuracyLCPCGnoimprovement", ft_float, accuracyLCPCGnoimprovement)
        DECLARE_MEMBER("minSeparationSpeed", ft_float, minSeparationSpeed)
        DECLARE_MEMBER("maxvCG", ft_float, maxvCG)
        DECLARE_MEMBER("maxwCG", ft_float, maxwCG)
        DECLARE_MEMBER("maxvUnproj", ft_float, maxvUnproj)
        DECLARE_MEMBER("bFlyMode", ft_int, bFlyMode)
        DECLARE_MEMBER("iCollisionMode", ft_int, iCollisionMode)
        DECLARE_MEMBER("bSingleStepMode", ft_int, bSingleStepMode)
        DECLARE_MEMBER("bDoStep", ft_int, bDoStep)
        DECLARE_MEMBER("fixedTimestep", ft_float, fixedTimestep)
        DECLARE_MEMBER("timeGranularity", ft_float, timeGranularity)
        DECLARE_MEMBER("iDrawHelpers", ft_int, iDrawHelpers)
        DECLARE_MEMBER("maxContactGap", ft_float, maxContactGap)
        DECLARE_MEMBER("maxContactGapPlayer", ft_float, maxContactGapPlayer)
        DECLARE_MEMBER("minBounceSpeed", ft_float, minBounceSpeed)
        DECLARE_MEMBER("bProhibitUnprojection", ft_int, bProhibitUnprojection)
        DECLARE_MEMBER("bUseDistanceContacts", ft_int, bUseDistanceContacts)
        DECLARE_MEMBER("unprojVelScale", ft_float, unprojVelScale)
        DECLARE_MEMBER("maxUnprojVel", ft_float, maxUnprojVel)
        DECLARE_MEMBER("bEnforceContacts", ft_int, bEnforceContacts)
        DECLARE_MEMBER("nMaxSubsteps", ft_int, nMaxSubsteps)
        DECLARE_MEMBER("nMaxSurfaces", ft_int, nMaxSurfaces)
        DECLARE_MEMBER("gravity", ft_vector, gravity)
        DECLARE_MEMBER("nGroupDamping", ft_int, nGroupDamping)
        DECLARE_MEMBER("groupDamping", ft_float, groupDamping)
        DECLARE_MEMBER("bBreakOnValidation", ft_int, bBreakOnValidation)
        DECLARE_MEMBER("bLogActiveObjects", ft_int, bLogActiveObjects)
        DECLARE_MEMBER("bMultiplayer", ft_int, bMultiplayer)
        DECLARE_MEMBER("bProfileEntities", ft_int, bProfileEntities)
        DECLARE_MEMBER("nGEBMaxCells", ft_int, nGEBMaxCells)
        DECLARE_MEMBER("maxVel", ft_float, maxVel)
        DECLARE_MEMBER("maxVelPlayers", ft_float, maxVelPlayers)
        DECLARE_MEMBER("maxContactGapSimple", ft_float, maxContactGapSimple)
        DECLARE_MEMBER("penaltyScale", ft_float, penaltyScale)
        DECLARE_MEMBER("bSkipRedundantColldet", ft_int, bSkipRedundantColldet)
        DECLARE_MEMBER("bLimitSimpleSolverEnergy", ft_int, bLimitSimpleSolverEnergy)
        DECLARE_MEMBER("numThreads", ft_int, numThreads)
    }
};

struct CPhysicalWorldSerializer
    : Serializer
{
    CPhysVarsSerializer* pPhysVarsSerializer;
    CEntityGridSerializer* pEntityGridSerializer;
    CPhysicalEntitySerializer* pStaticEntitySerializer;
    CRigidEntitySerializer* pRigidEntitySerializer;
    CWheeledVehicleEntitySerializer* pWheeledVehicleEntitySerializer;
    CArticulatedEntitySerializer* pArticulatedEntitySerializer;
    CLivingEntitySerializer* pLivingEntitySerializer;
    CRopeEntitySerializer* pRopeEntitySerializer;
    CSoftEntitySerializer* pSoftEntitySerializer;
    CAreaSerializer* pAreaSerializer;

    CPhysicalWorldSerializer(CPhysVarsSerializer* pvs, CEntityGridSerializer* egs, CPhysicalEntitySerializer* pes, CRigidEntitySerializer* res,
        CWheeledVehicleEntitySerializer* wves, CArticulatedEntitySerializer* aes, CLivingEntitySerializer* les, CRopeEntitySerializer* rpes, CSoftEntitySerializer* spes, CAreaSerializer* as)
        : pPhysVarsSerializer(pvs)
        , pEntityGridSerializer(egs)
        , pStaticEntitySerializer(pes)
        , pRigidEntitySerializer(res)
        , pWheeledVehicleEntitySerializer(wves)
        , pArticulatedEntitySerializer(aes)
        , pLivingEntitySerializer(les)
        , pRopeEntitySerializer(rpes)
        , pSoftEntitySerializer(spes)
        , pAreaSerializer(as)
    {
        CPhysicalWorld trg(0);
        DECLARE_PROC("PhysicsVars", &CPhysicalWorldSerializer::SerializeVars)
        DECLARE_PROC("Material", &CPhysicalWorldSerializer::SerializeMaterial)
        DECLARE_PROC("EntityGrid", &CPhysicalWorldSerializer::SerializeEntityGrid)
        DECLARE_PROC("Entity", &CPhysicalWorldSerializer::SerializeEntities)
        DECLARE_PROC("Area", &CPhysicalWorldSerializer::SerializeAreas)
    }

    int SerializeVars(parse_context& ctx, char* str, size_t)
    {
        CPhysicalWorld* pWorld = (CPhysicalWorld*)ctx.pobj;
        if (ctx.bSaving)
        {
            fprintf(ctx.f, "%.*sPhysicsVars\n", ctx.iStackPos + 1, g_strTabs);
        }
        ctx.PushState();
        ctx.pobj = &pWorld->m_vars;
        (ctx.pSerializer = pPhysVarsSerializer)->Serialize(ctx);
        return ctx.bSaving;
    }

    int SerializeEntityGrid(parse_context& ctx, char* str, size_t)
    {
        CPhysicalWorld* pWorld = (CPhysicalWorld*)ctx.pobj;
        if (ctx.bSaving)
        {
            fprintf(ctx.f, "%.*sEntityGrid\n", ctx.iStackPos + 1, g_strTabs);
        }
        ctx.PushState();
        ctx.pobj = pWorld;
        (ctx.pSerializer = pEntityGridSerializer)->Serialize(ctx);
        return ctx.bSaving;
    }

    int SerializeMaterial(parse_context& ctx, char* str, size_t)
    {
        CPhysicalWorld* pWorld = (CPhysicalWorld*)ctx.pobj;
        int i;
        if (ctx.bSaving)
        {
            for (i = 0; i < NSURFACETYPES; i++)
            {
                if (pWorld->m_BouncinessTable[i] != pWorld->m_BouncinessTable[NSURFACETYPES - 1] ||
                    pWorld->m_FrictionTable[i] != pWorld->m_FrictionTable[NSURFACETYPES - 1] ||
                    pWorld->m_DynFrictionTable[i] != pWorld->m_DynFrictionTable[NSURFACETYPES - 1] ||
                    pWorld->m_SurfaceFlagsTable[i] != pWorld->m_SurfaceFlagsTable[NSURFACETYPES - 1])
                {
                    fprintf(ctx.f, "%.*sMaterial %d  %.8g %.8g %.8g %u\n", ctx.iStackPos + 1, g_strTabs, i, pWorld->m_BouncinessTable[i],
                        pWorld->m_FrictionTable[i], pWorld->m_DynFrictionTable[i], pWorld->m_SurfaceFlagsTable[i]);
                }
            }
        }
        else
        {
            float bounciness, friction, dynFriction;
            unsigned int flags;
            sscanf_s(str, "%d %g %g %g %X", &i, &bounciness, &friction, &dynFriction, &flags);
            i &= NSURFACETYPES - 1;
            pWorld->m_BouncinessTable[i] = bounciness;
            pWorld->m_DynFrictionTable[i] = dynFriction;
            pWorld->m_FrictionTable[i] = friction;
            pWorld->m_SurfaceFlagsTable[i] = flags;
        }
        return ctx.bSaving;
    }

    int SerializeEntities(parse_context& ctx, char* str, size_t)
    {
        CPhysicalWorld* pWorld = (CPhysicalWorld*)ctx.pobj;
        CPhysicalEntity* pent;
        int i;

        if (ctx.bSaving)
        {
            for (i = 0; i < 5; i++)
            {
                for (pent = pWorld->m_pTypedEnts[i]; pent && pent->m_next; pent = pent->m_next)
                {
                    ;
                }
                for (; pent; pent = pent->m_prev)
                {
                    char* name = "Static";
                    Serializer* pSerializer = pStaticEntitySerializer;
                    switch (pent->GetType())
                    {
                    case PE_RIGID:
                        pSerializer = pRigidEntitySerializer;
                        name = "Rigid Body";
                        break;
                    case PE_WHEELEDVEHICLE:
                        pSerializer = pWheeledVehicleEntitySerializer;
                        name = "Wheeled Vehicle";
                        break;
                    case PE_ARTICULATED:
                        pSerializer = pArticulatedEntitySerializer;
                        name = "Articulated";
                        break;
                    case PE_LIVING:
                        pSerializer = pLivingEntitySerializer;
                        name = "Living";
                        break;
                    case PE_ROPE:
                        pSerializer = pRopeEntitySerializer;
                        name = "Rope";
                        break;
                    case PE_SOFT:
                        pSerializer = pSoftEntitySerializer;
                        name = "Soft";
                        break;
                    }
                    fprintf(ctx.f, "%.*sEntity (type %d : %s) %s\n", ctx.iStackPos + 1, g_strTabs, pent->GetType(), name,
                        pWorld->m_pRenderer ?
                        pWorld->m_pRenderer->GetForeignName(pent->m_pForeignData, pent->m_iForeignData, pent->m_iForeignFlags) : "");
                    ctx.PushState();
                    ctx.pobj = pent;
                    (ctx.pSerializer = pSerializer)->Serialize(ctx);
                }
            }
        }
        else
        {
            ctx.PushState();
            if (++pWorld->m_nEnts > pWorld->m_nEntsAlloc - 1)
            {
                pWorld->m_nEntsAlloc += 512;
                ReallocateList(pWorld->m_pTmpEntList, pWorld->m_nEntsAlloc - 512, pWorld->m_nEntsAlloc);
                ReallocateList(pWorld->m_pTmpEntList1, pWorld->m_nEntsAlloc - 512, pWorld->m_nEntsAlloc);
                ReallocateList(pWorld->m_pTmpEntList2, pWorld->m_nEntsAlloc - 512, pWorld->m_nEntsAlloc);
                ReallocateList(pWorld->m_pGroupMass, 0, pWorld->m_nEntsAlloc);
                ReallocateList(pWorld->m_pMassList, 0, pWorld->m_nEntsAlloc);
                ReallocateList(pWorld->m_pGroupIds, 0, pWorld->m_nEntsAlloc);
                ReallocateList(pWorld->m_pGroupNums, 0, pWorld->m_nEntsAlloc);
            }
            sscanf_s(str, "(type %d", &i);
            switch (i)
            {
            case PE_STATIC:
                pent = CPhysicalEntity::Create<CPhysicalEntity>(pWorld, 0);
                ctx.pSerializer = pStaticEntitySerializer;
                break;
            case PE_RIGID:
                pent = CPhysicalEntity::Create<CRigidEntity>(pWorld, 0);
                ctx.pSerializer = pRigidEntitySerializer;
                break;
            case PE_LIVING:
                pent = CPhysicalEntity::Create<CLivingEntity>(pWorld, 0);
                ctx.pSerializer = pLivingEntitySerializer;
                break;
            case PE_WHEELEDVEHICLE:
                pent = CPhysicalEntity::Create<CWheeledVehicleEntity>(pWorld, 0);
                ctx.pSerializer = pWheeledVehicleEntitySerializer;
                break;
            case PE_PARTICLE:
                pent = CPhysicalEntity::Create<CParticleEntity>(pWorld, 0);
                break;
            case PE_ARTICULATED:
                pent = CPhysicalEntity::Create<CArticulatedEntity>(pWorld, 0);
                ctx.pSerializer = pArticulatedEntitySerializer;
                break;
            case PE_ROPE:
                pent = CPhysicalEntity::Create<CRopeEntity>(pWorld, 0);
                ctx.pSerializer = pRopeEntitySerializer;
                break;
            case PE_SOFT:
                pent = CPhysicalEntity::Create<CSoftEntity>(pWorld, 0);
                ctx.pSerializer = pSoftEntitySerializer;
                break;
            }
            ctx.pobj = pent;
            ctx.pSerializer->Serialize(ctx);
        }
        return ctx.bSaving;
    }

    int SerializeAreas(parse_context& ctx, char* str, size_t)
    {
        CPhysicalWorld* pWorld = (CPhysicalWorld*)ctx.pobj;
        if (ctx.bSaving)
        {
            for (CPhysArea* pArea = pWorld->m_pGlobalArea; pArea; pArea = pArea->m_next)
            {
                fprintf(ctx.f, "%.*sArea %s\n", ctx.iStackPos + 1, g_strTabs, pWorld->m_pRenderer ? pWorld->m_pRenderer->GetForeignName(pArea->m_pForeignData, pArea->m_iForeignData, pArea->m_iForeignFlags) : "");
                ctx.PushState();
                ctx.pobj = pArea;
                (ctx.pSerializer = pAreaSerializer)->Serialize(ctx);
            }
        }
        else
        {
            ctx.PushState();
            if (!pWorld->m_pGlobalArea)
            {
                pWorld->m_pGlobalArea = new CPhysArea(pWorld);
                pWorld->m_pGlobalArea->m_gravity = pWorld->m_vars.gravity;
            }
            CPhysArea* pArea = new CPhysArea(pWorld);
            ctx.pobj = pArea;
            (ctx.pSerializer = pAreaSerializer)->Serialize(ctx);
        }
        return ctx.bSaving;
    }
};


void SerializeGeometries(CPhysicalWorld* pWorld, const char* fname, int bSave)
{
    CMeshSerializer ms;
    CHeightfieldSerializer hfs;
    CBoxSerializer bs;
    CCylinderSerializer cs;
    CSphereSerializer ss;
    CPhysGeometrySerializer pgs(&ms, &hfs, &bs, &cs, &ss);
    CExplosionShapeSerializer ess(&ms);
    CCrackSerializer crs(&ms);
    CGeomanSerializer gms(&pgs, &ess, &crs);

    char str[128];
    CHeightfield hf;
    parse_context ctx;
    fopen_s(&ctx.f, fname, bSave ? "wt" : "rt");
    if (!ctx.f)
    {
        return;
    }
    ctx.pWorld = pWorld;

    if (ctx.bSaving = bSave)
    {
        fputs("Terrain\n", ctx.f);
        ctx.pobj = pWorld->m_pHeightfield[0] ? pWorld->m_pHeightfield[0]->m_parts[0].pPhysGeom->pGeom : 0;
    }
    else
    {
        fgets(str, sizeof(str), ctx.f);
        ctx.pobj = &hf;
    }
    ctx.iStackPos =  0;
    g_hfData = 0;
    if (ctx.pobj)
    {
        (ctx.pSerializer = &hfs)->Serialize(ctx);
    }
    else
    {
        fputs("end\n", ctx.f);
    }
    if (!bSave)
    {
        if (g_hfData)
        {
            pWorld->SetHeightfieldData(&hf.m_hf, g_hfMats, g_nHfMats);
        }
        else
        {
            pWorld->SetHeightfieldData(0);
        }
    }

    if (ctx.bSaving = bSave)
    {
        fputs("Registered Geomeries\n", ctx.f);
    }
    else
    {
        fgets(str, sizeof(str), ctx.f);
    }
    ctx.iStackPos =  0;
    ctx.pobj = pWorld;
    (ctx.pSerializer = &gms)->Serialize(ctx);
    fclose(ctx.f);
}


void SerializeWorld(CPhysicalWorld* pWorld, const char* fname, int bSave)
{
    CPhysicalPlaceholderSerializer pps;
    CTetrahedronSerializer ts;
    CTetrLatticeSerializer tls(&ts);
    CPhysicalEntityPartSerializer peps(&tls);
    CPhysicalJointSerializerRead jsr;
    CPhysicalJointSerializerWrite jsw;
    CPhysicalEntitySerializer pes(&peps, &jsr, &jsw);
    CRigidBodySerializer rbs;
    CConstraintSerializer cs;
    CRigidEntitySerializer res(&peps, &jsr, &jsw, &rbs, &cs);
    CSuspSerializer ss;
    CWheeledVehicleEntitySerializer wves(&peps, &jsr, &jsw, &rbs, &cs, &ss);
    CAEPartInfoSerializer aepis;
    CJointSerializer js(&rbs);
    CArticulatedEntitySerializer aes(&peps, &jsr, &jsw, &rbs, &cs, &js, &aepis);
    CLivingEntitySerializer les(&peps);
    CRopeSegSerializer rss;
    CRopeVtxSerializer rvs;
    CRopeEntitySerializer rpes(&peps, &rvs, &rss);
    CSoftVtxSerializer svs;
    CSoftEdgeSerializer ses;
    CSoftEntitySerializer spes(&peps, &svs, &ses);
    CPhysVarsSerializer pvs;
    CEntityGridSerializer egs;
    CMeshSerializer ms;
    CHeightfieldSerializer hfs;
    CBoxSerializer bs;
    CCylinderSerializer cls;
    CSphereSerializer sps;
    CPhysGeometrySerializer pgs(&ms, &hfs, &bs, &cls, &sps);
    CAreaSerializer as(&pgs);
    CPhysicalWorldSerializer pws(&pvs, &egs, &pes, &res, &wves, &aes, &les, &rpes, &spes, &as);

    char str[128];
    parse_context ctx;
    ctx.bSaving = bSave;
    fopen_s(&ctx.f, fname, bSave ? "wt" : "rt");
    if (!ctx.f)
    {
        return;
    }
    if (ctx.bSaving = bSave)
    {
        fputs("World\n", ctx.f);
    }
    else
    {
        fgets(str, sizeof(str), ctx.f);
    }
    ctx.pWorld = pWorld;
    ctx.iStackPos =  0;
    ctx.pobj = pWorld;
    (ctx.pSerializer = &pws)->Serialize(ctx);
    fclose(ctx.f);
}

#endif
