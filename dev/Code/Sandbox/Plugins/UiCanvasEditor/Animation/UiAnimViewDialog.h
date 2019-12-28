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

#pragma once


#include <LyShine/Animation/IUiAnimation.h>
#include "UiEditorAnimationBus.h"
#include "UiAnimViewNodes.h"
#include "UiAnimViewSequence.h"
#include "UiAnimViewSequenceManager.h"
#include "AnimationContext.h"
#include "Undo/IUndoManagerListener.h"

#include <QMainWindow>

class QSplitter;
class QComboBox;
class QLabel;
class CustomizeKeyboardPage;
class CUiAnimViewDopeSheetBase;
class UiAnimViewCurveEditorDialog;
class CUiAnimViewKeyPropertiesDlg;

#define LIGHT_ANIMATION_SET_NAME "_LightAnimationSet"

class CUiAnimationCallback;
class CUiAnimViewFindDlg;

class CUiAnimViewDialog
    : public QMainWindow
    , public IUiAnimationContextListener
    , public IEditorNotifyListener
    , public IUiAnimViewSequenceListener
    , public IUiAnimViewSequenceManagerListener
    , IUndoManagerListener
    , public UiEditorAnimationStateBus::Handler
    , public UiEditorAnimListenerBus::Handler
{
    Q_OBJECT
public:
    friend CUiAnimationCallback;

    CUiAnimViewDialog(QWidget* pParent = NULL);
    ~CUiAnimViewDialog();

    static CUiAnimViewDialog* GetCurrentInstance() { return s_pUiAnimViewDialog; }

    void InvalidateDopeSheet();
    void Update();

    void ReloadSequences();
    void InvalidateSequence();

    void UpdateSequenceLockStatus();

    // IUiAnimationContextListener
    virtual void OnSequenceChanged(CUiAnimViewSequence* pNewSequence) override;

    // IUiAnimViewSequenceListener
    virtual void OnSequenceSettingsChanged(CUiAnimViewSequence* pSequence) override;

    // UiEditorAnimationStateInterface
    UiEditorAnimationStateInterface::UiEditorAnimationEditState GetCurrentEditState() override;
    void RestoreCurrentEditState(const UiEditorAnimationStateInterface::UiEditorAnimationEditState& animEditState);
    // ~UiEditorAnimationStateInterface

    // UiEditorAnimListenerInterface
    void OnActiveCanvasChanged() override;
    void OnUiElementsDeletedOrReAdded() override;
    // ~UiEditorAnimListenerInterface

    void UpdateDopeSheetTime(CUiAnimViewSequence* pSequence);

    const CUiAnimViewDopeSheetBase& GetUiAnimViewDopeSheet() const { return *m_wndDopeSheet; }

public slots:
    void OnPlay();

protected slots:
    void OnGoToPrevKey();
    void OnGoToNextKey();
    void OnAddKey();
    void OnDelKey();
    void OnMoveKey();
    void OnSlideKey();
    void OnScaleKey();
    void OnSyncSelectedTracksToBase();
    void OnSyncSelectedTracksFromBase();
    void OnAddSequence();
    void OnDelSequence();
    void OnEditSequence();
    void OnSequenceComboBox();
    void OnAddSelectedNode();
    void OnAddDirectorNode();
    void OnFindNode();

    void OnRecord();
    void OnGoToStart();
    void OnGoToEnd();
    void OnPlaySetScale();
    void OnStop();
    void OnStopHardReset();
    void OnPause();
    void OnLoop();

    void OnSnapNone();
    void OnSnapMagnet();
    void OnSnapFrame();
    void OnSnapTick();
    void OnSnapFPS();

    void OnCustomizeTrackColors();

    void OnBatchRender();

    void OnModeDopeSheet();
    void OnModeCurveEditor();
    void OnOpenCurveEditor();

    void OnViewTickInSeconds();
    void OnViewTickInFrames();

    void OnToggleDisable();
    void OnToggleMute();
    void OnMuteAll();
    void OnUnmuteAll();

protected:
    void keyPressEvent(QKeyEvent* event) override;
#if defined(AZ_PLATFORM_WINDOWS)
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
#endif

private slots:
    void ReadLayouts();

private:
    void UpdateActions();
    void ReloadSequencesComboBox();

    void SetEditLock(bool bLock);

    void InitMenu();
    void InitToolbar();
    void InitSequences();
    void OnAddEntityNodeMenu();

    void OnEditorNotifyEvent(EEditorNotifyEvent event) override;
    BOOL OnInitDialog();

    void SaveLayouts();
    void SaveMiscSettings() const;
    void ReadMiscSettings();
    void SaveTrackColors() const;
    void ReadTrackColors();

    void SetCursorPosText(float fTime);

    void SaveZoomScrollSettings();

    virtual void OnNodeSelectionChanged(CUiAnimViewSequence* pSequence) override;
    virtual void OnNodeRenamed(CUiAnimViewNode* pNode, const char* pOldName) override;

    virtual void OnSequenceAdded(CUiAnimViewSequence* pSequence);
    virtual void OnSequenceRemoved(CUiAnimViewSequence* pSequence);

    virtual void BeginUndoTransaction();
    virtual void EndUndoTransaction();
    void SaveSequenceTimingToXML();

#if defined(AZ_PLATFORM_WINDOWS)
    bool processRawInput(MSG* pMsg);
#endif

    // Instance
    static CUiAnimViewDialog* s_pUiAnimViewDialog;

    CUiAnimViewSequenceManager* m_sequenceManager;
    CUiAnimationContext* m_animationContext;
    IUiAnimationSystem* m_animationSystem;

    // GUI
    QSplitter* m_wndSplitter;
    CUiAnimViewNodesCtrl*   m_wndNodesCtrl;
    CUiAnimViewDopeSheetBase*   m_wndDopeSheet;
    QDockWidget* m_wndCurveEditorDock;
    UiAnimViewCurveEditorDialog*    m_wndCurveEditor;
    CUiAnimViewFindDlg* m_findDlg;
    QToolBar* m_mainToolBar;
    QToolBar* m_keysToolBar;
    QToolBar* m_playToolBar;
    QToolBar* m_viewToolBar;
    QComboBox* m_sequencesComboBox;

    QLabel* m_cursorPos;

    // UI Animation system
    CUiAnimationCallback* m_pUiAnimationCallback;

    // Current sequence
    QString m_currentSequenceName;

    // State
    bool m_bRecord;
    bool m_bPlay;
    bool m_bPause;
    bool m_bNeedReloadSequence;
    bool m_bIgnoreUpdates;
    bool m_bDoingUndoOperation;
    bool m_lazyInitDone;
    bool m_bEditLock;

    float m_fLastTime;

    int m_currentToolBarParamTypeId;
    std::vector<CUiAnimParamType> m_toolBarParamTypes;

    QHash<int, QAction*> m_actions;
};
