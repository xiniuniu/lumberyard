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

#include "TabWidgetPage.h"
#include <Gallery/ui_TabWidgetPage.h>

#include <AzQtComponents/Components/Widgets/TabWidget.h>
#include <QAction>
#include <QMessageBox>

#include <QDebug>

TabWidgetPage::TabWidgetPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::TabWidgetPage)
{
    ui->setupUi(this);

    QAction* action1 = new QAction(QIcon(":/stylesheet/img/table_error.png"), "Action 1", this);
    QAction* action2 = new QAction(QIcon(":/stylesheet/img/table_error.png"), "Action 2", this);
    QAction* action3 = new QAction(QIcon(":/stylesheet/img/table_error.png"), "Action 3", this);

    connect(action1, &QAction::triggered, this, [this]() {
        QMessageBox messageBox({}, "Action 1 triggered", "Action 1 has been triggered", QMessageBox::Ok);
        messageBox.exec();
    });

    connect(action2, &QAction::triggered, this, [this]() {
        QMessageBox messageBox({}, "Action 2 triggered", "Action 2 has been triggered", QMessageBox::Ok);
        messageBox.exec();
    });

    connect(action3, &QAction::triggered, this, [this]() {
        QMessageBox messageBox({}, "Action 3 triggered", "Action 3 has been triggered", QMessageBox::Ok);
        messageBox.exec();
    });

    ui->tabWidget->setActionToolBarVisible();
    ui->tabWidget_2->setActionToolBarVisible();
    ui->tabWidget_3->setActionToolBarVisible();

    ui->tabWidget->addAction(action1);
    ui->tabWidget->addAction(action2);
    ui->tabWidget->addAction(action3);

    ui->tabWidget_2->addAction(action1);
    ui->tabWidget_2->addAction(action2);
    ui->tabWidget_2->addAction(action3);

    ui->tabWidget_3->addAction(action1);
    ui->tabWidget_3->addAction(action2);
    ui->tabWidget_3->addAction(action3);

    auto* removeFirstAction = new QPushButton("Remove first action");
    auto* removeLastAction = new QPushButton("Remove last action");
    auto* changeActions = new QPushButton("Change actions");
    auto* toggleActionToolBars = new QPushButton("Toggle action toolbars");
    int* changeIdx = new int(0);

    connect(removeFirstAction, &QPushButton::clicked, this, [this]() {
        if (ui->tabWidget->actions().count())
        {
            ui->tabWidget->removeAction(ui->tabWidget->actions()[0]);
        }

        if (ui->tabWidget_2->actions().count())
        {
            ui->tabWidget_2->removeAction(ui->tabWidget_2->actions()[0]);
        }

        if (ui->tabWidget_3->actions().count())
        {
            ui->tabWidget_3->removeAction(ui->tabWidget_3->actions()[0]);
        }
    });

    connect(removeLastAction, &QPushButton::clicked, this, [this]() {
        if (ui->tabWidget->actions().count())
        {
            ui->tabWidget->removeAction(ui->tabWidget->actions()[ui->tabWidget->actions().count() - 1]);
        }

        if (ui->tabWidget_2->actions().count())
        {
            ui->tabWidget_2->removeAction(ui->tabWidget_2->actions()[ui->tabWidget_2->actions().count() - 1]);
        }

        if (ui->tabWidget_3->actions().count())
        {
            ui->tabWidget_3->removeAction(ui->tabWidget_3->actions()[ui->tabWidget_3->actions().count() - 1]);
        }
    });

    connect(changeActions, &QPushButton::clicked, this, [action1, action2, action3, changeIdx]() {
        if (*changeIdx)
        {
            action1->setText("Action 1");
            action2->setText("Action 2");
            action3->setText("Action 3");

            action1->setIcon(QIcon(":/stylesheet/img/table_error.png"));
            action2->setIcon(QIcon(":/stylesheet/img/table_error.png"));
            action3->setIcon(QIcon(":/stylesheet/img/table_error.png"));
        }
        else
        {
            action1->setText("Action A");
            action2->setText("Action B");
            action3->setText("Action C");

            action1->setIcon(QIcon(":/stylesheet/img/table_success.png"));
            action2->setIcon(QIcon(":/stylesheet/img/table_success.png"));
            action3->setIcon(QIcon(":/stylesheet/img/table_success.png"));
        }
        *changeIdx = 1 - *changeIdx;
    });

    connect(toggleActionToolBars, &QPushButton::clicked, this, [this]() {
        ui->tabWidget->setActionToolBarVisible(!ui->tabWidget->isActionToolBarVisible());
        ui->tabWidget_2->setActionToolBarVisible(!ui->tabWidget_2->isActionToolBarVisible());
        ui->tabWidget_3->setActionToolBarVisible(!ui->tabWidget_3->isActionToolBarVisible());
    });

    ui->verticalLayout->addWidget(removeFirstAction);
    ui->verticalLayout->addWidget(removeLastAction);
    ui->verticalLayout->addWidget(changeActions);
    ui->verticalLayout->addWidget(toggleActionToolBars);

    QString exampleText = R"(

QTabWidget docs: <a href="http://doc.qt.io/qt-5/qtabwidget.html">http://doc.qt.io/qt-5/qtabwidget.html</a><br/>
QTabBar docs: <a href="http://doc.qt.io/qt-5/qtabbar.html">http://doc.qt.io/qt-5/qtabbar.html</a><br/>

<pre>
#include &lt;AzQtComponents/Components/Widgets/TabWidget.h&gt;

// Include this only if you want to customize the action toolbar either with
// TabWidget::setActionToolBar(TabWidgetActionToolBar*) or TabWidget::TabWidget(TabWidgetActionToolBar*, QWidget*)
#include &lt;AzQtComponents/Components/Widgets/TabWidgetActionToolBar.h&gt;

AzQtComponents::TabWidget* tabWidget = new AzQtComponents::TabWidget();

// To make tabs closeable
tabWidget->setTabsCloseable(true);

// To make tabs moveable/draggable
tabWidget->setMovable(true);

// Adding some tabs to the TabWidget
QWidget* tab1 = new QWidget();
QWidget* tab2 = new QWidget();

tabWidget->addTab(widget1);
tabWidget->addTab(widget2);

// Shows the action toolbar: TabWidgetActionToolbar is bound to the TabWidget's action events
// and will create, change and delete buttons according to the TabWidget's actions status.
// Use tabWidget->setActionToolbarVisible(false) to disable it.
tabWidget->setActionToolBarVisible();

// You can also use a custom action toolbar widget
tabWidget->setActionToolBar(new AzQtComponents::TabWidgetActionToolBar(tabWidget));
// Or specify the action toolbar directly in the constructor
auto tabWidget2 = new AzQtComponents::TabWidget(new AzQtComponents::TabWidgetActionToolBar());
// In both cases the action toolbar is implicitly enabled
// Remember to include "TabWidgetActionToolBar.h" if you want to explictly specify the action toolbar!

// Once the action toolbar is enabled, create some action to be added to the TabWidget
// Setting the icon and the text for each one
QAction* action1 = new QAction(QIcon(":/stylesheet/img/action1_icon.png"), "Action 1", this);
QAction* action2 = new QAction(QIcon(":/stylesheet/img/action2_icon.png"), "Action 2", this);
QAction* action3 = new QAction(QIcon(":/stylesheet/img/action3_icon.png"), "Action 3", this);

connect(action1, &QAction::triggered, this, [this]() {
    QMessageBox messageBox({}, "Action 1 triggered", "Action 1 has been triggered", QMessageBox::Ok);
    messageBox.exec();
});

connect(action2, &QAction::triggered, this, [this]() {
    QMessageBox messageBox({}, "Action 2 triggered", "Action 2 has been triggered", QMessageBox::Ok);
    messageBox.exec();
});

connect(action3, &QAction::triggered, this, [this]() {
    QMessageBox messageBox({}, "Action 3 triggered", "Action 3 has been triggered", QMessageBox::Ok);
    messageBox.exec();
});

// Adding the actions to the TabWidget
tabWidget->addAction(action1);
tabWidget->addAction(action2);
tabWidget->addAction(action3);

// Removing actions from the TabWidget
tabWidget->removeAction(action1);

// Changing actions' icons or texts automatically reflects on the widget
action1->setText("New action1 text");
action1->setIcon(QIcon(":/stylesheet/img/new_action1_icon.png"));

</pre>

)";

    ui->exampleText->setHtml(exampleText);
}

#include <Gallery/TabWidgetPage.moc>
