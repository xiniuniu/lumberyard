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
#include "RefPicture.h"
#include "Geometry/EdMesh.h"
#include "../Material/MaterialManager.h"
#include "Viewport.h"

#define REFERENCE_PICTURE_MATERAIL_TEMPLATE "Editor/Materials/refpicture.mtl"
#define REFERENCE_PICTURE_BASE_OBJECT "Editor/Objects/refpicture.cgf"

#define DISPLAY_2D_SELECT_LINEWIDTH 2
#define DISPLAY_2D_SELECT_COLOR QColor(225, 0, 0)
#define DISPLAY_GEOM_SELECT_COLOR ColorB(250, 0, 250, 30)
#define DISPLAY_GEOM_SELECT_LINE_COLOR ColorB(255, 255, 0, 160)

_smart_ptr<CMaterial> g_RefPictureMaterialTemplate = 0;

//-----------------------------------------------------------------------------
_smart_ptr<IMaterial> GetRefPictureMaterial()
{
    if (!g_RefPictureMaterialTemplate)
    {
        QString filename = Path::MakeGamePath(REFERENCE_PICTURE_MATERAIL_TEMPLATE);

        XmlNodeRef mtlNode = GetISystem()->LoadXmlFromFile(filename.toUtf8().data());
        if (mtlNode)
        {
            // Create a template material without registering DB.
            g_RefPictureMaterialTemplate = GetIEditor()->GetMaterialManager()->CreateMaterial("RefPicture", mtlNode, MTL_FLAG_UIMATERIAL);

            // Create MatInfo by calling it here for the first time
            g_RefPictureMaterialTemplate->GetMatInfo();
            g_RefPictureMaterialTemplate->Release();
        }
    }

   _smart_ptr<IMaterial> pMat = GetIEditor()->Get3DEngine()->GetMaterialManager()->CloneMaterial(g_RefPictureMaterialTemplate->GetMatInfo());

    return pMat;
}

//-----------------------------------------------------------------------------
CRefPicture::CRefPicture()
    : m_pMaterial(0)
    , m_pRenderNode(0)
    , m_pGeometry(0)
    , m_aspectRatio(1.f)
{
    mv_scale.Set(1.0f);
    mv_scale.SetLimits(0.001f, 100.f);

    AddVariable(mv_picFilepath, "File", functor(*this, &CRefPicture::OnVariableChanged), IVariable::DT_FILE);
    AddVariable(mv_scale, "Scale", functor(*this, &CRefPicture::OnVariableChanged));

    m_pMaterial = GetRefPictureMaterial();
}

//-----------------------------------------------------------------------------
void CRefPicture::Display(DisplayContext& dc)
{
    if (dc.flags & DISPLAY_2D)
    {
        if (IsSelected())
        {
            dc.SetLineWidth(DISPLAY_2D_SELECT_LINEWIDTH);
            dc.SetColor(DISPLAY_2D_SELECT_COLOR);
        }
        else
        {
            dc.SetColor(GetColor());
        }

        dc.PushMatrix(GetWorldTM());
        dc.DrawWireBox(m_bbox.min, m_bbox.max);

        dc.PopMatrix();

        if (IsSelected())
        {
            dc.SetLineWidth(0);
        }

        if (m_pGeometry)
        {
            m_pGeometry->Display(dc);
        }
        return;
    }

    if (m_pGeometry)
    {
        if (!IsHighlighted())
        {
            m_pGeometry->Display(dc);
        }
        else if (gSettings.viewports.bHighlightMouseOverGeometry)
        {
            SGeometryDebugDrawInfo debugDrawInfo;
            debugDrawInfo.tm = GetWorldTM();
            debugDrawInfo.color = DISPLAY_GEOM_SELECT_COLOR;
            debugDrawInfo.lineColor = DISPLAY_GEOM_SELECT_LINE_COLOR;
            debugDrawInfo.bExtrude = true;
            m_pGeometry->DebugDraw(debugDrawInfo);
        }
    }

    DrawDefault(dc);
}

//-----------------------------------------------------------------------------
bool CRefPicture::HitTest(HitContext& hc)
{
    Vec3 localHit;
    Vec3 localRaySrc = hc.raySrc;
    Vec3 localRayDir = hc.rayDir;

    localRaySrc = m_invertTM.TransformPoint(localRaySrc);
    localRayDir = m_invertTM.TransformVector(localRayDir).GetNormalized();

    if (Intersect::Ray_AABB(localRaySrc, localRayDir, m_bbox, localHit))
    {
        if (hc.b2DViewport)
        {
            // World space distance.
            hc.dist = hc.raySrc.GetDistance(GetWorldTM().TransformPoint(localHit));
            return true;
        }


        if (m_pRenderNode)
        {
            IStatObj* pStatObj = m_pRenderNode->GetEntityStatObj(0);
            if (pStatObj)
            {
                SRayHitInfo hi;
                hi.inReferencePoint = localRaySrc;
                hi.inRay = Ray(localRaySrc, localRayDir);
                if (pStatObj->RayIntersection(hi))
                {
                    hc.dist = hc.raySrc.GetDistance(GetWorldTM().TransformPoint(hi.vHitPos));
                    return true;
                }
                return false;
            }
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
void CRefPicture::GetLocalBounds(AABB& box)
{
    box = m_bbox;
}

int CRefPicture::MouseCreateCallback(CViewport* view, EMouseEvent event, QPoint& point, int flags)
{
    if (event == eMouseMove || event == eMouseLDown)
    {
        Vec3 pos;
        if (GetIEditor()->GetAxisConstrains() != AXIS_TERRAIN)
        {
            pos = view->MapViewToCP(point);
        }
        else
        {
            // Snap to terrain.
            bool hitTerrain;
            pos = view->ViewToWorld(point, &hitTerrain);
            if (hitTerrain)
            {
                pos.z = GetIEditor()->GetTerrainElevation(pos.x, pos.y);
            }
        }

        pos = view->SnapToGrid(pos);
        SetPos(pos);
        if (event == eMouseLDown)
        {
            return MOUSECREATE_OK;
        }
        return MOUSECREATE_CONTINUE;
    }
    return CBaseObject::MouseCreateCallback(view, event, point, flags);
}


//-----------------------------------------------------------------------------
XmlNodeRef CRefPicture::Export(const QString& levelPath, XmlNodeRef& xmlNode)
{
    // Don't export this. Only relevant for editor.
    return 0;
}

//-----------------------------------------------------------------------------
bool CRefPicture::CreateGameObject()
{
    // Load geometry
    if (!m_pGeometry)
    {
        m_pGeometry = CEdMesh::LoadMesh(REFERENCE_PICTURE_BASE_OBJECT);
        assert(m_pGeometry);

        if (!m_pGeometry)
        {
            return false;
        }
    }

    // Create render node
    if (!m_pRenderNode)
    {
        m_pRenderNode = GetIEditor()->Get3DEngine()->CreateRenderNode(eERType_Brush);

        if (m_pGeometry)
        {
            m_pGeometry->GetBounds(m_bbox);
            Matrix34A tm = GetWorldTM();
            m_pRenderNode->SetEntityStatObj(0, m_pGeometry->GetIStatObj(), &tm);
            if (m_pGeometry->GetIStatObj())
            {
                m_pGeometry->GetIStatObj()->SetMaterial(m_pMaterial);
            }
        }
    }

    // Apply material & reference picture image

    UpdateImage(mv_picFilepath);

    return true;
}

//-----------------------------------------------------------------------------
void CRefPicture::Done()
{
    if (m_pRenderNode)
    {
        GetIEditor()->Get3DEngine()->DeleteRenderNode(m_pRenderNode);
        m_pRenderNode = 0;
    }

    if (m_pGeometry)
    {
        m_pGeometry = 0;
    }

    if (m_pMaterial)
    {
        m_pMaterial = 0;
    }

    CBaseObject::Done();
}

//-----------------------------------------------------------------------------
void CRefPicture::InvalidateTM(int nWhyFlags)
{
    CBaseObject::InvalidateTM(nWhyFlags);

    if (!(nWhyFlags & eObjectUpdateFlags_RestoreUndo)) // Can skip updating game object when restoring undo.
    {
        if (m_pRenderNode && m_pGeometry)
        {
            Matrix34A tm = GetWorldTM();
            m_pRenderNode->SetEntityStatObj(0, m_pGeometry->GetIStatObj(), &tm);
        }
    }

    m_invertTM = GetWorldTM();
    m_invertTM.Invert();
}

//-----------------------------------------------------------------------------
void CRefPicture::OnVariableChanged(IVariable*  piVariable)
{
    if (!QString::compare(piVariable->GetName(), "File"))
    {
        UpdateImage(mv_picFilepath);
    }
    else if (!QString::compare(piVariable->GetName(), "Scale"))
    {
        ApplyScale((float)mv_scale);
    }
}

//-----------------------------------------------------------------------------
void CRefPicture::UpdateImage(const QString& picturePath)
{
    if (!m_pMaterial || !m_pRenderNode)
    {
        return;
    }

    SShaderItem&            si(m_pMaterial->GetShaderItem());
    SInputShaderResources   isr(si.m_pShaderResources);

    // The following will create the diffuse slot if did not exist
    isr.m_TexturesResourcesMap[EFTT_DIFFUSE].m_Name = picturePath.toUtf8().data();

    SShaderItem siDst(GetIEditor()->GetRenderer()->EF_LoadShaderItem(si.m_pShader->GetName(), true, 0, &isr, si.m_pShader->GetGenerationMask()));
    m_pMaterial->AssignShaderItem(siDst);

    m_pRenderNode->SetMaterial(m_pMaterial);

    CImageEx image;
    if (CImageUtil::LoadImage(picturePath, image))
    {
        float w = (float)image.GetWidth();
        float h = (float)image.GetHeight();

        m_aspectRatio = (h != 0.f ? w / h : 1.f);
    }
    else
    {
        m_aspectRatio = 1.f;
    }

    ApplyScale((float)mv_scale);
}

//-----------------------------------------------------------------------------
void CRefPicture::ApplyScale(float fScale)
{
    // RefPicture is defined in the YZ plane, therefore ignore any scaling on X.
    float scaleX = 1.0f;
    float scaleY = m_aspectRatio * fScale;
    float scaleZ = fScale;

    CBaseObject::SetScale(Vec3(scaleX, scaleY, scaleZ));
}

#include <Objects/RefPicture.moc>