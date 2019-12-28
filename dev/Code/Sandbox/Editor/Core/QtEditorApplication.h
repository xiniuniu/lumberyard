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
#pragma once

#include <QApplication>
#include <QAbstractNativeEventFilter>
#include <QColor>
#include <QMap>
#include <QTranslator>
#include <QSet>
#include "IEventLoopHook.h"
#include <unordered_map>

#include <AzCore/PlatformDef.h>
#include <AzCore/UserSettings/UserSettingsProvider.h>

class QFileInfo;
typedef QList<QFileInfo> QFileInfoList;
class QByteArray;

#ifdef DEPRECATED_QML_SUPPORT
class QQmlEngine;
#endif


namespace AzQtComponents
{
    class LumberyardStylesheet;
}

enum EEditorNotifyEvent;

// This contains the main "QApplication"-derived class for the editor itself.
// it performs the message hooking and other functions to allow CryEdit to function.

namespace Editor
{
    typedef void(* ScanDirectoriesUpdateCallBack)(void);
    /*!
      \param directoryList A list of directories to search. ScanDirectories will also search the subdirectories of each of these.
      \param filters A list of filename filters. Supports * and ? wildcards. The filters will not apply to the directory names.
      \param files Any file that is found and matches any of the filters will be added to files.
      \param updateCallback An optional callback function that will be called once for every directory and subdirectory that is scanned.
    */
    void ScanDirectories(QFileInfoList& directoryList, const QStringList& filters, QFileInfoList& files, ScanDirectoriesUpdateCallBack updateCallback = nullptr);

    class EditorQtApplication
        : public QApplication
        , public QAbstractNativeEventFilter
        , public IEditorNotifyListener
        , public AZ::UserSettingsOwnerRequestBus::Handler
    {
        Q_OBJECT
        Q_PROPERTY(QSet<int> pressedKeys READ pressedKeys)
        Q_PROPERTY(int pressedMouseButtons READ pressedMouseButtons)
    public:
        // Call this before creating this object:
        static void InstallQtLogHandler();

        EditorQtApplication(int& argc, char** argv);
        virtual ~EditorQtApplication();
        void Initialize();

        void LoadSettings();
        void UnloadSettings();

        // AZ::UserSettingsOwnerRequestBus::Handler
        void SaveSettings() override;
        ////

        static EditorQtApplication* instance();

        static bool IsActive();

        bool isMovingOrResizing() const;

        // QAbstractNativeEventFilter:
        bool nativeEventFilter(const QByteArray& eventType, void* message, long* result) override;

        // IEditorNotifyListener:
        void OnEditorNotifyEvent(EEditorNotifyEvent event) override;

        // get the QML engine, if available.
#ifdef DEPRECATED_QML_SUPPORT
        QQmlEngine* GetQMLEngine() const;
#endif

        const QColor& GetColorByName(const QString& colorName);

        void EnableOnIdle(bool enable = true);
        bool OnIdleEnabled() const;

        bool eventFilter(QObject* object, QEvent* event) override;

        QSet<int> pressedKeys() const { return m_pressedKeys; }
        int pressedMouseButtons() const { return m_pressedButtons; }
#ifdef AZ_DEBUG_BUILD
        bool notify(QObject* receiver, QEvent* ev) override;
#endif

    public Q_SLOTS:

#ifdef DEPRECATED_QML_SUPPORT
        void InitializeQML();
        void UninitializeQML();
#endif

        void setIsMovingOrResizing(bool isMovingOrResizing);

        void EnableUI2(bool enable);

    signals:
        void skinChanged();

    private:
        enum TimerResetFlag
        {
            PollState,
            GameMode,
            EditorMode
        };
        void ResetIdleTimerInterval(TimerResetFlag = PollState);
        static QColor InterpolateColors(QColor a, QColor b, float factor);
        void RefreshStyleSheet();
        void InstallFilters();
        void UninstallFilters();
        void maybeProcessIdle();

        AzQtComponents::LumberyardStylesheet* m_stylesheet;

        bool m_inWinEventFilter = false;

#ifdef DEPRECATED_QML_SUPPORT
        QQmlEngine* m_qmlEngine = nullptr;
#endif

        // Translators
        void InstallEditorTranslators();
        void UninstallEditorTranslators();
        QTranslator* CreateAndInitializeTranslator(const QString& filename, const QString& directory);
        void DeleteTranslator(QTranslator*& translator);

        QTranslator* m_editorTranslator = nullptr;
        QTranslator* m_flowgraphTranslator = nullptr;
        QTranslator* m_assetBrowserTranslator = nullptr;
        QTimer* const m_idleTimer = nullptr;
        bool m_isMovingOrResizing = false;

        AZ::UserSettingsProvider m_localUserSettings;

        Qt::MouseButtons m_pressedButtons = Qt::NoButton;
        QSet<int> m_pressedKeys;

        bool m_activatedLocalUserSettings = false;
    };
} // namespace editor
