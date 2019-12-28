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

#include <AzQtComponents/Components/DockTabBar.h>
#include <AzQtComponents/Components/DockBar.h>
#include <AzQtComponents/Components/DockBarButton.h>
#include <AzQtComponents/Components/StyledDockWidget.h>
#include <AzQtComponents/Components/EditorProxyStyle.h>

#include <QAction>
#include <QApplication>
#include <QGraphicsOpacityEffect>
#include <QMenu>
#include <QMouseEvent>
#include <QToolButton>
#include <QStyleOptionTab>

// Constant for the width of the close button and its total offset (width + margin spacing)
static const int g_closeButtonWidth = 19;
static const int g_closeButtonOffset = g_closeButtonWidth + AzQtComponents::DockBar::ButtonsSpacing;
// Constant for the color of our tab indicator underlay
static const QColor g_tabIndicatorUnderlayColor(Qt::black);
// Constant for the opacity of our tab indicator underlay
static const qreal g_tabIndicatorUnderlayOpacity = 0.75;
// Constant for the duration of our tab animations (in milliseconds)
static const int g_tabAnimationDurationMS = 250;


namespace AzQtComponents
{
    /**
     * The close button is only present on the active tab, so return the close button offset for the
     * current index, otherwise none
     */
    int DockTabBar::closeButtonOffsetForIndex(const QStyleOptionTab* option)
    {
        return (option->state & QStyle::State_Selected) ? g_closeButtonOffset : 0;
    }

    /**
     * Return the tab size for the given index with a variable width based on the title length, and a preset height
     */
    QSize DockTabBar::tabSizeHint(const EditorProxyStyle* style, const QStyleOption* option, const QWidget* widget)
    {
        Q_UNUSED(style);

        const auto tabBar = qobject_cast<const DockTabBar*>(widget);
        if (!tabBar)
        {
            return QSize();
        }

        // If there's only one tab, then let the tab take up the entire width so that it appears the
        // same as a title bar
        if (tabBar->count() == 1 && tabBar->parentWidget())
        {
            return QSize(tabBar->parentWidget()->width(), DockBar::Height);
        }

        // Otherwise, use the variable width based on the title length
        const auto tabOption = qstyleoption_cast<const QStyleOptionTab*>(option);
        const int width = DockBar::getTitleMinWidth(tabOption->text) + DockTabBar::closeButtonOffsetForIndex(tabOption);
        return QSize(width, DockBar::Height);
    }

    QRect DockTabBar::rightButtonRect(const EditorProxyStyle* style, const QStyleOption* option, const QWidget* widget)
    {
        Q_UNUSED(style);

        const auto tabBar = qobject_cast<const DockTabBar*>(widget);
        if (!tabBar)
        {
            return QRect();
        }

        const auto tabOption = qstyleoption_cast<const QStyleOptionTab*>(option);
        const int vMargin = (tabOption->rect.height() - g_closeButtonWidth) / 2;
        // 1px offset to keep it pixel perfect with the original drawing code. I couldn't figure out
        // where it was coming from though.
        return tabOption->rect.adjusted(tabOption->rect.width() - g_closeButtonOffset - 1, // 1px offset
            vMargin, -AzQtComponents::DockBar::ButtonsSpacing, -vMargin);
    }

    /**
     * Create a dock tab widget that extends a QTabWidget with a custom DockTabBar to replace the default tab bar
     */
    DockTabBar::DockTabBar(QWidget* parent)
        : TabBar(parent)
        , m_tabIndicatorUnderlay(new QWidget(this))
        , m_leftButton(nullptr)
        , m_rightButton(nullptr)
        , m_contextMenu(nullptr)
        , m_closeTabMenuAction(nullptr)
        , m_closeTabGroupMenuAction(nullptr)
        , m_menuActionTabIndex(-1)
        , m_singleTabFillsWidth(false)
    {
        setFixedHeight(DockBar::Height);
        setMovable(true);

        // Handle our close tab button clicks
        QObject::connect(this, &DockTabBar::tabCloseRequested, this, &DockTabBar::closeTab);

        // Handle when our current tab index changes
        QObject::connect(this, &TabBar::currentChanged, this, &DockTabBar::currentIndexChanged);

        // Our QTabBar base class has left/right indicator buttons for scrolling
        // through the tab header if all the tabs don't fit in the given space for
        // the widget, but they just float over the tabs, so we have added a
        // semi-transparent underlay that will be positioned below them so that
        // it looks better
        QPalette underlayPalette;
        underlayPalette.setColor(QPalette::Background, g_tabIndicatorUnderlayColor);
        m_tabIndicatorUnderlay->setAutoFillBackground(true);
        m_tabIndicatorUnderlay->setPalette(underlayPalette);
        QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(m_tabIndicatorUnderlay);
        effect->setOpacity(g_tabIndicatorUnderlayOpacity);
        m_tabIndicatorUnderlay->setGraphicsEffect(effect);

        // The QTabBar has two QToolButton children that are used as left/right
        // indicators to scroll across the tab header when the width is too short
        // to fit all of the tabs
        for (QToolButton* button : findChildren<QToolButton*>(QString(), Qt::FindDirectChildrenOnly))
        {
            // Grab references to each button for use later
            if (button->accessibleName() == TabBar::tr("Scroll Left"))
            {
                m_leftButton = button;
            }
            else
            {
                m_rightButton = button;
            }
        }
    }

    /**
     * Handle resizing appropriately when our parent tab widget is resized,
     * otherwise when there is only one tab it won't know to stretch to the
     * full width
     */
    QSize DockTabBar::sizeHint() const
    {
        if (m_singleTabFillsWidth && count() == 1)
        {
            return TabBar::tabSizeHint(0);
        }

        return TabBar::sizeHint();
    }

    void DockTabBar::setSingleTabFillsWidth(bool singleTabFillsWidth)
    {
        if (m_singleTabFillsWidth == singleTabFillsWidth)
        {
            return;
        }

        m_singleTabFillsWidth = singleTabFillsWidth;
        emit singleTabFillsWidthChanged(m_singleTabFillsWidth);
    }

    /**
     * Any time the tab layout changes (e.g. tabs are added/removed/resized),
     * we need to check if we need to add our tab indicator underlay, and
     * update our tab close button position
     */
    void DockTabBar::tabLayoutChange()
    {
        TabBar::tabLayoutChange();
        // If the tab indicators are showing, then we need to show our underlay
        if (m_leftButton->isVisible())
        {
            // The underlay will take up the combined space behind the left and
            // right indicator buttons
            QRect total = m_leftButton->geometry();
            total = total.united(m_rightButton->geometry());
            m_tabIndicatorUnderlay->setGeometry(total);

            // The indicator buttons get raised when shown, so we need to stack
            // our underlay under the left button, which will place it under
            // both indicator buttons, and then show it
            m_tabIndicatorUnderlay->stackUnder(m_leftButton);
            m_tabIndicatorUnderlay->show();
        }
        else
        {
            m_tabIndicatorUnderlay->hide();
        }
    }

    void DockTabBar::tabInserted(int index)
    {
        auto closeButton = new DockBarButton(DockBarButton::CloseButton);
        connect(closeButton, &DockBarButton::clicked, this, [=]{
            emit tabCloseRequested(index);
        });
        const ButtonPosition closeSide = (ButtonPosition) style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, 0, this);
        setTabButton(index, closeSide, closeButton);
    }

    /**
     * Handle the right-click context menu event by displaying our custom menu
     * with options to close/undock individual tabs or the entire tab group
     */
    void DockTabBar::contextMenuEvent(QContextMenuEvent* event)
    {
        // Figure out the index of the tab the event was triggered on, or use
        // the currently active tab if the event was triggered in the header
        // dead zone
        int index = tabAt(event->pos());
        if (index == -1)
        {
            index = currentIndex();
        }
        m_menuActionTabIndex = index;

        // Need to create our context menu/actions if this is the first time
        // it has been invoked
        if (!m_contextMenu)
        {
            m_contextMenu = new QMenu(this);

            // Action to close the specified tab, and leave the text blank since
            // it will be dynamically set using the title of the specified tab
            m_closeTabMenuAction = m_contextMenu->addAction(QString());
            QObject::connect(m_closeTabMenuAction, &QAction::triggered, this, [this]() { emit closeTab(m_menuActionTabIndex); });

            // Action to close all of the tabs in our tab widget
            m_closeTabGroupMenuAction = m_contextMenu->addAction(tr("Close Tab Group"));
            QObject::connect(m_closeTabGroupMenuAction, &QAction::triggered, this, &DockTabBar::closeTabGroup);

            // Separate the close actions from the undock actions
            m_contextMenu->addSeparator();

            // Action to undock the specified tab, and leave the text blank since
            // it will be dynamically set using the title of the specified tab
            m_undockTabMenuAction = m_contextMenu->addAction(QString());
            QObject::connect(m_undockTabMenuAction, &QAction::triggered, this, [this]() { emit undockTab(m_menuActionTabIndex); });

            // Action to undock the entire tab widget
            m_undockTabGroupMenuAction = m_contextMenu->addAction(tr("Undock Tab Group"));
            QObject::connect(m_undockTabGroupMenuAction, &QAction::triggered, this ,[this]() { emit undockTab(-1); });
        }

        // Update the menu labels for the close/undock individual tab actions
        QString tabName = tabText(index);
        m_closeTabMenuAction->setText(tr("Close %1").arg(tabName));
        m_undockTabMenuAction->setText(tr("Undock %1").arg(tabName));

        // Only enable the close/undock group actions if we have more than one
        // tab in our tab widget
        bool enableGroupActions = (count() > 1);
        m_closeTabGroupMenuAction->setEnabled(enableGroupActions);

        // Disable the undock group action if our tab widget
        // container is the only dock widget in a floating window
        QWidget* tabWidget = parentWidget();
        bool enableUndock = true;
        if (tabWidget)
        {
            StyledDockWidget* tabWidgetContainer = qobject_cast<StyledDockWidget*>(tabWidget->parentWidget());
            if (tabWidgetContainer)
            {
                enableUndock = !tabWidgetContainer->isSingleFloatingChild();
            }
        }
        m_undockTabGroupMenuAction->setEnabled(enableGroupActions && enableUndock);

        // Enable the undock action if there are multiple tabs or if this isn't
        // a single tab in a floating window
        m_undockTabMenuAction->setEnabled(enableGroupActions || enableUndock);

        // Show the context menu
        m_contextMenu->exec(event->globalPos());
    }

    /**
     * Close all of the tabs in our tab widget
     */
    void DockTabBar::closeTabGroup()
    {
        // Close each of the tabs using our signal trigger so they are cleaned
        // up properly
        int numTabs = count();
        for (int i = 0; i < numTabs; ++i)
        {
            emit closeTab(0);
        }
    }

    /**
     * When our tab index changes, we need to force a resize event to trigger a layout change, since the tabSizeHint needs
     * to be updated because we only show the close button on the active tab
     */
    void DockTabBar::currentIndexChanged(int current)
    {
        resizeEvent(nullptr);

        const ButtonPosition closeSide = (ButtonPosition) style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, 0, this);
        const int numTabs = count();
        for (int i = 0; i < numTabs; ++i)
        {
            if (auto button = tabButton(i, closeSide))
            {
                button->setVisible(i == current);
            }
        }
    }

    /**
     * Override the mouse press event handler to fix a Qt issue where the QTabBar
     * doesn't ensure that it's the left mouse button that has been pressed
     * early enough, even though it properly checks for it first in its mouse
     * release event handler
     */
    void DockTabBar::mousePressEvent(QMouseEvent* event)
    {
        if (event->button() != Qt::LeftButton)
        {
            event->ignore();
            return;
        }

        TabBar::mousePressEvent(event);
    }

    /**
     * Send a dummy MouseButtonRelease event to the QTabBar to ensure that the tab move animations
     * get triggered when a tab is dragged out of the tab bar.
     */
    void DockTabBar::finishDrag()
    {
        QMouseEvent event(QEvent::MouseButtonRelease, {0.0f, 0.0f}, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        mouseReleaseEvent(&event);
    }

#include <Components/DockTabBar.moc>
} // namespace AzQtComponents