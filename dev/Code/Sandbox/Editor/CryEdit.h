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

#ifndef CRYINCLUDE_EDITOR_CRYEDIT_H
#define CRYINCLUDE_EDITOR_CRYEDIT_H
#pragma once

#include <AzCore/Outcome/Outcome.h>
#include <AzFramework/Asset/AssetSystemBus.h>
#include "WipFeatureManager.h"

#include "CryEditDoc.h"
#include "ViewPane.h"

class CCryDocManager;
class CQuickAccessBar;
class CMatEditMainDlg;
class CCryEditDoc;
class CEditCommandLineInfo;
class CMainFrame;
class CConsoleDialog;
struct mg_connection;
struct mg_request_info;
struct mg_context;
struct IEventLoopHook;
class QAction;
class MainWindow;
class QSharedMemory;

class SANDBOX_API RecentFileList
{
public:
    RecentFileList();

    void Remove(int index);
    void Add(const QString& filename);

    int GetSize();

    void GetDisplayName(QString& name, int index, const QString& curDir);

    QString& operator[](int index);

    void ReadList();
    void WriteList();

    static const int Max = 12;
    QStringList m_arrNames;
    QSettings m_settings;
};


#define PROJECT_CONFIGURATOR_GEM_PAGE "Gems Settings"


/**
* Bus for controlling the application's idle processing (may include things like entity updates, ticks, viewport rendering, etc.).
* This is sometimes necessary in special event-processing loops to prevent long (or infinite) processing time because Idle Processing
* can perpetually generate more events.
*/
class EditorIdleProcessing : public AZ::EBusTraits
{
public:
    using Bus = AZ::EBus<EditorIdleProcessing>;
    static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
    static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

    /// Disable the Editor's idle processing. EnableIdleProcessing() must be called exactly once when special processing is complete.
    virtual void DisableIdleProcessing() {}

    /// Re-enables Idle Processing. Must be called exactly one time for every call to DisableIdleProcessing().
    virtual void EnableIdleProcessing() {}
};

using EditorIdleProcessingBus = AZ::EBus<EditorIdleProcessing>;


class SANDBOX_API CCryEditApp
    : public QObject
    , protected AzFramework::AssetSystemInfoBus::Handler
    , protected EditorIdleProcessingBus::Handler
{
    friend MainWindow;
    Q_OBJECT
public:
    enum ECreateLevelResult
    {
        ECLR_OK = 0,
        ECLR_ALREADY_EXISTS,
        ECLR_DIR_CREATION_FAILED,
        ECLR_MAX_PATH_EXCEEDED
    };

    enum class TerrainTextureExportTechnique
    {
        NoExport,
        UseDefault,
        PromptUser
    };

    struct TerrainTextureExportSettings
    {
        TerrainTextureExportSettings() = default;

        static constexpr uint32 DefaultTextureResolution = 4096;

        TerrainTextureExportSettings(TerrainTextureExportTechnique exportType) :
            m_exportType(exportType)
        {
        }

        TerrainTextureExportSettings(TerrainTextureExportTechnique exportType, uint32 resolution) :
            m_exportType(exportType),
            m_defaultResolution(resolution)
        {
        }

        TerrainTextureExportTechnique m_exportType = TerrainTextureExportTechnique::PromptUser;
        uint32 m_defaultResolution = DefaultTextureResolution;

    };

    CCryEditApp();
    ~CCryEditApp();

    static CCryEditApp* instance();

    bool GetRootEnginePath(QDir& rootEnginePath) const;
    void OnToggleSelection(bool hide);
    bool CreateLevel(bool& wasCreateLevelOperationCancelled);
    void LoadFile(QString fileName);
    void ForceNextIdleProcessing() { m_bForceProcessIdle = true; }
    void KeepEditorActive(bool bActive) { m_bKeepEditorActive = bActive; };
    bool IsInTestMode() { return m_bTestMode; };
    bool IsInPreviewMode() { return m_bPreviewMode; };
    bool IsInExportMode() { return m_bExportMode; };
    bool IsInConsoleMode() { return m_bConsoleMode; };
    bool IsInLevelLoadTestMode() { return m_bLevelLoadTestMode; }
    bool IsInRegularEditorMode();
    bool IsExiting() const { return m_bExiting; }
    void EnableAccelerator(bool bEnable);
    void SaveAutoBackup();
    void SaveAutoRemind();
    void ExportToGame(const TerrainTextureExportSettings& terrainTextureSettings, bool bNoMsgBox = true);
    //! \param sTitleStr overwrites the default title - "Sandbox Editor 3 (tm)"
    void SetEditorWindowTitle(QString sTitleStr = QString(), QString sPreTitleStr = QString(), QString sPostTitleStr = QString());
    BOOL RegDelnodeRecurse(HKEY hKeyRoot, LPTSTR lpSubKey);
    RecentFileList* GetRecentFileList();
    virtual void AddToRecentFileList(const QString& lpszPathName);
    ECreateLevelResult CreateLevel(const QString& levelName, int resolution, int unitSize, bool bUseTerrain, QString& fullyQualifiedLevelName, const TerrainTextureExportSettings& terrainTextureSettings);
    void CloseCurrentLevel();
    static void InitDirectory();
    BOOL FirstInstance(bool bForceNewInstance = false);
    void InitFromCommandLine(CEditCommandLineInfo& cmdInfo);
    BOOL CheckIfAlreadyRunning();
    //! @return successful outcome if initialization succeeded. or failed outcome with error message.
    AZ::Outcome<void, AZStd::string> InitGameSystem(HWND hwndForInputSystem);
    void CreateSplashScreen();
    void InitPlugins();
    bool InitGame();

    BOOL InitConsole();
    int IdleProcessing(bool bBackground);
    bool IsWindowInForeground();
    void RunInitPythonScript(CEditCommandLineInfo& cmdInfo);
    void RegisterEventLoopHook(IEventLoopHook* pHook);
    void UnregisterEventLoopHook(IEventLoopHook* pHook);

    void DisableIdleProcessing() override;
    void EnableIdleProcessing() override;

    // Check for credentials - if found call OpenAWSConsoleFederated using the provided link, otherwise open the signup page
    void OnAWSLaunchConsolePage(const QString& str);

    bool IsProjectConfiguratorRunning() const;

    bool ToProjectConfigurator(const QString& msg, const QString& caption, const QString& location);

    bool ToExternalToolPrompt(const QString& msg, const QString& caption);
    bool OpenProjectConfiguratorSwitchProject();
    bool OpenProjectConfigurator(const QString& startPage) const;

    bool OpenSetupAssistant() const;
    QString GetRootEnginePath() const;
    // Overrides
    // ClassWizard generated virtual function overrides
public:
    virtual BOOL InitInstance();
    virtual int ExitInstance(int exitCode = 0);
    virtual BOOL OnIdle(LONG lCount);
    virtual CCryEditDoc* OpenDocumentFile(LPCTSTR lpszFileName);

    CCryDocManager* GetDocManager() { return m_pDocManager; }

    void RegisterActionHandlers();

    // Implementation
    void OnCreateLevel();
    void OnOpenLevel();
    void OnCreateSlice();
    void OnOpenSlice();
    void OnAppAbout();
    void OnOnlineDocumentation();
    void OnDocumentationTutorials();
    void OnDocumentationGlossary();
    void OnDocumentationLumberyard();
    void OnDocumentationGamelift();
    void OnDocumentationReleaseNotes();
    void OnDocumentationGameDevBlog();
    void OnDocumentationGettingStartedGuide();
    void OnDocumentationTwitchChannel();
    void OnDocumentationForums();
    void OnDocumentationAWSSupport();
    void OnDocumentationFeedback();
    void OnAWSCredentialEditor();
    void OnAWSResourceManagement();
    void OnAWSActiveDeployment();
    void OnAWSGameliftLearn();
    void OnAWSGameliftConsole();
    void OnAWSGameliftGetStarted();
    void OnAWSGameliftTrialWizard();
    void OnAWSCognitoConsole();
    void OnAWSDeviceFarmConsole();
    void OnAWSDynamoDBConsole();
    void OnAWSS3Console();
    void OnAWSLambdaConsole();
    void OnAWSLaunch();
    void OnAWSLaunchUpdate(QAction* action);
    void OnHowToSetUpCloudCanvas();
    void OnLearnAboutGameLift();
    void OnCommercePublish();
    void OnCommerceMerch();
    void OnContactSupportBug();
    void ToolTerrain();
    // Confetti Begin: Jurecka
    void OnOpenParticleEditor();
    // Confetti End: Jurecka
    void ProceduralCreation();
    void SaveTagLocations();
    void MeasurementSystemTool();
    void ToolLighting();
    void TerrainTextureExport();
    void RefineTerrainTextureTiles();
    void ToolTexture();
    void GenerateTerrainTextureWithPrompts();
    void GenerateTerrainTexture(const TerrainTextureExportSettings& exportSettings);
    void OnUpdateGenerateTerrainTexture(QAction* action);
    void GenerateTerrain();
    void OnUpdateGenerateTerrain(QAction* action);
    void SelectExportPlatform();
    void OnRealtimeAutoSync();
    void OnRealtimeAutoSyncUpdateUI(/*CCmdUI* pCmdUI*/);
    void OnExportSelectedObjects();
    void OnExportTerrainArea();
    void OnExportTerrainAreaWithObjects();
    void OnUpdateExportTerrainArea(QAction* action);
    void OnEditHold();
    void OnEditFetch();
    void OnCancelMode();
    void OnGeneratorsStaticobjects();
    void OnFileCreateopenlevel();
    void OnFileExportToGameNoSurfaceTexture();
    void OnEditInsertObject();
    void OnViewSwitchToGame();
    void OnViewDeploy();
    void OnEditSelectAll();
    void OnEditSelectNone();
    void OnEditDelete();
    void DeleteSelectedEntities(bool includeDescendants);
    void OnMoveObject();
    void OnSelectObject();
    void OnRenameObj();
    void OnSetHeight();
    void OnScriptCompileScript();
    void OnScriptEditScript();
    void OnEditmodeMove();
    void OnEditmodeRotate();
    void OnEditmodeScale();
    void OnEditToolLink();
    void OnUpdateEditToolLink(QAction* action);
    void OnEditToolUnlink();
    void OnUpdateEditToolUnlink(QAction* action);
    void OnEditmodeSelect();
    void OnEditEscape();
    void OnObjectSetArea();
    void OnObjectSetHeight();
    void OnObjectVertexSnapping();
    void OnUpdateEditmodeVertexSnapping(QAction* action);
    void OnUpdateEditmodeSelect(QAction* action);
    void OnUpdateEditmodeMove(QAction* action);
    void OnUpdateEditmodeRotate(QAction* action);
    void OnUpdateEditmodeScale(QAction* action);
    void OnObjectmodifyFreeze();
    void OnObjectmodifyUnfreeze();
    void OnEditmodeSelectarea();
    void OnUpdateEditmodeSelectarea(QAction* action);
    void OnSelectAxisX();
    void OnSelectAxisY();
    void OnSelectAxisZ();
    void OnSelectAxisXy();
    void OnUpdateSelectAxisX(QAction* action);
    void OnUpdateSelectAxisXy(QAction* action);
    void OnUpdateSelectAxisY(QAction* action);
    void OnUpdateSelectAxisZ(QAction* action);
    void OnUndo();
    void OnEditClone();
    void OnSelectionSave();
    void OnOpenAssetImporter();
    void OnSelectionLoad();
    void OnConvertSelectionToDesignerObject();
    void OnConvertSelectionToComponentEntity();
    void OnUpdateSelected(QAction* action);
    void OnAlignObject();
    void OnAlignToVoxel();
    void OnAlignToGrid();
    void OnUpdateAlignObject(QAction* action);
    void OnUpdateAlignToVoxel(QAction* action);
    void OnGroupAttach();
    void OnUpdateGroupAttach(QAction* action);
    void OnGroupClose();
    void OnUpdateGroupClose(QAction* action);
    void OnGroupDetach();
    void OnUpdateGroupDetach(QAction* action);
    void OnGroupMake();
    void OnUpdateGroupMake(QAction* action);
    void OnGroupOpen();
    void OnUpdateGroupOpen(QAction* action);
    void OnGroupUngroup();
    void OnUpdateGroupUngroup(QAction* action);
    void OnCloudsCreate();
    void OnCloudsDestroy();
    void OnUpdateCloudsDestroy(QAction* action);
    void OnCloudsOpen();
    void OnUpdateCloudsOpen(QAction* action);
    void OnCloudsClose();
    void OnUpdateCloudsClose(QAction* action);
    void OnMissionRename();
    void OnMissionSelect();
    void OnLockSelection();
    void OnEditLevelData();
    void OnFileEditLogFile();
    void OnFileResaveSlices();
    void OnFileEditEditorini();
    void OnSelectAxisTerrain();
    void OnSelectAxisSnapToAll();
    void OnUpdateSelectAxisTerrain(QAction* action);
    void OnUpdateSelectAxisSnapToAll(QAction* action);
    void OnPreferences();
    void OnReloadTextures();
    void OnReloadAllScripts();
    void OnReloadEntityScripts();
    void OnReloadActorScripts();
    void OnReloadItemScripts();
    void OnReloadAIScripts();
    void OnReloadGeometry();
    void OnReloadTerrain();
    void OnRedo();
    void OnUpdateRedo(QAction* action);
    void OnUpdateUndo(QAction* action);
    void OnLayerSelect();
    void OnTerrainCollision();
    void OnTerrainCollisionUpdate(QAction* action);
    void OnGenerateCgfThumbnails();
    void OnAIGenerateAll();
    void OnAIGenerateTriangulation();
    void OnAIGenerateWaypoint();
    void OnAIGenerateFlightNavigation();
    void OnAIGenerate3dvolumes();
    void OnAIValidateNavigation();
    void OnAIGenerate3DDebugVoxels();
    void OnAIClearAllNavigation();
    void OnAIGenerateSpawners();
    void OnAIGenerateCoverSurfaces();
    void OnAINavigationShowAreas();
    void OnAINavigationShowAreasUpdate(QAction* action);
    void OnAINavigationEnableContinuousUpdate();
    void OnAINavigationEnableContinuousUpdateUpdate(QAction* action);
    void OnAINavigationNewArea();
    void OnAINavigationFullRebuild();
    void OnAINavigationAddSeed();
    void OnVisualizeNavigationAccessibility();
    void OnVisualizeNavigationAccessibilityUpdate(QAction* action);
    void OnAINavigationDisplayAgent();
    void OnSwitchPhysics();
    void OnSwitchPhysicsUpdate(QAction* action);
    void OnSyncPlayer();
    void OnSyncPlayerUpdate(QAction* action);
    void OnResourcesReduceworkingset();
    void OnToolsEquipPacksEdit();
    void OnToolsUpdateProcVegetation();
    void OnDummyCommand() {};
    void OnShowHelpers();
    void OnStartStop();
    void OnNextKey();
    void OnPrevKey();
    void OnNextFrame();
    void OnPrevFrame();
    void OnSelectAll();
    void OnKeyAll();
    void OnToggleMultiplayer();
    void OnToggleMultiplayerUpdate(QAction* action);
    void OnTerrainResizeterrain();
    void OnUpdateTerrainResizeterrain(QAction* action);
    void OnFileSave();
    void OnFileSaveExternalLayers();
    void OnFileConvertLegacyEntities();
    void OnUpdateDocumentReady(QAction* action);
    void OnUpdateFileOpen(QAction* action);
    void OnUpdateCurrentLayer(QAction* action);
    void OnUpdateNonGameMode(QAction* action);

    void OnOpenProjectConfiguratorGems();
    void OnOpenProjectConfiguratorSwitchProject();

    void OnRefreshAssetDatabases();

signals:
    void RefreshAssetDatabases();

protected:
    // ------- AzFramework::AssetSystemInfoBus::Handler ------
    void OnError(AzFramework::AssetSystem::AssetSystemErrors error) override;
    // -------------------------------------------

private:
    void InitLevel(const CEditCommandLineInfo& cmdInfo);

    CMainFrame* GetMainFrame() const;
    void InitAccelManager();
    void WriteConfig();
    void TagLocation(int index);
    void GotoTagLocation(int index);
    void LoadTagLocations();
    bool UserExportToGame(const TerrainTextureExportSettings& terrainTextureSettings, bool bReloadTerrain, bool bNoMsgBox = true);
    static UINT LogoThread(LPVOID pParam);
    static void ShowSplashScreen(CCryEditApp* app);
    static void CloseSplashScreen();
    static void OutputStartupMessage(QString str);
    bool HasAWSCredentials();
    void ConvertLegacyEntities(bool closeOnCancel = true);
    bool ShowEnableDisableCryEntityRemovalDialog();

    void OpenAWSConsoleFederated(const QString& str);

    bool FixDanglingSharedMemory(const QString& sharedMemName) const;

    //! Displays level load errors after a certain number of idle frames have been processed.
    //! Due to the asyncrhonous nature of loading assets any errors that are reported by components
    //! can happen after the level is loaded. This method will wait for a few idle updates and then
    //! display the load errors to ensure all errors are displayed properly.
    void DisplayLevelLoadErrors();

#if AZ_TESTS_ENABLED
    //! Runs tests in the plugin specified as the file argument to the command line,
    //! passing in all extra parameters after the bootstrap test flag.
    //!
    //! Editor.exe -bootstrap_plugin_tests <plugin_path>/PluginName.dll [google test parameters]
    int RunPluginUnitTests(CEditCommandLineInfo& cmdInfo);
#endif

    class CEditorImpl* m_pEditor = nullptr;
    static CCryEditApp* s_currentInstance;
    //! True if editor is in test mode.
    //! Test mode is a special mode enabled when Editor ran with /test command line.
    //! In this mode editor starts up, but exit immediately after all initialization.
    bool m_bTestMode = false;
    bool m_bPrecacheShaderList = false;
    bool m_bPrecacheShaders = false;
    bool m_bPrecacheShadersLevels = false;
    bool m_bMergeShaders = false;
    bool m_bStatsShaderList = false;
    bool m_bStatsShaders = false;
    //! In this mode editor will load specified cry file, export t, and then close.
    bool m_bExportMode = false;
    QString m_exportFile;
    //! If application exiting.
    bool m_bExiting = false;
    //! True if editor is in preview mode.
    //! In this mode only very limited functionality is available and only for fast preview of models.
    bool m_bPreviewMode = false;
    // Only console window is created.
    bool m_bConsoleMode = false;
    // Skip showing the WelcomeScreenDialog
    bool m_bSkipWelcomeScreenDialog = false;
    // Level load test mode
    bool m_bLevelLoadTestMode = false;
    //! Current file in preview mode.
    char m_sPreviewFile[_MAX_PATH];
    //! True if "/runpython" was passed as a flag.
    bool m_bRunPythonScript = false;
    //! File to run on startup
    QString m_execFile;
    CMatEditMainDlg* m_pMatEditDlg = nullptr;
    CConsoleDialog* m_pConsoleDialog = nullptr;
    Vec3 m_tagLocations[12];
    Ang3 m_tagAngles[12];
    float m_fastRotateAngle = 45.0f;
    float m_moveSpeedStep = 0.1f;

    ULONG_PTR m_gdiplusToken;
    QSharedMemory* m_mutexApplication = nullptr;
    //! was the editor active in the previous frame ... needed to detect if the game lost focus and
    //! dispatch proper SystemEvent (needed to release input keys)
    bool m_bPrevActive = false;
    // If this flag is set, the next OnIdle() will update, even if the app is in the background, and then
    // this flag will be reset.
    bool m_bForceProcessIdle = false;
    // Keep the editor alive, even if no focus is set
    bool m_bKeepEditorActive = false;
    // Currently creating a new level
    bool m_creatingNewLevel = false;
    bool m_openingLevel = false;
    bool m_savingLevel = false;
    // Flag indicating if the errors for the currently loaded level have been displayed
    bool m_levelErrorsHaveBeenDisplayed = false;
    // Number of idle frames that have passed before displaying level errors
    int m_numBeforeDisplayErrorFrames = 0;

    QString m_lastOpenLevelPath;
    CQuickAccessBar* m_pQuickAccessBar = nullptr;
    IEventLoopHook* m_pEventLoopHook = nullptr;
    QString m_rootEnginePath;

    class CMannequinChangeMonitor* m_pChangeMonitor = nullptr;

    int m_disableIdleProcessingCounter = 0; //!< Counts requests to disable idle processing. When non-zero, idle processing will be disabled.

#if AZ_TESTS_ENABLED
    struct BootstrapTestInfo
    {
        BootstrapTestInfo()
            : didRun(false)
            , exitCode(0) { }

        bool didRun;
        int exitCode;
    } m_bootstrapTestInfo;
#endif

    CCryDocManager* m_pDocManager = nullptr;

private:
    void OnEditHide();
    void OnUpdateEditHide(QAction* action);
    void OnEditShowLastHidden();
    void OnEditUnhideall();
    void OnEditFreeze();
    void OnUpdateEditFreeze(QAction* action);
    void OnEditUnfreezeall();
    void OnSnap();
    void OnWireframe();
    void OnUpdateWireframe(QAction* action);
    void OnViewGridsettings();
    void OnViewConfigureLayout();

    // Tag Locations.
    void OnTagLocation1();
    void OnTagLocation2();
    void OnTagLocation3();
    void OnTagLocation4();
    void OnTagLocation5();
    void OnTagLocation6();
    void OnTagLocation7();
    void OnTagLocation8();
    void OnTagLocation9();
    void OnTagLocation10();
    void OnTagLocation11();
    void OnTagLocation12();

    void OnGotoLocation1();
    void OnGotoLocation2();
    void OnGotoLocation3();
    void OnGotoLocation4();
    void OnGotoLocation5();
    void OnGotoLocation6();
    void OnGotoLocation7();
    void OnGotoLocation8();
    void OnGotoLocation9();
    void OnGotoLocation10();
    void OnGotoLocation11();
    void OnGotoLocation12();
    void OnToolsLogMemoryUsage();
    void OnTerrainExportblock();
    void OnTerrainImportblock();
    void OnUpdateTerrainExportblock(QAction* action);
    void OnUpdateTerrainImportblock(QAction* action);
    void OnCustomizeKeyboard();
    void OnToolsConfiguretools();
    void OnToolsScriptHelp();
    void OnExportIndoors();
    void OnViewCycle2dviewport();
    void OnDisplayGotoPosition();
    void OnDisplaySetVector();
    void OnSnapangle();
    void OnUpdateSnapangle(QAction* action);
    void OnRuler();
    void OnUpdateRuler(QAction* action);
    void OnRotateselectionXaxis();
    void OnRotateselectionYaxis();
    void OnRotateselectionZaxis();
    void OnRotateselectionRotateangle();
    void OnConvertselectionTobrushes();
    void OnConvertselectionTosimpleentity();
    void OnConvertselectionToGameVolume();
    void OnEditRenameobject();
    void OnChangemovespeedIncrease();
    void OnChangemovespeedDecrease();
    void OnChangemovespeedChangestep();
    void OnModifyAipointPicklink();
    void OnModifyAipointPickImpasslink();
    void OnMaterialAssigncurrent();
    void OnMaterialResettodefault();
    void OnMaterialGetmaterial();
    void OnPhysicsGetState();
    void OnPhysicsResetState();
    void OnPhysicsSimulateObjects();
    void OnFileSavelevelresources();
    void OnClearRegistryData();
    void OnValidatelevel();
    void OnValidateObjectPositions();
    void OnToolsPreferences();
    void OnGraphicsSettings();
    void OnEditInvertselection();
    void OnPrefabsMakeFromSelection();
    void OnUpdatePrefabsMakeFromSelection(QAction* action);
    void OnAddSelectionToPrefab();
    void OnUpdateAddSelectionToPrefab(QAction* action);
    void OnCloneSelectionFromPrefab();
    void OnUpdateCloneSelectionFromPrefab(QAction* action);
    void OnExtractSelectionFromPrefab();
    void OnUpdateExtractSelectionFromPrefab(QAction* action);
    void OnPrefabsOpenAll();
    void OnPrefabsCloseAll();
    void OnPrefabsRefreshAll();
    void OnToolterrainmodifySmooth();
    void OnTerrainmodifySmooth();
    void OnTerrainVegetation();
    void OnTerrainPaintlayers();
    void OnSwitchToDefaultCamera();
    void OnUpdateSwitchToDefaultCamera(QAction* action);
    void OnSwitchToSequenceCamera();
    void OnUpdateSwitchToSequenceCamera(QAction* action);
    void OnSwitchToSelectedcamera();
    void OnUpdateSwitchToSelectedCamera(QAction* action);
    void OnSwitchcameraNext();
    void OnOpenProceduralMaterialEditor();
    void OnOpenMaterialEditor();
    void OnOpenMannequinEditor();
    void OnOpenCharacterTool();
    void OnOpenDataBaseView();
    void OnOpenFlowGraphView();
    void OnOpenAssetBrowserView();
    void OnOpenTrackView();
    void OnOpenAudioControlsEditor();
    void OnOpenUICanvasEditor();
    void OnOpenAIDebugger();
    void OnOpenLayerEditor();
    void OnOpenTerrainEditor();
    void OnGotoViewportSearch();
    void OnBrushMakehollow();
    void OnBrushCsgcombine();
    void OnBrushCsgsubstruct();
    void OnBrushCsgintersect();
    void OnSubobjectmodeVertex();
    void OnSubobjectmodeEdge();
    void OnSubobjectmodeFace();
    void OnSubobjectmodePivot();
    void OnBrushCsgsubstruct2();
    void OnMaterialPicktool();
    void OnTimeOfDay();
    void OnResolveMissingObjects();
    void OnChangeGameSpec(UINT nID);
    void SetGameSpecCheck(ESystemConfigSpec spec, ESystemConfigPlatform platform, int &nCheck, bool &enable);
    void OnUpdateGameSpec(QAction* action);
    void OnOpenQuickAccessBar();
    void OnToolsEnableFileChangeMonitoring();
    void OnUpdateToolsEnableFileChangeMonitoring(/*CCmdUI* pCmdUI*/);

    ////////////////////////////////////////////////////////////////////////////
    //CryGame
    ////////////////////////////////////////////////////////////////////////////
    void OnGameP1AutoGen();
    ////////////////////////////////////////////////////////////////////////////

public:
    void ExportLevel(bool bExportToGame, bool bExportTexture, bool bAutoExport);
    static bool Command_ExportToEngine();
    static void SubObjectModeVertex();
    static void SubObjectModeEdge();
    static void SubObjectModeFace();
    static void SubObjectModePivot();

    void OnFileExportOcclusionMesh();
};

//////////////////////////////////////////////////////////////////////////
class CCrySingleDocTemplate 
    : public QObject
{
private:
    explicit CCrySingleDocTemplate(const QMetaObject* pDocClass)
        : QObject()
        , m_documentClass(pDocClass)
    {
    }
public:
    enum Confidence
    {
        noAttempt,
        maybeAttemptForeign,
        maybeAttemptNative,
        yesAttemptForeign,
        yesAttemptNative,
        yesAlreadyOpen
    };

    template<typename DOCUMENT>
    static CCrySingleDocTemplate* create()
    {
        return new CCrySingleDocTemplate(&DOCUMENT::staticMetaObject);
    }
    ~CCrySingleDocTemplate() {};
    // avoid creating another CMainFrame
    // close other type docs before opening any things
    virtual CCryEditDoc* OpenDocumentFile(LPCTSTR lpszPathName, BOOL bAddToMRU, BOOL bMakeVisible);
    virtual CCryEditDoc* OpenDocumentFile(LPCTSTR lpszPathName, BOOL bMakeVisible = TRUE);
    virtual Confidence MatchDocType(LPCTSTR lpszPathName, CCryEditDoc*& rpDocMatch);

private:
    const QMetaObject* m_documentClass = nullptr;
};

class CDocTemplate;
class CCryDocManager
{
    CCrySingleDocTemplate* m_pDefTemplate = nullptr;
public:
    CCryDocManager();
    CCrySingleDocTemplate* SetDefaultTemplate(CCrySingleDocTemplate* pNew);
    // Copied from MFC to get rid of the silly ugly unoverridable doc-type pick dialog
    virtual void OnFileNew();
    virtual BOOL DoPromptFileName(QString& fileName, UINT nIDSTitle,
        DWORD lFlags, BOOL bOpenFileDialog, CDocTemplate* pTemplate);
    virtual CCryEditDoc* OpenDocumentFile(LPCTSTR lpszFileName, BOOL bAddToMRU);

    QVector<CCrySingleDocTemplate*> m_templateList;
};

#include <AzCore/Component/Component.h>

namespace AzToolsFramework
{
    //! A component to reflect scriptable commands for the Editor
    class CryEditPythonHandler
        : public AZ::Component
    {
    public:
        AZ_COMPONENT(CryEditPythonHandler, "{D4B19973-54D9-44BD-9E70-6069462A0CDC}")
        virtual ~CryEditPythonHandler() = default;

        static void Reflect(AZ::ReflectContext* context);

        // AZ::Component ...
        void Activate() override {}
        void Deactivate() override {}

        class CryEditHandler
        {
        public:
            AZ_RTTI(CryEditHandler, "{6C1FD05A-2F39-4094-80D4-CA526676F13E}")
            virtual ~CryEditHandler() = default;
        };

        class CryEditCheckoutHandler
        {
        public:
            AZ_RTTI(CryEditCheckoutHandler, "{C65EF439-6754-4ACD-AEA2-196F2DBA0AF3}")
            virtual ~CryEditCheckoutHandler() = default;
        };
    };

} // namespace AzToolsFramework

#endif // CRYINCLUDE_EDITOR_CRYEDIT_H
