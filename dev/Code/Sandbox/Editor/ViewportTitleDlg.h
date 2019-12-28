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

#ifndef CRYINCLUDE_EDITOR_VIEWPORTTITLEDLG_H
#define CRYINCLUDE_EDITOR_VIEWPORTTITLEDLG_H
#pragma once


#include "RenderViewport.h"

#include <functional>

// CViewportTitleDlg dialog
class CLayoutViewPane;
class CPopupMenuItem;

class QAbstractButton;
class QActionGroup;
class QMenu;

namespace Ui
{
    class ViewportTitleDlg;
}

//////////////////////////////////////////////////////////////////////////
class CViewportTitleDlg
    : public QWidget
    , public IEditorNotifyListener
    , public ISystemEventListener
{
    Q_OBJECT
public:
    CViewportTitleDlg(QWidget* pParent = nullptr);   // standard constructor
    virtual ~CViewportTitleDlg();

    void SetViewPane(CLayoutViewPane* pViewPane);
    void SetTitle(const QString& title);
    void OnViewportSizeChanged(int width, int height);
    void OnViewportFOVChanged(float fov);
    void SetFocusToSearchField();

    // Dialog Data
    enum ESearchResultHandling
    {
        ESRH_HIDE_OTHERS = 0,
        ESRH_FREEZE_OTHERS,
        ESRH_JUST_SELECT,
    };

    enum ESearchMode
    {
        ESM_BY_NAME = 0,
        ESM_BY_TYPE,
        ESM_BY_ASSET,
    };

    static void AddFOVMenus(QMenu* menu, std::function<void(float)> callback, const QStringList& customPresets);
    static void AddAspectRatioMenus(QMenu* menu, std::function<void(int, int)> callback, const QStringList& customPresets);
    static void AddResolutionMenus(QMenu* menu, std::function<void(int, int)> callback, const QStringList& customPresets);

    static void LoadCustomPresets(const QString& section, const QString& keyName, QStringList& outCustompresets);
    static void SaveCustomPresets(const QString& section, const QString& keyName, const QStringList& custompresets);
    static void UpdateCustomPresets(const QString& text, QStringList& custompresets);
    static void OnChangedDisplayInfo(ICVar*    pDisplayInfo, QAbstractButton* pDisplayInfoButton);

    bool eventFilter(QObject* object, QEvent* event) override;

protected:
    virtual void OnInitDialog();
    QMenu *InitializeViewportSearchMenu();

    virtual void OnEditorNotifyEvent(EEditorNotifyEvent event);
    void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;

    void OnMaximize();
    void OnToggleHelpers();
    void OnToggleDisplayInfo();
    void OnSearchTermChange();
    void OnViewportSearchButtonClicked(const QAction* clickedAction);
    void OnViewportSearchClear();


    QString m_title;

    CLayoutViewPane* m_pViewPane;

    ESearchMode m_searchMode;
    ESearchResultHandling m_searchResultHandling;
    bool m_bOR;

    static const int MAX_NUM_CUSTOM_PRESETS = 10;
    QStringList m_customResPresets;
    QStringList m_customFOVPresets;
    QStringList m_customAspectRatioPresets;


    uint64 m_displayInfoCallbackIndex;

    void UnhideUnfreezeAll();
    void SearchByType(const QStringList& terms);
    void SearchByName(const QStringList& terms);
    void SearchByAsset(const QStringList& terms);
    void UpdateSearchOptionsText();
    void InputNamesToSearchFromSelection();

    void OnMenuFOVCustom();

    void PopUpFOVMenu();

    void OnMenuAspectRatioCustom();
    void PopUpAspectMenu();

    void OnMenuResolutionCustom();
    void PopUpResolutionMenu();

    QScopedPointer<Ui::ViewportTitleDlg> m_ui;

    QActionGroup* m_searchModeActionGroup;
    QAction* m_searchByNameAction;
    QAction* m_searchByTypeAction;
    QAction* m_searchByAssetAction;
    QActionGroup* m_searchResultHandlingActionGroup;
    QAction* m_searchHideOthersAction;
    QAction* m_searchFreezeOthersAction;
    QAction* m_searchJustSelectAction;
    QActionGroup* m_searchMatchTypeActionGroup;
    QAction* m_searchAndAction;
    QAction* m_searchOrAction;
};

#endif // CRYINCLUDE_EDITOR_VIEWPORTTITLEDLG_H
