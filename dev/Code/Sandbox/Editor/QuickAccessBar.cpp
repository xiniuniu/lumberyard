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

// Description : A dialog bar for quickly accessing menu items and cvars


#include "StdAfx.h"
#include "QuickAccessBar.h"
#include "ui_QuickAccessBar.h"
#include "ToolBox.h"
#include "ViewManager.h"
#include "MainWindow.h"
#include "ActionManager.h"
#include "KeyboardCustomizationSettings.h"

#include <QCompleter>
#include <QMenuBar>
#include <QStringListModel>

CQuickAccessBar::CQuickAccessBar(QWidget* pParent)
    : QWidget(pParent, Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint)
    , m_lastViewPaneMapVersion(-1)
    , m_completer(new QCompleter(this))
    , m_model(new QStringListModel(this))
    , m_ui(new Ui::QuickAccessBar)
{
    m_ui->setupUi(this);
    m_ui->m_inputEdit->installEventFilter(this);
    m_ui->m_inputEdit->setCompleter(m_completer);
    m_completer->setModel(m_model);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setFilterMode(Qt::MatchContains);
    setFocusProxy(m_ui->m_inputEdit);
    OnInitDialog();

    connect(m_ui->m_inputEdit, &QLineEdit::returnPressed, this, &CQuickAccessBar::OnOK);
}

CQuickAccessBar::~CQuickAccessBar()
{

}

void CQuickAccessBar::OnInitDialog()
{
    // Make this window 50% alpha.
    setWindowOpacity(0.5);

    CollectMenuItems(MainWindow::instance()->menuBar());

    AddMRUFileItems();

    // Add console variables & commands.
    IConsole* console = GetIEditor()->GetSystem()->GetIConsole();
    std::vector<const char*> cmds;
    cmds.resize(console->GetNumVars());
    size_t cmdCount = console->GetSortedVars(&cmds[0], cmds.size());
    for (int i = 0; i < cmdCount; ++i)
    {
        m_model->setStringList(m_model->stringList() += cmds[i]);
    }
}

void CQuickAccessBar::OnOK()
{
    QString menuCmd = m_ui->m_inputEdit->text();
    if (!menuCmd.isEmpty())
    {
        auto actionItem = m_menuActionTable.find(menuCmd);
        if (actionItem != m_menuActionTable.end())
        {
            actionItem->second->trigger();
        }
        else
        {
            GetIEditor()->GetSystem()->GetIConsole()->ExecuteString(menuCmd.toUtf8().data());
        }

        m_ui->m_inputEdit->clear();
    }

    setVisible(false);
}

void CQuickAccessBar::OnCancel()
{
    setVisible(false);
}

bool CQuickAccessBar::eventFilter(QObject* object, QEvent* event)
{
    if (object == m_ui->m_inputEdit && event->type() == QEvent::FocusOut)
    {
        OnCancel();
    }
    return false;
}

void CQuickAccessBar::showEvent(QShowEvent* event)
{
    const int viewPaneVersion = MainWindow::instance()->ViewPaneVersion();

    if (viewPaneVersion != m_lastViewPaneMapVersion)
    {
        m_lastViewPaneMapVersion = viewPaneVersion;
        m_model->setStringList(QStringList());
        m_menuActionTable.clear();
        CollectMenuItems(MainWindow::instance()->menuBar());
    }

    m_ui->m_inputEdit->setFocus();
}

void CQuickAccessBar::AddMRUFileItems()
{
    RecentFileList* pMRUList = CCryEditApp::instance()->GetRecentFileList();

    // someone may set HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Policies\Explorer\NoRecentDocsHistory to 1, thus pMRUList is a nullptr
    if (!pMRUList)
    {
        return;
    }

    for (int i = 0; i < pMRUList->GetSize(); ++i)
    {
        QString mruText;
        pMRUList->GetDisplayName(mruText, i, "");
        if (mruText.isEmpty())
        {
            continue;
        }
        m_model->setStringList(m_model->stringList() += mruText);
        m_menuActionTable[mruText] = MainWindow::instance()->GetActionManager()->GetAction(ID_FILE_MRU_FILE1 + i);
    }

    m_model->sort(0);
}

void CQuickAccessBar::CollectMenuItems(QMenuBar* menuBar)
{
    foreach (auto action,  menuBar->actions())
    {
        CollectMenuItems(action, QString());
    }
    m_model->sort(0);
}

void CQuickAccessBar::CollectMenuItems(QAction* action, const QString& path)
{
    const QString actionText = RemoveAcceleratorAmpersands(action->text());
    const QString newPath = path.isEmpty() ? actionText : (path + "." + actionText);
    if (action->menu())
    {
        foreach (auto subAction, action->menu()->actions())
        {
            CollectMenuItems(subAction, newPath);
        }
    }
    else
    {
        const int id = action->data().toInt();
        if (id == 0 || actionText.isEmpty())
        {
            return;
        }

        m_model->setStringList(m_model->stringList() += newPath);
        m_menuActionTable[newPath] = action;
    }
}

#include <QuickAccessBar.moc>
