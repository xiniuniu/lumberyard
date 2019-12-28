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
#include "MainWindow.h"

#include "ISourceControl.h"

#include <sstream>
#include <QGuiApplication>
#include <QScreen>
#include <QSysInfo>
#include <QtUtilWin.h>

#include <AzToolsFramework/SourceControl/SourceControlAPI.h>

#pragma comment(lib, "Gdi32.lib")

//////////////////////////////////////////////////////////////////////////
// Global Instance of Editor settings.
//////////////////////////////////////////////////////////////////////////
SANDBOX_API SEditorSettings gSettings;

Q_GLOBAL_STATIC(QSettings, s_editorSettings);

const QString kDefaultColumnsForAssetBrowserList = "Filename,Path,LODs,Triangles,Submeshes,Filesize,Textures,Materials,Tags";
const int EditorSettingsVersion = 2; // bump this up on every substantial settings change

void KeepEditorActiveChanged(ICVar* keepEditorActive)
{
    const int iCVarKeepEditorActive = keepEditorActive->GetIVal();
    CCryEditApp::instance()->KeepEditorActive(iCVarKeepEditorActive);
}

void ToolbarIconSizeChanged(ICVar* toolbarIconSize)
{
    MainWindow::instance()->AdjustToolBarIconSize();
}

class SettingsGroup
{
public:
    explicit SettingsGroup(const QString& group)
        : m_group(group)
    {
        for (auto g : m_group.split('\\'))
        {
            s_editorSettings()->beginGroup(g);
        }
    }
    ~SettingsGroup()
    {
        for (auto g : m_group.split('\\'))
        {
            s_editorSettings()->endGroup();
        }
    }

private:
    const QString m_group;
};

namespace
{

    class QtApplicationListener
        : public AzToolsFramework::EditorEvents::Bus::Handler
    {
    public:
        QtApplicationListener()
        {
            AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
        }

        void NotifyQtApplicationAvailable(QApplication* application) override
        {
            gSettings.viewports.nDragSquareSize = application->startDragDistance();
            AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
            delete this;
        }
    };

}

//////////////////////////////////////////////////////////////////////////
SGizmoSettings::SGizmoSettings()
{
    axisGizmoSize = 0.2f;
    axisGizmoText = true;
    axisGizmoMaxCount = 50;
    helpersScale = 1.f;
    tagpointScaleMulti = 0.5f;
    rulerSphereScale = 0.5f;
    rulerSphereTrans = 0.5f;
}
//////////////////////////////////////////////////////////////////////////
SEditorSettings::SEditorSettings()
{
    bSettingsManagerMode = false;

    undoLevels = 50;
    m_undoSliceOverrideSaveValue = false;
    bShowDashboardAtStartup = true;
    m_showCircularDependencyError = true;
    bAutoloadLastLevelAtStartup = false;
    bMuteAudio = false;
    bEnableGameModeVR = false;


    objectHideMask = 0;
    objectSelectMask = 0xFFFFFFFF; // Initially all selectable.

    autoBackupEnabled = false;
    autoBackupTime = 10;
    autoBackupMaxCount = 3;
    autoRemindTime = 0;

    bAutoSaveTagPoints = false;

    bNavigationContinuousUpdate = false;
    bNavigationShowAreas = true;
    bNavigationDebugDisplay = false;
    bVisualizeNavigationAccessibility = false;
    navigationDebugAgentType = 0;

    editorConfigSpec = CONFIG_VERYHIGH_SPEC;  //arbitrary choice, but lets assume that we want things to initially look as good as possible in the editor.

    viewports.bAlwaysShowRadiuses = false;
    viewports.bAlwaysDrawPrefabBox = false;
    viewports.bAlwaysDrawPrefabInternalObjects = false;
    viewports.bSync2DViews = false;
    viewports.fDefaultAspectRatio = 800.0f / 600.0f;
    viewports.fDefaultFov = DEG2RAD(60); // 60 degrees (to fit with current game)
    viewports.bShowSafeFrame = false;
    viewports.bHighlightSelectedGeometry = false;
    viewports.bHighlightSelectedVegetation = true;
    viewports.bHighlightMouseOverGeometry = true;
    viewports.bShowMeshStatsOnMouseOver = false;
    viewports.bDrawEntityLabels = false;
    viewports.bShowTriggerBounds = false;
    viewports.bShowIcons = true;
    viewports.bDistanceScaleIcons = true;
    viewports.bShowSizeBasedIcons = false;
    viewports.nShowFrozenHelpers = true;
    viewports.bFillSelectedShapes = false;
    viewports.nTopMapTextureResolution = 512;
    viewports.bTopMapSwapXY = false;
    viewports.bShowGridGuide = true;
    viewports.bHideMouseCursorWhenCaptured = true;
    viewports.nDragSquareSize = 0; // We must initialize this after the Qt application object is available; see QtApplicationListener
    viewports.bEnableContextMenu = true;
    viewports.fWarningIconsDrawDistance = 50.0f;
    viewports.bShowScaleWarnings = false;
    viewports.bShowRotationWarnings = false;

    cameraMoveSpeed = 1;
    cameraRotateSpeed = 1;
    cameraFastMoveSpeed = 2;
    stylusMode = false;
    restoreViewportCamera = true;
    wheelZoomSpeed = 1;
    invertYRotation = false;
    invertPan = false;
    fBrMultiplier = 2;
    bPreviewGeometryWindow = true;
    bGeometryBrowserPanel = true;
    bBackupOnSave = true;
    backupOnSaveMaxCount = 3;
    bFlowGraphMigrationEnabled = true;
    bFlowGraphShowNodeIDs = false;
    bFlowGraphShowToolTip = true;
    bFlowGraphEdgesOnTopOfNodes = false;
    bFlowGraphHighlightEdges = true;
    bApplyConfigSpecInEditor = true;
    useLowercasePaths = 0;
    showErrorDialogOnLoad = 1;

    consoleBackgroundColorTheme = ConsoleColorTheme::Light;
    bShowTimeInConsole = false;
    bLayerDoubleClicking = false;

    enableSceneInspector = false;
    enableLegacyUI = false;

    strStandardTempDirectory = "Temp";
    strEditorEnv = "Editor/Editor.env";

    // Init source safe params.
    enableSourceControl = true;

    saveOnlyModified = false;
    freezeReadOnly = true;
    frozenSelectable = false;

#if AZ_TRAIT_OS_PLATFORM_APPLE
    textEditorForScript = "TextEdit";
    textEditorForShaders = "TextEdit";
    textEditorForBspaces = "TextEdit";
    textureEditor = "Photoshop";
#elif defined(AZ_PLATFORM_WINDOWS)
    textEditorForScript = "notepad++.exe";
    textEditorForShaders = "notepad++.exe";
    textEditorForBspaces = "notepad++.exe";
    textureEditor = "Photoshop.exe";
#else
    textEditorForScript = "";
    textEditorForShaders = "";
    textEditorForBspaces = "";
    textureEditor = "";
#endif
    animEditor = "";

    terrainTextureExport = "";

    sTextureBrowserSettings.nCellSize = 128;

    // Experimental features settings
    sExperimentalFeaturesSettings.bTotalIlluminationEnabled = false;

    //
    // Asset Browser settings init
    //
    sAssetBrowserSettings.nThumbSize = 128;
    sAssetBrowserSettings.bShowLoadedInLevel = false;
    sAssetBrowserSettings.bShowUsedInLevel = false;
    sAssetBrowserSettings.bAutoSaveFilterPreset = true;
    sAssetBrowserSettings.bShowFavorites = false;
    sAssetBrowserSettings.bHideLods = false;
    sAssetBrowserSettings.bAutoChangeViewportSelection = false;
    sAssetBrowserSettings.bAutoFilterFromViewportSelection = false;

    //////////////////////////////////////////////////////////////////////////
    // FlowGraph
    //////////////////////////////////////////////////////////////////////////
    showFlowgraphNotification = true;

    //////////////////////////////////////////////////////////////////////////
    // Mannequin settings
    //////////////////////////////////////////////////////////////////////////
    mannequinSettings.trackSize = kMannequinTrackSizeDefault;
    mannequinSettings.bCtrlForScrubSnapping = false;
    mannequinSettings.timelineWheelZoomSpeed = 1.0f;

    smartOpenSettings.rect = QRect();

    //////////////////////////////////////////////////////////////////////////
    // Initialize GUI settings.
    //////////////////////////////////////////////////////////////////////////
    gui.bWindowsVista = QSysInfo::WindowsVersion >= QSysInfo::WV_VISTA;

    gui.nToolbarIconSize = 0;

    int lfHeight = 8;// -MulDiv(8, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72);
    gui.nDefaultFontHieght = lfHeight;
    gui.hSystemFont = QFont("Ms Shell Dlg 2", lfHeight, QFont::Normal);
    gui.hSystemFontBold = QFont("Ms Shell Dlg 2", lfHeight, QFont::Bold);
    gui.hSystemFontItalic = QFont("Ms Shell Dlg 2", lfHeight, QFont::Normal, true);

    bForceSkyUpdate = true;

    bIsSearchFilterActive = false;

    backgroundUpdatePeriod = 0;
    g_TemporaryLevelName = nullptr;

    sMetricsSettings.bEnableMetricsTracking = true;

    sliceSettings.dynamicByDefault = false;

    bEnableUI2 = false;

    new QtApplicationListener(); // Deletes itself when it's done.
}

//////////////////////////////////////////////////////////////////////////
void SEditorSettings::SaveValue(const char* sSection, const char* sKey, int value)
{
    const SettingsGroup sg(sSection);
    s_editorSettings()->setValue(sKey, value);

    if (!bSettingsManagerMode)
    {
        if (GetIEditor()->GetSettingsManager())
        {
            GetIEditor()->GetSettingsManager()->SaveSetting(sSection, sKey, value);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void SEditorSettings::SaveValue(const char* sSection, const char* sKey, const QColor& value)
{
    const SettingsGroup sg(sSection);
    s_editorSettings()->setValue(sKey, QVariant::fromValue<int>(RGB(value.red(), value.green(), value.blue())));

    if (!bSettingsManagerMode)
    {
        if (GetIEditor()->GetSettingsManager())
        {
            GetIEditor()->GetSettingsManager()->SaveSetting(sSection, sKey, value);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void SEditorSettings::SaveValue(const char* sSection, const char* sKey, float value)
{
    const SettingsGroup sg(sSection);
    s_editorSettings()->setValue(sKey, QString::number(value));

    if (!bSettingsManagerMode)
    {
        if (GetIEditor()->GetSettingsManager())
        {
            GetIEditor()->GetSettingsManager()->SaveSetting(sSection, sKey, value);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void SEditorSettings::SaveValue(const char* sSection, const char* sKey, const QString& value)
{
    const SettingsGroup sg(sSection);
    s_editorSettings()->setValue(sKey, value);

    if (!bSettingsManagerMode)
    {
        if (GetIEditor()->GetSettingsManager())
        {
            GetIEditor()->GetSettingsManager()->SaveSetting(sSection, sKey, value);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void SEditorSettings::LoadValue(const char* sSection, const char* sKey, int& value)
{
    if (bSettingsManagerMode)
    {
        if (GetIEditor()->GetSettingsManager())
        {
            GetIEditor()->GetSettingsManager()->LoadSetting(sSection, sKey, value);
        }

        SaveValue(sSection, sKey, value);
    }
    else
    {
        const SettingsGroup sg(sSection);
        value = s_editorSettings()->value(sKey, value).toInt();

        if (GetIEditor()->GetSettingsManager())
        {
            GetIEditor()->GetSettingsManager()->SaveSetting(sSection, sKey, value);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void SEditorSettings::LoadValue(const char* sSection, const char* sKey, QColor& value)
{
    if (bSettingsManagerMode)
    {
        if (GetIEditor()->GetSettingsManager())
        {
            GetIEditor()->GetSettingsManager()->LoadSetting(sSection, sKey, value);
        }

        SaveValue(sSection, sKey, value);
    }
    else
    {
        const SettingsGroup sg(sSection);
        int defaultValue = RGB(value.red(), value.green(), value.blue());
        int v = s_editorSettings()->value(sKey, QVariant::fromValue<int>(defaultValue)).toInt();
        value = QColor(GetRValue(v), GetGValue(v), GetBValue(v));

        if (GetIEditor()->GetSettingsManager())
        {
            GetIEditor()->GetSettingsManager()->SaveSetting(sSection, sKey, value);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void SEditorSettings::LoadValue(const char* sSection, const char* sKey, float& value)
{
    if (bSettingsManagerMode)
    {
        if (GetIEditor()->GetSettingsManager())
        {
            GetIEditor()->GetSettingsManager()->LoadSetting(sSection, sKey, value);
        }

        SaveValue(sSection, sKey, value);
    }
    else
    {
        const SettingsGroup sg(sSection);
        const QString defaultVal = s_editorSettings()->value(sKey, QString::number(value)).toString();
        value = defaultVal.toDouble();

        if (GetIEditor()->GetSettingsManager())
        {
            GetIEditor()->GetSettingsManager()->SaveSetting(sSection, sKey, value);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void SEditorSettings::LoadValue(const char* sSection, const char* sKey, bool& value)
{
    if (bSettingsManagerMode)
    {
        if (GetIEditor()->GetSettingsManager())
        {
            GetIEditor()->GetSettingsManager()->LoadSetting(sSection, sKey, value);
        }

        SaveValue(sSection, sKey, value);
    }
    else
    {
        const SettingsGroup sg(sSection);
        value = s_editorSettings()->value(sKey, value).toInt();

        if (GetIEditor()->GetSettingsManager())
        {
            GetIEditor()->GetSettingsManager()->SaveSetting(sSection, sKey, value);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void SEditorSettings::LoadValue(const char* sSection, const char* sKey, QString& value)
{
    if (bSettingsManagerMode)
    {
        if (GetIEditor()->GetSettingsManager())
        {
            GetIEditor()->GetSettingsManager()->LoadSetting(sSection, sKey, value);
        }

        SaveValue(sSection, sKey, value);
    }
    else
    {
        const SettingsGroup sg(sSection);
        value = s_editorSettings()->value(sKey, value).toString();

        if (GetIEditor()->GetSettingsManager())
        {
            GetIEditor()->GetSettingsManager()->SaveSetting(sSection, sKey, value);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void SEditorSettings::LoadValue(const char* sSection, const char* sKey, ESystemConfigSpec& value)
{
    if (bSettingsManagerMode)
    {
        int valueCheck = 0;

        if (GetIEditor()->GetSettingsManager())
        {
            GetIEditor()->GetSettingsManager()->LoadSetting(sSection, sKey, valueCheck);
        }

        if (valueCheck >= CONFIG_AUTO_SPEC && valueCheck < END_CONFIG_SPEC_ENUM)
        {
            value = (ESystemConfigSpec)valueCheck;
            SaveValue(sSection, sKey, value);
        }
    }
    else
    {
        const SettingsGroup sg(sSection);
        auto valuecheck = static_cast<ESystemConfigSpec>(s_editorSettings()->value(sKey, QVariant::fromValue<int>(value)).toInt());
        if (valuecheck >= CONFIG_AUTO_SPEC && valuecheck < END_CONFIG_SPEC_ENUM)
        {
            value = valuecheck;

            if (GetIEditor()->GetSettingsManager())
            {
                GetIEditor()->GetSettingsManager()->SaveSetting(sSection, sKey, value);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void SEditorSettings::Save()
{
    QString strStringPlaceholder;

    // Save settings to registry.
    SaveValue("Settings", "UndoLevels", undoLevels);
    SaveValue("Settings", "UndoSliceOverrideSaveValue", m_undoSliceOverrideSaveValue);
    SaveValue("Settings", "ShowDashboardAtStartup", bShowDashboardAtStartup);
    SaveValue("Settings", "ShowCircularDependencyError", m_showCircularDependencyError);
    SaveValue("Settings", "AutoloadLastLevelAtStartup", bAutoloadLastLevelAtStartup);
    SaveValue("Settings", "MuteAudio", bMuteAudio);
    SaveValue("Settings", "AutoBackup", autoBackupEnabled);
    SaveValue("Settings", "AutoBackupTime", autoBackupTime);
    SaveValue("Settings", "AutoBackupMaxCount", autoBackupMaxCount);
    SaveValue("Settings", "AutoRemindTime", autoRemindTime);
    SaveValue("Settings", "CameraMoveSpeed", cameraMoveSpeed);
    SaveValue("Settings", "CameraRotateSpeed", cameraRotateSpeed);
    SaveValue("Settings", "StylusMode", stylusMode);
    SaveValue("Settings", "RestoreViewportCamera", restoreViewportCamera);
    SaveValue("Settings", "WheelZoomSpeed", wheelZoomSpeed);
    SaveValue("Settings", "InvertYRotation", invertYRotation);
    SaveValue("Settings", "InvertPan", invertPan);
    SaveValue("Settings", "BrMultiplier", fBrMultiplier);
    SaveValue("Settings", "CameraFastMoveSpeed", cameraFastMoveSpeed);
    SaveValue("Settings", "PreviewGeometryWindow", bPreviewGeometryWindow);
    SaveValue("Settings", "GeometryBrowserPanel", bGeometryBrowserPanel);
    SaveValue("Settings", "AutoSaveTagPoints", bAutoSaveTagPoints);

    SaveValue("Settings\\Navigation", "NavigationContinuousUpdate", bNavigationContinuousUpdate);
    SaveValue("Settings\\Navigation", "NavigationShowAreas", bNavigationShowAreas);
    SaveValue("Settings\\Navigation", "NavigationDebugDisplay", bNavigationDebugDisplay);
    SaveValue("Settings\\Navigation", "NavigationDebugAgentType", navigationDebugAgentType);
    SaveValue("Settings\\Navigation", "VisualizeNavigationAccessibility", bVisualizeNavigationAccessibility);

    SaveValue("Settings", "BackupOnSave", bBackupOnSave);
    SaveValue("Settings", "SaveBackupMaxCount", backupOnSaveMaxCount);
    SaveValue("Settings", "ApplyConfigSpecInEditor", bApplyConfigSpecInEditor);

    SaveValue("Settings", "editorConfigSpec", editorConfigSpec);

    SaveValue("Settings", "TemporaryDirectory", strStandardTempDirectory);
    SaveValue("Settings", "EditorEnv", strEditorEnv);

    SaveValue("Settings", "ConsoleBackgroundColorTheme", (int)consoleBackgroundColorTheme);

    SaveValue("Settings", "ShowTimeInConsole", bShowTimeInConsole);
    SaveValue("Settings", "LayerDoubleClicking", bLayerDoubleClicking);

    SaveValue("Settings", "EnableSceneInspector", enableSceneInspector);
    SaveValue("Settings", "EnableLegacyUI", enableLegacyUI);
    SaveValue("Settings", "ViewportInteractionModel", newViewportInteractionModel);
    
    //////////////////////////////////////////////////////////////////////////
    // Viewport settings.
    //////////////////////////////////////////////////////////////////////////
    SaveValue("Settings", "AlwaysShowRadiuses", viewports.bAlwaysShowRadiuses);
    SaveValue("Settings", "AlwaysShowPrefabBounds", viewports.bAlwaysDrawPrefabBox);
    SaveValue("Settings", "AlwaysShowPrefabObjects", viewports.bAlwaysDrawPrefabInternalObjects);
    SaveValue("Settings", "Sync2DViews", viewports.bSync2DViews);
    SaveValue("Settings", "DefaultFov", viewports.fDefaultFov);
    SaveValue("Settings", "AspectRatio", viewports.fDefaultAspectRatio);
    SaveValue("Settings", "ShowSafeFrame", viewports.bShowSafeFrame);
    SaveValue("Settings", "HighlightSelectedGeometry", viewports.bHighlightSelectedGeometry);
    SaveValue("Settings", "HighlightSelectedVegetation", viewports.bHighlightSelectedVegetation);
    SaveValue("Settings", "HighlightMouseOverGeometry", viewports.bHighlightMouseOverGeometry);
    SaveValue("Settings", "ShowMeshStatsOnMouseOver", viewports.bShowMeshStatsOnMouseOver);
    SaveValue("Settings", "DrawEntityLabels", viewports.bDrawEntityLabels);
    SaveValue("Settings", "ShowTriggerBounds", viewports.bShowTriggerBounds);
    SaveValue("Settings", "ShowIcons", viewports.bShowIcons);
    SaveValue("Settings", "ShowSizeBasedIcons", viewports.bShowSizeBasedIcons);
    SaveValue("Settings", "ShowFrozenHelpers", viewports.nShowFrozenHelpers);
    SaveValue("Settings", "FillSelectedShapes", viewports.bFillSelectedShapes);
    SaveValue("Settings", "MapTextureResolution", viewports.nTopMapTextureResolution);
    SaveValue("Settings", "MapSwapXY", viewports.bTopMapSwapXY);
    SaveValue("Settings", "ShowGridGuide", viewports.bShowGridGuide);
    SaveValue("Settings", "HideMouseCursorOnCapture", viewports.bHideMouseCursorWhenCaptured);
    SaveValue("Settings", "DragSquareSize", viewports.nDragSquareSize);
    SaveValue("Settings", "EnableContextMenu", viewports.bEnableContextMenu);
    SaveValue("Settings", "ToolbarIconSize", gui.nToolbarIconSize);
    SaveValue("Settings", "WarningIconsDrawDistance", viewports.fWarningIconsDrawDistance);
    SaveValue("Settings", "ShowScaleWarnings", viewports.bShowScaleWarnings);
    SaveValue("Settings", "ShowRotationWarnings", viewports.bShowRotationWarnings);

    //////////////////////////////////////////////////////////////////////////
    // Gizmos.
    //////////////////////////////////////////////////////////////////////////
    SaveValue("Settings", "AxisGizmoSize", gizmo.axisGizmoSize);
    SaveValue("Settings", "AxisGizmoText", gizmo.axisGizmoText);
    SaveValue("Settings", "AxisGizmoMaxCount", gizmo.axisGizmoMaxCount);
    SaveValue("Settings", "HelpersScale", gizmo.helpersScale);
    SaveValue("Settings", "TagPointScaleMulti", gizmo.tagpointScaleMulti);
    SaveValue("Settings", "RulerSphereScale", gizmo.rulerSphereScale);
    SaveValue("Settings", "RulerSphereTrans", gizmo.rulerSphereTrans);
    //////////////////////////////////////////////////////////////////////////

    SaveValue("Settings", "TextEditorScript", textEditorForScript);
    SaveValue("Settings", "TextEditorShaders", textEditorForShaders);
    SaveValue("Settings", "TextEditorBSpaces", textEditorForBspaces);
    SaveValue("Settings", "TextureEditor", textureEditor);
    SaveValue("Settings", "AnimationEditor", animEditor);

    SaveEnableSourceControlFlag(true);

    SaveValue("Settings", "SaveOnlyModified", saveOnlyModified);
    SaveValue("Settings", "FreezeReadOnly", freezeReadOnly);
    SaveValue("Settings", "FrozenSelectable", frozenSelectable);


    //////////////////////////////////////////////////////////////////////////
    // Snapping Settings.
    SaveValue("Settings\\Snap", "ConstructPlaneSize", snap.constructPlaneSize);
    SaveValue("Settings\\Snap", "ConstructPlaneDisplay", snap.constructPlaneDisplay);
    SaveValue("Settings\\Snap", "SnapMarkerDisplay", snap.markerDisplay);
    SaveValue("Settings\\Snap", "SnapMarkerColor", snap.markerColor);
    SaveValue("Settings\\Snap", "SnapMarkerSize", snap.markerSize);
    SaveValue("Settings\\Snap", "GridUserDefined", snap.bGridUserDefined);
    SaveValue("Settings\\Snap", "GridGetFromSelected", snap.bGridGetFromSelected);
    //////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////
    // HyperGraph Colors
    //////////////////////////////////////////////////////////////////////////
    SaveValue("Settings\\HyperGraph", "Opacity", hyperGraphColors.opacity);
    SaveValue("Settings\\HyperGraph", "ColorArrow", hyperGraphColors.colorArrow);
    SaveValue("Settings\\HyperGraph", "ColorInArrowHighlighted", hyperGraphColors.colorInArrowHighlighted);
    SaveValue("Settings\\HyperGraph", "ColorOutArrowHighlighted", hyperGraphColors.colorOutArrowHighlighted);
    SaveValue("Settings\\HyperGraph", "ColorPortEdgeHighlighted", hyperGraphColors.colorPortEdgeHighlighted);
    SaveValue("Settings\\HyperGraph", "ColorArrowDisabled", hyperGraphColors.colorArrowDisabled);
    SaveValue("Settings\\HyperGraph", "ColorNodeOutline", hyperGraphColors.colorNodeOutline);
    SaveValue("Settings\\HyperGraph", "ColorNodeBkg", hyperGraphColors.colorNodeBkg);
    SaveValue("Settings\\HyperGraph", "ColorNodeSelected", hyperGraphColors.colorNodeSelected);
    SaveValue("Settings\\HyperGraph", "ColorTitleText", hyperGraphColors.colorTitleText);
    SaveValue("Settings\\HyperGraph", "ColorTitleTextSelected", hyperGraphColors.colorTitleTextSelected);
    SaveValue("Settings\\HyperGraph", "ColorText", hyperGraphColors.colorText);
    SaveValue("Settings\\HyperGraph", "ColorBackground", hyperGraphColors.colorBackground);
    SaveValue("Settings\\HyperGraph", "ColorGrid", hyperGraphColors.colorGrid);
    SaveValue("Settings\\HyperGraph", "BreakPoint", hyperGraphColors.colorBreakPoint);
    SaveValue("Settings\\HyperGraph", "BreakPointDisabled", hyperGraphColors.colorBreakPointDisabled);
    SaveValue("Settings\\HyperGraph", "BreakPointArrow", hyperGraphColors.colorBreakPointArrow);
    SaveValue("Settings\\HyperGraph", "EntityPortNotConnected", hyperGraphColors.colorEntityPortNotConnected);
    SaveValue("Settings\\HyperGraph", "Port", hyperGraphColors.colorPort);
    SaveValue("Settings\\HyperGraph", "PortSelected", hyperGraphColors.colorPortSelected);
    SaveValue("Settings\\HyperGraph", "EntityTextInvalid", hyperGraphColors.colorEntityTextInvalid);
    SaveValue("Settings\\HyperGraph", "DownArrow", hyperGraphColors.colorDownArrow);
    SaveValue("Settings\\HyperGraph", "CustomNodeBkg", hyperGraphColors.colorCustomNodeBkg);
    SaveValue("Settings\\HyperGraph", "CustomSelectedNodeBkg", hyperGraphColors.colorCustomSelectedNodeBkg);
    SaveValue("Settings\\HyperGraph", "PortDebugging", hyperGraphColors.colorPortDebugging);
    SaveValue("Settings\\HyperGraph", "PortDebuggingText", hyperGraphColors.colorPortDebuggingText);
    SaveValue("Settings\\HyperGraph", "QuickSearchBackground", hyperGraphColors.colorQuickSearchBackground);
    SaveValue("Settings\\HyperGraph", "QuickSearchResultText", hyperGraphColors.colorQuickSearchResultText);
    SaveValue("Settings\\HyperGraph", "QuickSearchCountText", hyperGraphColors.colorQuickSearchCountText);
    SaveValue("Settings\\HyperGraph", "QuickSearchBorder", hyperGraphColors.colorQuickSearchBorder);
    SaveValue("Settings\\HyperGraph", "ColorDebugNodeTitle", hyperGraphColors.colorDebugNodeTitle);
    SaveValue("Settings\\HyperGraph", "ColorDebugNode", hyperGraphColors.colorDebugNode);

    //////////////////////////////////////////////////////////////////////////
    // HyperGraph Expert
    //////////////////////////////////////////////////////////////////////////
    SaveValue("Settings\\HyperGraph", "EnableMigration", bFlowGraphMigrationEnabled);
    SaveValue("Settings\\HyperGraph", "ShowNodeIDs", bFlowGraphShowNodeIDs);
    SaveValue("Settings\\HyperGraph", "ShowToolTip", bFlowGraphShowToolTip);
    SaveValue("Settings\\HyperGraph", "EdgesOnTopOfNodes", bFlowGraphEdgesOnTopOfNodes);
    SaveValue("Settings\\HyperGraph", "HighlightEdges", bFlowGraphHighlightEdges);
    //////////////////////////////////////////////////////////////////////////

    SaveValue("Settings", "TerrainTextureExport", terrainTextureExport);

    //////////////////////////////////////////////////////////////////////////
    // Texture browser settings
    //////////////////////////////////////////////////////////////////////////
    SaveValue("Settings\\TextureBrowser", "Cell Size", sTextureBrowserSettings.nCellSize);

    //////////////////////////////////////////////////////////////////////////
    // Experimental features settings
    //////////////////////////////////////////////////////////////////////////
    SaveValue("Settings\\ExperimentalFeatures", "TotalIlluminationEnabled", sExperimentalFeaturesSettings.bTotalIlluminationEnabled);

    ///////////////////////////////////////////////////////////////////////////
    SaveValue("Settings\\SelectObjectDialog", "Columns", selectObjectDialog.columns);
    SaveValue("Settings\\SelectObjectDialog", "LastColumnSortDirection", selectObjectDialog.nLastColumnSortDirection);

    //////////////////////////////////////////////////////////////////////////
    // Asset browser settings
    //////////////////////////////////////////////////////////////////////////
    SaveValue("Settings\\AssetBrowser", "ThumbSize", sAssetBrowserSettings.nThumbSize);
    SaveValue("Settings\\AssetBrowser", "ShowLoadedInLevel", sAssetBrowserSettings.bShowLoadedInLevel);
    SaveValue("Settings\\AssetBrowser", "ShowUsedInLevel", sAssetBrowserSettings.bShowUsedInLevel);
    SaveValue("Settings\\AssetBrowser", "FilenameSearch", sAssetBrowserSettings.sFilenameSearch);
    SaveValue("Settings\\AssetBrowser", "PresetName", sAssetBrowserSettings.sPresetName);
    SaveValue("Settings\\AssetBrowser", "ShowDatabases", sAssetBrowserSettings.sVisibleDatabaseNames);
    SaveValue("Settings\\AssetBrowser", "ShowFavorites", sAssetBrowserSettings.bShowFavorites);
    SaveValue("Settings\\AssetBrowser", "HideLods", sAssetBrowserSettings.bHideLods);
    SaveValue("Settings\\AssetBrowser", "AutoSaveFilterPreset", sAssetBrowserSettings.bAutoSaveFilterPreset);
    SaveValue("Settings\\AssetBrowser", "AutoChangeViewportSelection", sAssetBrowserSettings.bAutoChangeViewportSelection);
    SaveValue("Settings\\AssetBrowser", "AutoFilterFromViewportSelection", sAssetBrowserSettings.bAutoFilterFromViewportSelection);
    SaveValue("Settings\\AssetBrowser", "VisibleColumnNames", sAssetBrowserSettings.sVisibleColumnNames);
    SaveValue("Settings\\AssetBrowser", "ColumnNames", sAssetBrowserSettings.sColumnNames);
    
    //////////////////////////////////////////////////////////////////////////
    // FlowGraph
    //////////////////////////////////////////////////////////////////////////
    SaveValue("Settings\\FlowGraph", "ShowFlowGraphNotification", showFlowgraphNotification);
    
    //////////////////////////////////////////////////////////////////////////
    // Deep Selection Settings
    //////////////////////////////////////////////////////////////////////////
    SaveValue("Settings", "DeepSelectionNearness", deepSelectionSettings.fRange);
    SaveValue("Settings", "StickDuplicate", deepSelectionSettings.bStickDuplicate);


    //////////////////////////////////////////////////////////////////////////
    // Object Highlight Colors
    //////////////////////////////////////////////////////////////////////////
    SaveValue("Settings\\ObjectColors", "prefabHighlight", objectColorSettings.prefabHighlight);
    SaveValue("Settings\\ObjectColors", "groupHighlight", objectColorSettings.groupHighlight);
    SaveValue("Settings\\ObjectColors", "entityHighlight", objectColorSettings.entityHighlight);
    SaveValue("Settings\\ObjectColors", "BBoxAlpha", objectColorSettings.fBBoxAlpha);
    SaveValue("Settings\\ObjectColors", "GeometryHighlightColor", objectColorSettings.geometryHighlightColor);
    SaveValue("Settings\\ObjectColors", "SolidBrushGeometryHighlightColor", objectColorSettings.solidBrushGeometryColor);
    SaveValue("Settings\\ObjectColors", "GeometryAlpha", objectColorSettings.fGeomAlpha);
    SaveValue("Settings\\ObjectColors", "ChildGeometryAlpha", objectColorSettings.fChildGeomAlpha);

    SaveValue("Settings", "ForceSkyUpdate", gSettings.bForceSkyUpdate);

    //////////////////////////////////////////////////////////////////////////
    // Vertex snapping settings
    //////////////////////////////////////////////////////////////////////////
    SaveValue("Settings\\VertexSnapping", "VertexCubeSize", vertexSnappingSettings.vertexCubeSize);
    SaveValue("Settings\\VertexSnapping", "RenderPenetratedBoundBox", vertexSnappingSettings.bRenderPenetratedBoundBox);

    //////////////////////////////////////////////////////////////////////////
    // Mannequin settings
    //////////////////////////////////////////////////////////////////////////
    SaveValue("Settings\\Mannequin", "TrackSize", mannequinSettings.trackSize);
    SaveValue("Settings\\Mannequin", "CtrlForScrubSnapping", mannequinSettings.bCtrlForScrubSnapping);
    SaveValue("Settings\\Mannequin", "TimelineWheelZoomSpeed", mannequinSettings.timelineWheelZoomSpeed);

    //////////////////////////////////////////////////////////////////////////
    // Smart file open settings
    //////////////////////////////////////////////////////////////////////////
    SaveValue("Settings\\SmartFileOpen", "LastSearchTerm", smartOpenSettings.lastSearchTerm);
    SaveValue("Settings\\SmartFileOpen", "DlgRect.Left", smartOpenSettings.rect.left());
    SaveValue("Settings\\SmartFileOpen", "DlgRect.Top", smartOpenSettings.rect.top());
    SaveValue("Settings\\SmartFileOpen", "DlgRect.Right", smartOpenSettings.rect.right());
    SaveValue("Settings\\SmartFileOpen", "DlgRect.Bottom", smartOpenSettings.rect.bottom());

    //////////////////////////////////////////////////////////////////////////
    // Metrics settings
    //////////////////////////////////////////////////////////////////////////
    SaveValue("Settings\\Metrics", "EnableMetricsTracking",    sMetricsSettings.bEnableMetricsTracking);

    //////////////////////////////////////////////////////////////////////////
    // Slice settings
    //////////////////////////////////////////////////////////////////////////
    SaveValue("Settings\\Slices", "DynamicByDefault", sliceSettings.dynamicByDefault);

    //////////////////////////////////////////////////////////////////////////
    // UI 2.0 Settings
    //////////////////////////////////////////////////////////////////////////
    SaveValue("Settings", "EnableUI20", bEnableUI2);

    /*
    //////////////////////////////////////////////////////////////////////////
    // Save paths.
    //////////////////////////////////////////////////////////////////////////
    for (int id = 0; id < EDITOR_PATH_LAST; id++)
    {
        for (int i = 0; i < searchPaths[id].size(); i++)
        {
            CString path = searchPaths[id][i];
            CString key;
            key.Format( "Paths","Path_%.2d_%.2d",id,i );
            SaveValue( "Paths",key,path );
        }
    }
    */

    s_editorSettings()->sync();
}

//////////////////////////////////////////////////////////////////////////
void SEditorSettings::Load()
{
    const int settingsVersion = s_editorSettings()->value(QStringLiteral("Settings/EditorSettingsVersion"), 0).toInt();

    if (settingsVersion != EditorSettingsVersion)
    {
        s_editorSettings()->setValue(QStringLiteral("Settings/EditorSettingsVersion"), EditorSettingsVersion);
        Save();
        return;
    }

    QString     strPlaceholderString;
    // Load settings from registry.
    LoadValue("Settings", "UndoLevels", undoLevels);
    LoadValue("Settings", "UndoSliceOverrideSaveValue", m_undoSliceOverrideSaveValue);  
    LoadValue("Settings", "ShowDashboardAtStartup", bShowDashboardAtStartup);
    LoadValue("Settings", "ShowCircularDependencyError", m_showCircularDependencyError);
    LoadValue("Settings", "AutoloadLastLevelAtStartup", bAutoloadLastLevelAtStartup);
    LoadValue("Settings", "MuteAudio", bMuteAudio);
    LoadValue("Settings", "AutoBackup", autoBackupEnabled);
    LoadValue("Settings", "AutoBackupTime", autoBackupTime);
    LoadValue("Settings", "AutoBackupMaxCount", autoBackupMaxCount);
    LoadValue("Settings", "AutoRemindTime", autoRemindTime);
    LoadValue("Settings", "CameraMoveSpeed", cameraMoveSpeed);
    LoadValue("Settings", "CameraRotateSpeed", cameraRotateSpeed);
    LoadValue("Settings", "StylusMode", stylusMode);
    LoadValue("Settings", "RestoreViewportCamera", restoreViewportCamera);
    LoadValue("Settings", "WheelZoomSpeed", wheelZoomSpeed);
    LoadValue("Settings", "InvertYRotation", invertYRotation);
    LoadValue("Settings", "InvertPan", invertPan);
    LoadValue("Settings", "BrMultiplier", fBrMultiplier);
    LoadValue("Settings", "CameraFastMoveSpeed", cameraFastMoveSpeed);
    LoadValue("Settings", "PreviewGeometryWindow", bPreviewGeometryWindow);
    LoadValue("Settings", "GeometryBrowserPanel", bGeometryBrowserPanel);
    LoadValue("Settings", "AutoSaveTagPoints", bAutoSaveTagPoints);

    LoadValue("Settings\\Navigation", "NavigationContinuousUpdate", bNavigationContinuousUpdate);
    LoadValue("Settings\\Navigation", "NavigationShowAreas", bNavigationShowAreas);
    LoadValue("Settings\\Navigation", "NavigationDebugDisplay", bNavigationDebugDisplay);
    LoadValue("Settings\\Navigation", "NavigationDebugAgentType", navigationDebugAgentType);
    LoadValue("Settings\\Navigation", "VisualizeNavigationAccessibility", bVisualizeNavigationAccessibility);

    LoadValue("Settings", "BackupOnSave", bBackupOnSave);
    LoadValue("Settings", "SaveBackupMaxCount", backupOnSaveMaxCount);
    LoadValue("Settings", "ApplyConfigSpecInEditor", bApplyConfigSpecInEditor);
    LoadValue("Settings", "editorConfigSpec", editorConfigSpec);


    LoadValue("Settings", "TemporaryDirectory", strStandardTempDirectory);
    LoadValue("Settings", "EditorEnv", strEditorEnv);

    int consoleBackgroundColorThemeInt = (int)consoleBackgroundColorTheme;
    LoadValue("Settings", "ConsoleBackgroundColorTheme", consoleBackgroundColorThemeInt);
    consoleBackgroundColorTheme = (ConsoleColorTheme)consoleBackgroundColorThemeInt;
    if (consoleBackgroundColorTheme != ConsoleColorTheme::Dark && consoleBackgroundColorTheme != ConsoleColorTheme::Light)
    {
        consoleBackgroundColorTheme = ConsoleColorTheme::Light;
    }

    LoadValue("Settings", "ShowTimeInConsole", bShowTimeInConsole);
    LoadValue("Settings", "LayerDoubleClicking", bLayerDoubleClicking);

    LoadValue("Settings", "EnableSceneInspector", enableSceneInspector);
    LoadValue("Settings", "EnableLegacyUI", enableLegacyUI);
    LoadValue("Settings", "ViewportInteractionModel", newViewportInteractionModel);
    
    //////////////////////////////////////////////////////////////////////////
    // Viewport Settings.
    //////////////////////////////////////////////////////////////////////////
    LoadValue("Settings", "AlwaysShowRadiuses", viewports.bAlwaysShowRadiuses);
    LoadValue("Settings", "AlwaysShowPrefabBounds", viewports.bAlwaysDrawPrefabBox);
    LoadValue("Settings", "AlwaysShowPrefabObjects", viewports.bAlwaysDrawPrefabInternalObjects);
    LoadValue("Settings", "Sync2DViews", viewports.bSync2DViews);
    LoadValue("Settings", "DefaultFov", viewports.fDefaultFov);
    LoadValue("Settings", "AspectRatio", viewports.fDefaultAspectRatio);
    LoadValue("Settings", "ShowSafeFrame", viewports.bShowSafeFrame);
    LoadValue("Settings", "HighlightSelectedGeometry", viewports.bHighlightSelectedGeometry);
    LoadValue("Settings", "HighlightSelectedVegetation", viewports.bHighlightSelectedVegetation);
    LoadValue("Settings", "HighlightMouseOverGeometry", viewports.bHighlightMouseOverGeometry);
    LoadValue("Settings", "ShowMeshStatsOnMouseOver", viewports.bShowMeshStatsOnMouseOver);
    LoadValue("Settings", "DrawEntityLabels", viewports.bDrawEntityLabels);
    LoadValue("Settings", "ShowTriggerBounds", viewports.bShowTriggerBounds);
    LoadValue("Settings", "ShowIcons", viewports.bShowIcons);
    LoadValue("Settings", "ShowSizeBasedIcons", viewports.bShowSizeBasedIcons);
    LoadValue("Settings", "ShowFrozenHelpers", viewports.nShowFrozenHelpers);
    LoadValue("Settings", "FillSelectedShapes", viewports.bFillSelectedShapes);
    LoadValue("Settings", "MapTextureResolution", viewports.nTopMapTextureResolution);
    LoadValue("Settings", "MapSwapXY", viewports.bTopMapSwapXY);
    LoadValue("Settings", "ShowGridGuide", viewports.bShowGridGuide);
    LoadValue("Settings", "HideMouseCursorOnCapture", viewports.bHideMouseCursorWhenCaptured);
    LoadValue("Settings", "DragSquareSize", viewports.nDragSquareSize);
    LoadValue("Settings", "EnableContextMenu", viewports.bEnableContextMenu);
    LoadValue("Settings", "ToolbarIconSize", gui.nToolbarIconSize);
    LoadValue("Settings", "WarningIconsDrawDistance", viewports.fWarningIconsDrawDistance);
    LoadValue("Settings", "ShowScaleWarnings", viewports.bShowScaleWarnings);
    LoadValue("Settings", "ShowRotationWarnings", viewports.bShowRotationWarnings);

    //////////////////////////////////////////////////////////////////////////
    // Gizmos.
    //////////////////////////////////////////////////////////////////////////
    LoadValue("Settings", "AxisGizmoSize", gizmo.axisGizmoSize);
    LoadValue("Settings", "AxisGizmoText", gizmo.axisGizmoText);
    LoadValue("Settings", "AxisGizmoMaxCount", gizmo.axisGizmoMaxCount);
    LoadValue("Settings", "HelpersScale", gizmo.helpersScale);
    LoadValue("Settings", "TagPointScaleMulti", gizmo.tagpointScaleMulti);
    LoadValue("Settings", "RulerSphereScale", gizmo.rulerSphereScale);
    LoadValue("Settings", "RulerSphereTrans", gizmo.rulerSphereTrans);
    //////////////////////////////////////////////////////////////////////////

    LoadValue("Settings", "TextEditorScript", textEditorForScript);
    LoadValue("Settings", "TextEditorShaders", textEditorForShaders);
    LoadValue("Settings", "TextEditorBSpaces", textEditorForBspaces);
    LoadValue("Settings", "TextureEditor", textureEditor);
    LoadValue("Settings", "AnimationEditor", animEditor);

    LoadEnableSourceControlFlag();

    LoadValue("Settings", "SaveOnlyModified", saveOnlyModified);
    LoadValue("Settings", "FreezeReadOnly", freezeReadOnly);
    LoadValue("Settings", "FrozenSelectable", frozenSelectable);

    //////////////////////////////////////////////////////////////////////////
    // Snapping Settings.
    LoadValue("Settings\\Snap", "ConstructPlaneSize", snap.constructPlaneSize);
    LoadValue("Settings\\Snap", "ConstructPlaneDisplay", snap.constructPlaneDisplay);
    LoadValue("Settings\\Snap", "SnapMarkerDisplay", snap.markerDisplay);
    LoadValue("Settings\\Snap", "SnapMarkerColor", snap.markerColor);
    LoadValue("Settings\\Snap", "SnapMarkerSize", snap.markerSize);
    LoadValue("Settings\\Snap", "GridUserDefined", snap.bGridUserDefined);
    LoadValue("Settings\\Snap", "GridGetFromSelected", snap.bGridGetFromSelected);
    //////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////
    // HyperGraph
    //////////////////////////////////////////////////////////////////////////
    LoadValue("Settings\\HyperGraph", "Opacity", hyperGraphColors.opacity);
    LoadValue("Settings\\HyperGraph", "ColorArrow", hyperGraphColors.colorArrow);
    LoadValue("Settings\\HyperGraph", "ColorInArrowHighlighted", hyperGraphColors.colorInArrowHighlighted);
    LoadValue("Settings\\HyperGraph", "ColorOutArrowHighlighted", hyperGraphColors.colorOutArrowHighlighted);
    LoadValue("Settings\\HyperGraph", "ColorPortEdgeHighlighted", hyperGraphColors.colorPortEdgeHighlighted);
    LoadValue("Settings\\HyperGraph", "ColorArrowDisabled", hyperGraphColors.colorArrowDisabled);
    LoadValue("Settings\\HyperGraph", "ColorNodeOutline", hyperGraphColors.colorNodeOutline);
    LoadValue("Settings\\HyperGraph", "ColorNodeBkg", hyperGraphColors.colorNodeBkg);
    LoadValue("Settings\\HyperGraph", "ColorNodeSelected", hyperGraphColors.colorNodeSelected);
    LoadValue("Settings\\HyperGraph", "ColorTitleText", hyperGraphColors.colorTitleText);
    LoadValue("Settings\\HyperGraph", "ColorTitleTextSelected", hyperGraphColors.colorTitleTextSelected);
    LoadValue("Settings\\HyperGraph", "ColorText", hyperGraphColors.colorText);
    LoadValue("Settings\\HyperGraph", "ColorBackground", hyperGraphColors.colorBackground);
    LoadValue("Settings\\HyperGraph", "ColorGrid", hyperGraphColors.colorGrid);
    LoadValue("Settings\\HyperGraph", "BreakPoint", hyperGraphColors.colorBreakPoint);
    LoadValue("Settings\\HyperGraph", "BreakPointDisabled", hyperGraphColors.colorBreakPointDisabled);
    LoadValue("Settings\\HyperGraph", "BreakPointArrow", hyperGraphColors.colorBreakPointArrow);
    LoadValue("Settings\\HyperGraph", "EntityPortNotConnected", hyperGraphColors.colorEntityPortNotConnected);
    LoadValue("Settings\\HyperGraph", "Port", hyperGraphColors.colorPort);
    LoadValue("Settings\\HyperGraph", "PortSelected", hyperGraphColors.colorPortSelected);
    LoadValue("Settings\\HyperGraph", "EntityTextInvalid", hyperGraphColors.colorEntityTextInvalid);
    LoadValue("Settings\\HyperGraph", "DownArrow", hyperGraphColors.colorDownArrow);
    LoadValue("Settings\\HyperGraph", "CustomNodeBkg", hyperGraphColors.colorCustomNodeBkg);
    LoadValue("Settings\\HyperGraph", "CustomSelectedNodeBkg", hyperGraphColors.colorCustomSelectedNodeBkg);
    LoadValue("Settings\\HyperGraph", "PortDebugging", hyperGraphColors.colorPortDebugging);
    LoadValue("Settings\\HyperGraph", "PortDebuggingText", hyperGraphColors.colorPortDebuggingText);
    LoadValue("Settings\\HyperGraph", "QuickSearchBackground", hyperGraphColors.colorQuickSearchBackground);
    LoadValue("Settings\\HyperGraph", "QuickSearchResultText", hyperGraphColors.colorQuickSearchResultText);
    LoadValue("Settings\\HyperGraph", "QuickSearchCountText", hyperGraphColors.colorQuickSearchCountText);
    LoadValue("Settings\\HyperGraph", "QuickSearchBorder", hyperGraphColors.colorQuickSearchBorder);
    LoadValue("Settings\\HyperGraph", "ColorDebugNodeTitle", hyperGraphColors.colorDebugNodeTitle);
    LoadValue("Settings\\HyperGraph", "ColorDebugNode", hyperGraphColors.colorDebugNode);
    //////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////
    // HyperGraph Expert
    //////////////////////////////////////////////////////////////////////////
    LoadValue("Settings\\HyperGraph", "EnableMigration", bFlowGraphMigrationEnabled);
    LoadValue("Settings\\HyperGraph", "ShowNodeIDs", bFlowGraphShowNodeIDs);
    LoadValue("Settings\\HyperGraph", "ShowToolTip", bFlowGraphShowToolTip);
    LoadValue("Settings\\HyperGraph", "EdgesOnTopOfNodes", bFlowGraphEdgesOnTopOfNodes);
    LoadValue("Settings\\HyperGraph", "HighlightEdges", bFlowGraphHighlightEdges);

    //////////////////////////////////////////////////////////////////////////

    LoadValue("Settings", "TerrainTextureExport", terrainTextureExport);

    //////////////////////////////////////////////////////////////////////////
    // Texture browser settings
    //////////////////////////////////////////////////////////////////////////
    LoadValue("Settings\\TextureBrowser", "Cell Size", sTextureBrowserSettings.nCellSize);

    //////////////////////////////////////////////////////////////////////////
    // Experimental features settings
    //////////////////////////////////////////////////////////////////////////
    LoadValue("Settings\\ExperimentalFeatures", "TotalIlluminationEnabled", sExperimentalFeaturesSettings.bTotalIlluminationEnabled);

    //////////////////////////////////////////////////////////////////////////
    LoadValue("Settings\\SelectObjectDialog", "Columns", selectObjectDialog.columns);
    LoadValue("Settings\\SelectObjectDialog", "LastColumnSortDirection", selectObjectDialog.nLastColumnSortDirection);

    //////////////////////////////////////////////////////////////////////////
    // Asset browser settings
    //////////////////////////////////////////////////////////////////////////
    LoadValue("Settings\\AssetBrowser", "ThumbSize", sAssetBrowserSettings.nThumbSize);
    LoadValue("Settings\\AssetBrowser", "ShowLoadedInLevel", sAssetBrowserSettings.bShowLoadedInLevel);
    LoadValue("Settings\\AssetBrowser", "ShowUsedInLevel", sAssetBrowserSettings.bShowUsedInLevel);
    LoadValue("Settings\\AssetBrowser", "FilenameSearch", sAssetBrowserSettings.sFilenameSearch);
    LoadValue("Settings\\AssetBrowser", "PresetName", sAssetBrowserSettings.sPresetName);
    LoadValue("Settings\\AssetBrowser", "ShowDatabases", sAssetBrowserSettings.sVisibleDatabaseNames);
    LoadValue("Settings\\AssetBrowser", "ShowFavorites", sAssetBrowserSettings.bShowFavorites);
    LoadValue("Settings\\AssetBrowser", "HideLods", sAssetBrowserSettings.bHideLods);
    LoadValue("Settings\\AssetBrowser", "AutoSaveFilterPreset", sAssetBrowserSettings.bAutoSaveFilterPreset);
    LoadValue("Settings\\AssetBrowser", "AutoChangeViewportSelection", sAssetBrowserSettings.bAutoChangeViewportSelection);
    LoadValue("Settings\\AssetBrowser", "AutoFilterFromViewportSelection", sAssetBrowserSettings.bAutoFilterFromViewportSelection);
    LoadValue("Settings\\AssetBrowser", "VisibleColumnNames", sAssetBrowserSettings.sVisibleColumnNames);
    LoadValue("Settings\\AssetBrowser", "ColumnNames", sAssetBrowserSettings.sColumnNames);

    if (sAssetBrowserSettings.sVisibleColumnNames == ""
        || sAssetBrowserSettings.sColumnNames == "")
    {
        sAssetBrowserSettings.sColumnNames =
            sAssetBrowserSettings.sVisibleColumnNames = kDefaultColumnsForAssetBrowserList;
    }

    //////////////////////////////////////////////////////////////////////////
    // FlowGraph
    //////////////////////////////////////////////////////////////////////////
    LoadValue("Settings\\FlowGraph", "ShowFlowGraphNotification", showFlowgraphNotification);

    //////////////////////////////////////////////////////////////////////////
    // Deep Selection Settings
    //////////////////////////////////////////////////////////////////////////
    LoadValue("Settings", "DeepSelectionNearness", deepSelectionSettings.fRange);
    LoadValue("Settings", "StickDuplicate", deepSelectionSettings.bStickDuplicate);

    //////////////////////////////////////////////////////////////////////////
    // Object Highlight Colors
    //////////////////////////////////////////////////////////////////////////
    LoadValue("Settings\\ObjectColors", "PrefabHighlight", objectColorSettings.prefabHighlight);
    LoadValue("Settings\\ObjectColors", "GroupHighlight", objectColorSettings.groupHighlight);
    LoadValue("Settings\\ObjectColors", "EntityHighlight", objectColorSettings.entityHighlight);
    LoadValue("Settings\\ObjectColors", "BBoxAlpha", objectColorSettings.fBBoxAlpha);
    LoadValue("Settings\\ObjectColors", "GeometryHighlightColor", objectColorSettings.geometryHighlightColor);
    LoadValue("Settings\\ObjectColors", "SolidBrushGeometryHighlightColor", objectColorSettings.solidBrushGeometryColor);
    LoadValue("Settings\\ObjectColors", "GeometryAlpha", objectColorSettings.fGeomAlpha);
    LoadValue("Settings\\ObjectColors", "ChildGeometryAlpha", objectColorSettings.fChildGeomAlpha);

    LoadValue("Settings", "ForceSkyUpdate", gSettings.bForceSkyUpdate);

    //////////////////////////////////////////////////////////////////////////
    // Vertex snapping settings
    //////////////////////////////////////////////////////////////////////////
    LoadValue("Settings\\VertexSnapping", "VertexCubeSize", vertexSnappingSettings.vertexCubeSize);
    LoadValue("Settings\\VertexSnapping", "RenderPenetratedBoundBox", vertexSnappingSettings.bRenderPenetratedBoundBox);

    //////////////////////////////////////////////////////////////////////////
    // Mannequin settings
    //////////////////////////////////////////////////////////////////////////
    LoadValue("Settings\\Mannequin", "TrackSize", mannequinSettings.trackSize);
    LoadValue("Settings\\Mannequin", "CtrlForScrubSnapping", mannequinSettings.bCtrlForScrubSnapping);
    LoadValue("Settings\\Mannequin", "TimelineWheelZoomSpeed", mannequinSettings.timelineWheelZoomSpeed);

    //////////////////////////////////////////////////////////////////////////
    // Smart file open settings
    //////////////////////////////////////////////////////////////////////////
    int soRcLeft = 0;
    int soRcRight = 0;
    int soRcTop = 0;
    int soRcBottom = 0;

    LoadValue("Settings\\SmartFileOpen", "LastSearchTerm", smartOpenSettings.lastSearchTerm);
    LoadValue("Settings\\SmartFileOpen", "DlgRect.Left", soRcLeft);
    LoadValue("Settings\\SmartFileOpen", "DlgRect.Top", soRcTop);
    LoadValue("Settings\\SmartFileOpen", "DlgRect.Right", soRcRight);
    LoadValue("Settings\\SmartFileOpen", "DlgRect.Bottom", soRcBottom);

    // check for bad values
    QRect screenRc = QGuiApplication::primaryScreen()->availableGeometry();

    if (screenRc.contains(QPoint(soRcLeft, soRcTop))
        && screenRc.contains(QPoint(soRcRight, soRcBottom)))
    {
        smartOpenSettings.rect.setLeft(soRcLeft);
        smartOpenSettings.rect.setTop(soRcTop);
        smartOpenSettings.rect.setRight(soRcRight);
        smartOpenSettings.rect.setBottom(soRcBottom);
    }

    //////////////////////////////////////////////////////////////////////////
    // Metrics settings
    //////////////////////////////////////////////////////////////////////////
    LoadValue("Settings\\Metrics", "EnableMetricsTracking",    sMetricsSettings.bEnableMetricsTracking);

    //////////////////////////////////////////////////////////////////////////
    // Slice settings
    //////////////////////////////////////////////////////////////////////////
    LoadValue("Settings\\Slices", "DynamicByDefault", sliceSettings.dynamicByDefault);

    //////////////////////////////////////////////////////////////////////////
    // UI 2.0 Settings
    //////////////////////////////////////////////////////////////////////////
    LoadValue("Settings", "EnableUI20", bEnableUI2);

    //////////////////////////////////////////////////////////////////////////
    // Load paths.
    //////////////////////////////////////////////////////////////////////////
    for (int id = 0; id < EDITOR_PATH_LAST; id++)
    {
        if (id == EDITOR_PATH_UI_ICONS) // Skip UI icons path, not load it.
        {
            continue;
        }
        int i = 0;
        searchPaths[id].clear();
        while (true)
        {
            const QString key = QStringLiteral("Path_%1_%2").arg(id, 2, 10, QLatin1Char('.')).arg(i, 2, 10, QLatin1Char('.'));
            QString path;
            LoadValue("Paths", key.toUtf8().data(), path);
            if (path.isEmpty())
            {
                break;
            }
            searchPaths[id].push_back(path);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void SEditorSettings::PostInitApply()
{
    if (!gEnv || !gEnv->pConsole)
    {
        return;
    }

    // Create CVars.
    REGISTER_CVAR2("ed_highlightGeometry", &viewports.bHighlightMouseOverGeometry, viewports.bHighlightMouseOverGeometry, 0, "Highlight geometry when mouse over it");
    REGISTER_CVAR2("ed_showFrozenHelpers", &viewports.nShowFrozenHelpers, viewports.nShowFrozenHelpers, 0, "Show helpers of frozen objects");
    REGISTER_CVAR2("ed_lowercasepaths", &useLowercasePaths, useLowercasePaths, 0, "generate paths in lowercase");
    gEnv->pConsole->RegisterInt("fe_fbx_savetempfile", 0, 0, "When importing an FBX file into Facial Editor, this will save out a conversion FSQ to the Animations/temp folder for trouble shooting");

    REGISTER_CVAR2_CB("ed_toolbarIconSize", &gui.nToolbarIconSize, gui.nToolbarIconSize, VF_NULL, "Override size of the toolbar icons 0-default, 16,32,...", ToolbarIconSizeChanged);

    GetIEditor()->SetEditorConfigSpec(editorConfigSpec, GetISystem()->GetConfigPlatform());
    REGISTER_CVAR2("ed_backgroundUpdatePeriod", &backgroundUpdatePeriod, backgroundUpdatePeriod, 0, "Delay between frame updates (ms) when window is out of focus but not minimized. 0 = disable background update");
    REGISTER_CVAR2("ed_showErrorDialogOnLoad", &showErrorDialogOnLoad, showErrorDialogOnLoad, 0, "Show error dialog on level load");
    REGISTER_CVAR2_CB("ed_keepEditorActive", &keepEditorActive, 0, VF_NULL, "Keep the editor active, even if no focus is set", KeepEditorActiveChanged);
    REGISTER_CVAR2("g_TemporaryLevelName", &g_TemporaryLevelName, "temp_level", VF_NULL, "Temporary level named used for experimental levels.");

    gEnv->pConsole->RegisterInt("ed_showActorEntity", 0, VF_DUMPTODISK|VF_REQUIRE_APP_RESTART, "Change this to true to make the Actor Entity option appear (Legacy)");

CCryEditApp::instance()->KeepEditorActive(keepEditorActive > 0);
}

//////////////////////////////////////////////////////////////////////////
// needs to be called after crysystem has been loaded
void SEditorSettings::LoadDefaultGamePaths()
{
    //////////////////////////////////////////////////////////////////////////
    // Default paths.
    //////////////////////////////////////////////////////////////////////////
    if (searchPaths[EDITOR_PATH_OBJECTS].empty())
    {
        searchPaths[EDITOR_PATH_OBJECTS].push_back((Path::GetEditingGameDataFolder() + "/Objects").c_str());
    }
    if (searchPaths[EDITOR_PATH_TEXTURES].empty())
    {
        searchPaths[EDITOR_PATH_TEXTURES].push_back((Path::GetEditingGameDataFolder() + "/Textures").c_str());
    }
    if (searchPaths[EDITOR_PATH_SOUNDS].empty())
    {
        searchPaths[EDITOR_PATH_SOUNDS].push_back((Path::GetEditingGameDataFolder() + "/Sounds").c_str());
    }
    if (searchPaths[EDITOR_PATH_MATERIALS].empty())
    {
        searchPaths[EDITOR_PATH_MATERIALS].push_back((Path::GetEditingGameDataFolder() + "/Materials").c_str());
    }

    searchPaths[EDITOR_PATH_UI_ICONS].push_back("Editor\\UI\\Icons");
}

//////////////////////////////////////////////////////////////////////////
bool SEditorSettings::BrowseTerrainTexture(bool bIsSave)
{
    QString path;

    if (!terrainTextureExport.isEmpty())
    {
        path = Path::GetPath(terrainTextureExport);
    }
    else
    {
        path = Path::GetEditingGameDataFolder().c_str();
    }

    if (bIsSave)
    {
        return CFileUtil::SelectSaveFile("Bitmap Image File (*.bmp)", "bmp", path, terrainTextureExport);
    }
    else
    {
        return CFileUtil::SelectFile("Bitmap Image File (*.bmp)", path, terrainTextureExport);
    }
}

void EnableSourceControl(bool enable)
{
    // Source control component
    using SCRequestBus = AzToolsFramework::SourceControlConnectionRequestBus;
    SCRequestBus::Broadcast(&SCRequestBus::Events::EnableSourceControl, enable);
}

void SEditorSettings::SaveEnableSourceControlFlag(bool triggerUpdate /*= false*/)
{
    // Track the original source control value
    bool originalSourceControlFlag;
    LoadValue("Settings", "EnableSourceControl", originalSourceControlFlag);

    // Update only on change
    if (originalSourceControlFlag != enableSourceControl)
    {
        SaveValue("Settings", "EnableSourceControl", enableSourceControl);

        // If we are triggering any update for the source control flag, then set the control state
        if (triggerUpdate)
        {
            EnableSourceControl(enableSourceControl);
        }
    }
}

void SEditorSettings::LoadEnableSourceControlFlag()
{
    LoadValue("Settings", "EnableSourceControl", enableSourceControl);
    EnableSourceControl(enableSourceControl);
}
