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

#include <precompiled.h>

#include <ISystem.h>
#include <IConsole.h>

#include <Editor/View/Windows/MainWindow.h>
#include <Editor/GraphCanvas/GraphCanvasEditorNotificationBusId.h>

#include <QSplitter>
#include <QListView>
#include <QFileDialog>
#include <QShortcut>
#include <QKeySequence>
#include <QKeyEvent>
#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsSceneEvent>
#include <QMimeData>
#include <QCoreApplication>
#include <QMessageBox>
#include <QDir>
#include <QDirIterator>
#include <QProgressDialog>
#include <QToolButton>

#include <ScriptEvents/ScriptEventsAsset.h>

#include <Editor/GraphCanvas/Components/MappingComponent.h>
#include <Editor/Undo/ScriptCanvasUndoCache.h>
#include <Editor/Undo/ScriptCanvasUndoManager.h>
#include <Editor/View/Dialogs/UnsavedChangesDialog.h>
#include <Editor/View/Dialogs/PreviewMessage.h>
#include <Editor/View/Dialogs/Settings.h>
#include <Editor/View/Widgets/NodeFavorites.h>
#include <Editor/View/Widgets/ScriptCanvasNodePaletteDockWidget.h>
#include <Editor/View/Widgets/PropertyGrid.h>
#include <Editor/View/Widgets/NodeOutliner.h>
#include <Editor/View/Widgets/CommandLine.h>
#include <Editor/View/Widgets/GraphTabBar.h>
#include <Editor/View/Widgets/NodeFavorites.h>
#include <Editor/View/Widgets/CanvasWidget.h>
#include <Editor/View/Widgets/LogPanel.h>
#include <Editor/View/Widgets/LoggingPanel/LoggingWindow.h>
#include <Editor/View/Widgets/MainWindowStatusWidget.h>
#include <Editor/View/Widgets/NodePalette/NodePaletteModel.h>
#include <Editor/View/Widgets/StatisticsDialog/ScriptCanvasStatisticsDialog.h>
#include <Editor/View/Widgets/VariablePanel/VariableDockWidget.h>
#include <Editor/View/Widgets/UnitTestPanel/UnitTestDockWidget.h>
#include <Editor/View/Widgets/ValidationPanel/GraphValidationDockWidget.h>

#include <Editor/View/Windows/ui_MainWindow.h>
#include <Editor/View/Windows/Tools/BatchConverter/BatchConverter.h>

#include <Editor/Model/EntityMimeDataHandler.h>

#include <Editor/Utilities/RecentAssetPath.h>

#include <Editor/Metrics.h>
#include <Editor/Settings.h>
#include <Editor/Nodes/NodeUtils.h>

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Asset/AssetManager.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/Component/EntityUtils.h>
#include <AzCore/Serialization/IdUtils.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Vector4.h>

#include <AzFramework/Asset/AssetCatalog.h>
#include <AzFramework/StringFunc/StringFunc.h>

#include <AzToolsFramework/AssetBrowser/AssetBrowserBus.h>
#include <AzToolsFramework/AssetBrowser/AssetBrowserModel.h>
#include <AzToolsFramework/API/EditorAssetSystemAPI.h>
#include <AzToolsFramework/ToolsComponents/EditorEntityIdContainer.h>
#include <AzToolsFramework/ToolsComponents/GenericComponentWrapper.h>
#include <AzToolsFramework/ToolsComponents/ToolsAssetCatalogBus.h>
#include <AzToolsFramework/UI/UICore/WidgetHelpers.h>

#include <ScriptCanvas/Core/ScriptCanvasBus.h>
#include <ScriptCanvas/Core/Graph.h>
#include <ScriptCanvas/Assets/ScriptCanvasAsset.h>
#include <ScriptCanvas/Assets/ScriptCanvasAssetHandler.h>
#include <ScriptCanvas/Assets/ScriptCanvasDocumentContext.h>

#include <GraphCanvas/GraphCanvasBus.h>
#include <GraphCanvas/Components/Nodes/NodeBus.h>
#include <GraphCanvas/Components/GeometryBus.h>
#include <GraphCanvas/Components/GridBus.h>
#include <GraphCanvas/Components/ViewBus.h>
#include <GraphCanvas/Components/VisualBus.h>
#include <GraphCanvas/Components/MimeDataHandlerBus.h>
#include <GraphCanvas/Components/Connections/ConnectionBus.h>

#include <GraphCanvas/Styling/Parser.h>
#include <GraphCanvas/Styling/Style.h>
#include <GraphCanvas/Widgets/AssetEditorToolbar/AssetEditorToolbar.h>
#include <GraphCanvas/Widgets/Bookmarks/BookmarkDockWidget.h>
#include <GraphCanvas/Widgets/GraphCanvasMimeContainer.h>
#include <GraphCanvas/Widgets/MiniMapGraphicsView/MiniMapGraphicsView.h>
#include <GraphCanvas/Widgets/GraphCanvasEditor/GraphCanvasEditorCentralWidget.h>
#include <GraphCanvas/Widgets/GraphCanvasGraphicsView/GraphCanvasGraphicsView.h>
#include <GraphCanvas/Widgets/EditorContextMenu/EditorContextMenu.h>
#include <GraphCanvas/Widgets/EditorContextMenu/ContextMenus/BookmarkContextMenu.h>
#include <GraphCanvas/Widgets/EditorContextMenu/ContextMenus/CollapsedNodeGroupContextMenu.h>
#include <GraphCanvas/Widgets/EditorContextMenu/ContextMenus/ConnectionContextMenu.h>
#include <GraphCanvas/Widgets/EditorContextMenu/ContextMenus/NodeGroupContextMenu.h>
#include <GraphCanvas/Widgets/EditorContextMenu/ContextMenus/NodeContextMenu.h>
#include <GraphCanvas/Widgets/EditorContextMenu/ContextMenus/CommentContextMenu.h>
#include <GraphCanvas/Widgets/EditorContextMenu/ContextMenus/SceneContextMenu.h>
#include <GraphCanvas/Widgets/EditorContextMenu/ContextMenus/SlotContextMenu.h>
#include <GraphCanvas/Utils/NodeNudgingController.h>
#include <GraphCanvas/Types/ConstructPresets.h>

#include "ScriptCanvasContextMenus.h"
#include "EBusHandlerActionMenu.h"
#include "Editor/View/Widgets/NodePalette/CreateNodeMimeEvent.h"
#include "Editor/View/Widgets/NodePalette/EBusNodePaletteTreeItemTypes.h"

// Save Format Conversion
#include <AzCore/Component/EntityUtils.h>
#include <Editor/Include/ScriptCanvas/Components/EditorGraph.h>
////

namespace
{
    size_t s_scriptCanvasEditorDefaultNewNameCount = 0;
} // anonymous namespace.

namespace ScriptCanvasEditor
{
    using namespace AzToolsFramework;

    namespace
    {
        template <typename T>
        class ScopedVariableSetter
        {
        public:
            ScopedVariableSetter(T& value)
                : m_oldValue(value)
                , m_value(value)
            {
            }
            ScopedVariableSetter(T& value, const T& newValue)
                : m_oldValue(value)
                , m_value(value)
            {
                m_value = newValue;
            }

            ~ScopedVariableSetter()
            {
                m_value = m_oldValue;
            }

        private:
            T m_oldValue;
            T& m_value;
        };

        template<typename MimeDataDelegateHandler, typename ... ComponentArgs>
        AZ::EntityId CreateMimeDataDelegate(ComponentArgs... componentArgs)
        {
            AZ::Entity* mimeDelegateEntity = aznew AZ::Entity("MimeData Delegate");

            mimeDelegateEntity->CreateComponent<MimeDataDelegateHandler>(AZStd::forward<ComponentArgs>(componentArgs) ...);
            mimeDelegateEntity->Init();
            mimeDelegateEntity->Activate();

            return mimeDelegateEntity->GetId();
        }

        void CreateSaveDestinationDirectory()
        {
            AZStd::string unresolvedDestination("@devassets@/scriptcanvas");

            AZStd::array<char, AZ::IO::MaxPathLength> assetRoot;
            AZ::IO::FileIOBase::GetInstance()->ResolvePath(unresolvedDestination.c_str(), assetRoot.data(), assetRoot.size());

            // We just need the path to exist.
            QDir(assetRoot.data()).mkpath(".");
        }
    } // anonymous namespace.
    
    ////////////////
    // MainWindow
    ////////////////

    MainWindow::MainWindow()
        : QMainWindow(nullptr, Qt::Widget | Qt::WindowMinMaxButtonsHint)
        , ui(new Ui::MainWindow)
        , m_loadingNewlySavedFile(false)
        , m_isClosingTabs(false)
        , m_enterState(false)
        , m_ignoreSelection(false)
        , m_preventUndoStateUpdateCount(0)
        , m_queueCloseRequest(false)
        , m_hasQueuedClose(false)
        , m_isInAutomation(false)
        , m_allowAutoSave(true)
        , m_systemTickActions(0)
        , m_batchConverter(nullptr)
        , m_styleManager(ScriptCanvasEditor::AssetEditorId, "ScriptCanvas/StyleSheet/graphcanvas_style.json")
    {
        VariablePaletteRequestBus::Handler::BusConnect();

        AZStd::array<char, AZ::IO::MaxPathLength> unresolvedPath;
        AZ::IO::FileIOBase::GetInstance()->ResolvePath("@assets@/editor/translation/scriptcanvas_en_us.qm", unresolvedPath.data(), unresolvedPath.size());

        QString translationFilePath(unresolvedPath.data());
        if ( m_translator.load(QLocale::Language::English, translationFilePath) )
        {
            if ( !qApp->installTranslator(&m_translator) )
            {
                AZ_Warning("ScriptCanvas", false, "Error installing translation %s!", unresolvedPath.data());
            }
        }
        else
        {
            AZ_Warning("ScriptCanvas", false, "Error loading translation file %s", unresolvedPath.data());
        }

        AzToolsFramework::AssetBrowser::AssetBrowserModel* assetBrowserModel = nullptr;
        AzToolsFramework::AssetBrowser::AssetBrowserComponentRequestBus::BroadcastResult(assetBrowserModel, &AzToolsFramework::AssetBrowser::AssetBrowserComponentRequests::GetAssetBrowserModel);

        {
            m_scriptEventsAssetModel = new AzToolsFramework::AssetBrowser::AssetBrowserFilterModel(this);

            AzToolsFramework::AssetBrowser::AssetGroupFilter* scriptEventAssetFilter = new AzToolsFramework::AssetBrowser::AssetGroupFilter();
            scriptEventAssetFilter->SetAssetGroup(ScriptEvents::ScriptEventsAsset::GetGroup());
            scriptEventAssetFilter->SetFilterPropagation(AzToolsFramework::AssetBrowser::AssetBrowserEntryFilter::PropagateDirection::Down);

            // Filter doesn't actually work for whatever reason
            //m_scriptEventsAssetModel->SetFilter(AzToolsFramework::AssetBrowser::FilterConstType(scriptEventAssetFilter));
            m_scriptEventsAssetModel->setSourceModel(assetBrowserModel);
        }

        {
            m_scriptCanvasAssetModel = new AzToolsFramework::AssetBrowser::AssetBrowserFilterModel(this);

            AzToolsFramework::AssetBrowser::AssetGroupFilter* scriptCanvasAssetFilter = new AzToolsFramework::AssetBrowser::AssetGroupFilter();
            scriptCanvasAssetFilter->SetAssetGroup(ScriptCanvasAsset::GetGroup());
            scriptCanvasAssetFilter->SetFilterPropagation(AzToolsFramework::AssetBrowser::AssetBrowserEntryFilter::PropagateDirection::Down);

            // Filter doesn't actually work for whatever reason
            //m_scriptCanvasAssetModel->SetFilter(AzToolsFramework::AssetBrowser::FilterConstType(scriptCanvasAssetFilter));
            m_scriptCanvasAssetModel->setSourceModel(assetBrowserModel);
        }

        ui->setupUi(this);

        CreateMenus();
        UpdateRecentMenu();

        m_host = new QWidget();

        m_layout = new QVBoxLayout();

        m_emptyCanvas = aznew GraphCanvas::GraphCanvasEditorEmptyDockWidget(this);
        m_emptyCanvas->SetDragTargetText(tr("Use the File Menu or drag out a node from the Node Palette to create a new script.").toStdString().c_str());

        m_emptyCanvas->SetEditorId(ScriptCanvasEditor::AssetEditorId);

        m_emptyCanvas->RegisterAcceptedMimeType(Widget::NodePaletteDockWidget::GetMimeType());
        m_emptyCanvas->RegisterAcceptedMimeType(AzToolsFramework::EditorEntityIdContainer::GetMimeType());

        m_editorToolbar = aznew GraphCanvas::AssetEditorToolbar(ScriptCanvasEditor::AssetEditorId);

        {
            m_assignToSelectedEntity = new QToolButton();
            m_assignToSelectedEntity->setIcon(QIcon(":/ScriptCanvasEditorResources/Resources/attach_to_entity.png"));
            m_assignToSelectedEntity->setToolTip("Assigns the currently active graph to all of the currently selected entities.");

            m_selectedEntityMenu = new QMenu();

            m_assignToSelectedEntity->setPopupMode(QToolButton::ToolButtonPopupMode::MenuButtonPopup);
            m_assignToSelectedEntity->setMenu(m_selectedEntityMenu);
            m_assignToSelectedEntity->setEnabled(false);

            m_editorToolbar->AddCustomAction(m_assignToSelectedEntity);            

            QObject::connect(m_selectedEntityMenu, &QMenu::aboutToShow, this, &MainWindow::OnSelectedEntitiesAboutToShow);
            QObject::connect(m_assignToSelectedEntity, &QToolButton::clicked, this, &MainWindow::OnAssignToSelectedEntities);
        }

        {
            m_validateGraphToolButton = new QToolButton();
            m_validateGraphToolButton->setIcon(QIcon(":/ScriptCanvasEditorResources/Resources/validate_icon.png"));
            m_validateGraphToolButton->setEnabled(false);
        }

        m_editorToolbar->AddCustomAction(m_validateGraphToolButton);

        connect(m_validateGraphToolButton, &QToolButton::clicked, this, &MainWindow::OnValidateCurrentGraph);
        
        m_layout->addWidget(m_editorToolbar);

        // Tab bar and "+" button.
        {
            // This spacer keeps the "+" button right-aligned.
            m_plusButtonSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
            m_horizontalTabBarLayout = new QHBoxLayout();

            m_tabBar = new Widget::GraphTabBar(m_host);
            m_tabBar->hide();
            connect(m_tabBar, &QTabBar::tabCloseRequested, this, &MainWindow::OnTabCloseButtonPressed);
            connect(m_tabBar, &Widget::GraphTabBar::TabCloseNoButton, this, &MainWindow::OnTabCloseRequest);
            connect(m_tabBar, &Widget::GraphTabBar::TabInserted, this, &MainWindow::OnTabInserted);
            connect(m_tabBar, &Widget::GraphTabBar::TabRemoved, this, &MainWindow::OnTabRemoved);
            connect(m_tabBar, &Widget::GraphTabBar::SaveTab, this, &MainWindow::SaveTab);
            connect(m_tabBar, &Widget::GraphTabBar::CloseAllTabs, this, &MainWindow::CloseAllTabs);
            connect(m_tabBar, &Widget::GraphTabBar::CloseAllTabsBut, this, &MainWindow::CloseAllTabsBut);
            connect(m_tabBar, &Widget::GraphTabBar::CopyPathToClipboard, this, &MainWindow::CopyPathToClipboard);

            // We never use the m_tabBar and the m_plusButtonSpacer simultaneously.
            // We use the m_plusButtonSpacer to keep the "+" button right-aligned
            // when the m_tabBar is hidden.
            //
            // Both are added to the m_horizontalTabBarLayout because the initial
            // state (no tabs) requires the m_plusButtonSpacer, and m_tabBar is
            // hidden.
            m_horizontalTabBarLayout->addWidget(m_tabBar);
            m_horizontalTabBarLayout->addSpacerItem(m_plusButtonSpacer);

            QPushButton* plusButton = new QPushButton(tr("+"), this);
            plusButton->setFixedSize(20, 20);
            QObject::connect(plusButton, &QPushButton::clicked, this, &MainWindow::OnFileNew);
            m_horizontalTabBarLayout->addWidget(plusButton);
            m_layout->addLayout(m_horizontalTabBarLayout);
        }        

        m_commandLine = new Widget::CommandLine(this);
        m_commandLine->setBaseSize(QSize(size().width(), m_commandLine->size().height()));
        m_commandLine->setObjectName("CommandLine");
        m_commandLine->hide();

        m_layout->addWidget(m_commandLine);

        m_layout->addWidget(m_emptyCanvas);

        m_nodeOutliner = new Widget::NodeOutliner(this);
        m_nodeOutliner->setObjectName("NodeOutliner");

        m_minimap = aznew GraphCanvas::MiniMapDockWidget(ScriptCanvasEditor::AssetEditorId, this);
        m_minimap->setObjectName("MiniMapDockWidget");

        m_variableDockWidget = new VariableDockWidget(this);
        m_variableDockWidget->setObjectName("VariableManager");

        QObject::connect(m_variableDockWidget, &VariableDockWidget::OnVariableSelectionChanged, this, &MainWindow::OnVariableSelectionChanged);

        m_loggingWindow = aznew LoggingWindow(this);
        m_loggingWindow->setObjectName("LoggingWindow");

        m_validationDockWidget = aznew GraphValidationDockWidget(this);
        m_validationDockWidget->setObjectName("ValidationDockWidget");        

        m_propertyGrid = new Widget::PropertyGrid(this, "Node Inspector");
        m_propertyGrid->setObjectName("NodeInspector");

        m_bookmarkDockWidget = aznew GraphCanvas::BookmarkDockWidget(ScriptCanvasEditor::AssetEditorId, this);

        m_statusWidget = aznew MainWindowStatusWidget(this);
        statusBar()->addWidget(m_statusWidget,1);

        QObject::connect(m_statusWidget, &MainWindowStatusWidget::OnErrorButtonPressed, this, &MainWindow::OnShowValidationErrors);
        QObject::connect(m_statusWidget, &MainWindowStatusWidget::OnWarningButtonPressed, this, &MainWindow::OnShowValidationWarnings);

        m_nodePaletteModel.RepopulateModel();

        {
            const bool isInContextMenu = false;
            Widget::ScriptCanvasNodePaletteConfig nodePaletteConfig(m_nodePaletteModel, m_scriptEventsAssetModel, isInContextMenu);            

            m_nodePalette = aznew Widget::NodePaletteDockWidget(tr("Node Palette"), this, nodePaletteConfig);
            m_nodePalette->setObjectName("NodePalette");
        }

        // This needs to happen after the node palette is created, because we scrape for the variable data from inside
        // of there.
        m_variableDockWidget->PopulateVariablePalette(m_variablePaletteTypes);        

        m_ebusHandlerActionMenu = aznew EBusHandlerActionMenu();

        m_statisticsDialog = aznew StatisticsDialog(m_nodePaletteModel, m_scriptCanvasAssetModel, nullptr);
        m_statisticsDialog->hide();        

        m_presetEditor = aznew GraphCanvas::ConstructPresetDialog(nullptr);
        m_presetEditor->SetEditorId(ScriptCanvasEditor::AssetEditorId);        

        m_presetWrapper = new AzQtComponents::WindowDecorationWrapper(AzQtComponents::WindowDecorationWrapper::OptionAutoTitleBarButtons);
        m_presetWrapper->setGuest(m_presetEditor);
        m_presetWrapper->hide();

        m_host->setLayout(m_layout);

        setCentralWidget(m_host);

        QTimer::singleShot(0, [this]() {
            SetDefaultLayout();

            RestoreWorkspace();
            SaveWorkspace();
        });

        m_entityMimeDelegateId = CreateMimeDataDelegate<ScriptCanvasEditor::EntityMimeDataHandler>();

        ScriptCanvasEditor::GeneralRequestBus::Handler::BusConnect();
        ScriptCanvasEditor::AutomationRequestBus::Handler::BusConnect();

        UIRequestBus::Handler::BusConnect();
        UndoNotificationBus::Handler::BusConnect();
        GraphCanvas::AssetEditorRequestBus::Handler::BusConnect(ScriptCanvasEditor::AssetEditorId);
        GraphCanvas::AssetEditorSettingsRequestBus::Handler::BusConnect(ScriptCanvasEditor::AssetEditorId);
        ScriptCanvas::BatchOperationNotificationBus::Handler::BusConnect();
        AssetGraphSceneBus::Handler::BusConnect();
        AzToolsFramework::ToolsApplicationNotificationBus::Handler::BusConnect();

        CreateUndoManager();

        UINotificationBus::Broadcast(&UINotifications::MainWindowCreationEvent, this);
        Metrics::MetricsEventsBus::Broadcast(&Metrics::MetricsEventRequests::SendEditorMetric, ScriptCanvasEditor::Metrics::Events::Editor::Open, m_activeAssetId);

        // Show the PREVIEW welcome message
        m_userSettings = AZ::UserSettings::CreateFind<EditorSettings::ScriptCanvasEditorSettings>(AZ_CRC("ScriptCanvasPreviewSettings", 0x1c5a2965), AZ::UserSettings::CT_LOCAL);
        if (m_userSettings)
        {

            if (m_userSettings->m_showPreviewMessage)
            {
                QTimer::singleShot(1.f, this, [this]()
                {
                    PreviewMessageDialog* previewMessage = aznew PreviewMessageDialog(this);

                    QPoint centerPoint = frameGeometry().center();

                    previewMessage->adjustSize();
                    previewMessage->move(centerPoint.x() - previewMessage->width() / 2, centerPoint.y() - previewMessage->height() / 2);
                    previewMessage->Show();
                });
            }

            m_allowAutoSave = m_userSettings->m_autoSaveConfig.m_enabled;
            m_autoSaveTimer.setInterval(m_userSettings->m_autoSaveConfig.m_timeMS);

            m_userSettings->m_constructPresets.SetEditorId(ScriptCanvasEditor::AssetEditorId);
        }

        // These should be created after we load up the user settings so we can
        // initialize the user presets
        m_sceneContextMenu = aznew SceneContextMenu(m_nodePaletteModel, m_scriptEventsAssetModel);
        m_connectionContextMenu = aznew ConnectionContextMenu(m_nodePaletteModel, m_scriptEventsAssetModel);

        connect(m_nodePalette, &QDockWidget::visibilityChanged, this, &MainWindow::OnViewVisibilityChanged);
        connect(m_nodeOutliner, &QDockWidget::visibilityChanged, this, &MainWindow::OnViewVisibilityChanged);
        connect(m_minimap, &QDockWidget::visibilityChanged, this, &MainWindow::OnViewVisibilityChanged);
        connect(m_propertyGrid, &QDockWidget::visibilityChanged, this, &MainWindow::OnViewVisibilityChanged);
        connect(m_bookmarkDockWidget, &QDockWidget::visibilityChanged, this, &MainWindow::OnViewVisibilityChanged);
        connect(m_variableDockWidget, &QDockWidget::visibilityChanged, this, &MainWindow::OnViewVisibilityChanged);
        connect(m_loggingWindow, &QDockWidget::visibilityChanged, this, &MainWindow::OnViewVisibilityChanged);
        connect(m_validationDockWidget, &QDockWidget::visibilityChanged, this, &MainWindow::OnViewVisibilityChanged);
        connect(m_unitTestDockWidget, &QDockWidget::visibilityChanged, this, &MainWindow::OnViewVisibilityChanged);

        m_autoSaveTimer.setSingleShot(true);
        connect(&m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::OnAutoSave);
    }

    MainWindow::~MainWindow()
    {
        const bool updateAssetList = false;
        SaveWorkspace(updateAssetList);

        ScriptCanvas::BatchOperationNotificationBus::Handler::BusDisconnect();
        GraphCanvas::AssetEditorRequestBus::Handler::BusDisconnect();
        UndoNotificationBus::Handler::BusDisconnect();
        UIRequestBus::Handler::BusDisconnect();
        ScriptCanvasEditor::GeneralRequestBus::Handler::BusDisconnect();

        Clear();

        Metrics::MetricsEventsBus::Broadcast(&Metrics::MetricsEventRequests::SendMetric, ScriptCanvasEditor::Metrics::Events::Editor::Close);

        delete m_batchConverter;
        delete m_nodePalette;

        delete m_statisticsDialog;
        delete m_presetEditor;
    }

    void MainWindow::CreateMenus()
    {
        // File menu
        connect(ui->action_New_Script, &QAction::triggered, this, &MainWindow::OnFileNew);
        ui->action_New_Script->setShortcut(QKeySequence(QKeySequence::New));

        connect(ui->action_Open, &QAction::triggered, this, &MainWindow::OnFileOpen);
        ui->action_Open->setShortcut(QKeySequence(QKeySequence::Open));

        connect(ui->action_BatchConversion, &QAction::triggered, this, &MainWindow::RunBatchConversion);
        ui->action_BatchConversion->setVisible(true);

        // List of recent files.
        {
            QMenu* recentMenu = new QMenu("Open &Recent");

            for (int i = 0; i < m_recentActions.size(); ++i)
            {
                QAction* action = new QAction(this);
                action->setVisible(false);
                m_recentActions[i] = AZStd::make_pair(action, QMetaObject::Connection());
                recentMenu->addAction(action);
            }

            connect(recentMenu, &QMenu::aboutToShow, this, &MainWindow::UpdateRecentMenu);

            recentMenu->addSeparator();

            // Clear Recent Files.
            {
                QAction* action = new QAction("&Clear Recent Files", this);

                QObject::connect(action,
                    &QAction::triggered,
                    [this](bool /*checked*/)
                {
                    ClearRecentFile();
                    UpdateRecentMenu();
                });

                recentMenu->addAction(action);

            }

            ui->menuFile->insertMenu(ui->action_Save, recentMenu);
            ui->menuFile->insertSeparator(ui->action_Save);
        }

        connect(ui->action_Save, &QAction::triggered, this, &MainWindow::OnFileSaveCaller);
        ui->action_Save->setShortcut(QKeySequence(QKeySequence::Save));

        connect(ui->action_Save_As, &QAction::triggered, this, &MainWindow::OnFileSaveAsCaller);
        ui->action_Save_As->setShortcut(QKeySequence(tr("Ctrl+Shift+S", "File|Save As...")));

        QObject::connect(ui->action_Close,
            &QAction::triggered,
            [this](bool /*checked*/)
        {
            m_tabBar->tabCloseRequested(m_tabBar->currentIndex());
        });
        ui->action_Close->setShortcut(QKeySequence(QKeySequence::Close));

        // Edit Menu
        SetupEditMenu();

        // View menu
        connect(ui->action_ViewNodePalette, &QAction::triggered, this, &MainWindow::OnViewNodePalette);
        connect(ui->action_ViewOutline, &QAction::triggered, this, &MainWindow::OnViewOutline);

        // Disabling the Minimap since it does not play nicely with the Qt caching solution
        // And causing some weird visual issues.
        connect(ui->action_ViewMiniMap, &QAction::triggered, this, &MainWindow::OnViewMiniMap);
        ui->action_ViewMiniMap->setVisible(false);

        connect(ui->action_ViewProperties, &QAction::triggered, this, &MainWindow::OnViewProperties);
        connect(ui->action_ViewBookmarks, &QAction::triggered, this, &MainWindow::OnBookmarks);

        connect(ui->action_ViewVariableManager, &QAction::triggered, this, &MainWindow::OnVariableManager);
        connect(ui->action_ViewUnitTestManager, &QAction::triggered, this, &MainWindow::OnUnitTestManager);
        connect(m_variableDockWidget, &QDockWidget::visibilityChanged, this, &MainWindow::OnViewVisibilityChanged);
        connect(m_unitTestDockWidget, &QDockWidget::visibilityChanged, this, &MainWindow::OnViewVisibilityChanged);
        
        connect(ui->action_ViewLogWindow, &QAction::triggered, this, &MainWindow::OnViewLogWindow);
        connect(m_loggingWindow, &QDockWidget::visibilityChanged, this, &MainWindow::OnViewVisibilityChanged);

        connect(ui->action_ViewDebugger, &QAction::triggered, this, &MainWindow::OnViewDebugger);
        connect(ui->action_ViewCommandLine, &QAction::triggered, this, &MainWindow::OnViewCommandLine);
        connect(ui->action_ViewLog, &QAction::triggered, this, &MainWindow::OnViewLog);
        
        connect(ui->action_GraphValidation, &QAction::triggered, this, &MainWindow::OnViewGraphValidation);
        connect(ui->action_Debugging, &QAction::triggered, this, &MainWindow::OnViewDebuggingWindow);

        connect(ui->action_NodeStatistics, &QAction::triggered, this, &MainWindow::OnViewStatisticsPanel);
        connect(ui->action_PresetsEditor, &QAction::triggered, this, &MainWindow::OnViewPresetsEditor);

        connect(ui->action_ViewRestoreDefaultLayout, &QAction::triggered, this, &MainWindow::OnRestoreDefaultLayout);
    }

    void MainWindow::SignalActiveSceneChanged(const AZ::EntityId& scriptCanvasGraphId)
    {
        m_autoSaveTimer.stop();

        // The paste action refreshes based on the scene's mimetype
        RefreshPasteAction();

        AZ::EntityId graphCanvasGraphId = GetGraphCanvasGraphId(scriptCanvasGraphId);

        GraphCanvas::AssetEditorNotificationBus::Event(ScriptCanvasEditor::AssetEditorId, &GraphCanvas::AssetEditorNotifications::PreOnActiveGraphChanged);
        GraphCanvas::AssetEditorNotificationBus::Event(ScriptCanvasEditor::AssetEditorId, &GraphCanvas::AssetEditorNotifications::OnActiveGraphChanged, graphCanvasGraphId);
        GraphCanvas::AssetEditorNotificationBus::Event(ScriptCanvasEditor::AssetEditorId, &GraphCanvas::AssetEditorNotifications::PostOnActiveGraphChanged);

        GraphCanvas::ViewId viewId;
        GraphCanvas::SceneRequestBus::EventResult(viewId, graphCanvasGraphId, &GraphCanvas::SceneRequests::GetViewId);

        GraphCanvas::ViewNotificationBus::Handler::BusDisconnect();
        GraphCanvas::ViewNotificationBus::Handler::BusConnect(viewId);        

        m_validateGraphToolButton->setEnabled(scriptCanvasGraphId.IsValid());        
        ui->action_RemoveUnusedNodes->setEnabled(scriptCanvasGraphId.IsValid());
        ui->action_RemoveUnusedVariables->setEnabled(scriptCanvasGraphId.IsValid());
        ui->action_RemoveUnusedElements->setEnabled(scriptCanvasGraphId.IsValid());

        UpdateAssignToSelectionState();
    }

    void MainWindow::UpdateRecentMenu()
    {
        QStringList recentFiles = ReadRecentFiles();

        int recentCount = 0;
        for (auto filename : recentFiles)
        {
            if (!QFile::exists(filename))
            {
                continue;
            }

            auto& recent = m_recentActions[recentCount++];

            recent.first->setText(QString("&%1 %2").arg(QString::number(recentCount), filename));
            recent.first->setData(filename);
            recent.first->setVisible(true);

            QObject::disconnect(recent.second);
            recent.second = QObject::connect(recent.first,
                &QAction::triggered,
                [this, filename](bool /*checked*/)
            {
                OpenFile(filename.toUtf8().data());
            });
        }

        for (int i = recentCount; i < m_recentActions.size(); ++i)
        {
            auto& recent = m_recentActions[recentCount++];
            recent.first->setVisible(false);
        }
    }

    void MainWindow::OnViewVisibilityChanged(bool visibility)
    {
        UpdateViewMenu();
    }

    void MainWindow::closeEvent(QCloseEvent* event)
    {
        // If we are in the middle of saving a graph. We don't want to close ourselves down and potentially retrigger the saving logic.
        if (m_queueCloseRequest)
        {
            m_hasQueuedClose = true;
            event->ignore();
            return;
        }

        // Get all the unsaved assets.
        // TOFIX LY-62249.
        auto assetIdDataIter = m_assetGraphSceneMapper.m_assetIdToDataMap.begin();

        while (assetIdDataIter != m_assetGraphSceneMapper.m_assetIdToDataMap.end())
        {
            const AZ::Data::AssetId& assetId = assetIdDataIter->first;

            if (m_processedClosedAssetIds.count(assetId) > 0)
            {
                ++assetIdDataIter;
                continue;
            }

            m_processedClosedAssetIds.insert(assetId);

            // Get the state of the file.
            ScriptCanvasFileState fileState;
            DocumentContextRequestBus::BroadcastResult(fileState, &DocumentContextRequests::GetScriptCanvasAssetModificationState, assetId);

            // Only operate on NEW or MODIFIED files.
            if (fileState != ScriptCanvasFileState::NEW && fileState != ScriptCanvasFileState::MODIFIED)
            {
                ++assetIdDataIter;
                continue;
            }

            AZ_Assert((fileState == ScriptCanvasFileState::NEW || fileState == ScriptCanvasFileState::MODIFIED),
                "All UNMODIFIED files should have been CLOSED by now.");

            // Query the user.
            SetActiveAsset(assetId);
            QString tabName;
            QVariant data = GetTabData(assetId);
            if (data.isValid())
            {
                auto graphTabMetadata = data.value<Widget::GraphTabMetadata>();
                tabName = graphTabMetadata.m_tabName;
            }
            UnsavedChangesOptions shouldSaveResults = ShowSaveDialog(tabName);

            if (shouldSaveResults == UnsavedChangesOptions::SAVE)
            {
                DocumentContextRequests::SaveCB saveCB = [this, assetId](bool isSuccessful)
                {
                    if (isSuccessful)
                    {
                        // Continue closing.
                        qobject_cast<QWidget*>(parent())->close();
                    }
                    else
                    {
                        // Abort closing.
                        QMessageBox::critical(this, QString(), QObject::tr("Failed to save."));
                    }
                };
                SaveAsset(assetId, saveCB);
                event->ignore();
                return;
            }
            else if (shouldSaveResults == UnsavedChangesOptions::CANCEL_WITHOUT_SAVING)
            {
                m_processedClosedAssetIds.clear();
                event->ignore();
                
                return;
            }
            else if (shouldSaveResults == UnsavedChangesOptions::CONTINUE_WITHOUT_SAVING && fileState == ScriptCanvasFileState::NEW)
            {
                CloseScriptCanvasAsset(assetId);

                // Iterator was made invalid. Just quickly reprocess the repeated ids.
                assetIdDataIter = m_assetGraphSceneMapper.m_assetIdToDataMap.begin();
            }
            else
            {
                ++assetIdDataIter;
            }
        }

        SaveWorkspace();

        // Close all files.
        while (!m_assetGraphSceneMapper.m_assetIdToDataMap.empty())
        {
            const AZ::Data::AssetId& assetId = m_assetGraphSceneMapper.m_assetIdToDataMap.begin()->first;
            CloseScriptCanvasAsset(assetId);
        }

        m_processedClosedAssetIds.clear();
        
        event->accept();
    }

    UnsavedChangesOptions MainWindow::ShowSaveDialog(const QString& filename)
    {
        UnsavedChangesOptions shouldSaveResults = UnsavedChangesOptions::INVALID;
        UnsavedChangesDialog dialog(filename, this);
        dialog.exec();
        shouldSaveResults = dialog.GetResult();

        return shouldSaveResults;
    }

    bool MainWindow::SaveAsset(const AZ::Data::AssetId& unsavedAssetId, const DocumentContextRequests::SaveCB& saveCB)
    {
        SetActiveAsset(unsavedAssetId);
        return OnFileSave(saveCB);
    }

    void MainWindow::CreateUndoManager()
    {
        m_undoManager = AZStd::make_unique<UndoManager>();
    }

    void MainWindow::TriggerUndo()
    {
        DequeuePropertyGridUpdate();

        m_undoManager->Undo();
        SignalSceneDirty(GetActiveScriptCanvasGraphId());        

        m_propertyGrid->ClearSelection();
    }

    void MainWindow::TriggerRedo()
    {
        DequeuePropertyGridUpdate();

        m_undoManager->Redo();
        SignalSceneDirty(GetActiveScriptCanvasGraphId());

        m_propertyGrid->ClearSelection();
    }

    void MainWindow::RegisterVariableType(const ScriptCanvas::Data::Type& variableType)
    {        
        m_variablePaletteTypes.insert(ScriptCanvas::Data::ToAZType(variableType));
    }

    void MainWindow::OpenValidationPanel()
    {
        if (!m_validationDockWidget->isVisible())
        {
            OnViewGraphValidation();
        }
    }

    void MainWindow::PostUndoPoint(AZ::EntityId scriptCanvasGraphId)
    {
        if (m_preventUndoStateUpdateCount == 0 && !m_undoManager->IsInUndoRedo())
        {
            ScopedUndoBatch scopedUndoBatch("Modify Graph Canvas Scene");
            AZ::Entity* scriptCanvasEntity{};
            AZ::ComponentApplicationBus::BroadcastResult(scriptCanvasEntity, &AZ::ComponentApplicationRequests::FindEntity, scriptCanvasGraphId);
            m_undoManager->AddGraphItemChangeUndo(scriptCanvasEntity, "Graph Change");

            SignalSceneDirty(scriptCanvasGraphId);
        }

        const bool forceTimer = true;
        RestartAutoTimerSave(forceTimer);
    }

    void MainWindow::SignalSceneDirty(const AZ::EntityId& scriptCanvasEntityId)
    {
        auto assetGraphSceneData = m_assetGraphSceneMapper.GetBySceneId(scriptCanvasEntityId);
        if (assetGraphSceneData)
        {
            MarkAssetModified(assetGraphSceneData->m_tupleId.m_assetId);
        }
    }

    void MainWindow::PushPreventUndoStateUpdate()
    {
        ++m_preventUndoStateUpdateCount;
    }

    void MainWindow::PopPreventUndoStateUpdate()
    {
        if (m_preventUndoStateUpdateCount > 0)
        {
            --m_preventUndoStateUpdateCount;
        }
    }

    void MainWindow::ClearPreventUndoStateUpdate()
    {
        m_preventUndoStateUpdateCount = 0;
    }

    void MainWindow::MarkAssetModified(const AZ::Data::AssetId& assetId)
    {
        ScriptCanvasFileState fileState;
        DocumentContextRequestBus::BroadcastResult(fileState, &DocumentContextRequests::GetScriptCanvasAssetModificationState, assetId);
        if (fileState != ScriptCanvasFileState::NEW && fileState != ScriptCanvasFileState::MODIFIED)
        {
            DocumentContextRequestBus::Broadcast(&DocumentContextRequests::SetScriptCanvasAssetModificationState, assetId, ScriptCanvasFileState::MODIFIED);
        }
    }

    void MainWindow::MarkAssetUnmodified(const AZ::Data::AssetId& assetId)
    {
        ScriptCanvasFileState fileState;
        DocumentContextRequestBus::BroadcastResult(fileState, &DocumentContextRequests::GetScriptCanvasAssetModificationState, assetId);
        if (fileState != ScriptCanvasFileState::UNMODIFIED)
        {
            DocumentContextRequestBus::Broadcast(&DocumentContextRequests::SetScriptCanvasAssetModificationState, assetId, ScriptCanvasFileState::UNMODIFIED);
        }
    }

    void MainWindow::RefreshScriptCanvasAsset(const AZ::Data::Asset<ScriptCanvasAsset>& scriptCanvasAsset)
    {
        // Update AssetMapper with new Scene and GraphId 
        auto assetGraphSceneData = m_assetGraphSceneMapper.GetByAssetId(scriptCanvasAsset.GetId());
        if (!assetGraphSceneData || !scriptCanvasAsset.IsReady())
        {
            return;
        }

        GraphCanvas::SceneNotificationBus::MultiHandler::BusDisconnect(assetGraphSceneData->m_tupleId.m_scriptCanvasEntityId);
        GraphCanvas::SceneUIRequestBus::MultiHandler::BusDisconnect(assetGraphSceneData->m_tupleId.m_scriptCanvasEntityId);

        AZ::Entity* sceneEntity = scriptCanvasAsset.Get()->GetScriptCanvasEntity();
        AZ::EntityId scriptCanvasGraphId = sceneEntity ? sceneEntity->GetId() : AZ::EntityId();

        GraphCanvas::GraphId graphCanvsGraphId = GetGraphCanvasGraphId(scriptCanvasGraphId);

        GraphCanvas::AssetEditorNotificationBus::Event(ScriptCanvasEditor::AssetEditorId, &GraphCanvas::AssetEditorNotifications::OnGraphRefreshed, assetGraphSceneData->m_tupleId.m_scriptCanvasGraphId, graphCanvsGraphId);

        assetGraphSceneData->Set(scriptCanvasAsset);

        AZ::Outcome<ScriptCanvasAssetFileInfo, AZStd::string> assetInfoOutcome = AZ::Failure(AZStd::string());
        DocumentContextRequestBus::BroadcastResult(assetInfoOutcome, &DocumentContextRequests::GetFileInfo, scriptCanvasAsset.GetId());
        if (assetInfoOutcome)
        {
            const auto& assetFileInfo = assetInfoOutcome.GetValue();
            AZStd::string tabName;
            AzFramework::StringFunc::Path::GetFileName(assetFileInfo.m_absolutePath.data(), tabName);
            int tabIndex = -1;
            if (IsTabOpen(scriptCanvasAsset.GetId(), tabIndex))
            {
                auto tabVariant = m_tabBar->tabData(tabIndex);
                if (tabVariant.isValid())
                {
                    auto graphMetadata = tabVariant.value<Widget::GraphTabMetadata>();
                    graphMetadata.m_fileState = assetFileInfo.m_fileModificationState;
                    graphMetadata.m_tabName = tabName.data();
                    m_tabBar->SetGraphTabData(tabIndex, graphMetadata);
                    m_tabBar->setTabToolTip(tabIndex, assetFileInfo.m_absolutePath.data());
                }
            }
        }

        if (scriptCanvasGraphId.IsValid())
        {
            AZ::EntityId graphCanvasGraphId = GetGraphCanvasGraphId(scriptCanvasGraphId);

            GraphCanvas::SceneNotificationBus::MultiHandler::BusConnect(graphCanvasGraphId);
            GraphCanvas::SceneUIRequestBus::MultiHandler::BusConnect(graphCanvasGraphId);
            GraphCanvas::SceneMimeDelegateRequestBus::Event(graphCanvasGraphId, &GraphCanvas::SceneMimeDelegateRequests::AddDelegate, m_entityMimeDelegateId);

            GraphCanvas::SceneRequestBus::Event(graphCanvasGraphId, &GraphCanvas::SceneRequests::SetMimeType, Widget::NodePaletteDockWidget::GetMimeType());
            GraphCanvas::SceneMemberNotificationBus::Event(graphCanvasGraphId, &GraphCanvas::SceneMemberNotifications::OnSceneReady);
        }
    }

    AZ::Outcome<int, AZStd::string> MainWindow::CreateScriptCanvasAsset(AZStd::string_view assetPath, int tabIndex)
    {
        AZ::Data::Asset<ScriptCanvasAsset> newAsset;
        DocumentContextRequestBus::BroadcastResult(newAsset, &DocumentContextRequests::CreateScriptCanvasAsset, assetPath);
        m_assetGraphSceneMapper.Add(newAsset);

        int outTabIndex = -1;
        {
            // Insert tab block
            AZStd::string tabName;
            AzFramework::StringFunc::Path::GetFileName(assetPath.data(), tabName);
            m_tabBar->InsertGraphTab(tabIndex, newAsset.GetId(), tabName);
            if (!IsTabOpen(newAsset.GetId(), outTabIndex))
            {
                return AZ::Failure(AZStd::string::format("Unable to open new Script Canvas Asset with id %s in the Script Canvas Editor", newAsset.ToString<AZStd::string>().c_str()));
            }

            m_tabBar->setTabToolTip(outTabIndex, assetPath.data());
        }

        DocumentContextNotificationBus::MultiHandler::BusConnect(newAsset.GetId());
        ActivateAssetEntity(newAsset);

        return AZ::Success(outTabIndex);
    }

    AZ::Outcome<int, AZStd::string> MainWindow::OpenScriptCanvasAssetId(const AZ::Data::AssetId& assetId)
    {
        if (!assetId.IsValid())
        {
            return AZ::Failure(AZStd::string("Unable to open asset with invalid asset id"));
        }

        int outTabIndex = m_tabBar->FindTab(assetId);

        if (outTabIndex >= 0)
        {
            m_tabBar->SelectTab(assetId);
            return AZ::Success(outTabIndex);
        }

        outTabIndex = CreateAssetTab(assetId);

        AZ::Data::AssetInfo assetInfo;
        AZ::Data::AssetCatalogRequestBus::BroadcastResult(assetInfo, &AZ::Data::AssetCatalogRequests::GetAssetInfoById, assetId);

        if (assetInfo.m_relativePath.empty())
        {
            return AZ::Failure(AZStd::string("Unknown AssetId"));
        }

        if (assetInfo.m_assetType != azrtti_typeid<ScriptCanvasAsset>())
        {
            return AZ::Failure(AZStd::string("Invalid ScriptCanvas AssetId provided"));
        }

        if (outTabIndex == -1)
        {
            return AZ::Failure(AZStd::string::format("Unable to open existing Script Canvas Asset[%s] in the Script Canvas Editor", assetInfo.m_relativePath.c_str()));
        }

        AZ::Data::Asset<ScriptCanvasAsset> newAsset;
        DocumentContextRequestBus::BroadcastResult(newAsset, &DocumentContextRequests::LoadScriptCanvasAssetById, assetId, false);

        OpenAssetHelper(newAsset, outTabIndex);

        return AZ::Success(outTabIndex);
    }

    AZ::Outcome<int, AZStd::string> MainWindow::OpenScriptCanvasAsset(const AZ::Data::Asset<ScriptCanvasAsset>& scriptCanvasAsset, int tabIndex)
    {
        if (!scriptCanvasAsset.GetId().IsValid())
        {
            return AZ::Failure(AZStd::string("Unable to open asset with invalid asset id"));
        }

        int outTabIndex = m_tabBar->FindTab(scriptCanvasAsset.GetId());

        if (outTabIndex >= 0)
        {
            m_tabBar->SelectTab(scriptCanvasAsset.GetId());
            return AZ::Success(outTabIndex);
        }

        outTabIndex = CreateAssetTab(scriptCanvasAsset.GetId(), tabIndex);

        if (outTabIndex == -1)
        {
            return AZ::Failure(AZStd::string::format("Unable to open existing Script Canvas Asset with id %s in the Script Canvas Editor", scriptCanvasAsset.ToString<AZStd::string>().c_str()));
        }

        AZ::Data::Asset<ScriptCanvasAsset> newAsset;
        if (scriptCanvasAsset.GetStatus() == AZ::Data::AssetData::AssetStatus::Ready)
        {
            ScriptCanvasAssetFileInfo scFileInfo;
            AZ::Data::AssetInfo assetInfo;
            AZStd::string rootFilePath;
            AzToolsFramework::AssetSystemRequestBus::Broadcast(&AzToolsFramework::AssetSystemRequestBus::Events::GetAssetInfoById, scriptCanvasAsset.GetId(), azrtti_typeid<ScriptCanvasAsset>(), assetInfo, rootFilePath);
            AzFramework::StringFunc::Path::Join(rootFilePath.data(), assetInfo.m_relativePath.data(), scFileInfo.m_absolutePath);
            scFileInfo.m_fileModificationState = ScriptCanvasFileState::UNMODIFIED;
            DocumentContextRequestBus::Broadcast(&DocumentContextRequests::RegisterScriptCanvasAsset, scriptCanvasAsset.GetId(), scFileInfo);
            newAsset = scriptCanvasAsset;

            // Don't want to use the join since I don't want the normalized path
            if (!rootFilePath.empty() && !assetInfo.m_relativePath.empty() && !m_loadingNewlySavedFile)
            {
                int eraseCount = m_loadedWorkspaceAssets.erase(scriptCanvasAsset.GetId());

                if (eraseCount == 0)
                {
                    AZStd::string fullPath = AZStd::string::format("%s/%s", rootFilePath.c_str(), assetInfo.m_relativePath.c_str());

                    AddRecentFile(fullPath.c_str());
                }
            }
        }
        else
        {
            DocumentContextRequestBus::BroadcastResult(newAsset, &DocumentContextRequests::LoadScriptCanvasAssetById, scriptCanvasAsset.GetId(), false);
        }

        OpenAssetHelper(newAsset, outTabIndex);

        return AZ::Success(outTabIndex);
    }

    int MainWindow::CreateAssetTab(const AZ::Data::AssetId& assetId, int tabIndex)
    {
        // Reuse the tab if available
        int outTabIndex = -1;

        m_tabBar->InsertGraphTab(tabIndex, assetId);

        return m_tabBar->FindTab(assetId);
    }

    void MainWindow::OpenAssetHelper(AZ::Data::Asset<ScriptCanvasAsset>& asset, int tabIndex)
    {
        AZ::Outcome<ScriptCanvasAssetFileInfo, AZStd::string> assetInfoOutcome = AZ::Failure(AZStd::string());
        DocumentContextRequestBus::BroadcastResult(assetInfoOutcome, &DocumentContextRequests::GetFileInfo, asset.GetId());
        AZStd::string assetPath = assetInfoOutcome ? assetInfoOutcome.GetValue().m_absolutePath : "";

        AZStd::string tabName;
        AzFramework::StringFunc::Path::GetFileName(assetPath.data(), tabName);
        Widget::GraphTabMetadata graphMetadata = m_tabBar->tabData(tabIndex).value<Widget::GraphTabMetadata>();
        DocumentContextRequestBus::BroadcastResult(graphMetadata.m_fileState, &DocumentContextRequests::GetScriptCanvasAssetModificationState, graphMetadata.m_assetId);
        graphMetadata.m_tabName = tabName.data();

        m_tabBar->SetGraphTabData(tabIndex, graphMetadata);
        m_tabBar->setTabToolTip(tabIndex, assetPath.data());

        DocumentContextNotificationBus::MultiHandler::BusConnect(asset.GetId());
        // IsReady also checks the ReadyPreNotify state which signifies that the asset is ready
        // but the AssetBus::OnAssetReady event has not been dispatch
        // Here we only want to invoke the method if the OnAsseteReady event has been dispatch
        if (asset.GetStatus() == AZ::Data::AssetData::AssetStatus::Ready)
        {
            CloneAssetEntity(asset);
        }
        else
        {
            m_loadingAssets.insert(asset.GetId());
        }

        Metrics::MetricsEventsBus::Broadcast(&Metrics::MetricsEventRequests::SendGraphMetric, ScriptCanvasEditor::Metrics::Events::Canvas::OpenGraph, asset.GetId());
    }

    AZ::Outcome<int, AZStd::string> MainWindow::UpdateScriptCanvasAsset(const AZ::Data::Asset<ScriptCanvasAsset>& scriptCanvasAsset)
    {
        int outTabIndex = -1;

        PushPreventUndoStateUpdate();
        RefreshScriptCanvasAsset(scriptCanvasAsset);
        if (IsTabOpen(scriptCanvasAsset.GetId(), outTabIndex))
        {
            RefreshActiveAsset();
        }
        PopPreventUndoStateUpdate();

        if (outTabIndex == -1)
        {
            return AZ::Failure(AZStd::string::format("Script Canvas Asset %s is not open in a tab", scriptCanvasAsset.ToString<AZStd::string>().c_str()));
        }

        return AZ::Success(outTabIndex);
    }

    void MainWindow::RemoveScriptCanvasAsset(const AZ::Data::AssetId& assetId)
    {
        m_assetCreationRequests.erase(assetId);

        GeneralAssetNotificationBus::Event(assetId, &GeneralAssetNotifications::OnAssetUnloaded);

        DocumentContextRequestBus::Broadcast(&DocumentContextRequests::UnregisterScriptCanvasAsset, assetId);
        DocumentContextNotificationBus::MultiHandler::BusDisconnect(assetId);
        auto assetGraphSceneData = m_assetGraphSceneMapper.GetByAssetId(assetId);
        if (assetGraphSceneData)
        {
            auto& tupleId = assetGraphSceneData->m_tupleId;
            GraphCanvas::SceneNotificationBus::MultiHandler::BusDisconnect(tupleId.m_scriptCanvasEntityId);

            m_assetGraphSceneMapper.Remove(assetId);
            GraphCanvas::AssetEditorNotificationBus::Event(ScriptCanvasEditor::AssetEditorId, &GraphCanvas::AssetEditorNotifications::OnGraphUnloaded, tupleId.m_scriptCanvasGraphId);
        }
    }

    void MainWindow::MoveScriptCanvasAsset(const AZ::Data::Asset<ScriptCanvasAsset>& newAsset, const ScriptCanvasAssetFileInfo& newAssetFileInfo)
    {
        m_assetGraphSceneMapper.Add(newAsset);
        GraphCanvas::SceneNotificationBus::MultiHandler::BusConnect(newAsset.Get()->GetScriptCanvasEntity()->GetId());

        DocumentContextRequestBus::Broadcast(&DocumentContextRequests::RegisterScriptCanvasAsset, newAsset.GetId(), newAssetFileInfo);

        DocumentContextNotificationBus::MultiHandler::BusConnect(newAsset.GetId());
    }

    int MainWindow::CloseScriptCanvasAsset(const AZ::Data::AssetId& assetId)
    {
        int tabIndex = -1;
        if (IsTabOpen(assetId, tabIndex))
        {
            OnTabCloseRequest(tabIndex);
            Metrics::MetricsEventsBus::Broadcast(&Metrics::MetricsEventRequests::SendGraphMetric, ScriptCanvasEditor::Metrics::Events::Canvas::CloseGraph, assetId);
        }

        return tabIndex;
    }

    bool MainWindow::CreateScriptCanvasAssetFor(const AZ::EntityId& requestingEntityId)
    {
        for (auto createdAssetPair : m_assetCreationRequests)
        {
            if (createdAssetPair.second == requestingEntityId)
            {
                return OpenScriptCanvasAssetId(createdAssetPair.first).IsSuccess();                
            }
        }

        AZ::Data::AssetId previousAssetId = m_activeAssetId;

        OnFileNew();

        bool createdNewAsset = m_activeAssetId != previousAssetId;

        if (createdNewAsset)
        {
            m_assetCreationRequests[m_activeAssetId] = requestingEntityId;
        }

        return createdNewAsset;
    }

    bool MainWindow::IsScriptCanvasAssetOpen(const AZ::Data::AssetId& assetId) const
    {
        return m_assetGraphSceneMapper.GetByAssetId(assetId) != nullptr;
        //int tabIndex = -1;
        //return IsTabOpen(assetId, tabIndex);
    }

    const CategoryInformation* MainWindow::FindNodePaletteCategoryInformation(AZStd::string_view categoryPath) const
    {
        return m_nodePaletteModel.FindBestCategoryInformation(categoryPath);
    }

    const NodePaletteModelInformation* MainWindow::FindNodePaletteModelInformation(const ScriptCanvas::NodeTypeIdentifier& nodeType) const
    {
        return m_nodePaletteModel.FindNodePaletteInformation(nodeType);
    }

    AZStd::string MainWindow::GetSuggestedFullFilenameToSaveAs(const AZ::Data::AssetId& assetId)
    {
        AZ::Outcome<ScriptCanvasAssetFileInfo, AZStd::string> assetInfoOutcome = AZ::Failure(AZStd::string());
        DocumentContextRequestBus::BroadcastResult(assetInfoOutcome, &DocumentContextRequests::GetFileInfo, assetId);
        AZStd::string assetPath;
        if (assetInfoOutcome)
        {
            const auto& assetFileInfo = assetInfoOutcome.GetValue();
            assetPath = assetInfoOutcome.GetValue().m_absolutePath;
        }

        if (assetPath.empty())
        {
            int tabIndex = -1;
            if (IsTabOpen(assetId, tabIndex))
            {
                QVariant data = m_tabBar->tabData(tabIndex);
                if (data.isValid())
                {
                    auto graphTabMetadata = data.value<Widget::GraphTabMetadata>();
                    assetPath = AZStd::string::format("@devassets@/scriptcanvas/%s.%s", graphTabMetadata.m_tabName.toUtf8().data(), ScriptCanvasAssetHandler::GetFileExtension());
                }
            }
        }

        AZStd::array<char, AZ::IO::MaxPathLength> resolvedPath;
        AZ::IO::FileIOBase::GetInstance()->ResolvePath(assetPath.data(), resolvedPath.data(), resolvedPath.size());
        return resolvedPath.data();
    }

    void MainWindow::SaveScriptCanvasAsset(AZStd::string_view filename, AZ::Data::Asset<ScriptCanvasAsset> sourceAsset, const DocumentContextRequests::SaveCB& saveCB)
    {
        if (!sourceAsset.IsReady())
        {
            return;
        }

        AZStd::string watchFolder;
        AZ::Data::AssetInfo catalogAssetInfo;
        bool sourceInfoFound{};
        AZ_Error("Script Canvas", !filename.empty(), "Failed to save Script Canvas asset, check AssetProcessor for errors.");
        if (filename.empty())
        {
            return;
        }

        BlockCloseRequests();

        AzToolsFramework::AssetSystemRequestBus::BroadcastResult(sourceInfoFound, &AzToolsFramework::AssetSystemRequestBus::Events::GetSourceInfoBySourcePath, filename.data(), catalogAssetInfo, watchFolder);
        auto saveAssetId = sourceInfoFound ? catalogAssetInfo.m_assetId : AZ::Data::AssetId(AZ::Uuid::CreateRandom());

        AZ::EntityId requestorId;

        auto requestorIter = m_assetCreationRequests.find(sourceAsset.GetId());

        if (requestorIter != m_assetCreationRequests.end())
        {
            requestorId = requestorIter->second;
        }

        AZ::Data::Asset<ScriptCanvasAsset> saveAsset = CopyAssetForSave(saveAssetId, sourceAsset);
        DocumentContextRequests::SourceFileChangedCB idChangedCB = [this, saveAssetId, sourceAsset, saveAsset, requestorId](AZStd::string relPath, AZStd::string scanFolder, const AZ::Data::AssetId& newAssetId)
        {
            SourceFileChanged(saveAssetId, relPath, scanFolder, newAssetId);

            if (requestorId.IsValid())
            {
                EditorScriptCanvasComponentRequestBus::Event(requestorId, &EditorScriptCanvasComponentRequests::SetAssetId, this->m_activeAssetId);
            }
        };

        ////
        // Version Conversion for Save File Revisions: Introduced in 1.14
        bool forceReload = false;

        {
            AZ::Entity* clonedScriptCanvasEntity = saveAsset.GetAs<ScriptCanvasAsset>()->GetScriptCanvasEntity();

            ScriptCanvasEditor::Graph* graphComponent = AZ::EntityUtils::FindFirstDerivedComponent<ScriptCanvasEditor::Graph>(clonedScriptCanvasEntity);

            if (graphComponent->NeedsSaveConversion())
            {
                bool isActive = clonedScriptCanvasEntity->GetState() == AZ::Entity::State::ES_ACTIVE;

                if (isActive)
                {
                    clonedScriptCanvasEntity->Deactivate();
                }
                else if (clonedScriptCanvasEntity->GetState() != AZ::Entity::State::ES_INIT)
                {
                    // Entity needs to be init'd in order for the internal component states
                    // to be valid
                    clonedScriptCanvasEntity->Init();
                }

                graphComponent->ConvertSaveFormat();

                forceReload = true;
            }
        }
        ////

        AZStd::string tabName;
        AzFramework::StringFunc::Path::GetFileName(filename.data(), tabName);
        DocumentContextRequests::SaveCB wrappedSaveCB = [this, sourceAsset, saveAsset, tabName, forceReload, saveCB](bool saveSuccess)
        {
            if (sourceAsset != saveAsset || forceReload)
            {
                int saveTabIndex = m_tabBar->FindTab(saveAsset.GetId());
                if (saveTabIndex == -1)
                {
                    saveTabIndex = m_tabBar->FindTab(sourceAsset.GetId());
                }

                if (sourceAsset != saveAsset)
                {
                    ScriptCanvasAsset* scriptCanvasAsset = sourceAsset.Get();

                    if (scriptCanvasAsset)
                    {
                        AZ::Entity* entity = scriptCanvasAsset->GetScriptCanvasEntity();

                        if (entity)
                        {
                            ScriptCanvasEditor::Graph* editorGraph = AZ::EntityUtils::FindFirstDerivedComponent<ScriptCanvasEditor::Graph>(entity);

                            if (editorGraph)
                            {
                                editorGraph->ClearGraphCanvasScene();
                            }
                        }
                    }
                }

                CloseScriptCanvasAsset(sourceAsset.GetId());
                CloseScriptCanvasAsset(saveAsset.GetId());                

                this->m_loadingNewlySavedFile = true;
                auto openOutcome = OpenScriptCanvasAsset(saveAsset, saveTabIndex);
                this->m_loadingNewlySavedFile = false;

                if (openOutcome)
                {
                    QVariant data = m_tabBar->tabData(openOutcome.GetValue());
                    if (data.isValid())
                    {
                        auto graphMetadata = data.value<Widget::GraphTabMetadata>();
                        m_tabBar->SetTabText(openOutcome.GetValue(), tabName.data(), graphMetadata.m_fileState);
                    }

                    AddRecentFile(m_newlySavedFile.c_str());
                }
            }
            if (saveCB)
            {
                saveCB(saveSuccess);
            }

            AZ::Entity* savedAssetEntity = saveAsset.GetAs<ScriptCanvasAsset>()->GetScriptCanvasEntity();
            ScriptCanvasEditor::Graph* graphComponent = AZ::EntityUtils::FindFirstDerivedComponent<ScriptCanvasEditor::Graph>(savedAssetEntity);

            if (graphComponent)
            {
                Metrics::MetricsEventsBus::Broadcast(&Metrics::MetricsEventRequests::SendGraphStatistics, saveAsset.GetId(), graphComponent->GetNodeUsageStatistics());
            }

            // Once we finish saving. Run the Validator.
            if (m_activeAssetId == saveAsset.GetId())
            {
                const bool displayAsNotification = true;
                m_validationDockWidget->OnRunValidator(displayAsNotification);
            }

            this->UnblockCloseRequests();
        };

        DocumentContextRequestBus::Broadcast(&DocumentContextRequests::SaveScriptCanvasAsset, filename, saveAsset, wrappedSaveCB, idChangedCB);
    }

    void MainWindow::OpenFile(const char* fullPath)
    {
        AZStd::string watchFolder;
        AZ::Data::AssetInfo assetInfo;
        bool sourceInfoFound{};
        AzToolsFramework::AssetSystemRequestBus::BroadcastResult(sourceInfoFound, &AzToolsFramework::AssetSystemRequestBus::Events::GetSourceInfoBySourcePath, fullPath, assetInfo, watchFolder);
        if (sourceInfoFound && m_assetGraphSceneMapper.m_assetIdToDataMap.count(assetInfo.m_assetId) == 0)
        {
            SetRecentAssetId(assetInfo.m_assetId);

            AZ::Data::Asset<ScriptCanvasAsset> newAsset;
            DocumentContextRequestBus::BroadcastResult(newAsset, &DocumentContextRequests::LoadScriptCanvasAssetById, assetInfo.m_assetId, false);
            auto openOutcome = OpenScriptCanvasAsset(newAsset);
            if (openOutcome)
            {
                AddRecentFile(fullPath);
            }
            else
            {
                AZ_Warning("Script Canvas", openOutcome, "%s", openOutcome.GetError().data());
            }
        }
    }

    AZ::Data::Asset<ScriptCanvasAsset> MainWindow::CopyAssetForSave(const AZ::Data::AssetId& assetId, AZ::Data::Asset<ScriptCanvasAsset> oldAsset)
    {
        AZ::Data::Asset<ScriptCanvasAsset> newAsset = oldAsset;
        if (assetId != oldAsset.GetId())
        {
            newAsset = aznew ScriptCanvasAsset(assetId, AZ::Data::AssetData::AssetStatus::Ready);
            auto serializeContext = AZ::EntityUtils::GetApplicationSerializeContext();
            serializeContext->CloneObjectInplace(newAsset.Get()->GetScriptCanvasData(), &oldAsset.Get()->GetScriptCanvasData());
            AZStd::unordered_map<AZ::EntityId, AZ::EntityId> entityIdMap;
            AZ::IdUtils::Remapper<AZ::EntityId>::GenerateNewIdsAndFixRefs(&newAsset.Get()->GetScriptCanvasData(), entityIdMap, serializeContext);
        }

        return newAsset;
    }

    GraphCanvas::Endpoint MainWindow::HandleProposedConnection(const GraphCanvas::GraphId& graphId, const GraphCanvas::ConnectionId& connectionId, const GraphCanvas::Endpoint& endpoint, const GraphCanvas::NodeId& nodeId, const QPoint& screenPoint)
    {
        GraphCanvas::Endpoint retVal;

        GraphCanvas::ConnectionType connectionType = GraphCanvas::ConnectionType::CT_Invalid;
        GraphCanvas::SlotRequestBus::EventResult(connectionType, endpoint.GetSlotId(), &GraphCanvas::SlotRequests::GetConnectionType);

        GraphCanvas::NodeId currentTarget = nodeId;

        while (!retVal.IsValid() && currentTarget.IsValid())
        {
            AZStd::vector<AZ::EntityId> targetSlotIds;
            GraphCanvas::NodeRequestBus::EventResult(targetSlotIds, currentTarget, &GraphCanvas::NodeRequests::GetSlotIds);

            AZStd::list< GraphCanvas::Endpoint > endpoints;

            for (const auto& targetSlotId : targetSlotIds)
            {
                GraphCanvas::Endpoint proposedEndpoint(currentTarget, targetSlotId);

                bool canCreate = false;
                GraphCanvas::SlotRequestBus::EventResult(canCreate, endpoint.GetSlotId(), &GraphCanvas::SlotRequests::CanCreateConnectionTo, proposedEndpoint);

                if (canCreate)
                {
                    GraphCanvas::SlotGroup slotGroup = GraphCanvas::SlotGroups::Invalid;
                    GraphCanvas::SlotRequestBus::EventResult(slotGroup, targetSlotId, &GraphCanvas::SlotRequests::GetSlotGroup);

                    bool isVisible = slotGroup != GraphCanvas::SlotGroups::Invalid;
                    GraphCanvas::SlotLayoutRequestBus::EventResult(isVisible, currentTarget, &GraphCanvas::SlotLayoutRequests::IsSlotGroupVisible, slotGroup);

                    if (isVisible)
                    {
                        endpoints.push_back(proposedEndpoint);
                    }
                }
            }

            if (!endpoints.empty())
            {
                if (endpoints.size() == 1)
                {
                    retVal = endpoints.front();
                }
                else
                {
                    QMenu menu;

                    for (GraphCanvas::Endpoint proposedEndpoint : endpoints)
                    {
                        QAction* action = aznew EndpointSelectionAction(proposedEndpoint);
                        menu.addAction(action);
                    }

                    QAction* result = menu.exec(screenPoint);

                    if (result != nullptr)
                    {
                        EndpointSelectionAction* selectedEnpointAction = static_cast<EndpointSelectionAction*>(result);
                        retVal = selectedEnpointAction->GetEndpoint();
                    }
                    else
                    {
                        retVal.Clear();
                    }
                }

                if (retVal.IsValid())
                {
                    // Double safety check. This should be gauranteed by the previous checks. But just extra safety.
                    bool canCreateConnection = false;
                    GraphCanvas::SlotRequestBus::EventResult(canCreateConnection, endpoint.GetSlotId(), &GraphCanvas::SlotRequests::CanCreateConnectionTo, retVal);

                    if (!canCreateConnection)
                    {
                        retVal.Clear();
                    }
                }
            }
            else
            {
                retVal.Clear();
            }

            if (!retVal.IsValid())
            {
                bool isWrapped = false;
                GraphCanvas::NodeRequestBus::EventResult(isWrapped, currentTarget, &GraphCanvas::NodeRequests::IsWrapped);

                if (isWrapped)
                {
                    GraphCanvas::NodeRequestBus::EventResult(currentTarget, currentTarget, &GraphCanvas::NodeRequests::GetWrappingNode);
                }
                else
                {
                    currentTarget.SetInvalid();
                }
            }            
        }

        return retVal;
    }

    void MainWindow::SourceFileChanged(const AZ::Data::AssetId& saveAssetId, AZStd::string relPath, AZStd::string scanFolder, const AZ::Data::AssetId& newAssetId)
    {
        if (saveAssetId == newAssetId)
        {
            return;
        }

        // Fixes up the asset Ids associated with an open Script Canvas asset 
        auto saveAssetMapper = m_assetGraphSceneMapper.GetByAssetId(saveAssetId);
        if (saveAssetMapper)
        {
            bool isActiveAsset = saveAssetId == m_activeAssetId;

            // Move over the old asset data to a new ScriptCanvasAsset with the newAssetId
            AZ::Data::Asset<ScriptCanvasAsset> remappedAsset = aznew ScriptCanvasAsset(newAssetId, saveAssetMapper->m_asset.GetStatus());
            remappedAsset.Get()->GetScriptCanvasData() = AZStd::move(saveAssetMapper->m_asset.Get()->GetScriptCanvasData());
            
            // Extracts the Scene UndoState from the Undo Manager before it can be deleted by RemoveScriptCanvasAsset
            // Removes the ScriptCanvasAsset from the MainWindow which disconnects all EBuses and unloads the scene which would delete the Scene UndoState
            AZStd::unique_ptr<SceneUndoState> sceneUndoState = m_undoManager->ExtractSceneUndoState(remappedAsset.Get()->GetScriptCanvasEntity()->GetId());
            RemoveScriptCanvasAsset(saveAssetId);
            m_undoManager->InsertUndoState(remappedAsset.Get()->GetScriptCanvasEntity()->GetId(), AZStd::move(sceneUndoState));

            ScriptCanvasAssetFileInfo assetFileInfo;
            AzFramework::StringFunc::Path::Join(scanFolder.data(), relPath.data(), assetFileInfo.m_absolutePath);
            assetFileInfo.m_fileModificationState = ScriptCanvasFileState::UNMODIFIED;
            MoveScriptCanvasAsset(remappedAsset, assetFileInfo);

            // Re-purpose tab index with the new asset data

            AZStd::string tabName;
            AzFramework::StringFunc::Path::GetFileName(assetFileInfo.m_absolutePath.data(), tabName);
            int saveTabIndex = m_tabBar->FindTab(saveAssetId);
            QVariant data = m_tabBar->tabData(saveTabIndex);
            if (data.isValid())
            {   
                auto graphMetadata = data.value<Widget::GraphTabMetadata>();
                graphMetadata.m_tabName = tabName.data();
                graphMetadata.m_assetId = remappedAsset.GetId();
                graphMetadata.m_fileState = assetFileInfo.m_fileModificationState;
                m_tabBar->SetGraphTabData(saveTabIndex, graphMetadata);
                m_tabBar->setTabToolTip(saveTabIndex, assetFileInfo.m_absolutePath.data());

                if (isActiveAsset)
                {
                    SetActiveAsset(newAssetId);
                }
            }
        }
    }

    void MainWindow::OnFileNew()
    {
        AZStd::string newAssetName = AZStd::string::format("Untitled-%i", ++s_scriptCanvasEditorDefaultNewNameCount);

        AZStd::array<char, AZ::IO::MaxPathLength> assetRootArray;
        if (!AZ::IO::FileIOBase::GetInstance()->ResolvePath("@devassets@/scriptcanvas", assetRootArray.data(), assetRootArray.size()))
        {
            AZ_ErrorOnce("Script Canvas", false, "Unable to resolve @devassets@ path");
        }

        AZStd::string assetPath;
        AzFramework::StringFunc::Path::Join(assetRootArray.data(), (newAssetName + "." + ScriptCanvasAssetHandler::GetFileExtension()).data(), assetPath);
        auto createOutcome = CreateScriptCanvasAsset(assetPath);
        if (createOutcome)
        {
            Metrics::MetricsEventsBus::Broadcast(&Metrics::MetricsEventRequests::SendMetric, ScriptCanvasEditor::Metrics::Events::Editor::NewFile);
        }
        else
        {
            AZ_Warning("Script Canvas", createOutcome, "%s", createOutcome.GetError().data());
        }
    }

    bool MainWindow::OnFileSave(const DocumentContextRequests::SaveCB& saveCB)
    {
        bool saveAttempt = false;
        if (m_activeAssetId.IsValid())
        {
            auto& assetId = m_activeAssetId;

            saveAttempt = SaveAssetImpl(assetId, saveCB);
        }
        return saveAttempt;
    }

    bool MainWindow::OnFileSaveAs(const DocumentContextRequests::SaveCB& saveCB)
    {
        if (!m_activeAssetId.IsValid())
        {
            return false;
        }

        return SaveAssetAsImpl(m_activeAssetId, saveCB);
    }

    bool MainWindow::SaveAssetImpl(const AZ::Data::AssetId& assetId, const DocumentContextRequests::SaveCB& saveCB)
    {
        bool saveAttempt = false;

        ScriptCanvasFileState fileState;
        DocumentContextRequestBus::BroadcastResult(fileState, &DocumentContextRequests::GetScriptCanvasAssetModificationState, assetId);
        if (fileState == ScriptCanvasFileState::NEW)
        {
            saveAttempt = SaveAssetAsImpl(assetId, saveCB);
        }
        else
        {
            PrepareAssetForSave(assetId);

            auto assetMapper = m_assetGraphSceneMapper.GetByAssetId(assetId);
            if (assetMapper && assetMapper->m_asset.IsReady())
            {
                auto scriptCanvasAsset = assetMapper->m_asset;
                AZ::Outcome<ScriptCanvasAssetFileInfo, AZStd::string> assetInfoOutcome = AZ::Failure(AZStd::string());
                DocumentContextRequestBus::BroadcastResult(assetInfoOutcome, &DocumentContextRequests::GetFileInfo, scriptCanvasAsset.GetId());
                if (assetInfoOutcome)
                {
                    SaveScriptCanvasAsset(assetInfoOutcome.GetValue().m_absolutePath, scriptCanvasAsset, saveCB);
                    saveAttempt = true;
                }
                else
                {
                    AZ_Warning("Script Canvas", assetInfoOutcome, "%s", assetInfoOutcome.GetError().data());
                }
            }
        }

        return saveAttempt;
    }

    bool MainWindow::SaveAssetAsImpl(const AZ::Data::AssetId& assetId, const DocumentContextRequests::SaveCB& saveCB)
    {
        if (m_activeAssetId != assetId)
        {
            QVariant data = GetTabData(assetId);

            if (data.isValid())
            {
                auto graphTabMetadata = data.value<Widget::GraphTabMetadata>();
                OnChangeActiveGraphTab(graphTabMetadata);
            }
        }

        PrepareAssetForSave(assetId);

        CreateSaveDestinationDirectory();
        AZStd::string suggestedFilename = GetSuggestedFullFilenameToSaveAs(assetId);
        QString filter = tr("Script Canvas Files (%1)").arg(ScriptCanvasAssetHandler::GetFileFilter());        
        QString selectedFile;

        bool isValidFileName = false;

        while (!isValidFileName)
        {
            selectedFile = QFileDialog::getSaveFileName(this, tr("Save As..."), suggestedFilename.data(), filter);

            // If the selected file is empty that means we just cancelled.
            // So we want to break out.
            if (!selectedFile.isEmpty())
            {
                AZStd::string filePath = selectedFile.toUtf8().data();
                AZStd::string fileName;

                if (AzFramework::StringFunc::Path::GetFileName(filePath.c_str(), fileName))
                {
                    isValidFileName = !(fileName.empty());
                }
                else
                {
                    QMessageBox::information(this, "Unable to Save", "File name cannot be empty");
                }

            }
            else
            {                
                break;
            }
        }

        if (isValidFileName)
        {
            auto assetMapper = m_assetGraphSceneMapper.GetByAssetId(assetId);
            if (assetMapper && assetMapper->m_asset.IsReady())
            {
                auto scriptCanvasAsset = assetMapper->m_asset;
                SaveScriptCanvasAsset(selectedFile.toUtf8().data(), scriptCanvasAsset, saveCB);

                m_newlySavedFile = selectedFile.toUtf8().data();

                // Forcing the file add here, since we are creating a new file
                AddRecentFile(m_newlySavedFile.c_str());

                return true;
            }
        }

        return false;
    }

    void MainWindow::OnFileOpen()
    {
        AZ::SerializeContext* serializeContext = nullptr;
        EBUS_EVENT_RESULT(serializeContext, AZ::ComponentApplicationBus, GetSerializeContext);
        AZ_Assert(serializeContext, "Failed to acquire application serialize context.");

        AZ::Data::AssetId openId = ReadRecentAssetId();

        AZStd::string assetRoot;
        {
            AZStd::array<char, AZ::IO::MaxPathLength> assetRootChar;
            AZ::IO::FileIOBase::GetInstance()->ResolvePath("@devassets@", assetRootChar.data(), assetRootChar.size());
            assetRoot = assetRootChar.data();
        }

        AZStd::string assetPath;
        AZ::Data::AssetCatalogRequestBus::BroadcastResult(assetPath, &AZ::Data::AssetCatalogRequests::GetAssetPathById, openId);
        if (!assetPath.empty())
        {
            assetPath = AZStd::string::format("%s/%s", assetRoot.c_str(), assetPath.c_str());
        }

        if (!openId.IsValid() || !QFile::exists(assetPath.c_str()))
        {
            assetPath = AZStd::string::format("%s/scriptcanvas", assetRoot.c_str());
        }

        QString filter = tr("Script Canvas Files (%1)").arg(ScriptCanvasAssetHandler::GetFileFilter());

        QFileDialog dialog(nullptr, tr("Open..."), assetPath.c_str(), filter);
        dialog.setFileMode(QFileDialog::ExistingFiles);

        if (dialog.exec() == QDialog::Accepted) 
        {
            for (const QString& open : dialog.selectedFiles())
            {
                OpenFile(open.toUtf8().data());
            }
        }
    }

    void MainWindow::SetupEditMenu()
    {
        ui->action_Undo->setShortcut(QKeySequence::Undo);
        ui->action_Cut->setShortcut(QKeySequence(QKeySequence::Cut));
        ui->action_Copy->setShortcut(QKeySequence(QKeySequence::Copy));
        ui->action_Paste->setShortcut(QKeySequence(QKeySequence::Paste));
        ui->action_Delete->setShortcut(QKeySequence(QKeySequence::Delete));

        connect(ui->menuEdit, &QMenu::aboutToShow, this, &MainWindow::OnEditMenuShow);

        // Edit Menu
        connect(ui->action_Undo, &QAction::triggered, this, &MainWindow::TriggerUndo);
        connect(ui->action_Redo, &QAction::triggered, this, &MainWindow::TriggerRedo);
        connect(ui->action_Cut, &QAction::triggered, this, &MainWindow::OnEditCut);
        connect(ui->action_Copy, &QAction::triggered, this, &MainWindow::OnEditCopy);
        connect(ui->action_Paste, &QAction::triggered, this, &MainWindow::OnEditPaste);
        connect(ui->action_Duplicate, &QAction::triggered, this, &MainWindow::OnEditDuplicate);
        connect(ui->action_Delete, &QAction::triggered, this, &MainWindow::OnEditDelete);        
        connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &MainWindow::RefreshPasteAction);        
        connect(ui->action_RemoveUnusedNodes, &QAction::triggered, this, &MainWindow::OnRemoveUnusedNodes);
        connect(ui->action_RemoveUnusedVariables, &QAction::triggered, this, &MainWindow::OnRemoveUnusedVariables);
        connect(ui->action_RemoveUnusedElements, &QAction::triggered, this, &MainWindow::OnRemoveUnusedElements);
        connect(ui->action_Screenshot, &QAction::triggered, this, &MainWindow::OnScreenshot);
        connect(ui->action_SelectAll, &QAction::triggered, this, &MainWindow::OnSelectAll);
        connect(ui->action_SelectInputs, &QAction::triggered, this, &MainWindow::OnSelectInputs);
        connect(ui->action_SelectOutputs, &QAction::triggered, this, &MainWindow::OnSelectOutputs);
        connect(ui->action_SelectConnected, &QAction::triggered, this, &MainWindow::OnSelectConnected);
        connect(ui->action_ClearSelection, &QAction::triggered, this, &MainWindow::OnClearSelection);
        connect(ui->action_EnableSelection, &QAction::triggered, this, &MainWindow::OnEnableSelection);
        connect(ui->action_DisableSelection, &QAction::triggered, this, &MainWindow::OnDisableSelection);
        connect(ui->action_AlignTop, &QAction::triggered, this, &MainWindow::OnAlignTop);
        connect(ui->action_AlignBottom, &QAction::triggered, this, &MainWindow::OnAlignBottom);
        connect(ui->action_AlignLeft, &QAction::triggered, this, &MainWindow::OnAlignLeft);
        connect(ui->action_AlignRight, &QAction::triggered, this, &MainWindow::OnAlignRight);

        ui->action_ZoomIn->setShortcuts({ QKeySequence(Qt::CTRL + Qt::Key_Plus),
                                          QKeySequence(Qt::CTRL + Qt::Key_Equal)
                                        });

        // View Menu
        connect(ui->action_ShowEntireGraph, &QAction::triggered, this, &MainWindow::OnShowEntireGraph);
        connect(ui->action_ZoomIn, &QAction::triggered, this, &MainWindow::OnZoomIn);
        connect(ui->action_ZoomOut, &QAction::triggered, this, &MainWindow::OnZoomOut);
        connect(ui->action_ZoomSelection, &QAction::triggered, this, &MainWindow::OnZoomToSelection);
        connect(ui->action_GotoStartOfChain, &QAction::triggered, this, &MainWindow::OnGotoStartOfChain);
        connect(ui->action_GotoEndOfChain, &QAction::triggered, this, &MainWindow::OnGotoEndOfChain);


        connect(ui->action_GlobalPreferences, &QAction::triggered, [this]() 
        {
            bool originalSetting = m_userSettings && m_userSettings->m_showExcludedNodes;

            ScriptCanvasEditor::SettingsDialog(ui->action_GlobalPreferences->text(), AZ::EntityId(), this).exec();

            bool settingChanged = m_userSettings && m_userSettings->m_showExcludedNodes;

            if (m_userSettings)
            {
                if (m_userSettings->m_autoSaveConfig.m_enabled)
                {
                    m_allowAutoSave = true;
                    m_autoSaveTimer.setInterval(m_userSettings->m_autoSaveConfig.m_timeMS);
                }
                else
                {
                    m_allowAutoSave = false;
                }
            }

            if (originalSetting != settingChanged)
            {
                m_nodePaletteModel.RepopulateModel();

                m_nodePalette->ResetModel();
                m_statisticsDialog->ResetModel();
                m_variableDockWidget->PopulateVariablePalette(m_variablePaletteTypes);
                
                delete m_sceneContextMenu;
                m_sceneContextMenu = aznew SceneContextMenu(m_nodePaletteModel, m_scriptEventsAssetModel);

                delete m_connectionContextMenu;
                m_connectionContextMenu = aznew ConnectionContextMenu(m_nodePaletteModel, m_scriptEventsAssetModel);

                SignalActiveSceneChanged(GetActiveScriptCanvasGraphId());
            }

        });

        connect(ui->action_GraphPreferences, &QAction::triggered, [this]() {
            AZ::EntityId scriptCanvasSceneId = GetActiveScriptCanvasGraphId();
            if (!scriptCanvasSceneId.IsValid())
            {
                return;
            }

            m_autoSaveTimer.stop();

            ScriptCanvasEditor::SettingsDialog(ui->action_GlobalPreferences->text(), scriptCanvasSceneId, this).exec();
        });
    }

    void MainWindow::OnEditMenuShow()
    {
        RefreshGraphPreferencesAction();

        ui->action_Screenshot->setEnabled(GetActiveGraphCanvasGraphId().IsValid());
        ui->menuSelect->setEnabled(GetActiveGraphCanvasGraphId().IsValid());
        ui->action_ClearSelection->setEnabled(GetActiveGraphCanvasGraphId().IsValid());
        ui->menuAlign->setEnabled(GetActiveGraphCanvasGraphId().IsValid());
    }

    void MainWindow::RefreshPasteAction()
    {
        AZStd::string copyMimeType;
        GraphCanvas::SceneRequestBus::EventResult(copyMimeType, GetActiveGraphCanvasGraphId(), &GraphCanvas::SceneRequests::GetCopyMimeType);

        const bool pasteableClipboard = (!copyMimeType.empty() && QApplication::clipboard()->mimeData()->hasFormat(copyMimeType.c_str()))
                                        || GraphVariablesTableView::HasCopyVariableData();

        ui->action_Paste->setEnabled(pasteableClipboard);
    }

    void MainWindow::RefreshGraphPreferencesAction()
    {
        ui->action_GraphPreferences->setEnabled(GetActiveGraphCanvasGraphId().IsValid());
    }

    void MainWindow::OnEditCut()
    {
        AZ::EntityId graphCanvasGraphId = GetActiveGraphCanvasGraphId();
        GraphCanvas::SceneRequestBus::Event(graphCanvasGraphId, &GraphCanvas::SceneRequests::CutSelection);
    }

    void MainWindow::OnEditCopy()
    {
        AZ::EntityId graphCanvasGraphId = GetActiveGraphCanvasGraphId();
        GraphCanvas::SceneRequestBus::Event(graphCanvasGraphId, &GraphCanvas::SceneRequests::CopySelection);
    }

    void MainWindow::OnEditPaste()
    {
        AZ::EntityId graphCanvasGraphId = GetActiveGraphCanvasGraphId();
        GraphCanvas::SceneRequestBus::Event(graphCanvasGraphId, &GraphCanvas::SceneRequests::Paste);
    }

    void MainWindow::OnEditDuplicate()
    {
        AZ::EntityId graphCanvasGraphId = GetActiveGraphCanvasGraphId();
        GraphCanvas::SceneRequestBus::Event(graphCanvasGraphId, &GraphCanvas::SceneRequests::DuplicateSelection);
    }

    void MainWindow::OnEditDelete()
    {
        AZ::EntityId graphCanvasGraphId = GetActiveGraphCanvasGraphId();
        GraphCanvas::SceneRequestBus::Event(graphCanvasGraphId, &GraphCanvas::SceneRequests::DeleteSelection);
    }

    void MainWindow::OnRemoveUnusedVariables()
    {
        AZ::EntityId scriptCanvasGraphId = GetActiveScriptCanvasGraphId();
        EditorGraphRequestBus::Event(scriptCanvasGraphId, &EditorGraphRequests::RemoveUnusedVariables);
    }

    void MainWindow::OnRemoveUnusedNodes()
    {
        AZ::EntityId graphCanvasGraphId = GetActiveGraphCanvasGraphId();
        GraphCanvas::SceneRequestBus::Event(graphCanvasGraphId, &GraphCanvas::SceneRequests::RemoveUnusedNodes);
    }

    void MainWindow::OnRemoveUnusedElements()
    {
        AZ::EntityId graphCanvasGraphId = GetActiveGraphCanvasGraphId();
        GraphCanvas::SceneRequestBus::Event(graphCanvasGraphId, &GraphCanvas::SceneRequests::RemoveUnusedElements);
    }

    void MainWindow::OnScreenshot()
    {
        AZ::EntityId graphCanvasGraphId = GetActiveGraphCanvasGraphId();

        GraphCanvas::ViewId viewId;
        GraphCanvas::SceneRequestBus::EventResult(viewId, graphCanvasGraphId, &GraphCanvas::SceneRequests::GetViewId);

        GraphCanvas::ViewRequestBus::Event(viewId, &GraphCanvas::ViewRequests::ScreenshotSelection);
    }

    void MainWindow::OnSelectAll()
    {
        AZ::EntityId graphCanvasGraphId = GetActiveGraphCanvasGraphId();

        GraphCanvas::SceneRequestBus::Event(graphCanvasGraphId, &GraphCanvas::SceneRequests::SelectAll);
    }

    void MainWindow::OnSelectInputs()
    {
        AZ::EntityId graphCanvasGraphId = GetActiveGraphCanvasGraphId();

        GraphCanvas::SceneRequestBus::Event(graphCanvasGraphId, &GraphCanvas::SceneRequests::SelectAllRelative, GraphCanvas::ConnectionType::CT_Input);        
    }

    void MainWindow::OnSelectOutputs()
    {
        AZ::EntityId graphCanvasGraphId = GetActiveGraphCanvasGraphId();

        GraphCanvas::SceneRequestBus::Event(graphCanvasGraphId, &GraphCanvas::SceneRequests::SelectAllRelative, GraphCanvas::ConnectionType::CT_Output);

        GraphCanvas::ViewId viewId;
        GraphCanvas::SceneRequestBus::EventResult(viewId, graphCanvasGraphId, &GraphCanvas::SceneRequests::GetViewId);
    }

    void MainWindow::OnSelectConnected()
    {
        AZ::EntityId graphCanvasGraphId = GetActiveGraphCanvasGraphId();
        GraphCanvas::SceneRequestBus::Event(graphCanvasGraphId, &GraphCanvas::SceneRequests::SelectConnectedNodes);
    }

    void MainWindow::OnClearSelection()
    {
        AZ::EntityId graphCanvasGraphId = GetActiveGraphCanvasGraphId();
        GraphCanvas::SceneRequestBus::Event(graphCanvasGraphId, &GraphCanvas::SceneRequests::ClearSelection);
    }

    void MainWindow::OnEnableSelection()
    {
        AZ::EntityId graphCanvasGraphId = GetActiveGraphCanvasGraphId();
        GraphCanvas::SceneRequestBus::Event(graphCanvasGraphId, &GraphCanvas::SceneRequests::EnableSelection);
    }

    void MainWindow::OnDisableSelection()
    {
        AZ::EntityId graphCanvasGraphId = GetActiveGraphCanvasGraphId();
        GraphCanvas::SceneRequestBus::Event(graphCanvasGraphId, &GraphCanvas::SceneRequests::DisableSelection);
    }

    void MainWindow::OnAlignTop()
    {
        GraphCanvas::AlignConfig alignConfig;

        alignConfig.m_horAlign = GraphCanvas::GraphUtils::HorizontalAlignment::None;
        alignConfig.m_verAlign = GraphCanvas::GraphUtils::VerticalAlignment::Top;
        alignConfig.m_alignTime = GetAlignmentTime();

        AlignSelected(alignConfig);
    }

    void MainWindow::OnAlignBottom()
    {
        GraphCanvas::AlignConfig alignConfig;

        alignConfig.m_horAlign = GraphCanvas::GraphUtils::HorizontalAlignment::None;
        alignConfig.m_verAlign = GraphCanvas::GraphUtils::VerticalAlignment::Bottom;
        alignConfig.m_alignTime = GetAlignmentTime();

        AlignSelected(alignConfig);
    }

    void MainWindow::OnAlignLeft()
    {
        GraphCanvas::AlignConfig alignConfig;

        alignConfig.m_horAlign = GraphCanvas::GraphUtils::HorizontalAlignment::Left;
        alignConfig.m_verAlign = GraphCanvas::GraphUtils::VerticalAlignment::None;
        alignConfig.m_alignTime = GetAlignmentTime();

        AlignSelected(alignConfig);
    }

    void MainWindow::OnAlignRight()
    {
        GraphCanvas::AlignConfig alignConfig;

        alignConfig.m_horAlign = GraphCanvas::GraphUtils::HorizontalAlignment::Right;
        alignConfig.m_verAlign = GraphCanvas::GraphUtils::VerticalAlignment::None;
        alignConfig.m_alignTime = GetAlignmentTime();

        AlignSelected(alignConfig);
    }

    void MainWindow::AlignSelected(const GraphCanvas::AlignConfig& alignConfig)
    {
        AZ::EntityId graphCanvasGraphId = GetActiveGraphCanvasGraphId();

        AZStd::vector< GraphCanvas::NodeId > selectedNodes;
        GraphCanvas::SceneRequestBus::EventResult(selectedNodes, graphCanvasGraphId, &GraphCanvas::SceneRequests::GetSelectedNodes);

        GraphCanvas::GraphUtils::AlignNodes(selectedNodes, alignConfig);
    }

    void MainWindow::OnShowEntireGraph()
    {
        AZ::EntityId graphCanvasGraphId = GetActiveGraphCanvasGraphId();

        GraphCanvas::ViewId viewId;
        GraphCanvas::SceneRequestBus::EventResult(viewId, graphCanvasGraphId, &GraphCanvas::SceneRequests::GetViewId);

        GraphCanvas::ViewRequestBus::Event(viewId, &GraphCanvas::ViewRequests::ShowEntireGraph);
    }

    void MainWindow::OnZoomIn()
    {
        AZ::EntityId graphCanvasGraphId = GetActiveGraphCanvasGraphId();

        GraphCanvas::ViewId viewId;
        GraphCanvas::SceneRequestBus::EventResult(viewId, graphCanvasGraphId, &GraphCanvas::SceneRequests::GetViewId);

        GraphCanvas::ViewRequestBus::Event(viewId, &GraphCanvas::ViewRequests::ZoomIn);
    }

    void MainWindow::OnZoomOut()
    {
        AZ::EntityId graphCanvasGraphId = GetActiveGraphCanvasGraphId();

        GraphCanvas::ViewId viewId;
        GraphCanvas::SceneRequestBus::EventResult(viewId, graphCanvasGraphId, &GraphCanvas::SceneRequests::GetViewId);

        GraphCanvas::ViewRequestBus::Event(viewId, &GraphCanvas::ViewRequests::ZoomOut);
    }

    void MainWindow::OnZoomToSelection()
    {
        AZ::EntityId graphCanvasGraphId = GetActiveGraphCanvasGraphId();

        GraphCanvas::ViewId viewId;
        GraphCanvas::SceneRequestBus::EventResult(viewId, graphCanvasGraphId, &GraphCanvas::SceneRequests::GetViewId);

        GraphCanvas::ViewRequestBus::Event(viewId, &GraphCanvas::ViewRequests::CenterOnSelection);
    }

    void MainWindow::OnGotoStartOfChain()
    {
        AZ::EntityId graphCanvasGraphId = GetActiveGraphCanvasGraphId();

        GraphCanvas::ViewId viewId;
        GraphCanvas::SceneRequestBus::EventResult(viewId, graphCanvasGraphId, &GraphCanvas::SceneRequests::GetViewId);

        GraphCanvas::ViewRequestBus::Event(viewId, &GraphCanvas::ViewRequests::CenterOnStartOfChain);
    }

    void MainWindow::OnGotoEndOfChain()
    {
        AZ::EntityId graphCanvasGraphId = GetActiveGraphCanvasGraphId();

        GraphCanvas::ViewId viewId;
        GraphCanvas::SceneRequestBus::EventResult(viewId, graphCanvasGraphId, &GraphCanvas::SceneRequests::GetViewId);

        GraphCanvas::ViewRequestBus::Event(viewId, &GraphCanvas::ViewRequests::CenterOnEndOfChain);
    }

    void MainWindow::OnScriptCanvasAssetReady(const AZ::Data::Asset<ScriptCanvasAsset>& scriptCanvasAsset)
    {
        int eraseCount = m_loadingAssets.erase(scriptCanvasAsset.GetId());
        if (eraseCount > 0)
        {
            CloneAssetEntity(scriptCanvasAsset);
            
            AZ::Data::AssetInfo assetInfo;
            AZStd::string rootFilePath;
            AzToolsFramework::AssetSystemRequestBus::Broadcast(&AzToolsFramework::AssetSystemRequestBus::Events::GetAssetInfoById, scriptCanvasAsset.GetId(), azrtti_typeid<ScriptCanvasAsset>(), assetInfo, rootFilePath);

            // Don't want to use the join since I don't want the normalized path
            if (!rootFilePath.empty() && !assetInfo.m_relativePath.empty())
            {
                int eraseCount = m_loadedWorkspaceAssets.erase(scriptCanvasAsset.GetId());

                if (eraseCount == 0)
                {
                    AZStd::string fullPath = AZStd::string::format("%s/%s", rootFilePath.c_str(), assetInfo.m_relativePath.c_str());
                    AddRecentFile(fullPath.c_str());
                }
            }
        }

        if (m_loadingAssets.empty() && m_postLoadFocusId.IsValid())
        {
             m_tabBar->SelectTab(m_postLoadFocusId);
             m_postLoadFocusId.SetInvalid();
        }
    }

    void MainWindow::CloneAssetEntity(AZ::Data::Asset<ScriptCanvasAsset> scriptCanvasAsset)
    {
        // The ScriptCanvas Asset loaded in the Script Canvas Editor should not share the memory with the version in the AssetManager
        // This is to allow a modified asset in the SC Editor to close without modifying the canonical asset
        ScriptCanvasAsset* assetData = aznew ScriptCanvasAsset(scriptCanvasAsset.GetId(), AZ::Data::AssetData::AssetStatus::Ready);
        auto& scriptCanvasData = assetData->GetScriptCanvasData();

        // Clone asset data into SC Editor asset
        auto serializeContext = AZ::EntityUtils::GetApplicationSerializeContext();
        serializeContext->CloneObjectInplace(scriptCanvasData, &scriptCanvasAsset.Get()->GetScriptCanvasData());
                
        AZStd::unordered_map<AZ::EntityId, AZ::EntityId> assetToEditorMap;
        AZ::IdUtils::Remapper<AZ::EntityId>::GenerateNewIdsAndFixRefs(&scriptCanvasData, assetToEditorMap, serializeContext);
                
        AZStd::unordered_map<AZ::EntityId, AZ::EntityId> editorToAssetMap;
        for (auto assetToEditor : assetToEditorMap)
        {
            editorToAssetMap.emplace(assetToEditor.second, assetToEditor.first);
        }

        ActivateAssetEntity(assetData);

        AssetGraphSceneData* cloneAssetSceneData = m_assetGraphSceneMapper.GetByAssetId(scriptCanvasAsset.GetId());
        cloneAssetSceneData->m_assetToEditorEntityIdMap = AZStd::move(assetToEditorMap);
        cloneAssetSceneData->m_editorToAssetEntityIdMap = AZStd::move(editorToAssetMap);

        GeneralAssetNotificationBus::Event(cloneAssetSceneData->m_asset.GetId(), &GeneralAssetNotifications::OnAssetVisualized);
    }

    void MainWindow::ActivateAssetEntity(AZ::Data::Asset<ScriptCanvasAsset> scriptCanvasAsset)
    {
        if (AZ::Entity* scriptCanvasEntity{ scriptCanvasAsset.Get()->GetScriptCanvasEntity() })
        {
            if (scriptCanvasEntity->GetState() == AZ::Entity::ES_CONSTRUCTED)
            {
                scriptCanvasEntity->Init();
            }
            if (scriptCanvasEntity->GetState() == AZ::Entity::ES_INIT)
            {
                scriptCanvasEntity->Activate();
            }

            AZStd::string graphName;
            AZ::Data::AssetCatalogRequestBus::BroadcastResult(graphName, &AZ::Data::AssetCatalogRequests::GetAssetPathById, scriptCanvasAsset.GetId());
            if (!graphName.empty())
            {
                scriptCanvasEntity->SetName(graphName);
            }

            m_assetGraphSceneMapper.Add(scriptCanvasAsset);
            AssetGraphSceneData* assetMapper = m_assetGraphSceneMapper.GetByAssetId(scriptCanvasAsset.GetId());
            SetActiveAsset(scriptCanvasAsset.GetId());

            UpdateScriptCanvasAsset(scriptCanvasAsset);

            GraphCanvas::GraphId graphCanvasGraphId = GetGraphCanvasGraphId(scriptCanvasEntity->GetId());
            GraphCanvas::AssetEditorNotificationBus::Event(ScriptCanvasEditor::AssetEditorId, &GraphCanvas::AssetEditorNotifications::OnGraphLoaded, graphCanvasGraphId);

            UndoCache* undoCache = m_undoManager->GetSceneUndoCache(scriptCanvasEntity->GetId());
            if (undoCache)
            {
                undoCache->UpdateCache(scriptCanvasEntity->GetId());
            }
        }
    }

    void MainWindow::OnCanUndoChanged(bool canUndo)
    {
        ui->action_Undo->setEnabled(canUndo);
    }

    void MainWindow::OnCanRedoChanged(bool canRedo)
    {
        ui->action_Redo->setEnabled(canRedo);
    }

    GraphCanvas::ContextMenuAction::SceneReaction MainWindow::HandleContextMenu(GraphCanvas::EditorContextMenu& editorContextMenu, const AZ::EntityId& memberId, const QPoint& screenPoint, const QPointF& scenePoint) const
    {
        AZ::Vector2 sceneVector(scenePoint.x(), scenePoint.y());
        GraphCanvas::GraphId graphCanvasGraphId = GetActiveGraphCanvasGraphId();

        editorContextMenu.RefreshActions(graphCanvasGraphId, memberId);

        QAction* result = editorContextMenu.exec(screenPoint);

        GraphCanvas::ContextMenuAction* contextMenuAction = qobject_cast<GraphCanvas::ContextMenuAction*>(result);

        if (contextMenuAction)
        {
            return contextMenuAction->TriggerAction(graphCanvasGraphId, sceneVector);
        }
        else
        {
            return GraphCanvas::ContextMenuAction::SceneReaction::Nothing;
        }
    }

    void MainWindow::OnAutoSave()
    {
        if (m_allowAutoSave)
        {
            auto& assetId = m_activeAssetId;

            ScriptCanvasFileState fileState;
            DocumentContextRequestBus::BroadcastResult(fileState, &DocumentContextRequests::GetScriptCanvasAssetModificationState, assetId);

            if (fileState != ScriptCanvasFileState::NEW)
            {
                OnFileSaveCaller();
            }
        }
    }

    //! GeneralRequestBus
    void MainWindow::OnChangeActiveGraphTab(const Widget::GraphTabMetadata& graphMetadata)
    {
        SetActiveAsset(graphMetadata.m_assetId);
    }

    AZ::EntityId MainWindow::GetActiveGraphCanvasGraphId() const
    {
        return GetGraphCanvasGraphId(GetActiveScriptCanvasGraphId());
    }

    AZ::EntityId MainWindow::GetActiveScriptCanvasGraphId() const
    {
        AssetGraphSceneData* assetGraphSceneData = m_assetGraphSceneMapper.GetByAssetId(m_activeAssetId);
        return assetGraphSceneData ? assetGraphSceneData->m_tupleId.m_scriptCanvasEntityId : AZ::EntityId();
    }

    GraphCanvas::GraphId MainWindow::GetGraphCanvasGraphId(const AZ::EntityId& scriptCanvasGraphId) const
    {
        AZ::EntityId graphCanvasId;
        EditorGraphRequestBus::EventResult(graphCanvasId, scriptCanvasGraphId, &EditorGraphRequests::GetGraphCanvasGraphId);

        return graphCanvasId;
    }

    AZ::EntityId MainWindow::GetScriptCanvasGraphId(const GraphCanvas::GraphId& graphCanvasGraphId) const
    {
        const AZStd::any* userData = nullptr;
        GraphCanvas::SceneRequestBus::EventResult(userData, graphCanvasGraphId, &GraphCanvas::SceneRequests::GetUserDataConst);

        if (userData && userData->is<AZ::EntityId>())
        {
            return (*AZStd::any_cast<AZ::EntityId>(userData));
        }

        return AZ::EntityId();
    }

    AZ::EntityId MainWindow::FindGraphCanvasGraphIdByAssetId(const AZ::Data::AssetId& assetId) const
    {
        auto assetGraphSceneData = m_assetGraphSceneMapper.GetByAssetId(m_activeAssetId);
        if (!assetGraphSceneData)
        {
            AZ_Error("Script Canvas", assetId.IsValid(), "Asset %s does not belong to this window", m_activeAssetId.ToString<AZStd::string>().data());
            return AZ::EntityId();
        }

        auto scriptCanvasAsset = assetGraphSceneData->m_asset;
        if (scriptCanvasAsset.IsReady() && scriptCanvasAsset.Get()->GetScriptCanvasEntity()->GetState() == AZ::Entity::ES_ACTIVE)
        {
            const AssetGraphSceneId& tupleId = assetGraphSceneData->m_tupleId;

            return GetGraphCanvasGraphId(tupleId.m_scriptCanvasEntityId);
        }

        return AZ::EntityId();
    }

    AZ::EntityId MainWindow::FindScriptCanvasGraphIdByAssetId(const AZ::Data::AssetId& assetId) const
    {
        auto assetGraphSceneData = m_assetGraphSceneMapper.GetByAssetId(m_activeAssetId);
        if (!assetGraphSceneData)
        {
            AZ_Error("Script Canvas", assetId.IsValid(), "Asset %s does not belong to this window", m_activeAssetId.ToString<AZStd::string>().data());
            return AZ::EntityId();
        }

        auto scriptCanvasAsset = assetGraphSceneData->m_asset;
        if (scriptCanvasAsset.IsReady() && scriptCanvasAsset.Get()->GetScriptCanvasEntity()->GetState() == AZ::Entity::ES_ACTIVE)
        {
            const AssetGraphSceneId& tupleId = assetGraphSceneData->m_tupleId;

            return tupleId.m_scriptCanvasEntityId;
        }

        return AZ::EntityId();
    }

    bool MainWindow::IsInUndoRedo(const AZ::EntityId& graphCanvasGraphId) const
    {
        if (GetActiveGraphCanvasGraphId() == graphCanvasGraphId)
        {
            return m_undoManager->IsInUndoRedo();
        }

        return false;
    }

    bool MainWindow::IsActiveInUndoRedo() const
    {
        return m_undoManager->IsInUndoRedo();
    }

    QVariant MainWindow::GetTabData(const AZ::Data::AssetId& assetId)
    {
        for (int tabIndex = 0; tabIndex < m_tabBar->count(); ++tabIndex)
        {
            QVariant data = m_tabBar->tabData(tabIndex);
            if (data.isValid())
            {
                auto graphTabMetadata = data.value<Widget::GraphTabMetadata>();
                if (graphTabMetadata.m_assetId == assetId)
                {
                    return data;
                }
            }
        }
        return QVariant();
    }

    bool MainWindow::IsTabOpen(const AZ::Data::AssetId& assetId, int& outTabIndex) const
    {
        int tabIndex = m_tabBar->FindTab(assetId);
        if (-1 != tabIndex)
        {
            outTabIndex = tabIndex;
            return true;
        }
        return false;
    }


    void MainWindow::SetActiveAsset(const AZ::Data::AssetId& assetId)
    {
        if (m_activeAssetId == assetId)
        {
            return;
        }

        m_tabBar->SelectTab(assetId);

        if (m_activeAssetId.IsValid())
        {
            QVariant graphTabMetadataVariant = GetTabData(m_activeAssetId);
            if (graphTabMetadataVariant.isValid())
            {
                auto hostMetadata = graphTabMetadataVariant.value<Widget::GraphTabMetadata>();
                if (hostMetadata.m_hostWidget)
                {
                    hostMetadata.m_hostWidget->hide();
                    m_layout->removeWidget(hostMetadata.m_hostWidget);
                }
            }
        }

        if (assetId.IsValid())
        {
            m_activeAssetId = assetId;
            RefreshActiveAsset();
        }
        else
        {
            m_activeAssetId.SetInvalid();
            m_emptyCanvas->show();
            SignalActiveSceneChanged(AZ::EntityId());
        }

        RefreshSelection();
    }

    void MainWindow::RefreshActiveAsset()
    {
        if (m_activeAssetId.IsValid())
        {
            auto assetGraphSceneData = m_assetGraphSceneMapper.GetByAssetId(m_activeAssetId);
            if (!assetGraphSceneData)
            {
                AZ_Error("Script Canvas", m_activeAssetId.IsValid(), "Asset %s does not belong to this window", m_activeAssetId.ToString<AZStd::string>().data());
                return;
            }

            auto scriptCanvasAsset = assetGraphSceneData->m_asset;
            if (scriptCanvasAsset.IsReady() && scriptCanvasAsset.Get()->GetScriptCanvasEntity()->GetState() == AZ::Entity::ES_ACTIVE)
            {
                const AssetGraphSceneId& tupleId = assetGraphSceneData->m_tupleId;
                QVariant graphTabMetadataVariant = GetTabData(m_activeAssetId);
                if (graphTabMetadataVariant.isValid())
                {
                    auto hostMetadata = graphTabMetadataVariant.value<Widget::GraphTabMetadata>();
                    if (hostMetadata.m_hostWidget)
                    {
                        if (auto canvasWidget = qobject_cast<Widget::CanvasWidget*>(hostMetadata.m_hostWidget))
                        {
                            canvasWidget->ShowScene(tupleId.m_scriptCanvasEntityId);
                        }
                        m_layout->addWidget(hostMetadata.m_hostWidget);
                        hostMetadata.m_hostWidget->show();
                    }
                }

                m_emptyCanvas->hide();

                SignalActiveSceneChanged(tupleId.m_scriptCanvasEntityId);
            }
        }
    }

    void MainWindow::Clear()
    {
        m_tabBar->RemoveAllBars();

        m_assetGraphSceneMapper.m_assetIdToDataMap;
        AZStd::vector<AZ::Data::AssetId> assetIds;
        for (const auto& assetIdDataPair : m_assetGraphSceneMapper.m_assetIdToDataMap)
        {
            assetIds.push_back(assetIdDataPair.first);
        }

        for (const AZ::Data::AssetId& assetId : assetIds)
        {
            RemoveScriptCanvasAsset(assetId);
        }
        SetActiveAsset({});
    }

    void MainWindow::OnTabInserted(int index)
    {
        // This is invoked AFTER a new tab has been inserted.

        if (m_tabBar->count() == 1)
        {
            // The first tab has been added.
            m_tabBar->show();

            // We DON'T need the spacer keep
            // the "+" button right-aligned.
            m_horizontalTabBarLayout->removeItem(m_plusButtonSpacer);
        }
    }

    void MainWindow::OnTabRemoved(int index)
    {
        // This is invoked AFTER an existing tab has been removed.

        if (m_tabBar->count() == 0)
        {
            // The last tab has been removed.
            m_tabBar->hide();

            // We NEED the spacer keep the
            // "+" button right-aligned.
            m_horizontalTabBarLayout->insertSpacerItem(0, m_plusButtonSpacer);
        }
    }

    void MainWindow::OnTabCloseButtonPressed(int index)
    {
        QVariant data = m_tabBar->tabData(index);
        if (data.isValid())
        {
            auto graphTabMetadata = data.value<Widget::GraphTabMetadata>();

            UnsavedChangesOptions saveDialogResults = UnsavedChangesOptions::CONTINUE_WITHOUT_SAVING;
            if (graphTabMetadata.m_fileState == ScriptCanvasFileState::NEW || graphTabMetadata.m_fileState == ScriptCanvasFileState::MODIFIED)
            {
                SetActiveAsset(graphTabMetadata.m_assetId);
                saveDialogResults = ShowSaveDialog(graphTabMetadata.m_tabName);
            }

            if (saveDialogResults == UnsavedChangesOptions::SAVE)
            {
                const AZ::Data::AssetId assetId = graphTabMetadata.m_assetId;
                auto saveCB = [this, assetId](bool isSuccessful)
                {
                    if (isSuccessful)
                    {
                        int tabIndex = -1;
                        if (IsTabOpen(assetId, tabIndex))
                        {
                            OnTabCloseRequest(tabIndex);
                        }
                    }
                    else
                    {
                        QMessageBox::critical(this, QString(), QObject::tr("Failed to save."));
                    }
                };
                SaveAsset(graphTabMetadata.m_assetId, saveCB);
            }
            else if (saveDialogResults == UnsavedChangesOptions::CONTINUE_WITHOUT_SAVING)
            {
                OnTabCloseRequest(index);
            }
        }
    }

    void MainWindow::SaveTab(int index)
    {        
        QVariant data = m_tabBar->tabData(index);
        if (data.isValid())
        {
            auto graphTabMetadata = data.value<Widget::GraphTabMetadata>();
            SaveAssetImpl(graphTabMetadata.m_assetId, nullptr);
        }
        
    }

    void MainWindow::CloseAllTabs()
    {
        m_isClosingTabs = true;
        m_skipTabOnClose.SetInvalid();

        CloseNextTab();
    }

    void MainWindow::CloseAllTabsBut(int index)
    {
        QVariant data = m_tabBar->tabData(index);
        if (data.isValid())
        {
            auto graphTabMetadata = data.value<Widget::GraphTabMetadata>();

            m_isClosingTabs = true;
            m_skipTabOnClose = graphTabMetadata.m_assetId;
            CloseNextTab();
        }        
    }

    void MainWindow::CopyPathToClipboard(int index)
    {
        QVariant data = m_tabBar->tabData(index);

        if (data.isValid())
        {
            QClipboard* clipBoard = QGuiApplication::clipboard();

            auto graphTabMetadata = data.value<Widget::GraphTabMetadata>();

            AZ::Data::AssetInfo assetInfo;
            AZ::Data::AssetCatalogRequestBus::BroadcastResult(assetInfo, &AZ::Data::AssetCatalogRequests::GetAssetInfoById, graphTabMetadata.m_assetId);

            if (!assetInfo.m_relativePath.empty())
            {
                AZStd::string absolutePath;
                AzToolsFramework::AssetSystemRequestBus::Broadcast(&AzToolsFramework::AssetSystem::AssetSystemRequest::GetFullSourcePathFromRelativeProductPath, assetInfo.m_relativePath, absolutePath);                

                clipBoard->setText(absolutePath.c_str());
            }
            else
            {
                clipBoard->setText(m_tabBar->tabText(index));
            }
        }        
    }

    void MainWindow::CloseNextTab()
    {
        if (m_isClosingTabs)
        {
            if (m_tabBar->count() == 0
                || (m_tabBar->count() == 1 && m_skipTabOnClose.IsValid()))
            {
                m_isClosingTabs = false;
                m_skipTabOnClose.SetInvalid();
                return;
            }

            int tab = 0;

            while (tab < m_tabBar->count())
            {                
                QVariant data = m_tabBar->tabData(tab);
                if (data.isValid())
                {
                    auto graphTabMetadata = data.value<Widget::GraphTabMetadata>();

                    if (graphTabMetadata.m_assetId != m_skipTabOnClose)
                    {                        
                        break;
                    }
                }

                tab++;
            }

            m_tabBar->tabCloseRequested(tab);
        }
    }

    void MainWindow::OnTabCloseRequest(int index)
    {
        QVariant data = m_tabBar->tabData(index);
        if (data.isValid())
        {
            auto graphTabMetadata = data.value<Widget::GraphTabMetadata>();
            if (graphTabMetadata.m_assetId == m_activeAssetId)
            {
                SetActiveAsset({});
            }

            if (graphTabMetadata.m_hostWidget)
            {
                graphTabMetadata.m_hostWidget->hide();
            }

            m_tabBar->CloseTab(index);
            m_tabBar->update();
            RemoveScriptCanvasAsset(graphTabMetadata.m_assetId);

            // Handling various close all events because the save is async need to deal with this in a bunch of different ways
            CloseNextTab();

            if (m_tabBar->count() == 0)
            {
                // The last tab has been removed.
                SetActiveAsset({});
            }
        }
    }

    void MainWindow::OnSelectionChanged()
    {
        QueuePropertyGridUpdate();
    }

    void MainWindow::OnVariableSelectionChanged(const AZStd::vector<AZ::EntityId>& variablePropertyIds)
    {
        m_selectedVariableIds = variablePropertyIds;

        QueuePropertyGridUpdate();
    }

    void MainWindow::QueuePropertyGridUpdate()
    {
        // Selection will be ignored when a delete operation is are taking place to prevent slowdown from processing
        // too many events at once.
        if (!m_ignoreSelection && !m_isInAutomation)
        {
            AddSystemTickAction(SystemTickActionFlag::RefreshPropertyGrid);
        }
    }

    void MainWindow::DequeuePropertyGridUpdate()
    {
        RemoveSystemTickAction(SystemTickActionFlag::RefreshPropertyGrid);
    }

    void MainWindow::SetDefaultLayout()
    {
        // Disable updates while we restore the layout to avoid temporary glitches
        // as the panes are moved around
        setUpdatesEnabled(false);

        if (m_commandLine)
        {
            m_commandLine->hide();
        }

        if (m_validationDockWidget)
        {
            addDockWidget(Qt::BottomDockWidgetArea, m_validationDockWidget);
            m_validationDockWidget->setFloating(false);
            m_validationDockWidget->hide();
        }

        if (m_logPanel)
        {
            addDockWidget(Qt::BottomDockWidgetArea, m_logPanel);
            m_logPanel->setFloating(false);
            m_logPanel->hide();
        }

        if (m_minimap)
        {
            addDockWidget(Qt::LeftDockWidgetArea, m_minimap);
            m_minimap->setFloating(false);
            m_minimap->hide();
        }

        if (m_nodePalette)
        {
            addDockWidget(Qt::LeftDockWidgetArea, m_nodePalette);
            m_nodePalette->setFloating(false);
            m_nodePalette->show();
        }

        if (m_variableDockWidget)
        {
            addDockWidget(Qt::RightDockWidgetArea, m_variableDockWidget);
            m_variableDockWidget->setFloating(false);
            m_variableDockWidget->show();
        }

        if (m_unitTestDockWidget)
        {
            addDockWidget(Qt::LeftDockWidgetArea, m_unitTestDockWidget);
            m_unitTestDockWidget->setFloating(false);
            m_unitTestDockWidget->hide();
        }

        if (m_loggingWindow)
        {
            addDockWidget(Qt::BottomDockWidgetArea, m_loggingWindow);
            m_loggingWindow->setFloating(false);
            m_loggingWindow->hide();
        }

        if (m_propertyGrid)
        {
            addDockWidget(Qt::RightDockWidgetArea, m_propertyGrid);
            m_propertyGrid->setFloating(false);
            m_propertyGrid->show();
        }

        if (m_bookmarkDockWidget)
        {
            addDockWidget(Qt::RightDockWidgetArea, m_bookmarkDockWidget);
            m_bookmarkDockWidget->setFloating(false);
            m_bookmarkDockWidget->hide();
        }

        if (m_nodeOutliner)
        {
            addDockWidget(Qt::RightDockWidgetArea, m_nodeOutliner);
            m_nodeOutliner->setFloating(false);
            m_nodeOutliner->hide();
        }

        if (m_minimap)
        {
            addDockWidget(Qt::RightDockWidgetArea, m_minimap);
            m_minimap->setFloating(false);
            m_minimap->hide();
        }
        
        resizeDocks(
        { m_nodePalette, m_propertyGrid },
        { static_cast<int>(size().width() * 0.15f), static_cast<int>(size().width() * 0.2f) },
            Qt::Horizontal);

        resizeDocks({ m_nodePalette, m_minimap },
        { static_cast<int>(size().height() * 0.70f), static_cast<int>(size().height() * 0.30f) },
            Qt::Vertical);
            

        resizeDocks({ m_propertyGrid, m_variableDockWidget },
        { static_cast<int>(size().height() * 0.70f), static_cast<int>(size().height() * 0.30f) },
            Qt::Vertical);

        resizeDocks({ m_validationDockWidget }, { static_cast<int>(size().height() * 0.01) }, Qt::Vertical);

        // Disabled until debugger is implemented
        //resizeDocks({ m_logPanel }, { static_cast<int>(size().height() * 0.1f) }, Qt::Vertical);

        // Re-enable updates now that we've finished adjusting the layout
        setUpdatesEnabled(true);

        m_defaultLayout = saveState();

        UpdateViewMenu();
    }

    void MainWindow::RefreshSelection()
    {
        AZ::EntityId scriptCanvasGraphId = GetActiveScriptCanvasGraphId();

        AZ::EntityId graphCanvasGraphId;
        EditorGraphRequestBus::EventResult(graphCanvasGraphId, scriptCanvasGraphId, &EditorGraphRequests::GetGraphCanvasGraphId);
                
        bool hasCopiableSelection = false;
        bool hasSelection = false;

        if (m_activeAssetId.IsValid())
        {
            if (graphCanvasGraphId.IsValid())
            {
                // Get the selected nodes.
                GraphCanvas::SceneRequestBus::EventResult(hasCopiableSelection, graphCanvasGraphId, &GraphCanvas::SceneRequests::HasCopiableSelection);
            }

            AZStd::vector< AZ::EntityId > selection;
            GraphCanvas::SceneRequestBus::EventResult(selection, graphCanvasGraphId, &GraphCanvas::SceneRequests::GetSelectedItems);

            selection.reserve(selection.size() + m_selectedVariableIds.size());
            selection.insert(selection.end(), m_selectedVariableIds.begin(), m_selectedVariableIds.end());

            if (!selection.empty())
            {
                hasSelection = true;
                m_propertyGrid->SetSelection(selection);
            }
            else
            {
                m_propertyGrid->ClearSelection();
            }
        }
        else
        {
            m_propertyGrid->ClearSelection();
        }

        // cut, copy and duplicate only works for specified items
        ui->action_Cut->setEnabled(hasCopiableSelection);
        ui->action_Copy->setEnabled(hasCopiableSelection);
        ui->action_Duplicate->setEnabled(hasCopiableSelection);

        ui->action_EnableSelection->setEnabled(hasCopiableSelection);
        ui->action_DisableSelection->setEnabled(hasCopiableSelection);

        // Delete will work for anything that is selectable
        ui->action_Delete->setEnabled(hasSelection);        
    }

    void MainWindow::OnViewNodePalette()
    {
        if (m_nodePalette)
        {
            m_nodePalette->toggleViewAction()->trigger();
        }
    }

    void MainWindow::OnViewOutline()
    {
        if (m_nodeOutliner)
        {
            m_nodeOutliner->toggleViewAction()->trigger();
        }
    }

    void MainWindow::OnViewMiniMap()
    {
        if (m_minimap)
        {
            m_minimap->toggleViewAction()->trigger();
        }
    }

    void MainWindow::OnViewLogWindow()
    {
        if (m_loggingWindow)
        {
            m_loggingWindow->toggleViewAction()->trigger();
        }
    }

    void MainWindow::OnViewGraphValidation()
    {
        if (m_validationDockWidget)
        {
            m_validationDockWidget->toggleViewAction()->trigger();
        }
    }

    void MainWindow::OnViewDebuggingWindow()
    {
        if (m_loggingWindow)
        {
            m_loggingWindow->toggleViewAction()->trigger();
        }
    }

    void MainWindow::OnViewStatisticsPanel()
    {
        if (m_statisticsDialog)
        {
            m_statisticsDialog->InitStatisticsWindow();
            m_statisticsDialog->show();
            m_statisticsDialog->raise();
            m_statisticsDialog->activateWindow();
        }
    }

    void MainWindow::OnViewPresetsEditor()
    {
        if (m_presetEditor && m_presetWrapper)
        {
            QSize boundingBox = size();
            QPointF newPosition = mapToGlobal(QPoint(boundingBox.width() * 0.5f, boundingBox.height() * 0.5f));

            m_presetEditor->show();

            m_presetWrapper->show();
            m_presetWrapper->raise();
            m_presetWrapper->activateWindow();

            QRect geometry = m_presetWrapper->geometry();
            QSize originalSize = geometry.size();

            newPosition.setX(newPosition.x() - geometry.width() * 0.5f);
            newPosition.setY(newPosition.y() - geometry.height() * 0.5f);

            geometry.setTopLeft(newPosition.toPoint());

            geometry.setWidth(originalSize.width());
            geometry.setHeight(originalSize.height());

            m_presetWrapper->setGeometry(geometry);
        }
    }

    void MainWindow::OnViewProperties()
    {
        if (m_propertyGrid)
        {
            m_propertyGrid->toggleViewAction()->trigger();
        }
    }

    void MainWindow::OnViewDebugger()
    {
    }

    void MainWindow::OnViewCommandLine()
    {
        if (m_commandLine->isVisible())
        {
            m_commandLine->hide();
        }
        else
        {
            m_commandLine->show();
        }
    }

    void MainWindow::OnViewLog()
    {
        if (m_logPanel)
        {
            m_logPanel->toggleViewAction()->trigger();
        }
    }

    void MainWindow::OnBookmarks()
    {
        if (m_bookmarkDockWidget)
        {
            m_bookmarkDockWidget->toggleViewAction()->trigger();
        }
    }

    void MainWindow::OnVariableManager()
    {
        if (m_variableDockWidget)
        {
            m_variableDockWidget->toggleViewAction()->trigger();
        }
    }

    void MainWindow::OnUnitTestManager()
    {
        if (m_unitTestDockWidget == nullptr)
        {
            CreateUnitTestWidget();
        }

        if (m_unitTestDockWidget)
        {
            m_unitTestDockWidget->toggleViewAction()->trigger();
        }
    }

    void MainWindow::OnRestoreDefaultLayout()
    {
        if (!m_defaultLayout.isEmpty())
        {
            restoreState(m_defaultLayout);
            UpdateViewMenu();
        }
    }

    void MainWindow::UpdateViewMenu()
    {
        if (ui->action_ViewBookmarks->isChecked() != m_bookmarkDockWidget->isVisible())
        {
            QSignalBlocker signalBlocker(ui->action_ViewBookmarks);
            ui->action_ViewBookmarks->setChecked(m_bookmarkDockWidget->isVisible());
        }

        if (ui->action_ViewMiniMap->isChecked() != m_minimap->isVisible())
        {
            QSignalBlocker signalBlocker(ui->action_ViewMiniMap);
            ui->action_ViewMiniMap->setChecked(m_minimap->isVisible());
        }

        if (ui->action_ViewNodePalette->isChecked() != m_nodePalette->isVisible())
        {
            QSignalBlocker signalBlocker(ui->action_ViewNodePalette);
            ui->action_ViewNodePalette->setChecked(m_nodePalette->isVisible());
        }

        if (ui->action_ViewOutline->isChecked() != m_nodeOutliner->isVisible())
        {
            QSignalBlocker signalBlocker(ui->action_ViewOutline);
            ui->action_ViewOutline->setChecked(m_nodeOutliner->isVisible());
        }

        if (ui->action_ViewProperties->isChecked() != m_propertyGrid->isVisible())
        {
            QSignalBlocker signalBlocker(ui->action_ViewProperties);
            ui->action_ViewProperties->setChecked(m_propertyGrid->isVisible());
        }

        if (ui->action_ViewVariableManager->isChecked() != m_variableDockWidget->isVisible())
        {
            QSignalBlocker signalBlocker(ui->action_ViewVariableManager);
            ui->action_ViewVariableManager->setChecked(m_variableDockWidget->isVisible());
        }

        if (ui->action_ViewLogWindow->isChecked() != m_loggingWindow->isVisible())
        {
            QSignalBlocker signalBlocker(ui->action_ViewLogWindow);
            ui->action_ViewLogWindow->setChecked(m_loggingWindow->isVisible());
        }

        if (ui->action_GraphValidation->isChecked() != m_validationDockWidget->isVisible())
        {
            QSignalBlocker signalBlocker(ui->action_GraphValidation);

            ui->action_GraphValidation->setChecked(m_validationDockWidget->isVisible());
        }

        if (ui->action_Debugging->isChecked() != m_loggingWindow->isVisible())
        {
            ui->action_Debugging->setChecked(m_loggingWindow->isVisible());
        }        

        // Want these two elements to be mutually exclusive.
        if (m_statusWidget->isVisible() == m_validationDockWidget->isVisible())
        {
            statusBar()->setVisible(!m_validationDockWidget->isVisible());
            m_statusWidget->setVisible(!m_validationDockWidget->isVisible());
        }
    }

    void MainWindow::DeleteNodes(const AZ::EntityId& graphCanvasGraphId, const AZStd::vector<AZ::EntityId>& nodes)
    {
        // clear the selection then delete the nodes that were selected
        GraphCanvas::SceneRequestBus::Event(graphCanvasGraphId, &GraphCanvas::SceneRequests::ClearSelection);
        GraphCanvas::SceneRequestBus::Event(graphCanvasGraphId, &GraphCanvas::SceneRequests::Delete, AZStd::unordered_set<AZ::EntityId>{ nodes.begin(), nodes.end() });
    }

    void MainWindow::DeleteConnections(const AZ::EntityId& graphCanvasGraphId, const AZStd::vector<AZ::EntityId>& connections)
    {
        ScopedVariableSetter<bool> scopedIgnoreSelection(m_ignoreSelection, true);
        GraphCanvas::SceneRequestBus::Event(graphCanvasGraphId, &GraphCanvas::SceneRequests::Delete, AZStd::unordered_set<AZ::EntityId>{ connections.begin(), connections.end() });
    }

    void MainWindow::DisconnectEndpoints(const AZ::EntityId& graphCanvasGraphId, const AZStd::vector<GraphCanvas::Endpoint>& endpoints)
    {
        AZStd::unordered_set<AZ::EntityId> connections;
        for (const auto& endpoint : endpoints)
        {
            AZStd::vector<AZ::EntityId> endpointConnections;
            GraphCanvas::SceneRequestBus::EventResult(endpointConnections, graphCanvasGraphId, &GraphCanvas::SceneRequests::GetConnectionsForEndpoint, endpoint);
            connections.insert(endpointConnections.begin(), endpointConnections.end());
        }
        DeleteConnections(graphCanvasGraphId, { connections.begin(), connections.end() });
    }

    void MainWindow::SaveWorkspace(bool updateAssetList)
    {
        auto workspace = AZ::UserSettings::CreateFind<EditorSettings::EditorWorkspace>(AZ_CRC("ScriptCanvasEditorWindowState", 0x10c47d36), AZ::UserSettings::CT_LOCAL);
        if (workspace)
        {
            workspace->Init(saveState(), saveGeometry());

            if (updateAssetList && m_tabBar)
            {
                AZStd::vector< AZ::Data::AssetId > activeAssets;

                activeAssets.reserve(m_tabBar->count());

                AZ::Data::AssetId focusedAssetId = m_tabBar->FindAssetId(m_tabBar->currentIndex());

                for (int i = 0; i < m_tabBar->count(); ++i)
                {
                    AZ::Data::AssetId assetId = m_tabBar->FindAssetId(i);

                    ScriptCanvasFileState fileState;
                    DocumentContextRequestBus::BroadcastResult(fileState, &DocumentContextRequests::GetScriptCanvasAssetModificationState, assetId);

                    if (fileState != ScriptCanvasFileState::NEW)
                    {
                        activeAssets.emplace_back(assetId);
                    }
                    else if (assetId == focusedAssetId)
                    {
                        focusedAssetId.SetInvalid();
                    }
                }

                // If our currently foccussed asset won't be restored, just show the first element.
                if (!focusedAssetId.IsValid())
                {
                    if (!activeAssets.empty())
                    {
                        focusedAssetId = activeAssets.front();
                    }
                }

                workspace->ConfigureActiveAssets(focusedAssetId, activeAssets);
            }
        }
    }

    void MainWindow::RestoreWorkspace()
    {
        auto workspace = AZ::UserSettings::Find<EditorSettings::EditorWorkspace>(AZ_CRC("ScriptCanvasEditorWindowState", 0x10c47d36), AZ::UserSettings::CT_LOCAL);
        if (workspace)
        {
            workspace->Restore(this);

            UpdateViewMenu();

            if (m_userSettings && m_userSettings->m_rememberOpenCanvases)
            {
                AZStd::vector< AZ::Data::AssetId > assetIds = workspace->GetActiveAssetIds();

                for (const auto& assetId : assetIds)
                {
                    m_loadedWorkspaceAssets.insert(assetId);
                    OpenScriptCanvasAssetId(assetId);
                }

                AZ::Data::AssetId focusedAssetId = workspace->GetFocusedAssetId();

                if (focusedAssetId.IsValid())
                {
                    m_postLoadFocusId = focusedAssetId;
                }
            }
        }
    }

    void MainWindow::RunBatchConversion()
    {
        if (m_batchConverter == nullptr)
        {
            QFileDialog directoryDialog(this);
            directoryDialog.setFileMode(QFileDialog::FileMode::Directory);

            if (directoryDialog.exec())
            {
                QStringList directories = directoryDialog.selectedFiles();

                m_batchConverter = aznew ScriptCanvasBatchConverter(this, directories);
            }
        }
    }

    void MainWindow::OnShowValidationErrors()
    {
        m_userSettings->m_showValidationErrors = true;

        if (!m_validationDockWidget->isVisible())
        {
            OnViewGraphValidation();

            // If the window wasn't visible, it doesn't seem to get the signals.
            // So need to manually prompt it to get the desired result
            m_validationDockWidget->OnShowErrors();
        }
    }

    void MainWindow::OnShowValidationWarnings()
    {
        m_userSettings->m_showValidationWarnings = true;

        if (!m_validationDockWidget->isVisible())
        {
            OnViewGraphValidation();

            // If the window wasn't visible, it doesn't seem to get the signals.
            // So need to manually prompt it to get the desired result
            m_validationDockWidget->OnShowWarnings();
        }
    }

    void MainWindow::OnValidateCurrentGraph()
    {
        OpenValidationPanel();

        m_validationDockWidget->OnRunValidator();
    }

    void MainWindow::OnViewParamsChanged(const GraphCanvas::ViewParams& viewParams)
    {
        AZ_UNUSED(viewParams);
        RestartAutoTimerSave();
    }

    void MainWindow::OnZoomChanged(qreal zoomLevel)
    {
        RestartAutoTimerSave();
    }

    void MainWindow::AfterEntitySelectionChanged(const AzToolsFramework::EntityIdList&, const AzToolsFramework::EntityIdList&)
    {
        UpdateAssignToSelectionState();
    }

    void MainWindow::UpdateAssignToSelectionState()
    {
        AzToolsFramework::EntityIdList selectedEntityIds;
        AzToolsFramework::ToolsApplicationRequestBus::BroadcastResult(selectedEntityIds, &AzToolsFramework::ToolsApplicationRequests::GetSelectedEntities);

        bool buttonEnabled = m_activeAssetId.IsValid() && selectedEntityIds.size() >= 1;

        if (buttonEnabled)
        {
            ScriptCanvasFileState fileState;
            DocumentContextRequestBus::BroadcastResult(fileState, &DocumentContextRequests::GetScriptCanvasAssetModificationState, m_activeAssetId);

            if (fileState == ScriptCanvasFileState::INVALID
                || fileState == ScriptCanvasFileState::NEW)
            {
                buttonEnabled = false;
            }
        }       

        m_assignToSelectedEntity->setEnabled(buttonEnabled);
    }

    NodeIdPair MainWindow::ProcessCreateNodeMimeEvent(GraphCanvas::GraphCanvasMimeEvent* mimeEvent, const AZ::EntityId& graphCanvasGraphId, AZ::Vector2 nodeCreationPos)
    {
        if (!m_isInAutomation)
        {
            GraphCanvas::SceneRequestBus::Event(graphCanvasGraphId, &GraphCanvas::SceneRequests::ClearSelection);
        }

        NodeIdPair retVal;

        if (azrtti_istypeof<CreateNodeMimeEvent>(mimeEvent))
        {
            CreateNodeMimeEvent* createEvent = static_cast<CreateNodeMimeEvent*>(mimeEvent);            

            if (createEvent->ExecuteEvent(nodeCreationPos, nodeCreationPos, graphCanvasGraphId))
            {
                retVal = createEvent->GetCreatedPair();
            }
        }
        else if (azrtti_istypeof<SpecializedCreateNodeMimeEvent>(mimeEvent))
        {
            SpecializedCreateNodeMimeEvent* specializedCreationEvent = static_cast<SpecializedCreateNodeMimeEvent*>(mimeEvent);
            retVal = specializedCreationEvent->ConstructNode(graphCanvasGraphId, nodeCreationPos);
        }

        return retVal;
    }

    const GraphCanvas::GraphCanvasTreeItem* MainWindow::GetNodePaletteRoot() const
    {
        return m_nodePalette->GetTreeRoot();
    }

    void MainWindow::SignalAutomationBegin()
    {
        m_isInAutomation = true;
    }

    void MainWindow::SignalAutomationEnd()
    {
        m_isInAutomation = false;
    }

    AZ::EntityId MainWindow::FindEditorNodeIdByAssetNodeId(const AZ::Data::AssetId& assetId, AZ::EntityId assetNodeId) const
    {
        if (auto sceneData = m_assetGraphSceneMapper.GetByAssetId(assetId))
        {
            auto iter = sceneData->m_assetToEditorEntityIdMap.find(assetNodeId);
            if (iter != sceneData->m_assetToEditorEntityIdMap.end())
            {
                return iter->second;
            }
        }

        return AZ::EntityId{};
    }

    AZ::EntityId MainWindow::FindAssetNodeIdByEditorNodeId(const AZ::Data::AssetId& assetId, AZ::EntityId editorNodeId) const
    {
        if (auto sceneData = m_assetGraphSceneMapper.GetByAssetId(assetId))
        {
            auto iter = sceneData->m_editorToAssetEntityIdMap.find(editorNodeId);
            if (iter != sceneData->m_editorToAssetEntityIdMap.end())
            {
                return iter->second;
            }
        }

        return AZ::EntityId{};
    }

    GraphCanvas::Endpoint MainWindow::CreateNodeForProposal(const AZ::EntityId& connectionId, const GraphCanvas::Endpoint& endpoint, const QPointF& scenePoint, const QPoint& screenPoint)
    {
        PushPreventUndoStateUpdate();

        GraphCanvas::Endpoint retVal;

        AZ::EntityId graphCanvasGraphId = (*GraphCanvas::SceneUIRequestBus::GetCurrentBusId());

        m_sceneContextMenu->FilterForSourceSlot(graphCanvasGraphId, endpoint.GetSlotId());
        m_sceneContextMenu->RefreshActions(graphCanvasGraphId, connectionId);

        QAction* action = m_sceneContextMenu->exec(screenPoint);

        // If the action returns null. We need to check if it was our widget, or just a close command.
        if (action == nullptr)
        {
            GraphCanvas::GraphCanvasMimeEvent* mimeEvent = m_sceneContextMenu->GetNodePalette()->GetContextMenuEvent();

            if (mimeEvent)
            {
                bool isValid = false;
                NodeIdPair finalNode = ProcessCreateNodeMimeEvent(mimeEvent, graphCanvasGraphId, AZ::Vector2(scenePoint.x(), scenePoint.y()));

                if (finalNode.m_graphCanvasId.IsValid())
                {
                    GraphCanvas::VisualRequestBus::Event(finalNode.m_graphCanvasId, &GraphCanvas::VisualRequests::SetVisible, false);
                    retVal = HandleProposedConnection(graphCanvasGraphId, connectionId, endpoint, finalNode.m_graphCanvasId, screenPoint);
                }

                if (retVal.IsValid())
                {
                    GraphCanvas::GraphUtils::CreateOpportunisticConnectionsBetween(endpoint, retVal);
                    GraphCanvas::VisualRequestBus::Event(finalNode.m_graphCanvasId, &GraphCanvas::VisualRequests::SetVisible, true);

                    AZ::Vector2 position;
                    GraphCanvas::GeometryRequestBus::EventResult(position, retVal.GetNodeId(), &GraphCanvas::GeometryRequests::GetPosition);

                    QPointF connectionPoint;
                    GraphCanvas::SlotUIRequestBus::EventResult(connectionPoint, retVal.GetSlotId(), &GraphCanvas::SlotUIRequests::GetConnectionPoint);
                    
                    qreal verticalOffset = connectionPoint.y() - position.GetY();
                    position.SetY(scenePoint.y() - verticalOffset);

                    qreal horizontalOffset = connectionPoint.x() - position.GetX();
                    position.SetX(scenePoint.x() - horizontalOffset);

                    GraphCanvas::GeometryRequestBus::Event(retVal.GetNodeId(), &GraphCanvas::GeometryRequests::SetPosition, position);

                    GraphCanvas::SceneNotificationBus::Event(graphCanvasGraphId, &GraphCanvas::SceneNotifications::PostCreationEvent);
                }
                else
                {
                    GraphCanvas::GraphUtils::DeleteOutermostNode(graphCanvasGraphId, finalNode.m_graphCanvasId);
                }
            }
        }

        PopPreventUndoStateUpdate();

        return retVal;
    }

    void MainWindow::OnWrapperNodeActionWidgetClicked(const AZ::EntityId& wrapperNode, const QRect& actionWidgetBoundingRect, const QPointF& scenePoint, const QPoint& screenPoint)
    {
        AZ::EntityId sceneId = (*GraphCanvas::SceneUIRequestBus::GetCurrentBusId());

        if (EBusHandlerNodeDescriptorRequestBus::FindFirstHandler(wrapperNode) != nullptr)
        {
            m_ebusHandlerActionMenu->SetEbusHandlerNode(wrapperNode);

            // We don't care about the result, since the actions are done on demand with the menu
            m_ebusHandlerActionMenu->exec(screenPoint);
        }
        else if (ScriptCanvasWrapperNodeDescriptorRequestBus::FindFirstHandler(wrapperNode) != nullptr)
        {
            ScriptCanvasWrapperNodeDescriptorRequestBus::Event(wrapperNode, &ScriptCanvasWrapperNodeDescriptorRequests::OnWrapperAction, actionWidgetBoundingRect, scenePoint, screenPoint);
        }
    }

    void MainWindow::OnSelectionManipulationBegin()
    {
        m_ignoreSelection = true;
    }

    void MainWindow::OnSelectionManipulationEnd()
    {
        m_ignoreSelection = false;
        OnSelectionChanged();
    }

    AZ::EntityId MainWindow::CreateNewGraph()
    {
        AZ::EntityId graphId;

        OnFileNew();

        if (m_activeAssetId.IsValid())
        {
            graphId = GetActiveGraphCanvasGraphId();
        }

        return graphId;
    }

    void MainWindow::CustomizeConnectionEntity(AZ::Entity* connectionEntity)
    {
        connectionEntity->CreateComponent<SceneMemberMappingComponent>();
    }

    void MainWindow::ShowAssetPresetsMenu(GraphCanvas::ConstructType constructType)
    {
        OnViewPresetsEditor();

        if (m_presetEditor)
        {
            m_presetEditor->SetActiveConstructType(constructType);
        }
    }

    //! Hook for receiving context menu events for each QGraphicsScene
    GraphCanvas::ContextMenuAction::SceneReaction MainWindow::ShowSceneContextMenu(const QPoint& screenPoint, const QPointF& scenePoint)
    {
        bool tryDaisyChain = (QApplication::keyboardModifiers() & Qt::KeyboardModifier::ShiftModifier) != 0;

        GraphCanvas::GraphId graphCanvasGraphId = GetActiveGraphCanvasGraphId();
        AZ::EntityId scriptCanvasGraphId = GetActiveScriptCanvasGraphId();

        if (!graphCanvasGraphId.IsValid() || !scriptCanvasGraphId.IsValid())
        {
            // Nothing to do.
            return GraphCanvas::ContextMenuAction::SceneReaction::Nothing;
        }

        m_sceneContextMenu->ResetSourceSlotFilter();
        m_sceneContextMenu->RefreshActions(graphCanvasGraphId, AZ::EntityId());
        QAction* action = m_sceneContextMenu->exec(screenPoint);

        GraphCanvas::ContextMenuAction::SceneReaction reaction = GraphCanvas::ContextMenuAction::SceneReaction::Nothing;

        if (action == nullptr)
        {
            GraphCanvas::GraphCanvasMimeEvent* mimeEvent = m_sceneContextMenu->GetNodePalette()->GetContextMenuEvent();
            
            NodeIdPair finalNode = ProcessCreateNodeMimeEvent(mimeEvent, graphCanvasGraphId, AZ::Vector2(scenePoint.x(), scenePoint.y()));

            GraphCanvas::SceneRequestBus::Event(graphCanvasGraphId, &GraphCanvas::SceneRequests::ClearSelection);

            if (finalNode.m_graphCanvasId.IsValid())
            {
                GraphCanvas::VisualRequestBus::Event(finalNode.m_graphCanvasId, &GraphCanvas::VisualRequests::SetVisible, true);

                AZ::Vector2 position;
                GraphCanvas::GeometryRequestBus::EventResult(position, finalNode.m_graphCanvasId, &GraphCanvas::GeometryRequests::GetPosition);
                GraphCanvas::GeometryRequestBus::Event(finalNode.m_graphCanvasId, &GraphCanvas::GeometryRequests::SetPosition, position);

                GraphCanvas::SceneNotificationBus::Event(graphCanvasGraphId, &GraphCanvas::SceneNotifications::PostCreationEvent);

                if (tryDaisyChain)
                {
                    QTimer::singleShot(50, [graphCanvasGraphId, finalNode, screenPoint, scenePoint]()
                    {
                        GraphCanvas::SceneRequestBus::Event(graphCanvasGraphId, &GraphCanvas::SceneRequests::HandleProposalDaisyChain, finalNode.m_graphCanvasId, GraphCanvas::SlotTypes::ExecutionSlot, GraphCanvas::CT_Output, screenPoint, scenePoint);
                    });
                }
            }
        }
        else
        {
            GraphCanvas::ContextMenuAction* contextMenuAction = qobject_cast<GraphCanvas::ContextMenuAction*>(action);

            if (contextMenuAction)
            {
                PushPreventUndoStateUpdate();
                AZ::Vector2 mousePoint(scenePoint.x(), scenePoint.y());
                reaction = contextMenuAction->TriggerAction(graphCanvasGraphId, mousePoint);
                PopPreventUndoStateUpdate();
            }
        }

        return reaction;
    }

    //! Hook for receiving context menu events for each QGraphicsScene
    GraphCanvas::ContextMenuAction::SceneReaction MainWindow::ShowNodeContextMenu(const AZ::EntityId& nodeId, const QPoint& screenPoint, const QPointF& scenePoint)
    {
        GraphCanvas::NodeContextMenu contextMenu(ScriptCanvasEditor::AssetEditorId);
        return HandleContextMenu(contextMenu, nodeId, screenPoint, scenePoint);
    }

    GraphCanvas::ContextMenuAction::SceneReaction MainWindow::ShowCommentContextMenu(const AZ::EntityId& nodeId, const QPoint& screenPoint, const QPointF& scenePoint)
    {
        GraphCanvas::CommentContextMenu contextMenu(ScriptCanvasEditor::AssetEditorId);
        return HandleContextMenu(contextMenu, nodeId, screenPoint, scenePoint);
    }

    GraphCanvas::ContextMenuAction::SceneReaction MainWindow::ShowNodeGroupContextMenu(const AZ::EntityId& groupId, const QPoint& screenPoint, const QPointF& scenePoint)
    {
        GraphCanvas::NodeGroupContextMenu contextMenu(ScriptCanvasEditor::AssetEditorId);
        return HandleContextMenu(contextMenu, groupId, screenPoint, scenePoint);
    }

    GraphCanvas::ContextMenuAction::SceneReaction MainWindow::ShowCollapsedNodeGroupContextMenu(const AZ::EntityId& nodeId, const QPoint& screenPoint, const QPointF& scenePoint)
    {
        GraphCanvas::CollapsedNodeGroupContextMenu contextMenu(ScriptCanvasEditor::AssetEditorId);
        return HandleContextMenu(contextMenu, nodeId, screenPoint, scenePoint);
    }

    GraphCanvas::ContextMenuAction::SceneReaction MainWindow::ShowBookmarkContextMenu(const AZ::EntityId& bookmarkId, const QPoint& screenPoint, const QPointF& scenePoint)
    {
        GraphCanvas::BookmarkContextMenu contextMenu(ScriptCanvasEditor::AssetEditorId);
        return HandleContextMenu(contextMenu, bookmarkId, screenPoint, scenePoint);
    }

    GraphCanvas::ContextMenuAction::SceneReaction MainWindow::ShowConnectionContextMenu(const AZ::EntityId& connectionId, const QPoint& screenPoint, const QPointF& scenePoint)
    {
        PushPreventUndoStateUpdate();

        GraphCanvas::ContextMenuAction::SceneReaction reaction = GraphCanvas::ContextMenuAction::SceneReaction::Nothing;

        AZ::Vector2 sceneVector(scenePoint.x(), scenePoint.y());
        GraphCanvas::GraphId graphCanvasGraphId = GetActiveGraphCanvasGraphId();

        m_connectionContextMenu->RefreshActions(graphCanvasGraphId, connectionId);

        QAction* result = m_connectionContextMenu->exec(screenPoint);

        GraphCanvas::ContextMenuAction* contextMenuAction = qobject_cast<GraphCanvas::ContextMenuAction*>(result);

        // If the action returns null. We need to check if it was our widget, or just a close command.
        if (contextMenuAction)
        {
            reaction = contextMenuAction->TriggerAction(graphCanvasGraphId, sceneVector);
        }
        else
        {
            GraphCanvas::GraphCanvasMimeEvent* mimeEvent = m_connectionContextMenu->GetNodePalette()->GetContextMenuEvent();

            if (mimeEvent)
            {
                bool isValid = false;
                NodeIdPair finalNode = ProcessCreateNodeMimeEvent(mimeEvent, graphCanvasGraphId, AZ::Vector2(scenePoint.x(), scenePoint.y()));

                GraphCanvas::Endpoint sourceEndpoint;
                GraphCanvas::ConnectionRequestBus::EventResult(sourceEndpoint, connectionId, &GraphCanvas::ConnectionRequests::GetSourceEndpoint);

                GraphCanvas::Endpoint targetEndpoint;
                GraphCanvas::ConnectionRequestBus::EventResult(targetEndpoint, connectionId, &GraphCanvas::ConnectionRequests::GetTargetEndpoint);

                if (finalNode.m_graphCanvasId.IsValid())
                {
                    GraphCanvas::ConnectionSpliceConfig spliceConfig;
                    spliceConfig.m_allowOpportunisticConnections = true;

                    if (!GraphCanvas::GraphUtils::SpliceNodeOntoConnection(finalNode.m_graphCanvasId, connectionId, spliceConfig))
                    {
                        GraphCanvas::GraphUtils::DeleteOutermostNode(graphCanvasGraphId, finalNode.m_graphCanvasId);
                    }
                    else
                    {
                        reaction = GraphCanvas::ContextMenuAction::SceneReaction::PostUndo;

                        // Now we can deal with the alignment of the node.
                        GraphCanvas::VisualRequestBus::Event(finalNode.m_graphCanvasId, &GraphCanvas::VisualRequests::SetVisible, true);

                        AZ::Vector2 position(0,0);
                        GraphCanvas::GeometryRequestBus::EventResult(position, finalNode.m_graphCanvasId, &GraphCanvas::GeometryRequests::GetPosition);

                        QPointF sourceConnectionPoint(0,0);
                        GraphCanvas::SlotUIRequestBus::EventResult(sourceConnectionPoint, spliceConfig.m_splicedSourceEndpoint.GetSlotId(), &GraphCanvas::SlotUIRequests::GetConnectionPoint);

                        QPointF targetConnectionPoint(0,0);
                        GraphCanvas::SlotUIRequestBus::EventResult(targetConnectionPoint, spliceConfig.m_splicedTargetEndpoint.GetSlotId(), &GraphCanvas::SlotUIRequests::GetConnectionPoint);

                        // Average our two points so we splice roughly in the center of our node.
                        QPointF connectionPoint = (sourceConnectionPoint + targetConnectionPoint) * 0.5f;

                        qreal verticalOffset = connectionPoint.y() - position.GetY();
                        position.SetY(scenePoint.y() - verticalOffset);

                        qreal horizontalOffset = connectionPoint.x() - position.GetX();
                        position.SetX(scenePoint.x() - horizontalOffset);

                        GraphCanvas::GeometryRequestBus::Event(finalNode.m_graphCanvasId, &GraphCanvas::GeometryRequests::SetPosition, position);

                        if (IsSplicedNodeNudgingEnabled())
                        {
                            GraphCanvas::NodeNudgingController nudgingController(graphCanvasGraphId, { finalNode.m_graphCanvasId });
                            nudgingController.FinalizeNudging();
                        }

                        GraphCanvas::SceneNotificationBus::Event(graphCanvasGraphId, &GraphCanvas::SceneNotifications::PostCreationEvent);
                    }
                }
            }
        }

        PopPreventUndoStateUpdate();

        return reaction;
    }

    GraphCanvas::ContextMenuAction::SceneReaction MainWindow::ShowSlotContextMenu(const AZ::EntityId& slotId, const QPoint& screenPoint, const QPointF& scenePoint)
    {
        GraphCanvas::SlotContextMenu contextMenu(ScriptCanvasEditor::AssetEditorId);
        return HandleContextMenu(contextMenu, slotId, screenPoint, scenePoint);
    }

    void MainWindow::OnSystemTick()
    {
        if (HasSystemTickAction(SystemTickActionFlag::RefreshPropertyGrid))
        {
            RemoveSystemTickAction(SystemTickActionFlag::RefreshPropertyGrid);            
            RefreshSelection();
        }

        if (HasSystemTickAction(SystemTickActionFlag::CloseWindow))
        {
            RemoveSystemTickAction(SystemTickActionFlag::CloseWindow);
            qobject_cast<QWidget*>(parent())->close();
        }

        AZ::SystemTickBus::Handler::BusDisconnect();
    }

    void MainWindow::OnCommandStarted(AZ::Crc32)
    {
        PushPreventUndoStateUpdate();
    }

    void MainWindow::OnCommandFinished(AZ::Crc32)
    {
        PopPreventUndoStateUpdate();
    }

    void MainWindow::SignalBatchOperationComplete(BatchOperatorTool* batchTool)
    {
        if (m_batchConverter == batchTool)
        {
            delete m_batchConverter;
            m_batchConverter = nullptr;
        }
    }

    void MainWindow::PrepareActiveAssetForSave()
    {
        PrepareAssetForSave(m_activeAssetId);
    }

    void MainWindow::PrepareAssetForSave(const AZ::Data::AssetId& assetId)
    {
        AZ::EntityId scriptCanvasGraphId = FindGraphCanvasGraphIdByAssetId(assetId);
        AZ::EntityId graphCanvasGraphId = FindScriptCanvasGraphIdByAssetId(assetId);

        GraphCanvas::GraphModelRequestBus::Event(graphCanvasGraphId, &GraphCanvas::GraphModelRequests::OnSaveDataDirtied, scriptCanvasGraphId);
        GraphCanvas::GraphModelRequestBus::Event(graphCanvasGraphId, &GraphCanvas::GraphModelRequests::OnSaveDataDirtied, graphCanvasGraphId);
    }

    void MainWindow::RestartAutoTimerSave(bool forceTimer)
    {
        if (m_autoSaveTimer.isActive() || forceTimer)
        {
            m_autoSaveTimer.stop();
            m_autoSaveTimer.start();
        }
    }

    void MainWindow::OnSelectedEntitiesAboutToShow()
    {
        AzToolsFramework::EntityIdList selectedEntityIds;
        AzToolsFramework::ToolsApplicationRequestBus::BroadcastResult(selectedEntityIds, &AzToolsFramework::ToolsApplicationRequests::GetSelectedEntities);

        m_selectedEntityMenu->clear();

        for (const AZ::EntityId& entityId : selectedEntityIds)
        {
            if (EditorScriptCanvasComponentRequestBus::FindFirstHandler(entityId) != nullptr)
            {
                AZ::NamedEntityId namedEntityId(entityId);

                QAction* actionElement = new QAction(namedEntityId.GetName().data(), m_selectedEntityMenu);

                QObject::connect(actionElement, &QAction::triggered, [this, entityId]() {
                    OnAssignToEntity(entityId);
                });

                m_selectedEntityMenu->addAction(actionElement);
            }
        }
    }

    void MainWindow::OnAssignToSelectedEntities()
    {
        AzToolsFramework::EntityIdList selectedEntityIds;
        AzToolsFramework::ToolsApplicationRequestBus::BroadcastResult(selectedEntityIds, &AzToolsFramework::ToolsApplicationRequests::GetSelectedEntities);

        for (const AZ::EntityId& entityId : selectedEntityIds)
        {
            if (EditorScriptCanvasComponentRequestBus::FindFirstHandler(entityId) != nullptr)
            {
                OnAssignToEntity(entityId);
            }
        }
    }

    void MainWindow::OnAssignToEntity(const AZ::EntityId& entityId)
    {        
        ScriptCanvasFileState fileState;
        DocumentContextRequestBus::BroadcastResult(fileState, &DocumentContextRequests::GetScriptCanvasAssetModificationState, m_activeAssetId);

        if (fileState != ScriptCanvasFileState::NEW)
        {
            EditorScriptCanvasComponentRequestBus::Event(entityId, &EditorScriptCanvasComponentRequests::SetAssetId, m_activeAssetId);
        }        
    }

    bool MainWindow::HasSystemTickAction(SystemTickActionFlag action)
    {
        return (m_systemTickActions & action) != 0;
    }

    void MainWindow::RemoveSystemTickAction(SystemTickActionFlag action)
    {
        m_systemTickActions = m_systemTickActions & (~action);
    }

    void MainWindow::AddSystemTickAction(SystemTickActionFlag action)
    {
        if (!AZ::SystemTickBus::Handler::BusIsConnected())
        {
            AZ::SystemTickBus::Handler::BusConnect();
        }

        m_systemTickActions |= action;
    }

    void MainWindow::BlockCloseRequests()
    {
        m_queueCloseRequest = true;
    }

    void MainWindow::UnblockCloseRequests()
    {
        if (m_queueCloseRequest)
        {
            m_queueCloseRequest = false;

            if (m_hasQueuedClose)
            {
                qobject_cast<QWidget*>(parent())->close();
            }
        }
    }

    double MainWindow::GetSnapDistance() const
    {
        if (m_userSettings)
        {
            return m_userSettings->m_snapDistance;
        }

        return 10.0;
    }

    bool MainWindow::IsBookmarkViewportControlEnabled() const
    {
        if (m_userSettings)
        {
            return m_userSettings->m_allowBookmarkViewpointControl;
        }

        return false;
    }

    bool MainWindow::IsDragNodeCouplingEnabled() const
    {
        if (m_userSettings)
        {
            return m_userSettings->m_dragNodeCouplingConfig.m_enabled;
        }

        return false;
    }

    AZStd::chrono::milliseconds MainWindow::GetDragCouplingTime() const
    {
        if (m_userSettings)
        {
            return AZStd::chrono::milliseconds(m_userSettings->m_dragNodeCouplingConfig.m_timeMS);
        }

        return AZStd::chrono::milliseconds(500);
    }

    bool MainWindow::IsDragConnectionSpliceEnabled() const
    {
        if (m_userSettings)
        {
            return m_userSettings->m_dragNodeSplicingConfig.m_enabled;
        }

        return false;
    }

    AZStd::chrono::milliseconds MainWindow::GetDragConnectionSpliceTime() const
    {
        if (m_userSettings)
        {
            return AZStd::chrono::milliseconds(m_userSettings->m_dragNodeSplicingConfig.m_timeMS);
        }

        return AZStd::chrono::milliseconds(500);
    }

    bool MainWindow::IsDropConnectionSpliceEnabled() const
    {
        if (m_userSettings)
        {
            return m_userSettings->m_dropNodeSplicingConfig.m_enabled;
        }

        return false;
    }

    AZStd::chrono::milliseconds MainWindow::GetDropConnectionSpliceTime() const
    {
        if (m_userSettings)
        {
            return AZStd::chrono::milliseconds(m_userSettings->m_dropNodeSplicingConfig.m_timeMS);
        }

        return AZStd::chrono::milliseconds(500);
    }

    bool MainWindow::IsSplicedNodeNudgingEnabled() const
    {        
        if (m_userSettings)
        {
            return m_userSettings->m_allowNodeNudgingOnSplice;
        }

        return false;
    }

    bool MainWindow::IsShakeToDespliceEnabled() const
    {
        if (m_userSettings)
        {
            return m_userSettings->m_shakeDespliceConfig.m_enabled;
        }

        return false;
    }

    int MainWindow::GetShakesToDesplice() const
    {
        if (m_userSettings)
        {
            return m_userSettings->m_shakeDespliceConfig.m_shakeCount;
        }

        return 3;
    }

    float MainWindow::GetMinimumShakePercent() const
    {
        if (m_userSettings)
        {
            return m_userSettings->m_shakeDespliceConfig.GetMinimumShakeLengthPercent();
        }

        return 0.03f;
    }

    float MainWindow::GetShakeDeadZonePercent() const 
    {
        if (m_userSettings)
        {
            return m_userSettings->m_shakeDespliceConfig.GetDeadZonePercent();
        }

        return 0.01f;
    }

    float MainWindow::GetShakeStraightnessPercent() const 
    {
        if (m_userSettings)
        {
            return m_userSettings->m_shakeDespliceConfig.GetStraightnessPercent();
        }

        return 0.75f;
    }

    AZStd::chrono::milliseconds MainWindow::GetMaximumShakeDuration() const
    {
        if (m_userSettings)
        {
            return AZStd::chrono::milliseconds(m_userSettings->m_shakeDespliceConfig.m_maximumShakeTimeMS);
        }

        return AZStd::chrono::milliseconds(500);
    }

    AZStd::chrono::milliseconds MainWindow::GetAlignmentTime() const
    {
        if (m_userSettings)
        {
            return AZStd::chrono::milliseconds(m_userSettings->m_alignmentTimeMS);
        }

        return AZStd::chrono::milliseconds(250);
    }

    float MainWindow::GetMaxZoom() const
    {
        if (m_userSettings)
        {
            return m_userSettings->m_zoomSettings.GetMaxZoom();
        }

        return 2.0f;
    }

    float MainWindow::GetEdgePanningPercentage() const
    {
        if (m_userSettings)
        {
            return m_userSettings->m_edgePanningSettings.GetEdgeScrollPercent();
        }

        return 0.1f;
    }

    float MainWindow::GetEdgePanningScrollSpeed() const
    {
        if (m_userSettings)
        {
            return m_userSettings->m_edgePanningSettings.GetEdgeScrollSpeed();
        }

        return 100.0f;
    }

    GraphCanvas::EditorConstructPresets* MainWindow::GetConstructPresets() const
    {
        if (m_userSettings)
        {
            return &m_userSettings->m_constructPresets;
        }

        return nullptr;
    }

    const GraphCanvas::ConstructTypePresetBucket* MainWindow::GetConstructTypePresetBucket(GraphCanvas::ConstructType constructType) const
    {
        GraphCanvas::EditorConstructPresets* presets = GetConstructPresets();

        if (presets)
        {
            return presets->FindPresetBucket(constructType);
        }

        return nullptr;
    }

    GraphCanvas::Styling::ConnectionCurveType MainWindow::GetConnectionCurveType() const
    {
        if (m_userSettings)
        {
            return m_userSettings->m_stylingSettings.GetConnectionCurveType();
        }

        return GraphCanvas::Styling::ConnectionCurveType::Straight;
    }

    GraphCanvas::Styling::ConnectionCurveType MainWindow::GetDataConnectionCurveType() const
    {
        if (m_userSettings)
        {
            return m_userSettings->m_stylingSettings.GetDataConnectionCurveType();
        }

        return GraphCanvas::Styling::ConnectionCurveType::Straight;
    }

    bool MainWindow::AllowNodeDisabling() const
    {
        return true;
    }

    void MainWindow::CreateUnitTestWidget()
    {
        m_unitTestDockWidget = aznew UnitTestDockWidget(this);
        m_unitTestDockWidget->setObjectName("TestManager");
        m_unitTestDockWidget->setFloating(true);
        m_unitTestDockWidget->hide();
    }

#include <Editor/View/Windows/MainWindow.moc>
}