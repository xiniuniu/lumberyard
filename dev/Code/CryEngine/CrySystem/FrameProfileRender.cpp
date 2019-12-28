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

// Description : Rendering of FrameProfiling information.


#include "StdAfx.h"
#include "FrameProfileSystem.h"
#include <IRenderer.h>
#include <ILog.h>
#include <IRenderAuxGeom.h>
#include <IScriptSystem.h>
#include "ITextModeConsole.h"

#include <CryFile.h>

#include "Statistics.h"

#ifdef WIN32
#include <psapi.h>
LINK_SYSTEM_LIBRARY(psapi.lib)
#endif

#ifdef LINUX
#include <dlfcn.h>
#endif

#ifdef USE_FRAME_PROFILER

//! Time is in milliseconds.
#define PROFILER_MIN_DISPLAY_TIME 0.01f

#define VARIANCE_MULTIPLIER 2.0f

//! 5 seconds from hot to cold in peaks.
#define MAX_DISPLAY_ROWS 80

extern int CryMemoryGetAllocatedSize();
extern int CryMemoryGetPoolSize();

#define COL_DIFF_HISTOGRAMS 50

namespace FrameProfileRenderConstants
{
    const float c_yScale = 1.3f;
    const int c_yStepSizeText = (int)(10.f * c_yScale);
    const int c_yStepSizeTextMeter = (int)(8.f * c_yScale);
    const float c_yNextControlGap = 12.f * c_yScale;
    const float c_fontScale =  1.0f * c_yScale;
}


//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::DrawLabel(float col, float row, float* fColor, float glow, const char* szText, float fScale)
{
    float ColumnSize = COL_SIZE;
    float RowSize = ROW_SIZE;

    SDrawTextInfo ti;
    ti.xscale = ti.yscale = fScale * 1.2f;
    ti.flags = eDrawText_2D | eDrawText_FixedSize | eDrawText_Monospace;
    ti.color[0] = fColor[0];
    ti.color[1] = fColor[1];
    ti.color[2] = fColor[2];
    ti.color[3] = fColor[3];

    if (glow > 0.1f)
    {
        ti.color[0] = fColor[0];
        ti.color[1] = fColor[1];
        ti.color[2] = fColor[2];
        ti.color[3] = glow;
        m_pRenderer->DrawTextQueued(Vec3(ColumnSize * col + 1, m_baseY + RowSize * row + 1 - m_offset, 0.5f), ti, szText);
    }
    ti.color[0] = fColor[0];
    ti.color[1] = fColor[1];
    ti.color[2] = fColor[2];
    ti.color[3] = fColor[3];
    m_pRenderer->DrawTextQueued(Vec3(ColumnSize * col + 1, m_baseY + RowSize * row + 1 - m_offset, 0.5f), ti, szText);

    if (ITextModeConsole* pTC = gEnv->pSystem->GetITextModeConsole())
    {
        pTC->PutText((int)col, (int)(m_textModeBaseExtra + row + std::max(0.0f, (m_baseY - 120.0f) / ROW_SIZE)), szText);
    }
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::DrawRect(float x1, float y1, float x2, float y2, float* fColor)
{
    m_pRenderer->SetMaterialColor(fColor[0], fColor[1], fColor[2], fColor[3]);
    m_pRenderer->SetMaterialColor(fColor[0], fColor[1], fColor[2], fColor[3]);
    int w = m_pRenderer->GetWidth();
    int h = m_pRenderer->GetHeight();

    y1 -= m_offset;
    y2 -= m_offset;
    //float dx = 800.0f/w;
    //float dy = 600.0f/h;
    float dx = 1.0f / w;
    float dy = 1.0f / h;
    x1 *= dx;
    x2 *= dx;
    y1 *= dy;
    y2 *= dy;

    ColorB col((uint8)(fColor[0] * 255.0f), (uint8)(fColor[1] * 255.0f), (uint8)(fColor[2] * 255.0f), (uint8)(fColor[3] * 255.0f));

    IRenderAuxGeom* pAux = m_pRenderer->GetIRenderAuxGeom();
    SAuxGeomRenderFlags flags = pAux->GetRenderFlags();
    flags.SetMode2D3DFlag(e_Mode2D);
    pAux->SetRenderFlags(flags);
    pAux->DrawLine(Vec3(x1, y1, 0), col, Vec3(x2, y1, 0), col);
    pAux->DrawLine(Vec3(x1, y2, 0), col, Vec3(x2, y2, 0), col);
    pAux->DrawLine(Vec3(x1, y1, 0), col, Vec3(x1, y2, 0), col);
    pAux->DrawLine(Vec3(x2, y1, 0), col, Vec3(x2, y2, 0), col);
}

//////////////////////////////////////////////////////////////////////////
inline float CalculateVarianceFactor(float value, float variance)
{
    //variance = fabs(variance - value*value);
    float difference = (float)sqrt_tpl(variance);

    const float VALUE_EPSILON = 0.000001f;
    value = (float)fabs(value);
    // Prevent division by zero.
    if (value < VALUE_EPSILON)
    {
        return 0;
    }
    float factor = 0;
    if (value > 0.01f)
    {
        factor = (difference / value) * VARIANCE_MULTIPLIER;
    }

    return factor;
}

//////////////////////////////////////////////////////////////////////////
inline void CalculateColor(float value, float variance, float* outColor, float& glow)
{
    float ColdColor[4] = { 0.15f, 0.9f, 0.15f, 1 };
    float HotColor[4] = { 1, 1, 1, 1 };

    glow = 0;

    float factor = CalculateVarianceFactor(value, variance);
    if (factor < 0)
    {
        factor = 0;
    }
    if (factor > 1)
    {
        factor = 1;
    }

    // Interpolate Hot to Cold color with variance factor.
    for (int k = 0; k < 4; k++)
    {
        outColor[k] = HotColor[k] * factor + ColdColor[k] * (1.0f - factor);
    }

    // Figure out whether to start up the glow as well.
    const float GLOW_RANGE = 0.5f;
    const float GLOW_ALPHA_MAX = 0.5f;
    float glow_alpha = (factor - GLOW_RANGE) / (1 - GLOW_RANGE);
    glow = glow_alpha * GLOW_ALPHA_MAX;
}

//////////////////////////////////////////////////////////////////////////
inline bool CompareFrameProfilersValue(const CFrameProfileSystem::SProfilerDisplayInfo& p1, const CFrameProfileSystem::SProfilerDisplayInfo& p2)
{
    return p1.pProfiler->m_displayedValue > p2.pProfiler->m_displayedValue;
}

//////////////////////////////////////////////////////////////////////////
inline bool CompareFrameProfilersCount(const CFrameProfileSystem::SProfilerDisplayInfo& p1, const CFrameProfileSystem::SProfilerDisplayInfo& p2)
{
    return p1.averageCount > p2.averageCount;
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::AddDisplayedProfiler(CFrameProfiler* pProfiler, int level)
{
    //if (pProfiler->m_displayedValue == 0)
    //return;

    char bExpended = pProfiler->m_bExpended;

    int newLevel = level + 1;
    if (!m_bSubsystemFilterEnabled || pProfiler->m_subsystem == (uint8)m_subsystemFilter)
    {
        SProfilerDisplayInfo info;
        info.level = level;
        info.pProfiler = pProfiler;
        m_displayedProfilers.push_back(info);
    }
    else
    {
        bExpended = 1;
        newLevel = level;
    }
    // Find childs.
    //@TODO Very Slow, optimize that.
    if (bExpended && pProfiler->m_bHaveChildren)
    {
        for (int i = 0; i < (int)m_pProfilers->size(); i++)
        {
            CFrameProfiler* pCur = (*m_pProfilers)[i];
            if (pCur->m_pParent == pProfiler)
            {
                AddDisplayedProfiler(pCur, newLevel);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::CalcDisplayedProfilers()
{
    if (m_bDisplayedProfilersValid)
    {
        return;
    }

    m_bDisplayedProfilersValid = true;
    m_displayedProfilers.reserve(m_pProfilers->size());
    m_displayedProfilers.resize(0);

    //////////////////////////////////////////////////////////////////////////
    // Get all displayed profilers.
    //////////////////////////////////////////////////////////////////////////
    if (m_displayQuantity == TOTAL_TIME || m_bEnableHistograms)
    {
        if (m_bEnableHistograms)
        {
            // In extended mode always add first item selected profiler.
            if (m_pGraphProfiler)
            {
                SProfilerDisplayInfo info;
                info.level = 0;
                info.pProfiler = m_pGraphProfiler;
                m_displayedProfilers.push_back(info);
            }
        }
        // Go through all profilers.
        for (int i = 0; i < (int)m_pProfilers->size(); i++)
        {
            CFrameProfiler* pProfiler = (*m_pProfilers)[i];
            if (!pProfiler->m_pParent && pProfiler->m_displayedValue >= 0.01f)
            {
                //pProfiler->m_bExpended = true;
                AddDisplayedProfiler(pProfiler, 0);
            }
        }
        if (m_displayedProfilers.empty())
        {
            m_bDisplayedProfilersValid = false;
        }
        return;
    }

    if (m_displayQuantity == COUNT_INFO)
    {
        // Go through all profilers.
        for (int i = 0; i < (int)m_pProfilers->size(); i++)
        {
            CFrameProfiler* pProfiler = (*m_pProfilers)[i];
            // Skip this profiler if its filtered out.
            if (m_bSubsystemFilterEnabled && pProfiler->m_subsystem != (uint8)m_subsystemFilter)
            {
                continue;
            }
            int count = pProfiler->m_countHistory.GetAverage();
            if (count > 1)
            {
                SProfilerDisplayInfo info;
                info.level = 0;
                info.averageCount = count;
                info.pProfiler = pProfiler;
                m_displayedProfilers.push_back(info);
            }
        }
        std::sort(m_displayedProfilers.begin(), m_displayedProfilers.end(), CompareFrameProfilersCount);
        if ((int)m_displayedProfilers.size() > m_maxProfileCount)
        {
            m_displayedProfilers.resize(m_maxProfileCount);
        }
        return;
    }

    // Go through all profilers.
    for (int i = 0; i < (int)m_pProfilers->size(); i++)
    {
        CFrameProfiler* pProfiler = (*m_pProfilers)[i];
        // Skip this profiler if its filtered out.
        if (m_bSubsystemFilterEnabled && pProfiler->m_subsystem != (uint8)m_subsystemFilter)
        {
            continue;
        }

        if (pProfiler->m_displayedValue > PROFILER_MIN_DISPLAY_TIME)
        {
            SProfilerDisplayInfo info;
            info.level = 0;
            info.pProfiler = pProfiler;
            m_displayedProfilers.push_back(info);
        }
    }
    //if (m_displayQuantity != EXTENDED_INFO)
    {
        //////////////////////////////////////////////////////////////////////////
        // sort displayed profilers by thier time.
        // Sort profilers by display value or count.
        std::sort(m_displayedProfilers.begin(), m_displayedProfilers.end(), CompareFrameProfilersValue);
        if ((int)m_displayedProfilers.size() > m_maxProfileCount)
        {
            m_displayedProfilers.resize(m_maxProfileCount);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
CFrameProfiler* CFrameProfileSystem::GetSelectedProfiler()
{
    if (m_displayedProfilers.empty())
    {
        return 0;
    }
    if (m_selectedRow < 0)
    {
        m_selectedRow = 0;
    }
    if (m_selectedRow > (int)m_displayedProfilers.size() - 1)
    {
        m_selectedRow = (int)m_displayedProfilers.size() - 1;
    }
    return m_displayedProfilers[m_selectedRow].pProfiler;
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::Render()
{
    m_textModeBaseExtra = 0;

    if (m_bDisplayMemoryInfo)
    {
        RenderMemoryInfo();
    }

    if (!m_bDisplay)
    {
        return;
    }

    FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);

    m_baseY = 80;

    m_textModeBaseExtra = 2;
    ROW_SIZE = 10;
    COL_SIZE = 11;

    m_pRenderer = m_pSystem->GetIRenderer();
    if (!m_pRenderer)
    {
        return;
    }

    if (!m_bCollectionPaused)
    {
        m_selectedRow = -1;
    }

    float colText = 60.0f - 4 * gEnv->IsDedicated();
    float colExtended = 1.0f;
    float row = 0;

    int width = m_pRenderer->GetWidth();
    int height = m_pRenderer->GetHeight();

    TransformationMatrices backupSceneMatrices;
    m_pRenderer->Set2DMode(width, height, backupSceneMatrices);

    //////////////////////////////////////////////////////////////////////////
    // Check if displayed profilers must be recalculated.
    if (m_displayQuantity == TOTAL_TIME || m_bEnableHistograms)
    {
        if (m_bCollectionPaused)
        {
            m_bDisplayedProfilersValid = false;
        }
    }
    else
    {
        m_bDisplayedProfilersValid = false;
    }
    //////////////////////////////////////////////////////////////////////////

    // Calculate which profilers must be displayed, and sort by importance.
    if (m_displayQuantity != SUBSYSTEM_INFO)
    {
        CalcDisplayedProfilers();
    }

    if (m_bEnableHistograms)
    {
        m_baseY = 50;
    }

    if (m_displayQuantity != PEAK_TIME)
    {
        if (m_bEnableHistograms)
        {
            RenderProfilerHeader(colExtended, row, m_bEnableHistograms);
        }
        else
        {
            RenderProfilerHeader(colText, row, m_bEnableHistograms);
        }

        if (m_bEnableHistograms)
        {
            ROW_SIZE = (float)m_histogramsHeight + 4;
        }

        //////////////////////////////////////////////////////////////////////////
        if (m_bEnableHistograms)
        {
            RenderProfilers(colExtended, 0, true);
        }
        else if (m_displayQuantity == SUBSYSTEM_INFO)
        {
            RenderSubSystems(colText, 0);
        }
        else if (m_displayQuantity != PEAKS_ONLY)
        {
            RenderProfilers(colText, 0, false);
        }
        //////////////////////////////////////////////////////////////////////////
    }

    // Render Peaks.
    if (m_displayQuantity == PEAK_TIME || m_bDrawGraph || m_bPageFaultsGraph)
    {
        DrawGraph();
    }

    float fpeaksLastRow = 0;

    if (m_peaks.size() > 0 && m_displayQuantity != PEAK_TIME)
    {
        fpeaksLastRow = RenderPeaks();
    }

    if (GetAdditionalSubsystems())
    {
        float colPeaks = 16.0f;
        RenderSubSystems(colPeaks, std::max(30.0f, fpeaksLastRow + 2));
    }

    m_pRenderer->Unset2DMode(backupSceneMatrices);

    if (m_bEnableHistograms)
    {
        RenderHistograms();
    }
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::RenderProfilers(float col, float row, bool bExtended)
{
    //  float HeaderColor[4] = { 1,1,0,1 };
    float CounterColor[4] = { 0, 0.8f, 1, 1 };

    int width = m_pRenderer->GetWidth();
    int height = m_pRenderer->GetHeight();

    // Header.
    m_baseY += 40;
    row = 0;

    CFrameProfiler* pSelProfiler = 0;
    if (m_bCollectionPaused)
    {
        pSelProfiler = GetSelectedProfiler();
    }

    if (CFrameProfileSystem::profile_log)
    {
        CryLogAlways("======================= Start Profiler Frame %d ==========================", gEnv->pRenderer->GetFrameID(false));
        CryLogAlways("|\tCount\t|\tSelf\t|\tTotal\t|\tModule\t|");
        CryLogAlways("|\t____\t|\t_____\t|\t_____\t|\t_____\t|");

        int logType = abs(CFrameProfileSystem::profile_log);
        for (int i = 0; i < (int)m_displayedProfilers.size(); i++)
        {
            CFrameProfiler* pProfiler = m_displayedProfilers[i].pProfiler;

            if (logType == 1)
            {
                int cave = pProfiler->m_countHistory.GetAverage();
                float fTotalTimeMs = pProfiler->m_totalTimeHistory.GetAverage();
                float fSelfTimeMs = pProfiler->m_selfTimeHistory.GetAverage();
                CryLogAlways("|\t%d\t|\t%.2f\t|\t%.2f%%\t|\t%s\t|\t %s", cave, fSelfTimeMs, fTotalTimeMs, GetModuleName(pProfiler), GetFullName(pProfiler));
            }
            else if (logType == 2)
            {
                int c_min = pProfiler->m_countHistory.GetMin();
                int c_max = pProfiler->m_countHistory.GetMax();
                float t1_min = pProfiler->m_totalTimeHistory.GetMin();
                float t1_max = pProfiler->m_totalTimeHistory.GetMax();
                float t0_min = pProfiler->m_selfTimeHistory.GetMin();
                float t0_max = pProfiler->m_selfTimeHistory.GetMax();
                CryLogAlways("|\t%d/%d\t|\t%.2f/%.2f\t|\t%.2f/%.2f%%\t|\t%s\t|\t %s", c_min, c_max, t0_min, t0_max, t1_min, t1_max, GetModuleName(pProfiler), GetFullName(pProfiler));
            }
        }

        CryLogAlways("======================= End Profiler Frame %d ==========================", gEnv->pRenderer->GetFrameID(false));
        if (CFrameProfileSystem::profile_log > 0) // reset logging so only logs one frame.
        {
            CFrameProfileSystem::profile_log = 0;
        }
    }

    // Go through all profilers.
    for (int i = 0; i < (int)m_displayedProfilers.size(); i++)
    {
        SProfilerDisplayInfo& dispInfo = m_displayedProfilers[i];
        CFrameProfiler* pProfiler = m_displayedProfilers[i].pProfiler;
        //filter stall profilers (caused by averaging)
        if (pProfiler && ((m_displayQuantity != STALL_TIME && pProfiler->m_isStallProfiler) ||
                          (m_displayQuantity == STALL_TIME && !pProfiler->m_isStallProfiler)))
        {
            continue;
        }
        if (i > m_selectedRow + MAX_DISPLAY_ROWS)
        {
            break;
        }

        float rectX1 = col * COL_SIZE;
        float rectX2 = width - 2.0f;
        float rectY1 = m_baseY + row * ROW_SIZE + 2;
        float rectY2 = m_baseY + (row + 1) * ROW_SIZE + 2;

        dispInfo.x = rectX1;
        dispInfo.y = rectY1;

        if (dispInfo.y - m_offset + ROW_SIZE >= height)
        {
            continue;
        }

        /*
        if (m_bCollectionPaused)
        {
        if (m_mouseX > rectX1 && m_mouseX < rectX2 && m_mouseY > rectY1 && m_mouseY < rectY2)
        {
        // Mouse inside this rectangle.
        m_selectedRow = i;
        }
        }
        */

        if (i == m_selectedRow && m_bCollectionPaused)
        {
            float SelColor[4] = { 1, 1, 1, 1 };
            DrawRect(rectX1, rectY1, rectX2, rectY2, SelColor);
        }

        RenderProfiler(pProfiler, dispInfo.level, col, row, bExtended, (pSelProfiler == pProfiler));
        row += 1.0f;
    }
    m_baseY -= 40;
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::RenderProfilerHeader(float col, float row, bool bExtended)
{
    char szText[256];
    float MainHeaderColor[4] = { 0, 1, 1, 1 };
    float HeaderColor[4] = { 1, 1, 0, 1 };
    float CounterColor[4] = { 0, 0.8f, 1, 1 };
    const float origCol = col;

    bool bShowFrameTimeSummary = false;

    const char* sValueName = "Time";
    if (m_bMemoryProfiling)
    {
        sValueName = "KB(s)";
    }

    azstrcpy(szText, AZ_ARRAY_SIZE(szText), "");
    // Draw general statistics.
    switch ((int)m_displayQuantity)
    {
    case SELF_TIME:
    case SELF_TIME_EXTENDED:
        cry_strcpy(szText, "Profile Mode: Self Time");
        break;
    case TOTAL_TIME:
    case TOTAL_TIME_EXTENDED:
        cry_strcpy(szText, "Profile Mode: Hierarchical Time");
        break;
    case PEAK_TIME:
        cry_strcpy(szText, "Profile Mode: Peak Time");
        break;
    case COUNT_INFO:
        cry_strcpy(szText, "Profile Mode: Calls Number");
        break;
    case SUBSYSTEM_INFO:
        cry_strcpy(szText, "Profile Mode: Subsystems");
        break;
    case STANDARD_DEVIATION:
        cry_strcpy(szText, "Profile Mode: Standard Deviation");
        sValueName = "StdDev";
        break;
    case ALLOCATED_MEMORY:
        cry_strcpy(szText, "Profile Mode: Memory Allocations");
        sValueName = "KB(s)";
        break;
    case ALLOCATED_MEMORY_BYTES:
        cry_strcpy(szText, "Profile Mode: Memory Allocations (Bytes)");
        sValueName = "Bytes";
        break;
    case PEAKS_ONLY:
        cry_strcpy(szText, "Profile Mode: Peaks Only");
        sValueName = "Bytes";
        break;
    }

    if (m_bCollectionPaused)
    {
        cry_strcat(szText, " (Paused)");
    }

    if (!m_filterThreadName.empty())
    {
        cry_strcat(szText, " (Thread:");
        cry_strcat(szText, m_filterThreadName.c_str());
        cry_strcat(szText, ")");
    }

    row--;
    DrawLabel(col, row++, MainHeaderColor, 0, szText);
    if (m_displayQuantity != STALL_TIME && m_displayQuantity != PEAKS_ONLY)
    {
        sprintf_s(szText, "FrameTime: %4.2fms, OverheadTime: %4.2fms, LostTime: %4.2fms", m_frameSecAvg * 1000.f, m_frameOverheadSecAvg * 1000.f, m_frameLostSecAvg * 1000.f);
        if (m_nPagesFaultsPerSec)
        {
            char* sEnd = szText + strlen(szText);
            sprintf_s(sEnd, szText + sizeof(szText) - sEnd, ", PF/Sec: %d", m_nPagesFaultsPerSec);
        }
        DrawLabel(col, row++, MainHeaderColor, 0, szText);
    }

    // Header.
    if (bExtended)
    {
        row = 0;
        m_baseY += 24;
        DrawLabel(col, row, HeaderColor, 0, "Max");
        DrawLabel(col + 5, row, HeaderColor, 0, "Min");
        DrawLabel(col + 10, row, HeaderColor, 0, "Ave");
        if (m_displayQuantity == TOTAL_TIME_EXTENDED)
        {
            DrawLabel(col + 15, row, HeaderColor, 0, "Self");
        }
        else
        {
            DrawLabel(col + 15, row, HeaderColor, 0, "Now");
        }
        DrawLabel(col + 2, row, CounterColor, 0, "/cnt");
        DrawLabel(col + 5 + 2, row, CounterColor, 0, "/cnt");
        DrawLabel(col + 10 + 2, row, CounterColor, 0, "/cnt");
        DrawLabel(col + 15 + 2, row, CounterColor, 0, "/cnt");
        m_baseY -= 24;
    }
    else if (m_displayQuantity == SUBSYSTEM_INFO)
    {
        DrawLabel(col, row, CounterColor, 0, "Percent");
        DrawLabel(col + 4, row, HeaderColor, 0, sValueName);
        DrawLabel(col + 8, row, HeaderColor, 0, " Function");
    }
    else if (m_displayQuantity != PEAKS_ONLY)
    {
        //      col = 45;
        DrawLabel(col - 3.5f, row, CounterColor, 0, "Count/");
        DrawLabel(col + 1, row, HeaderColor, 0, sValueName);
        if (m_displayQuantity != STALL_TIME)
        {
            DrawLabel(col + 5, row, HeaderColor, 0, " Function");
        }
        else
        {
            DrawLabel(col + 5, row, HeaderColor, 0, " Function (Action)");
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::RenderProfiler(CFrameProfiler* pProfiler, int level, float col, float row, bool bExtended, bool bSelected)
{
    char szText[128];
    //  float HeaderColor[4] = { 1,1,0,1 };
    float ValueColor[4] = { 0, 1, 0, 1 };
    float CounterColor[4] = { 0, 0.8f, 1, 1 };
    float TextColor[4] = { 1, 1, 1, 1 };
    float SelectedColor[4] = { 1, 0, 0, 1 };
    float GraphColor[4] = { 1, 0.3f, 0, 1 };

    float colTextOfs = 3.5f + 4 * gEnv->IsDedicated();
    float colCountOfs = -4.5f - 3 * gEnv->IsDedicated();
    float glow = 0;

    const char* sValueFormat = "%4.2f";
    if (m_bMemoryProfiling)
    {
        sValueFormat = "%6.f";
        colTextOfs += 2;
    }

    if (!bExtended)
    {
        col += level;
        // Simple info.
        float value = pProfiler->m_displayedValue;
        float variance = pProfiler->m_variance;
        CalculateColor(value, variance, ValueColor, glow);

        if (bSelected)
        {
            memcpy(ValueColor, SelectedColor, sizeof(ValueColor));
            glow = 0;
        }
        else if (m_pGraphProfiler == pProfiler)
        {
            memcpy(ValueColor, GraphColor, sizeof(ValueColor));
            glow = 0;
        }

        sprintf_s(szText, sValueFormat, value);
        DrawLabel(col, row, ValueColor, 0, szText);

        int cave = pProfiler->m_countHistory.GetAverage();
        if (cave > 1)
        {
            sprintf_s(szText, "%6d/", cave);
            DrawLabel(col + colCountOfs, row, CounterColor, 0, szText);
        }

        if (m_displayQuantity == TOTAL_TIME && pProfiler->m_bHaveChildren)
        {
            if (pProfiler->m_bExpended)
            {
                azstrcpy(szText, AZ_ARRAY_SIZE(szText), "-");
            }
            else
            {
                azstrcpy(szText, AZ_ARRAY_SIZE(szText), "+");
            }
        }
        else
        {
            *szText = 0;
        }
        cry_strcat(szText, GetFullName(pProfiler));
        if (m_displayQuantity == STALL_TIME && pProfiler->m_stallCause)
        {
            char buf[128];
            sprintf_s(buf, " (%s)", pProfiler->m_stallCause);
            cry_strcat(szText, buf);
        }
        DrawLabel(col + colTextOfs, row, ValueColor, glow, szText);

        // Render min/max values
        float tmin = 0, tmax = 0;
        if (m_displayQuantity == TOTAL_TIME)
        {
            tmin = pProfiler->m_totalTimeHistory.GetMin();
            tmax = pProfiler->m_totalTimeHistory.GetMax();
        }
        else if (m_displayQuantity == SELF_TIME)
        {
            tmin = pProfiler->m_selfTimeHistory.GetMin();
            tmax = pProfiler->m_selfTimeHistory.GetMax();
        }
        else if (m_displayQuantity == COUNT_INFO)
        {
            tmin = (float)pProfiler->m_countHistory.GetMin();
            tmax = (float)pProfiler->m_countHistory.GetMax();
        }
        sprintf_s(szText, "min:%4.2f max:%4.2f", tmin, tmax);
        DrawLabel(col + colTextOfs - 17, row, ValueColor, glow, szText, 0.8f);
    }
    else
    {
        // Extended display.
        if (bSelected)
        {
            memcpy(ValueColor, SelectedColor, sizeof(ValueColor));
            glow = 1;
        }

        float tmin, tmax, tave, tnow;
        int cmin, cmax, cave, cnow;
        if (m_displayQuantity == TOTAL_TIME_EXTENDED)
        {
            tmin = pProfiler->m_totalTimeHistory.GetMin();
            tmax = pProfiler->m_totalTimeHistory.GetMax();
            tave = pProfiler->m_totalTimeHistory.GetAverage();
            tnow = pProfiler->m_selfTimeHistory.GetAverage();
            //tnow = pProfiler->m_totalTimeHistory.GetLast();
        }
        else
        {
            tmin = pProfiler->m_selfTimeHistory.GetMin();
            tmax = pProfiler->m_selfTimeHistory.GetMax();
            tave = pProfiler->m_selfTimeHistory.GetAverage();
            tnow = pProfiler->m_selfTimeHistory.GetLast();
        }

        cmin = pProfiler->m_countHistory.GetMin();
        cmax = pProfiler->m_countHistory.GetMax();
        cave = pProfiler->m_countHistory.GetAverage();
        cnow = pProfiler->m_countHistory.GetLast();
        // Extensive info.
        sprintf_s(szText, sValueFormat, tmax);
        DrawLabel(col, row, ValueColor, 0, szText);
        sprintf_s(szText, sValueFormat, tmin);
        DrawLabel(col + 5, row, ValueColor, 0, szText);
        sprintf_s(szText, sValueFormat, tave);
        DrawLabel(col + 10, row, ValueColor, 0, szText);
        sprintf_s(szText, sValueFormat, tnow);
        DrawLabel(col + 15, row, ValueColor, 0, szText);

        if (cmax > 1)
        {
            sprintf_s(szText, "/%d", cmax);
            DrawLabel(col + 3, row, CounterColor, 0, szText);
        }
        if (cmin > 1)
        {
            sprintf_s(szText, "/%d", cmin);
            DrawLabel(col + 5 + 3, row, CounterColor, 0, szText);
        }
        if (cave > 1)
        {
            sprintf_s(szText, "/%d", cave);
            DrawLabel(col + 10 + 3, row, CounterColor, 0, szText);
        }
        if (cnow > 1)
        {
            sprintf_s(szText, "/%d", cnow);
            DrawLabel(col + 15 + 3, row, CounterColor, 0, szText);
        }

        // Simple info.
        //    float value = pProfiler->m_displayedValue;
        //    float variance = pProfiler->m_variance;
        //CalculateColor( value,variance,TextColor,glow );

        if (pProfiler->m_bHaveChildren)
        {
            if (pProfiler->m_bExpended)
            {
                cry_strcpy(szText, "-");
            }
            else
            {
                cry_strcpy(szText, "+");
            }
        }
        else
        {
            *szText = 0;
        }
        cry_strcat(szText, GetFullName(pProfiler));

        DrawLabel(col + 20 + level, row, TextColor, glow, szText);

        //float x1 = (col-1)*COL_SIZE + 2;
        //float y1 = m_baseY + row*ROW_SIZE;
        //float x2 = x1 + ROW_SIZE;
        //float y2 = y1 + ROW_SIZE;
        //float half = ROW_SIZE/2.0f;
        //DrawRect( x1,y1+half,x2,y1+half+1,ValueColor );
        //DrawRect( x1+half,y1,x1+half+1,y2,ValueColor );
        //DrawRect( x1,y1,x2,y2,HeaderColor );
    }
}

//////////////////////////////////////////////////////////////////////////
float CFrameProfileSystem::RenderPeaks()
{
    char szText[128];
    float PeakColor[4] = { 1, 1, 1, 1 };
    float HotPeakColor[4] = { 1, 1, 1, 1 };
    float ColdPeakColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float PeakHeaderColor[4] = { 0, 1, 1, 1 };
    float PeakCounterColor[4] = { 0, 0.8f, 1, 1 };
    float CounterColor[4] = { 0, 0.8f, 1, 1 };
    float PageFaultsColor[4] = { 1, 0.2f, 1, 1 };

    // changed from define to adjustable value
    float fHotToColdTime = GetPeakDisplayDuration();
    float colPeaks = 8.0f;
    float row = 0;
    float col = colPeaks;

    sprintf_s(szText, "Latest Peaks");
    DrawLabel(colPeaks, row++, PeakHeaderColor, 0, szText);

    float currTimeSec = gEnv->pTimer->TicksToSeconds(m_totalProfileTime);

    std::vector<SPeakRecord>& rPeaks = m_peaks;

    // Go through all peaks.
    for (int i = 0; i < (int)rPeaks.size(); i++)
    {
        SPeakRecord& peak = rPeaks[i];
        if (!peak.pProfiler)
        {
            continue;
        }

        float age = (currTimeSec - 1.0f) - peak.when;
        float ageFactor = age / fHotToColdTime;
        if (ageFactor < 0)
        {
            ageFactor = 0;
        }
        if (ageFactor > 1)
        {
            ageFactor = 1;
        }
        for (int k = 0; k < 4; k++)
        {
            PeakColor[k] = ColdPeakColor[k] * ageFactor + HotPeakColor[k] * (1.0f - ageFactor);
        }

        if (!m_bMemoryProfiling)
        {
            sprintf_s(szText, "%4.2fms", peak.peakValue);
        }
        else
        {
            sprintf_s(szText, "%6.f", peak.peakValue);
        }
        DrawLabel(col, row, PeakColor, 0, szText);

        if (peak.count > 1)
        {
            for (int k = 0; k < 4; k++)
            {
                PeakCounterColor[k] = CounterColor[k] * (1.0f - ageFactor);
            }
            sprintf_s(szText, "%4d/", peak.count);
            DrawLabel(col - 3, row, PeakCounterColor, 0, szText);
        }
        if (peak.pageFaults > 0)
        {
            for (int k = 0; k < 4; k++)
            {
                PeakCounterColor[k] = PageFaultsColor[k] * (1.0f - ageFactor);
            }
            sprintf_s(szText, "(%4d)", peak.pageFaults);
            DrawLabel(col - 6, row, PeakCounterColor, 0, szText);
        }

        cry_strcpy(szText, GetFullName(peak.pProfiler));

        DrawLabel(col + 5, row, PeakColor, 0, szText);

        row += 1.0f;

        if (age > fHotToColdTime)
        {
            rPeaks.erase(m_peaks.begin() + i);
            i--;
        }
    }

    return row;
}


void DrawTextLabel(IRenderer* pRenderer, float& x, float& y, float* pColor, float fFontScale, const char* format, ...)
{
    char buffer[ 512 ];
    va_list args;
    va_start(args, format);
    vsprintf_s(buffer, format, args);
    va_end(args);

    pRenderer->Draw2dLabel(x, y, fFontScale, pColor, false, "%s", buffer);
    y += FrameProfileRenderConstants::c_yStepSizeText;
}

void GetColor(float scale, float* pColor)
{
    if (scale <= 0.5f)
    {
        pColor[ 0 ] = 0;
        pColor[ 1 ] = 1;
        pColor[ 2 ] = 0;
        pColor[ 3 ] = 1;
    }
    else if (scale <= 0.75f)
    {
        pColor[ 0 ] = (scale - 0.5f) * 4.0f;
        pColor[ 1 ] = 1;
        pColor[ 2 ] = 0;
        pColor[ 3 ] = 1;
    }
    else if (scale <= 1.0f)
    {
        pColor[ 0 ] = 1;
        pColor[ 1 ] = 1 - (scale - 0.75f) * 4.0f;
        pColor[ 2 ] = 0;
        pColor[ 3 ] = 1;
    }
    else
    {
        float time(gEnv->pTimer->GetAsyncCurTime());
        float blink(sinf(time * 6.28f) * 0.5f + 0.5f);
        pColor[ 0 ] = 1;
        pColor[ 1 ] = blink;
        pColor[ 2 ] = blink;
        pColor[ 3 ] = 1;
    }
}

void DrawMeter(IRenderer* pRenderer, float& x, float& y, float scale, float screenWidth, float screenHeight, float targetBarWidth = 0.21f)
{
    IRenderAuxGeom*  pAuxRenderer = pRenderer->GetIRenderAuxGeom();

    //Aux Render setup
    SAuxGeomRenderFlags oldFlags = pAuxRenderer->GetRenderFlags();

    SAuxGeomRenderFlags flags(e_Def2DPublicRenderflags);
    flags.SetDepthTestFlag(e_DepthTestOff);
    flags.SetDepthWriteFlag(e_DepthWriteOff);
    flags.SetCullMode(e_CullModeNone);
    pAuxRenderer->SetRenderFlags(flags);

    // draw frame for meter
    vtx_idx indLines[ 8 ] =
    {
        0, 1, 1, 2,
        2, 3, 3, 0
    };

    const float barWidth = targetBarWidth > 1.0f ? targetBarWidth / screenWidth : targetBarWidth;

    const float yellowStart = 0.5f * barWidth;
    const float redStart = 0.75f * barWidth;

    Vec3 frame[ 4 ] =
    {
        Vec3((x - 1) / screenWidth, (y - 1) / screenHeight, 0),
        Vec3(x / screenWidth + barWidth, (y - 1) / screenHeight, 0),
        Vec3(x / screenWidth + barWidth, (y + FrameProfileRenderConstants::c_yStepSizeTextMeter) / screenHeight, 0),
        Vec3((x - 1) / screenWidth, (y + FrameProfileRenderConstants::c_yStepSizeTextMeter) / screenHeight, 0)
    };

    pAuxRenderer->DrawLines(frame, 4, indLines, 8, ColorB(255, 255, 255, 255));

    // draw meter itself
    vtx_idx indTri[ 6 ] =
    {
        0, 1, 2,
        0, 2, 3
    };

    // green part (0.0 <= scale <= 0.5)
    {
        float lScale(max(min(scale, 0.5f), 0.0f));

        Vec3 bar[ 4 ] =
        {
            Vec3(x / screenWidth, y / screenHeight, 0),
            Vec3(x / screenWidth + lScale * barWidth, y / screenHeight, 0),
            Vec3(x / screenWidth + lScale * barWidth, (y + FrameProfileRenderConstants::c_yStepSizeTextMeter) / screenHeight, 0),
            Vec3(x / screenWidth, (y + FrameProfileRenderConstants::c_yStepSizeTextMeter) / screenHeight, 0)
        };
        pAuxRenderer->DrawTriangles(bar, 4, indTri, 6, ColorB(0, 255, 0, 255));
    }

    // green to yellow part (0.5 < scale <= 0.75)
    if (scale > 0.5f)
    {
        float lScale(min(scale, 0.75f));

        Vec3 bar[ 4 ] =
        {
            Vec3(x / screenWidth + yellowStart, y / screenHeight, 0),
            Vec3(x / screenWidth + lScale * barWidth, y / screenHeight, 0),
            Vec3(x / screenWidth + lScale * barWidth, (y + FrameProfileRenderConstants::c_yStepSizeTextMeter) / screenHeight, 0),
            Vec3(x / screenWidth + yellowStart, (y + FrameProfileRenderConstants::c_yStepSizeTextMeter) / screenHeight, 0)
        };

        float color[ 4 ];
        GetColor(lScale, color);

        ColorB colSegStart(0, 255, 0, 255);
        ColorB colSegEnd((uint8) (color[ 0 ] * 255), (uint8) (color[ 1 ] * 255), (uint8) (color[ 2 ] * 255), (uint8) (color[ 3 ] * 255));

        ColorB col[ 4 ] =
        {
            colSegStart,
            colSegEnd,
            colSegEnd,
            colSegStart
        };

        pAuxRenderer->DrawTriangles(bar, 4, indTri, 6, col);
    }

    // yellow to red part (0.75 < scale <= 1.0)
    if (scale > 0.75f)
    {
        float lScale(min(scale, 1.0f));

        Vec3 bar[ 4 ] =
        {
            Vec3(x / screenWidth + redStart, y / screenHeight, 0),
            Vec3(x / screenWidth + lScale * barWidth, y / screenHeight, 0),
            Vec3(x / screenWidth + lScale * barWidth, (y + FrameProfileRenderConstants::c_yStepSizeTextMeter) /  screenHeight, 0),
            Vec3(x / screenWidth + redStart, (y + FrameProfileRenderConstants::c_yStepSizeTextMeter) / screenHeight, 0)
        };

        float color[ 4 ];
        GetColor(lScale, color);

        ColorB colSegStart(255, 255, 0, 255);
        ColorB colSegEnd((uint8) (color[ 0 ] * 255), (uint8) (color[ 1 ] * 255), (uint8) (color[ 2 ] * 255), (uint8) (color[ 3 ] * 255));

        ColorB col[ 4 ] =
        {
            colSegStart,
            colSegEnd,
            colSegEnd,
            colSegStart
        };

        pAuxRenderer->DrawTriangles(bar, 4, indTri, 6, col);
    }

    y += FrameProfileRenderConstants::c_yStepSizeTextMeter;

    //restore Aux Render setup
    pAuxRenderer->SetRenderFlags(oldFlags);
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::DrawGraph()
{
    // Layout:
    //  - Repeated X times
    //   -------------------------------------------------------------
    //  |                                                   Safe Area                                                       |
    //   -------------------------------------------------------------
    //   -----------------------     ---------------------------------
    //  |                                               |       |                                                                   |
    //  |             Text Area           |     |                       Graph Area                      |
    //  |                                               |       |                                                                   |
    //   -----------------------     ---------------------------------
    //

    const float VALUE_EPSILON = 0.000001f;

    // UI item layout information
    const float cWorkerGraphScale = 180.f;       // Worker Graph displays the last X frames
    const float cTextAreaWidth = 220.f;              // Absolute Text Area width

    // Do not render in the top X% of rt top area to allow space for the r_DisplayInfo 1 profiler
    const float cTopSafeArea = 0.13f;

    // UI Control colour items
    float labelColor[4] = { 1.f, 1.f, 1.f, 1.f };
    float labelColDarkGreen[4] = { 0, 0.6f, 0.2f, 1.f };
    float labelColRed[4] = { 1.f, 0, 0, 1.f };
    float labelColorSuspended[4] = { 0.25f, 0.25f, 0.25f, 1.f };
    ColorF graphColor(0, 0.85f, 0, 1.f);

    // RT Area
    const float nRtWidth  = (float)m_pRenderer->GetWidth();
    const float nRtHeight = (float)m_pRenderer->GetHeight();

    // Calculate overscan adjustment (for those elements that do not obey it)(see r_OverScanBoarder)
    Vec2 overscanBorder = Vec2(0.0f, 0.0f);
    gEnv->pRenderer->EF_Query(EFQ_OverscanBorders, overscanBorder);
    //Vec2 overscanBorder = *(Vec2*)gEnv->pRenderer->EF_Query(EFQ_OverscanBorders, );

    const int fOverscanAdjX = (int)(overscanBorder.x * (float)nRtWidth);
    const int fOverscanAdjY = (int)(overscanBorder.y * (float)nRtHeight);

    // Surface Area
    const float nSafeAreaY = nRtHeight * cTopSafeArea;
    const float nSurfaceWidth  = nRtWidth - fOverscanAdjX;
    const float nSurfaceHeight  = nRtHeight - nSafeAreaY - fOverscanAdjX;

    // Calculate worker graph dimensions
    const float nTextAreaWidth =  cTextAreaWidth * FrameProfileRenderConstants::c_yScale;
    const float nGraphStartX = nTextAreaWidth + nRtWidth * 0.015f * FrameProfileRenderConstants::c_yScale; // Fixed gap of 1.5% of RT width

    const int   nGraphWidth = (int)(nSurfaceWidth - nGraphStartX - nSurfaceWidth * 0.05f * FrameProfileRenderConstants::c_yScale); // Add a 5% of RT width gap to the right RT edge

    const float nTextAreaWidthOvrScnAdj = nTextAreaWidth + fOverscanAdjX;
    const float nGraphStartXOvrScnAdj       = nGraphStartX + fOverscanAdjX;

    // Absolute coordinates tracker
    float x = 0;
    float y = 0;

    //************************
    // Page Fault Information
    //************************
    // Display values via a log10 graph
#if defined(WIN32) || defined(WIN64)
    {
        if (nGraphWidth + 1 != m_timeGraphPageFault.size())
        {
            m_timeGraphPageFault.resize(nGraphWidth + 1);
        }

        const float cLogGraphScale = 5.f;
        const float yStart = y;
        DrawTextLabel(m_pRenderer, x, y, labelColDarkGreen, FrameProfileRenderConstants::c_fontScale, "Page Faults:");

        x += 5;
        DrawTextLabel(m_pRenderer, x, y, labelColor, FrameProfileRenderConstants::c_fontScale, "Faults: %6i  ", m_nPagesFaultsLastFrame);

        // Display values via a log10 graph
        // log10(10,000) = 4, because 10^4 = 10,000
        float logValue = 0;

        if (m_nPagesFaultsLastFrame)
        {
            logValue = log10((float)m_nPagesFaultsLastFrame + VALUE_EPSILON);
        }

        x += fOverscanAdjX;
        y += fOverscanAdjY + 5; // Push the meter down a further 5 for correct look
        DrawMeter(m_pRenderer, x, y, logValue * (100.f / cLogGraphScale) * 0.01f, nRtWidth, nRtHeight, nTextAreaWidthOvrScnAdj - x);
        x -= fOverscanAdjX;
        y -= fOverscanAdjY;
        x -= 5;

        float charPosX = x + nTextAreaWidth - 20.f * FrameProfileRenderConstants::c_yScale;
        float charPosY = y;
        DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale, "10000");

        m_timeGraphPageFault[0] = (unsigned char)(255.f - clamp_tpl(logValue *  (255.f / cLogGraphScale), 0.f, 255.f));

        y += FrameProfileRenderConstants::c_yStepSizeText  * 2.f; // Add some extra height to the graph
        const float graphHeight = y - yStart;
        m_pRenderer->Graph(&m_timeGraphPageFault[0], (int)(x + nGraphStartXOvrScnAdj), (int)(yStart + fOverscanAdjY), nGraphWidth, (int)graphHeight, 0, 2, 0, graphColor, 0);

        // Draw time graph indicator
        charPosX = x + nGraphStartX + - 1.f;
        charPosY = y - 5.f;
        DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale, "|");

        // Draw graph resolution information
        const float stepY = graphHeight / 5.f;
        charPosX = x + nGraphStartX + nGraphWidth + 5.f * FrameProfileRenderConstants::c_yScale;  // Use FrameProfileRenderConstants::c_yScale to adjust for larger font size
        charPosY = yStart;
        DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale * 0.85f, "%i", 10000);
        charPosY += stepY;
        DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale * 0.85f, "%i", 1000);
        charPosY += stepY;
        DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale * 0.85f, "%i", 100);
        /*  charPosY += stepY;
        DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale * 0.85f, "%i", 0);*/

        y += FrameProfileRenderConstants::c_yNextControlGap;
    }
#endif

    //************************
    // Frame Time Information
    //************************
    {
        if (nGraphWidth + 1 != m_timeGraphFrameTime.size())
        {
            m_timeGraphFrameTime.resize(nGraphWidth + 1);
        }

        if (m_displayQuantity != COUNT_INFO)
        {
            const float yStart = y;

            DrawTextLabel(m_pRenderer, x, y, labelColDarkGreen, FrameProfileRenderConstants::c_fontScale, "Frame Timings:");

            const float frameTime = m_frameTimeHistory.GetLast();
            const float framesPerSec = 1.0f / ((frameTime + VALUE_EPSILON) * 0.001f); // Convert ms to fps (1/ms = fps)
            const float framePercentage = frameTime * (1.f / 200.f);

            x += 5;
            DrawTextLabel(m_pRenderer, x, y, labelColor, FrameProfileRenderConstants::c_fontScale, "Time: %06.2fms", frameTime);

            x += fOverscanAdjX;
            y += fOverscanAdjY + 5; // Push the meter down a further 5 for correct look
            DrawMeter(m_pRenderer, x, y, framePercentage, nRtWidth, nRtHeight, nTextAreaWidthOvrScnAdj - x);
            x -= fOverscanAdjX;
            y -= fOverscanAdjY;

            float charPosX = x + nTextAreaWidth - 20.f * FrameProfileRenderConstants::c_yScale;
            float charPosY = y - 2.5f;
            DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale * 0.88f, "200ms");


            DrawTextLabel(m_pRenderer, x, y, labelColor, FrameProfileRenderConstants::c_fontScale, "Frames: %06.2ffps", framesPerSec);
            x += fOverscanAdjX;
            y += fOverscanAdjY + 5; // Push the meter down a further 5 for correct look
            DrawMeter(m_pRenderer, x, y, framesPerSec * 0.01f, nRtWidth, nRtHeight, nTextAreaWidthOvrScnAdj - x);
            x -= fOverscanAdjX;
            y -= fOverscanAdjY;

            charPosX = x + nTextAreaWidth - 20.f * FrameProfileRenderConstants::c_yScale;
            ;
            charPosY = y;
            DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale * 0.88f, "100fps");
            x -= 5;

            // Draw graph resolution information
            charPosX = x + nGraphStartX + nGraphWidth + 5.f * FrameProfileRenderConstants::c_yScale;
            charPosY = yStart;
            DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale * 0.85f, "%ims", 200);

            y += FrameProfileRenderConstants::c_yNextControlGap;
        }
    }

    // Draw graph resolution
    float charPosX = x + nGraphStartX + nGraphWidth - 35.f * FrameProfileRenderConstants::c_yScale;  // Use FrameProfileRenderConstants::c_yScale to adjust for larger font size
    float charPosY = y - FrameProfileRenderConstants::c_yStepSizeText;
    DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale, "%d frames", nGraphWidth);
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::RenderHistograms()
{
    ColorF HistColor(0, 1, 0, 1);

    // Draw histograms.
    int h = m_pRenderer->GetHeight();
    int w = m_pRenderer->GetWidth();

    int graphStyle = 2; // histogram.

    float fScale = 1.0f; // histogram.

    m_pRenderer->SetMaterialColor(1, 1, 1, 1);
    for (int i = 0; i < (int)m_displayedProfilers.size(); i++)
    {
        if (i > MAX_DISPLAY_ROWS)
        {
            break;
        }
        SProfilerDisplayInfo& dispInfo = m_displayedProfilers[i];
        CFrameProfilerGraph* pGraph = dispInfo.pProfiler->m_pGraph;
        if (!pGraph)
        {
            continue;
        }

        // Add a value to graph.
        pGraph->m_x = (int)(dispInfo.x + COL_DIFF_HISTOGRAMS * COL_SIZE);
        pGraph->m_y = (int)(dispInfo.y);
        if (pGraph->m_y >= h)
        {
            continue;
        }
        // Render histogram.
        m_pRenderer->Graph(&pGraph->m_data[0], pGraph->m_x, pGraph->m_y, pGraph->m_width, pGraph->m_height, m_histogramsCurrPos, graphStyle, 0, HistColor, fScale);
    }
    if (!m_bCollectionPaused)
    {
        m_histogramsCurrPos++;
        if (m_histogramsCurrPos >= m_histogramsMaxPos)
        {
            m_histogramsCurrPos = 0;
        }
    }
}

void CFrameProfileSystem::RenderSubSystems(float col, float row)
{
    char szText[128];
    float HeaderColor[4] = { 1, 1, 0, 1 };
    float ValueColor[4] = { 0, 1, 0, 1 };
    float CounterColor[4] = { 0, 0.8f, 1, 1 };

    float colPercOfs = -3.0f;
    float colTextOfs = 4.0f;
    colTextOfs = 9.0f;

    int height = m_pRenderer->GetHeight();


    m_baseY += 40;

    // Go through all profilers.
    for (int i = 0; i < PROFILE_LAST_SUBSYSTEM; i++)
    {
        // Simple info.
        float value = m_subsystems[i].selfTime;
        m_subsystems[i].selfTime = 0;
        const char* sName = m_subsystems[i].name;
        if (!sName)
        {
            continue;
        }

        //sprintf_s( szText, "%4.2fms %4.2fms",value, maxVal);
        if (!m_bMemoryProfiling)
        {
            sprintf_s(szText, "%4.2fms", value);
        }
        else
        {
            sprintf_s(szText, "%6.0f", value);
        }

        if (value > m_subsystems[i].budgetTime)
        {
            DrawLabel(col, row, HeaderColor, 0, szText);
            DrawLabel(col + colTextOfs, row, HeaderColor, 0, sName);
        }
        else
        {
            DrawLabel(col, row, ValueColor, 0, szText);
            DrawLabel(col + colTextOfs, row, ValueColor, 0, sName);
        }

        if (m_frameSecAvg != 0.f)
        {
            int percent = FtoI(value / (m_frameSecAvg * 1000) * 100);
            sprintf_s(szText, "%2d%%", percent);
            DrawLabel(col + colPercOfs, row, CounterColor, 0, szText);
        }

        row += 1.0f;
    }
    m_baseY -= 40;
}

#if AZ_LEGACY_CRYSYSTEM_TRAIT_USE_PACKED_PEHEADER
#pragma pack(push,1)
struct PEHeader
{
    DWORD signature;
    IMAGE_FILE_HEADER _head;
    IMAGE_OPTIONAL_HEADER opt_head;
    IMAGE_SECTION_HEADER* section_header; // actual number in NumberOfSections
};
#pragma pack(pop)
#endif

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::RenderMemoryInfo()
{
#if AZ_LEGACY_CRYSYSTEM_TRAIT_USE_RENDERMEMORY_INFO

    m_pRenderer = gEnv->pRenderer;
    if (!m_pRenderer)
    {
        return;
    }

    m_baseY = 0;
    m_textModeBaseExtra = -1;
    ROW_SIZE = 11;
    COL_SIZE = 11;

    float col = 1;
    float row = 1;

    float HeaderColor[4] = { 0.3f, 1, 1, 1 };
    float ModuleColor[4] = { 1, 1, 1, 1 };
    float StaticColor[4] = { 1, 1, 1, 1 };
    float NumColor[4] = { 1, 0, 1, 1 };
    float TotalColor[4] = { 1, 1, 1, 1 };

    char szText[128];
    float fLabelScale = 1.1f;

    ILog* pLog = gEnv->pLog;
    //////////////////////////////////////////////////////////////////////////
    // Show memory usage.
    //////////////////////////////////////////////////////////////////////////
    int memUsage = 0;//CryMemoryGetAllocatedSize();
    int64 totalAll = 0;
    int luaMemUsage = gEnv->pScriptSystem ? gEnv->pScriptSystem->GetScriptAllocSize() : 0;

    row++; // reserve for static.
    row++;
    row++;

    sprintf_s(szText, "Lua Allocated Memory: %d KB", luaMemUsage / 1024);
    DrawLabel(col, row++, HeaderColor, 0, szText, fLabelScale);

    if (m_bLogMemoryInfo)
    {
        pLog->Log(szText);
    }
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    float col1 = col + 15;
    float col2 = col1 + 8;
#if defined(LINUX)
    float col3 = col2;
#else
    float col3 = col2 + 7;
#endif
    float col4 = col3 + 7;

    DrawLabel(col, row++, HeaderColor, 0, "-----------------------------------------------------------------------------------------------------------------------------------", fLabelScale);
    DrawLabel(col, row, HeaderColor, 0, "Module", fLabelScale);
    DrawLabel(col1, row, HeaderColor, 0, "Dynamic(KB)", fLabelScale);
#if !defined(LINUX)
    DrawLabel(col2, row, HeaderColor, 0, "Static(KB)", fLabelScale);
#endif
    DrawLabel(col3, row, HeaderColor, 0, "Num Allocs", fLabelScale);
    DrawLabel(col4, row, HeaderColor, 0, "Total Allocs(KB)", fLabelScale);
    float col5 = col4 + 10;
    DrawLabel(col5, row, HeaderColor, 0, "Total Wasted(KB)", fLabelScale);
    int totalUsedInModulesStatic = 0;

    row++;

#ifdef WIN32
    PROCESS_MEMORY_COUNTERS ProcessMemoryCounters = { sizeof(ProcessMemoryCounters) };
    GetProcessMemoryInfo(GetCurrentProcess(), &ProcessMemoryCounters, sizeof(ProcessMemoryCounters));
    SIZE_T WorkingSetSize = ProcessMemoryCounters.WorkingSetSize;
    SIZE_T QuotaPagedPoolUsage = ProcessMemoryCounters.QuotaPagedPoolUsage;
    SIZE_T PagefileUsage = ProcessMemoryCounters.PagefileUsage;
    SIZE_T PageFaultCount = ProcessMemoryCounters.PageFaultCount;

    sprintf_s(szText, "WindowsInfo: PagefileUsage: %u WorkingSetSize: %u, QuotaPagedPoolUsage: %u PageFaultCount: %u\n",
        (uint)PagefileUsage / 1024,
        (uint)WorkingSetSize / 1024,
        (uint)QuotaPagedPoolUsage / 1024,
        (uint) PageFaultCount);

    DrawLabel(col, row++, HeaderColor, 0, "-----------------------------------------------------------------------------------------------------------------------------------", fLabelScale);
    //  sprintf_s( szText,"WindowsInfo",countedMemoryModules );
    DrawLabel(col, row, HeaderColor, 0, szText, fLabelScale);
#endif

    int memUsageInMB_SysCopyMeshes = 0;
    int memUsageInMB_SysCopyTextures = 0;
    m_pRenderer->EF_Query(EFQ_Alloc_APIMesh, memUsageInMB_SysCopyMeshes);
    m_pRenderer->EF_Query(EFQ_Alloc_APITextures, memUsageInMB_SysCopyTextures);

    totalAll += memUsage;
    totalAll += memUsageInMB_SysCopyMeshes;
    totalAll += memUsageInMB_SysCopyTextures;

    sprintf_s(szText, "Total Allocated Memory: %" PRId64 " KB (DirectX Textures: %d KB, VB: %d Kb)", totalAll / 1024, memUsageInMB_SysCopyTextures / 1024, memUsageInMB_SysCopyMeshes / 1024);

    DrawLabel(col, 1, HeaderColor, 0, szText, fLabelScale);

    m_bLogMemoryInfo = false;

#endif //#if defined(WIN32)
}
#undef VARIANCE_MULTIPLIER

#endif // USE_FRAME_PROFILER

#undef COL_DIFF_HISTOGRAMS
