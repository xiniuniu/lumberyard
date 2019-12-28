
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
#include "StdAfx.h"

#include "QtEditorApplication.h"

#include <QByteArray>
#include <QWidget>
#include <QWheelEvent>
#include <QAbstractEventDispatcher>
#include <QFile>
#include <QFileInfo>
#include <QDir>

#ifdef DEPRECATED_QML_SUPPORT
#include <QQmlEngine>
#endif

#include <QDebug>
#include "../Plugins/EditorUI_QT/UIFactory.h"
#include <AzQtComponents/Components/LumberyardStylesheet.h>
#include <AzQtComponents/Components/Titlebar.h>
#include <AzQtComponents/Components/WindowDecorationWrapper.h>
#include <AzQtComponents/Utilities/QtPluginPaths.h>
#include <QToolBar>
#include <QTimer>
#include <QLoggingCategory>

#if defined(AZ_PLATFORM_WINDOWS)
#include <QtGui/qpa/qplatformnativeinterface.h>
#include <QtGui/private/qhighdpiscaling_p.h>
#endif

#include "Material/MaterialManager.h"

#include "Util/BoostPythonHelpers.h"

#include <AzCore/EBus/EBus.h>
#include <AzCore/UserSettings/UserSettings.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/IO/SystemFile.h>

#include <AzToolsFramework/Metrics/LyEditorMetricsBus.h>

#if defined(AZ_PLATFORM_WINDOWS)
#   include <AzFramework/Input/Buses/Notifications/RawInputNotificationBus_Platform.h>
#endif // defined(AZ_PLATFORM_WINDOWS)

enum
{
    // in milliseconds
    GameModeIdleFrequency = 0,
    EditorModeIdleFrequency = 1,
    InactiveModeFrequency = 10,
    UninitializedFrequency = 9999,
};

#ifdef DEPRECATED_QML_SUPPORT

// QML imports that go in the editor folder (relative to the project root)
#define QML_IMPORT_USER_LIB_PATH "Editor/UI/qml"

// QML Imports that are part of Qt (relative to the executable)
#define QML_IMPORT_SYSTEM_LIB_PATH "qtlibs/qml"

#endif // #ifdef DEPRECATED_QML_SUPPORT

Q_LOGGING_CATEGORY(InputDebugging, "lumberyard.editor.input")

// internal, private namespace:
namespace
{
    class RecursionGuard
    {
    public:
        RecursionGuard(bool& value)
            : m_refValue(value)
        {
            m_reset = !value;
            m_refValue = true;
        }

        ~RecursionGuard()
        {
            if (m_reset)
            {
                m_refValue = false;
            }
        }

        bool areWeRecursing()
        {
            return !m_reset;
        }

    private:
        bool& m_refValue;
        bool m_reset;
    };

    class GlobalEventFilter
        : public QObject
    {
    public:
        explicit GlobalEventFilter(QObject* watch)
            : QObject(watch) {}

        bool eventFilter(QObject* obj, QEvent* e) override
        {
            static bool recursionChecker = false;
            RecursionGuard guard(recursionChecker);

            if (guard.areWeRecursing())
            {
                return false;
            }

            switch (e->type())
            {
                case QEvent::Wheel:
                {
                    auto wheelEvent = static_cast<QWheelEvent*>(e);

                    // make the wheel event fall through to windows underneath the mouse, even if they don't have focus
                    QWidget* widget = QApplication::widgetAt(wheelEvent->globalPos());
                    if (widget && obj != widget)
                    {
                        QPoint mappedPos = widget->mapFromGlobal(wheelEvent->globalPos());

                        QWheelEvent wheelEventCopy(mappedPos, wheelEvent->globalPos(), wheelEvent->pixelDelta(),
                            wheelEvent->angleDelta(), wheelEvent->delta(), wheelEvent->orientation(), wheelEvent->buttons(),
                            wheelEvent->modifiers(), wheelEvent->phase(), wheelEvent->source());

                        return QApplication::instance()->sendEvent(widget, &wheelEventCopy);
                    }
                }
                break;

                case QEvent::KeyPress:
                case QEvent::KeyRelease:
                {
                    if (GetIEditor()->IsInGameMode())
                    {
                        // don't let certain keys fall through to the game when it's running
                        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(e);
                        auto key = keyEvent->key();

                        if ((key == Qt::Key_Alt) || (key == Qt::Key_AltGr) || ((key >= Qt::Key_F1) && (key <= Qt::Key_F35)))
                        {
                            return true;
                        }
                    }
                }
                break;

                case QEvent::Shortcut:
                {
                    // eat shortcuts in game mode
                    if (GetIEditor()->IsInGameMode())
                    {
                        return true;
                    }
                }
                break;

                case QEvent::MouseButtonPress:
                case QEvent::MouseButtonRelease:
                case QEvent::MouseButtonDblClick:
                case QEvent::MouseMove:
                {
#if AZ_TRAIT_OS_PLATFORM_APPLE
                    auto widget = qobject_cast<QWidget*>(obj);
                    if (widget && widget->graphicsProxyWidget() != nullptr)
                    {
                        QMouseEvent* me = static_cast<QMouseEvent*>(e);
                        QWidget* target = qApp->widgetAt(QCursor::pos());
                        if (target)
                        {
                            QMouseEvent ev(me->type(), target->mapFromGlobal(QCursor::pos()), me->button(), me->buttons(), me->modifiers());
                            qApp->notify(target, &ev);
                            return true;
                        }
                    }
#endif
                    GuardMouseEventSelectionChangeMetrics(e);
                }
                break;
            }

            return false;
        }

    private:
        bool m_mouseButtonWasDown = false;

        void GuardMouseEventSelectionChangeMetrics(QEvent* e)
        {
            // Force the metrics collector to queue up any selection changed metrics until mouse release, so that we don't
            // get flooded with multiple selection changed events when one, sent on mouse release, is enough.
            if (e->type() == QEvent::MouseButtonPress)
            {
                if (!m_mouseButtonWasDown)
                {
                    AzToolsFramework::EditorMetricsEventsBus::Broadcast(&AzToolsFramework::EditorMetricsEventsBus::Events::BeginSelectionChange);
                    m_mouseButtonWasDown = true;
                }
            }
            else if (e->type() == QEvent::MouseButtonRelease)
            {
                // This is a tricky case. We don't want to send the end selection change event too early
                // because there might be other things responding to the mouse release after this, and we want to
                // block handling of the selection change events until we're entirely finished with the mouse press.
                // So, queue the handling with a single shot timer, but then check the state of the mouse buttons
                // to ensure that they haven't been pressed in between the release and the timer firing off.
                QTimer::singleShot(0, this, [this]() {
                    if (!QApplication::mouseButtons() && m_mouseButtonWasDown)
                    {
                        AzToolsFramework::EditorMetricsEventsBus::Broadcast(&AzToolsFramework::EditorMetricsEventsBus::Events::EndSelectionChange);
                        m_mouseButtonWasDown = false;
                    }
                });
            }
        }
    };

    static void LogToDebug(QtMsgType Type, const QMessageLogContext& Context, const QString& message)
    {
#if defined(WIN32) || defined(WIN64)
        OutputDebugStringW(L"Qt: ");
        OutputDebugStringW(reinterpret_cast<const wchar_t*>(message.utf16()));
        OutputDebugStringW(L"\n");
#endif
    }
}

namespace Editor
{
    void ScanDirectories(QFileInfoList& directoryList, const QStringList& filters, QFileInfoList& files, ScanDirectoriesUpdateCallBack updateCallback)
    {
        while (!directoryList.isEmpty())
        {
            QDir directory(directoryList.front().absoluteFilePath(), "*", QDir::Name | QDir::IgnoreCase, QDir::AllEntries);
            directoryList.pop_front();

            if (directory.exists())
            {
                // Append each file from this directory that matches one of the filters to files
                directory.setNameFilters(filters);
                directory.setFilter(QDir::Files);
                files.append(directory.entryInfoList());

                // Add all of the subdirectories from this directory to the queue to be searched
                directory.setNameFilters(QStringList("*"));
                directory.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
                directoryList.append(directory.entryInfoList());
                if (updateCallback)
                {
                    updateCallback();
                }
            }
        }
    }

    EditorQtApplication::EditorQtApplication(int& argc, char** argv)
        : QApplication(argc, argv)
        , m_inWinEventFilter(false)
        , m_stylesheet(new AzQtComponents::LumberyardStylesheet(this))
        , m_idleTimer(new QTimer(this))
    {
        m_idleTimer->setInterval(UninitializedFrequency);

        setWindowIcon(QIcon(":/Application/res/editor_icon.ico"));

        // set the default key store for our preferences:
        setOrganizationName("Amazon");
        setOrganizationDomain("amazon.com");
        setApplicationName("Lumberyard");

        connect(m_idleTimer, &QTimer::timeout, this, &EditorQtApplication::maybeProcessIdle);

        connect(this, &QGuiApplication::applicationStateChanged, this, [this] { ResetIdleTimerInterval(PollState); });
        installEventFilter(this);

        // Disable our debugging input helpers by default
        QLoggingCategory::setFilterRules(QStringLiteral("lumberyard.editor.input.*=false"));
    }

    void EditorQtApplication::Initialize()
    {
        GetIEditor()->RegisterNotifyListener(this);

        m_stylesheet->Initialize(this, !gSettings.bEnableUI2);

        // install QTranslator
        InstallEditorTranslators();

        // install hooks and filters last and revoke them first
        InstallFilters();

#ifdef DEPRECATED_QML_SUPPORT
        InitializeQML();
#endif // #ifdef DEPRECATED_QML_SUPPORT

        // install this filter. It will be a parent of the application and cleaned up when it is cleaned up automically
        auto globalEventFilter = new GlobalEventFilter(this);
        installEventFilter(globalEventFilter);

        //Setup reusable dialogs
        UIFactory::Initialize();
    }

    void EditorQtApplication::LoadSettings() 
    {
        AZ::SerializeContext* context;
        EBUS_EVENT_RESULT(context, AZ::ComponentApplicationBus, GetSerializeContext);
        AZ_Assert(context, "No serialize context");
        char resolvedPath[AZ_MAX_PATH_LEN];
        AZ::IO::FileIOBase::GetInstance()->ResolvePath("@user@/EditorUserSettings.xml", resolvedPath, AZ_MAX_PATH_LEN);
        m_localUserSettings.Load(resolvedPath, context);
        m_localUserSettings.Activate(AZ::UserSettings::CT_LOCAL);
        AZ::UserSettingsOwnerRequestBus::Handler::BusConnect(AZ::UserSettings::CT_LOCAL);
        m_activatedLocalUserSettings = true;
    }

    void EditorQtApplication::UnloadSettings()
    {
        if (m_activatedLocalUserSettings)
        {
            SaveSettings();
            m_localUserSettings.Deactivate();
            AZ::UserSettingsOwnerRequestBus::Handler::BusDisconnect();
            m_activatedLocalUserSettings = false;
        }
    }

    void EditorQtApplication::SaveSettings()
    {
        if (m_activatedLocalUserSettings)
        {
            AZ::SerializeContext* context;
            EBUS_EVENT_RESULT(context, AZ::ComponentApplicationBus, GetSerializeContext);
            AZ_Assert(context, "No serialize context");

            char resolvedPath[AZ_MAX_PATH_LEN];
            AZ::IO::FileIOBase::GetInstance()->ResolvePath("@user@/EditorUserSettings.xml", resolvedPath, AZ_ARRAY_SIZE(resolvedPath));
            m_localUserSettings.Save(resolvedPath, context);
        }
    }

    void EditorQtApplication::maybeProcessIdle()
    {
        if (!m_isMovingOrResizing)
        {
            if (auto winapp = CCryEditApp::instance())
            {
                winapp->OnIdle(0);
            }
        }
    }

    void EditorQtApplication::InstallQtLogHandler()
    {
        qInstallMessageHandler(LogToDebug);
    }

    void EditorQtApplication::InstallFilters()
    {
        if (auto dispatcher = QAbstractEventDispatcher::instance())
        {
            dispatcher->installNativeEventFilter(this);
        }
    }

    void EditorQtApplication::UninstallFilters()
    {
        if (auto dispatcher = QAbstractEventDispatcher::instance())
        {
            dispatcher->removeNativeEventFilter(this);
        }
    }

    EditorQtApplication::~EditorQtApplication()
    {
        if (GetIEditor())
        {
            GetIEditor()->UnregisterNotifyListener(this);
        }

        //Clean reusable dialogs
        UIFactory::Deinitialize();

#ifdef DEPRECATED_QML_SUPPORT
        UninitializeQML();
#endif // #ifdef DEPRECATED_QML_SUPPORT

        UninstallFilters();

        UninstallEditorTranslators();
    }

#ifdef AZ_DEBUG_BUILD
    static QString objectParentsToString(QObject* obj)
    {
        QString result;
        if (obj)
        {
            QDebug stream(&result);
            QObject* p = obj->parent();
            while (p)
            {
                stream << p << ":";
                p = p->parent();
            }
        }
        return result;
    }

    bool EditorQtApplication::notify(QObject* receiver, QEvent* ev)
    {
        /*
        QEvent::Type evType = ev->type();
        if (evType == QEvent::MouseButtonPress ||
                evType == QEvent::KeyPress ||
                evType == QEvent::Shortcut ||
                evType == QEvent::ShortcutOverride ||
                evType == QEvent::KeyPress ||
                evType == QEvent::KeyRelease)
        {
            qCDebug(InputDebugging) << "Attempting to deliver" << evType << "to" << receiver << "; receiver's parents=(" << objectParentsToString(receiver) << "); pre-accepted=" << ev->isAccepted();
            bool processed = QApplication::notify(receiver, ev);
            qCDebug(InputDebugging) << "    processed=" << processed << "; accepted=" << ev->isAccepted()
                                    << "focusWidget=" << focusWidget();

            if (QWidget::mouseGrabber() || QWidget::keyboardGrabber())
            {
                qCDebug(InputDebugging) << "Mouse Grabber=" << QWidget::mouseGrabber()
                                        << "; Key Grabber=" << QWidget::keyboardGrabber();
            }

            if (QWidget* popup = QApplication::activePopupWidget())
            {
                qCDebug(InputDebugging) << "popup=" << popup;
            }

            if (QWidget* modal = QApplication::activeModalWidget())
            {
                qCDebug(InputDebugging) << "modal=" << modal;
            }

            return processed;
        }
        */

        return QApplication::notify(receiver, ev);
    }
#endif // #ifdef AZ_DEBUG_BUILD

    static QWindow* windowForWidget(const QWidget* widget)
    {
        QWindow* window = widget->windowHandle();
        if (window)
        {
            return window;
        }
        const QWidget* nativeParent = widget->nativeParentWidget();
        if (nativeParent)
        {
            return nativeParent->windowHandle();
        }

        return nullptr;
    }

#if defined(AZ_PLATFORM_WINDOWS)
    bool EditorQtApplication::nativeEventFilter(const QByteArray& eventType, void* message, long* result)
    {
        MSG* msg = (MSG*)message;

        if (msg->message == WM_MOVING || msg->message == WM_SIZING)
        {
            m_isMovingOrResizing = true;
        }
        else if (msg->message == WM_EXITSIZEMOVE)
        {
            m_isMovingOrResizing = false;
        }

        // Prevent the user from being able to move the window in game mode.
        // This is done during the hit test phase to bypass the native window move messages. If the window
        // decoration wrapper title bar contains the cursor, set the result to HTCLIENT instead of
        // HTCAPTION.
        if (msg->message == WM_NCHITTEST && GetIEditor()->IsInGameMode())
        {
            const LRESULT defWinProcResult = DefWindowProc(msg->hwnd, msg->message, msg->wParam, msg->lParam);
            if (defWinProcResult == 1)
            {
                if (QWidget* widget = QWidget::find((WId)msg->hwnd))
                {
                    if (auto wrapper = qobject_cast<const AzQtComponents::WindowDecorationWrapper *>(widget))
                    {
                        AzQtComponents::TitleBar* titleBar = wrapper->titleBar();
                        const short global_x = static_cast<short>(LOWORD(msg->lParam));
                        const short global_y = static_cast<short>(HIWORD(msg->lParam));

                        const QPoint globalPos = QHighDpi::fromNativePixels(QPoint(global_x, global_y), widget->window()->windowHandle());
                        const QPoint local = titleBar->mapFromGlobal(globalPos);
                        if (titleBar->draggableRect().contains(local) && !titleBar->isTopResizeArea(globalPos))
                        {
                            *result = HTCLIENT;
                            return true;
                        }
                    }
                }
            }
        }

        // Ensure that the Windows WM_INPUT messages get passed through to the AzFramework input system,
        // but only while in game mode so we don't accumulate raw input events before we start actually
        // ticking the input devices, otherwise the queued events will get sent when entering game mode.
        if (msg->message == WM_INPUT && GetIEditor()->IsInGameMode())
        {
            UINT rawInputSize;
            const UINT rawInputHeaderSize = sizeof(RAWINPUTHEADER);
            GetRawInputData((HRAWINPUT)msg->lParam, RID_INPUT, NULL, &rawInputSize, rawInputHeaderSize);

            AZStd::array<BYTE, sizeof(RAWINPUT)> rawInputBytesArray;
            LPBYTE rawInputBytes = rawInputBytesArray.data();

            const UINT bytesCopied = GetRawInputData((HRAWINPUT)msg->lParam, RID_INPUT, rawInputBytes, &rawInputSize, rawInputHeaderSize);
            CRY_ASSERT(bytesCopied == rawInputSize);

            RAWINPUT* rawInput = (RAWINPUT*)rawInputBytes;
            CRY_ASSERT(rawInput);

            AzFramework::RawInputNotificationBusWindows::Broadcast(&AzFramework::RawInputNotificationsWindows::OnRawInputEvent, *rawInput);

            return false;
        }
        else if (msg->message == WM_DEVICECHANGE)
        {
            if (msg->wParam == 0x0007) // DBT_DEVNODES_CHANGED
            {
                AzFramework::RawInputNotificationBusWindows::Broadcast(&AzFramework::RawInputNotificationsWindows::OnRawInputDeviceChangeEvent);
            }
            return true;
        }

        return false;
    }
#endif

    void EditorQtApplication::OnEditorNotifyEvent(EEditorNotifyEvent event)
    {
        switch (event)
        {
            case eNotify_OnStyleChanged:
                RefreshStyleSheet();
                emit skinChanged();
            break;

            case eNotify_OnQuit:
                GetIEditor()->UnregisterNotifyListener(this);
            break;

            case eNotify_OnBeginGameMode:
                // GetIEditor()->IsInGameMode() Isn't reliable when called from within the notification handler
                ResetIdleTimerInterval(GameMode);
            break;
            case eNotify_OnEndGameMode:
                ResetIdleTimerInterval(EditorMode);
            break;
        }
    }

    QColor EditorQtApplication::InterpolateColors(QColor a, QColor b, float factor)
    {
        return QColor(int(a.red() * (1.0f - factor) + b.red() * factor),
            int(a.green() * (1.0f - factor) + b.green() * factor),
            int(a.blue() * (1.0f - factor) + b.blue() * factor),
            int(a.alpha() * (1.0f - factor) + b.alpha() * factor));
    }

    static void WriteStylesheetForQtDesigner(const QString& processedStyle)
    {
        QString outputStylePath = QDir::cleanPath(QDir::homePath() + QDir::separator() + "lumberyard_editor_stylesheet.qss");
        QFile outputStyleFile(outputStylePath);
        bool successfullyWroteStyleFile = false;
        if (outputStyleFile.open(QFile::WriteOnly))
        {
            QTextStream outStream(&outputStyleFile);
            outStream << processedStyle;
            outputStyleFile.close();
            successfullyWroteStyleFile = true;

            if (GetIEditor() != nullptr)
            {
                if (GetIEditor()->GetSystem() != nullptr)
                {
                    if (GetIEditor()->GetSystem()->GetILog() != nullptr)
                    {
                        GetIEditor()->GetSystem()->GetILog()->LogWithType(IMiniLog::eMessage, "Wrote LumberYard's compiled Qt Style to '%s'", outputStylePath.toUtf8().data());
                    }
                }
            }
        }
    }

    void EditorQtApplication::RefreshStyleSheet()
    {
        m_stylesheet->Refresh(this);
    }

#ifdef DEPRECATED_QML_SUPPORT
    void EditorQtApplication::InitializeQML()
    {
        if (!m_qmlEngine)
        {
            m_qmlEngine = new QQmlEngine();

            // assumption:  Qt is already initialized.  You can use Qt's stuff to do anything you want now.
            QDir appDir(QCoreApplication::applicationDirPath());
            m_qmlEngine->addImportPath(appDir.filePath(QML_IMPORT_SYSTEM_LIB_PATH));
            // now find engine root and use that:

            QString rootDir = AzQtComponents::FindEngineRootDir(this);
            if (!rootDir.isEmpty())
            {
                m_qmlEngine->addImportPath(QDir(rootDir).filePath(QML_IMPORT_USER_LIB_PATH));
            }

            // now that QML is initialized, broadcast to any interested parties:
            GetIEditor()->Notify(eNotify_QMLReady);
        }
    }


    void EditorQtApplication::UninitializeQML()
    {
        if (m_qmlEngine)
        {
            GetIEditor()->Notify(eNotify_BeforeQMLDestroyed);
            delete m_qmlEngine;
            m_qmlEngine = nullptr;
        }
    }
#endif // #ifdef DEPRECATED_QML_SUPPORT

    void EditorQtApplication::setIsMovingOrResizing(bool isMovingOrResizing)
    {
        if (m_isMovingOrResizing == isMovingOrResizing)
        {
            return;
        }

        m_isMovingOrResizing = isMovingOrResizing;
    }

    bool EditorQtApplication::isMovingOrResizing() const
    {
        return m_isMovingOrResizing;
    }

    void EditorQtApplication::EnableUI2(bool enable)
    {
        m_stylesheet->SwitchUI(this, !enable);
    }

    const QColor& EditorQtApplication::GetColorByName(const QString& name)
    {
        return m_stylesheet->GetColorByName(name);
    }

#ifdef DEPRECATED_QML_SUPPORT
    QQmlEngine* EditorQtApplication::GetQMLEngine() const
    {
        return m_qmlEngine;
    }
#endif // #ifdef DEPRECATED_QML_SUPPORT

    EditorQtApplication* EditorQtApplication::instance()
    {
        return static_cast<EditorQtApplication*>(QApplication::instance());
    }

    bool EditorQtApplication::IsActive()
    {
        return applicationState() == Qt::ApplicationActive;
    }

    QTranslator* EditorQtApplication::CreateAndInitializeTranslator(const QString& filename, const QString& directory)
    {
        Q_ASSERT(QFile::exists(directory + "/" + filename));

        QTranslator* translator = new QTranslator();
        translator->load(filename, directory);
        installTranslator(translator);
        return translator;
    }

    void EditorQtApplication::InstallEditorTranslators()
    {
        m_editorTranslator =        CreateAndInitializeTranslator("editor_en-us.qm", ":/Translations");
        m_flowgraphTranslator =     CreateAndInitializeTranslator("flowgraph_en-us.qm", ":/Translations");
        m_assetBrowserTranslator =  CreateAndInitializeTranslator("assetbrowser_en-us.qm", ":/Translations");
    }

    void EditorQtApplication::DeleteTranslator(QTranslator*& translator)
    {
        removeTranslator(translator);
        delete translator;
        translator = nullptr;
    }

    void EditorQtApplication::UninstallEditorTranslators()
    {
        DeleteTranslator(m_editorTranslator);
        DeleteTranslator(m_flowgraphTranslator);
        DeleteTranslator(m_assetBrowserTranslator);
    }

    void EditorQtApplication::EnableOnIdle(bool enable)
    {
        if (enable)
        {
            if (m_idleTimer->interval() == UninitializedFrequency)
            {
                ResetIdleTimerInterval();
            }

            m_idleTimer->start();
        }
        else
        {
            m_idleTimer->stop();
        }
    }

    bool EditorQtApplication::OnIdleEnabled() const
    {
        if (m_idleTimer->interval() == UninitializedFrequency)
        {
            return false;
        }

        return m_idleTimer->isActive();
    }

    void EditorQtApplication::ResetIdleTimerInterval(TimerResetFlag flag)
    {
        bool isInGameMode = flag == GameMode;
        if (flag == PollState)
        {
            isInGameMode = GetIEditor() ? GetIEditor()->IsInGameMode() : false;
        }

        // Game mode takes precedence over anything else
        if (isInGameMode)
        {
            m_idleTimer->setInterval(GameModeIdleFrequency);
        }
        else
        {
            if (applicationState() & Qt::ApplicationActive)
            {
                m_idleTimer->setInterval(EditorModeIdleFrequency);
            }
            else
            {
                m_idleTimer->setInterval(InactiveModeFrequency);
            }
        }
    }

    bool EditorQtApplication::eventFilter(QObject* object, QEvent* event)
    {
        switch (event->type())
        {
        case QEvent::MouseButtonPress:
            m_pressedButtons |= reinterpret_cast<QMouseEvent*>(event)->button();
            break;
        case QEvent::MouseButtonRelease:
            m_pressedButtons &= ~(reinterpret_cast<QMouseEvent*>(event)->button());
            break;
        case QEvent::KeyPress:
            m_pressedKeys.insert(reinterpret_cast<QKeyEvent*>(event)->key());
            break;
        case QEvent::KeyRelease:
            m_pressedKeys.remove(reinterpret_cast<QKeyEvent*>(event)->key());
            break;
#ifdef AZ_PLATFORM_WINDOWS
        case QEvent::Leave:
        {
            // if we receive a leave event for a toolbar on Windows
            // check first whether we really left it. If we didn't: start checking
            // for the tool bar under the mouse by timer to check when we really left.
            // Synthesize a new leave event then. Workaround for LY-69788
            auto toolBarAt = [](const QPoint& pos) -> QToolBar* {
                QWidget* widget = qApp->widgetAt(pos);
                while (widget != nullptr)
                {
                    if (QToolBar* tb = qobject_cast<QToolBar*>(widget))
                    {
                        return tb;
                    }
                    widget = widget->parentWidget();
                }
                return nullptr;
            };
            if (object == toolBarAt(QCursor::pos()))
            {
                QTimer* t = new QTimer(object);
                t->start(100);
                connect(t, &QTimer::timeout, object, [t, object, toolBarAt]() {
                    if (object != toolBarAt(QCursor::pos()))
                    {
                        QEvent event(QEvent::Leave);
                        qApp->sendEvent(object, &event);
                        t->deleteLater();
                    }
                });
                return true;
            }
            break;
        }
#endif
        default:
            break;
        }
        return QApplication::eventFilter(object, event);
    }
} // end namespace Editor

#include <Core/QtEditorApplication.moc>
