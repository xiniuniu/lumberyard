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
#include "precompiled.h"

#include <QMenu>
#include <QAction>

#include <Editor/View/Widgets/LoggingPanel/LoggingWindow.h>
#include <Editor/View/Widgets/LoggingPanel/ui_LoggingWindow.h>

namespace ScriptCanvasEditor
{
    //////////////////
    // LoggingWindow
    //////////////////

    LoggingWindow::LoggingWindow(QWidget* parentWidget)
        : AzQtComponents::StyledDockWidget(parentWidget)
        , m_ui(new Ui::LoggingWindow)
    {
        m_ui->setupUi(this);

        // Hack to hide the close button on the first tab. Since we always want it open.
        m_ui->tabWidget->setTabsClosable(true);
        m_ui->tabWidget->tabBar()->setTabButton(0, QTabBar::ButtonPosition::RightSide, nullptr);
        m_ui->tabWidget->tabBar()->setTabButton(0, QTabBar::ButtonPosition::LeftSide, nullptr);

        m_pivotGroup.addButton(m_ui->entitiesPivotButton);
        m_pivotGroup.addButton(m_ui->graphsPivotButton);
        m_pivotGroup.setExclusive(true);

        m_ui->entitiesPivotButton->setChecked(true);

        QObject::connect(m_ui->entitiesPivotButton, &QPushButton::pressed, this, &LoggingWindow::PivotOnEntities);
        QObject::connect(m_ui->graphsPivotButton, &QPushButton::pressed, this, &LoggingWindow::PivotOnGraphs);
        QObject::connect(m_ui->tabWidget, &QTabWidget::currentChanged, this, &LoggingWindow::OnActiveTabChanged);        

        m_entityPageIndex = m_ui->stackedWidget->indexOf(m_ui->entitiesPage);
        m_graphPageIndex = m_ui->stackedWidget->indexOf(m_ui->graphsPage);

        OnActiveTabChanged(m_ui->tabWidget->currentIndex());
        PivotOnEntities();
    }

    LoggingWindow::~LoggingWindow()
    {

    }

    void LoggingWindow::OnActiveTabChanged(int index)
    {
        LoggingWindowSession* windowSession = qobject_cast<LoggingWindowSession*>(m_ui->tabWidget->currentWidget());

        if (windowSession)
        {
            m_activeDataId = windowSession->GetDataId();
        }

        m_ui->entityPivotWidget->SwitchDataSource(m_activeDataId);
        m_ui->graphPivotWidget->SwitchDataSource(m_activeDataId);
    }

    void LoggingWindow::PivotOnEntities()
    {
        m_ui->stackedWidget->setCurrentIndex(m_ui->stackedWidget->indexOf(m_ui->entitiesPage));
    }

    void LoggingWindow::PivotOnGraphs()
    {
        m_ui->stackedWidget->setCurrentIndex(m_ui->stackedWidget->indexOf(m_ui->graphsPage));
    }

    PivotTreeWidget* LoggingWindow::GetActivePivotWidget() const
    {
        if (m_ui->stackedWidget->currentIndex() == m_entityPageIndex)
        {
            return m_ui->entityPivotWidget;
        }

        return nullptr;
    }

#include <Editor/View/Widgets/LoggingPanel/LoggingWindow.moc>
}