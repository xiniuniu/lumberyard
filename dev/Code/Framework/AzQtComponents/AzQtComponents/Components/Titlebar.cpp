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

#include <qglobal.h> // For Q_OS_WIN

#include <AzQtComponents/Components/Titlebar.h>
#include <AzQtComponents/Components/ButtonDivider.h>
#include <AzQtComponents/Components/ConfigHelpers.h>
#include <AzQtComponents/Components/StyledDockWidget.h>
#include <AzQtComponents/Components/DockBar.h>
#include <AzQtComponents/Components/DockMainWindow.h>
#include <AzQtComponents/Components/StyleHelpers.h>
#include <AzQtComponents/Components/DockTabBar.h>

#include <QApplication>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QWindow>
#include <QPainter>
#include <QMenu>
#include <QDesktopWidget>
AZ_PUSH_DISABLE_WARNING(4251 4244, "-Wunknown-warning-option") // 4251: 'QHighDpiScaling::m_logicalDpi': struct 'QPair<qreal,qreal>' needs to have dll-interface to be used by clients of class 'QHighDpiScaling'
                                                               // 4244: 'argument': conversion from 'qreal' to 'int', possible loss of data
#include <QtGui/private/qhighdpiscaling_p.h>
AZ_POP_DISABLE_WARNING
#include <QLabel>
#include <QStackedLayout>
#include <QVector>

namespace AzQtComponents
{
    // Works like QWidget::window() but also considers a docked QDockWidget.
    // This is used when clicking on the custom titlebar, if the custom titlebar is on a dock widget
    // we don't want to close the host, but only the dock.
    QWidget* actualTopLevelFor(QWidget* w)
    {
        if (!w)
        {
            return nullptr;
        }

        if (w->isWindow())
        {
            return w;
        }

        if (qobject_cast<QDockWidget*>(w))
        {
            return w;
        }

        return actualTopLevelFor(w->parentWidget());
    }

    TitleBarLabel::TitleBarLabel(QWidget* parent)
        : QLabel(parent)
    {
    }

    TitleBarLabel::~TitleBarLabel()
    {
    }

    QSize TitleBarLabel::minimumSizeHint() const
    {
        // Override the QLabel minimumSizeHint width to let other
        // widgets know we can deal with less space.
        return QFrame::minimumSizeHint().boundedTo({0, std::numeric_limits<int>::max()});
    }

    TitleBar::Config TitleBar::loadConfig(QSettings& settings)
    {
        Config config = defaultConfig();

        // TitleBar
        {
            ConfigHelpers::GroupGuard title(&settings, QStringLiteral("TitleBar"));
            ConfigHelpers::read<int>(settings, QStringLiteral("Height"), config.titleBar.height);
            ConfigHelpers::read<int>(settings, QStringLiteral("SimpleHeight"), config.titleBar.simpleHeight);
            ConfigHelpers::read<bool>(settings, QStringLiteral("AppearAsTabBar"), config.titleBar.appearAsTabBar);
        }

        // Icon
        {
            ConfigHelpers::GroupGuard icon(&settings, QStringLiteral("Icon"));
            ConfigHelpers::read<bool>(settings, QStringLiteral("Visible"), config.icon.visible);
        }

        // Title
        {
            ConfigHelpers::GroupGuard title(&settings, QStringLiteral("Title"));
            ConfigHelpers::read<int>(settings, QStringLiteral("Indent"), config.title.indent);
            ConfigHelpers::read<bool>(settings, QStringLiteral("VisibleWhenSimple"), config.title.visibleWhenSimple);
        }

        // Event Log Details
        {
            ConfigHelpers::GroupGuard buttons(&settings, QStringLiteral("Buttons"));
            ConfigHelpers::read<bool>(settings, QStringLiteral("ShowDividerButtons"), config.buttons.showDividerButtons);
            ConfigHelpers::read<int>(settings, QStringLiteral("Spacing"), config.buttons.spacing);
        }

        return config;
    }

    TitleBar::Config TitleBar::defaultConfig()
    {
        Config config;

        config.titleBar.height = 32;
        config.titleBar.simpleHeight = 14;
        config.titleBar.appearAsTabBar = true;

        config.icon.visible = false;

        config.title.indent = 0;
        config.title.visibleWhenSimple = false;

        config.buttons.showDividerButtons = false;
        config.buttons.spacing = 12;

        return config;
    }

    TitleBar::TitleBar(QWidget* parent)
        : QFrame(parent)
    {
        // To allow drawSimple and tearEnabled to be used in the style sheets, ensure the widget is
        // repolished when the properties change.
        StyleHelpers::repolishWhenPropertyChanges(this, &TitleBar::drawSimpleChanged);
        StyleHelpers::repolishWhenPropertyChanges(this, &TitleBar::tearEnabledChanged);

        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

        m_stackedLayout = new QStackedLayout(this);
        m_stackedLayout->setContentsMargins(0,0,0,0);
        m_stackedLayout->setSpacing(0);

        auto tabBarContainer = new QWidget(this);
        auto tabBarlayout = new QHBoxLayout(tabBarContainer);
        tabBarlayout->setContentsMargins(0,0,0,0);
        tabBarlayout->setSpacing(0);

        m_tabBar = new DockTabBar(tabBarContainer);
        m_tabBar->setMovable(false);
        m_tabBar->installEventFilter(this);
        m_tabBar->addTab({});
        connect(m_tabBar, &DockTabBar::tabCloseRequested, this, &TitleBar::handleClose);
        m_tabBar->setDrawBase(false);

        tabBarlayout->addWidget(m_tabBar);
        tabBarlayout->addStretch();

        m_stackedLayout->addWidget(tabBarContainer);

        auto container = new QWidget(this);
        auto layout = new QHBoxLayout(container);
        layout->setContentsMargins(0,0,0,0);
        layout->setSpacing(0);

        m_stackedLayout->addWidget(container);

        m_icon = new QLabel(container);
        m_icon->setObjectName(QStringLiteral("icon"));
        layout->addWidget(m_icon);

        // Don't use AzQtComponenets::ElidingLabel because when used in combination with the
        // stretch, its width is reduced to zero. ElidingLabel also displays an elipsis ('...')
        // which was not present in the original TitleBar painting code.
        m_label = new TitleBarLabel(container);
        m_label->setObjectName(QStringLiteral("title"));
        layout->addWidget(m_label);

        layout->addStretch();

        m_buttonsContainer = new QFrame(container);
        m_buttonsContainer->setObjectName(QStringLiteral("buttons"));
        m_buttonsLayout = new QHBoxLayout(m_buttonsContainer);
        m_buttonsLayout->setContentsMargins(0,0,0,0);
        m_buttonsLayout->setSpacing(DockBar::ButtonsSpacing);
        layout->addWidget(m_buttonsContainer);

        setCursor(m_originalCursor);
        setButtons({ DockBarButton::CloseButton });
        setMouseTracking(true);

        connect(this, &TitleBar::drawSimpleChanged, this, &TitleBar::updateTitle);
        connect(this, &TitleBar::drawSimpleChanged, this, &TitleBar::updateTitleBar);
        connect(this, &TitleBar::windowTitleOverrideChanged, this, &TitleBar::updateTitle);
        updateTitle();
        updateTitleBar();

        // We have to do the lowering of the title bar whenever the widget goes from being docked in a QDockWidgetGroupWindow, but
        // there's no reliable way to figure out when that is, so we use a timer instead and just lower it ever time through.
        const int FIX_STACK_ORDER_INTERVAL_IN_MILLISECONDS = 500;
        startTimer(FIX_STACK_ORDER_INTERVAL_IN_MILLISECONDS);

        connect(&m_enableMouseTrackingTimer, &QTimer::timeout, this, &TitleBar::checkEnableMouseTracking);
        m_enableMouseTrackingTimer.setInterval(500);
    }

    TitleBar::~TitleBar()
    {
    }

    void TitleBar::handleClose()
    {
        actualTopLevelFor(this)->close();
    }

    void TitleBar::handleMaximize()
    {
        QWidget* w = window();
        if (isMaximized())
        {
            w->showNormal();
        }
        else
        {
            w->showMaximized();

            // Need to separately resize based on the available geometry for
            // the screen because since floating windows are frameless, on
            // Windows 10 they end up taking up the entire screen when maximized
            // instead of respecting the available space (e.g. taskbar)
            w->setGeometry(QApplication::desktop()->availableGeometry(w));
        }
    }

    void TitleBar::handleMinimize()
    {
        window()->showMinimized();
    }

    bool TitleBar::hasButton(DockBarButton::WindowDecorationButton buttonType) const
    {
        return m_buttons.contains(buttonType);
    }

    bool TitleBar::buttonIsEnabled(DockBarButton::WindowDecorationButton buttonType) const
    {
        if (hasButton(buttonType))
        {
            DockBarButton* button = findButton(buttonType);
            if (button)
            {
                return button->isEnabled();
            }
        }

        return false;
    }

    void TitleBar::handleMoveRequest()
    {
        delete m_interactiveWindowGeometryChanger;
        if (auto topLevelWidget = window())
        {
            m_interactiveWindowGeometryChanger = new InteractiveWindowMover(topLevelWidget->windowHandle(), this);
        }
    }

    void TitleBar::handleSizeRequest()
    {
        delete m_interactiveWindowGeometryChanger;
        if (auto topLevelWidget = window())
        {
            m_interactiveWindowGeometryChanger = new InteractiveWindowResizer(topLevelWidget->windowHandle(), this);
        }
    }

    void TitleBar::setDrawSideBorders(bool enable)
    {
        if (m_drawSideBorders != enable)
        {
            m_drawSideBorders = enable;
            update();
            emit drawSideBordersChanged(m_drawSideBorders);
        }
    }

    /**
     * Change the title bar drawing mode between default and simple
     */
    void TitleBar::setDrawSimple(bool enable)
    {
        if (m_drawSimple != enable)
        {
            m_drawSimple = enable;
            setupButtons();
            update();
            emit drawSimpleChanged(m_drawSimple);
        }
    }

    void TitleBar::setDragEnabled(bool enable)
    {
        m_dragEnabled = enable;
    }

    void TitleBar::setTearEnabled(bool enable)
    {
        if (enable != m_tearEnabled)
        {
            m_tearEnabled = enable;
            update();
            emit tearEnabledChanged(m_tearEnabled);
        }
    }

    void TitleBar::setDrawAsTabBar(bool enable)
    {
        if (enable != m_appearAsTabBar)
        {
            m_appearAsTabBar = enable;
            update();
            emit drawAsTabBarChanged(m_appearAsTabBar);
        }
    }

    QSize TitleBar::sizeHint() const
    {
        const int width = QFrame::sizeHint().width();
        const int height = style()->pixelMetric(QStyle::PM_TitleBarHeight, nullptr, this);
        return QSize(width, height);
    }

    void TitleBar::setWindowTitleOverride(const QString& titleOverride)
    {
        if (m_titleOverride != titleOverride)
        {
            m_titleOverride = titleOverride;
            update();
            emit windowTitleOverrideChanged(m_titleOverride);
        }
    }

    bool TitleBar::event(QEvent* event)
    {
        switch (event->type())
        {
            case QEvent::ParentChange:
            case QEvent::Show:
            case QEvent::WindowTitleChange:
                updateTitle();
                break;
            case QEvent::StyleChange:
                // Reset the TitleBar height when the style changes. It is still too early to do it
                // in TitleBar::unpolish because the widget doesn't have the new style yet.
                updateTitleBar();
                break;
            default:
                break;
        }
        return QFrame::event(event);
    }

    DockBarButton* TitleBar::findButton(DockBarButton::WindowDecorationButton buttonType) const
    {
        // Use the layout to get the right button widget, instead of the buttonContainer,
        // as old buttons get deleteLater'd, which means they might still be around.
        // Buttons still in the layout are guaranteed to be the right ones.
        int itemsInLayout = m_buttonsLayout->count();
        for (int i = 0; i < itemsInLayout; i++)
        {
            QLayoutItem* item = m_buttonsLayout->itemAt(i);
            DockBarButton* button = qobject_cast<DockBarButton*>(item->widget());
            if (button && button->buttonType() == buttonType)
            {
                return button;
            }
        }

        return nullptr;
    }

    void TitleBar::disableButton(DockBarButton::WindowDecorationButton buttonType)
    {
        DockBarButton* button = findButton(buttonType);
        if (button)
        {
            button->setEnabled(false);
        }
    }

    void TitleBar::enableButton(DockBarButton::WindowDecorationButton buttonType)
    {
        DockBarButton* button = findButton(buttonType);
        if (button)
        {
            button->setEnabled(true);
        }
    }

    QString TitleBar::title() const
    {
        QString result;
        if (!m_titleOverride.isEmpty())
        {
            // title override takes precedence.
            result = m_titleOverride;
        }
        else if (auto dock = qobject_cast<QDockWidget*>(parentWidget()))
        {
            // A docked QDockWidget has the title of the dockwidget, not of the top-level QWidget

            result = dock->windowTitle();
        }
        else if (auto w = window())
        {
            result = w->windowTitle();
        }

        if (result.isEmpty())
        {
            result = QApplication::applicationName();
        }

        return result;
    }

    void TitleBar::updateTitle()
    {
        const auto text = m_drawSimple ? QApplication::applicationName() : title();
        m_label->setText(text);
        m_tabBar->setTabText(0, text);
    }

    void TitleBar::updateTitleBar()
    {
        setFixedHeight(style()->pixelMetric(QStyle::PM_TitleBarHeight, nullptr, this));
        m_label->setVisible(m_drawSimple ? m_showLabelWhenSimple : true);
        const int currentIndex = !m_drawSimple && m_appearAsTabBar && isTitleBarForDockWidget() ? 0 : 1;
        m_stackedLayout->setCurrentIndex(currentIndex);
    }

    void TitleBar::contextMenuEvent(QContextMenuEvent*)
    {
        // If this titlebar is for the main editor window or one of the floating
        // title bars, then use the standard context menu with min/max/close/etc... actions
        StyledDockWidget* dockWidgetParent = qobject_cast<StyledDockWidget*>(parentWidget());
        // Main Window title bar, old title bar in old docking, and new title bar will use standard context menu
        if (!dockWidgetParent || m_drawSimple)
        {
            updateStandardContextMenu();
        }
        // the old title bar in the new docking will use new context menu
        else
        {
            updateDockedContextMenu();
        }

        m_contextMenu->exec(QCursor::pos());
    }

    bool TitleBar::eventFilter(QObject* watched, QEvent* event)
    {
        if (watched != m_tabBar)
        {
            return QFrame::eventFilter(watched, event);
        }

        switch (event->type())
        {
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonRelease:
            case QEvent::MouseButtonDblClick:
            case QEvent::MouseMove:
                // Filter our these events, but make sure we ignore them so they get propagated past
                // the tab bar.
                event->ignore();
                return true;

            default:
                break;
        }

        return QFrame::eventFilter(watched, event);
    }

    bool TitleBar::polish(Style* style, QWidget* widget, const Config& config)
    {
        Q_UNUSED(style);
        Q_UNUSED(config);

        auto titleBar = qobject_cast<TitleBar*>(widget);
        if (!titleBar)
        {
            return false;
        }

        titleBar->m_icon->setVisible(config.icon.visible);
        titleBar->m_label->setIndent(config.title.indent);
        titleBar->m_showLabelWhenSimple = config.title.visibleWhenSimple;
        titleBar->setDrawAsTabBar(config.titleBar.appearAsTabBar);
        titleBar->m_buttonsLayout->setSpacing(config.buttons.spacing);
        titleBar->setupButtons(config.buttons.showDividerButtons);

        titleBar->updateTitleBar();

        return true;
    }

    bool TitleBar::unpolish(Style* style, QWidget* widget, const Config& config)
    {
        Q_UNUSED(style);
        Q_UNUSED(config);

        auto titleBar = qobject_cast<TitleBar*>(widget);
        if (!titleBar)
        {
            return false;
        }

        titleBar->m_icon->setVisible(true);
        titleBar->m_label->setIndent(-1);
        titleBar->m_showLabelWhenSimple = true;
        titleBar->setDrawAsTabBar(false);
        titleBar->m_buttonsLayout->setSpacing(DockBar::ButtonsSpacing);
        titleBar->setupButtons();

        titleBar->updateTitleBar();

        return true;
    }

    int TitleBar::titleBarHeight(const Style* style, const QStyleOption* option, const QWidget* widget, const Config& config, const TabWidget::Config& tabConfig)
    {
        Q_UNUSED(style);
        Q_UNUSED(option);

        if (auto titleBar = qobject_cast<const TitleBar*>(widget))
        {
            if (titleBar->drawSimple())
            {
                return config.titleBar.simpleHeight;
            }
            else if (titleBar->drawAsTabBar() && titleBar->isTitleBarForDockWidget())
            {
                return tabConfig.tabHeight;
            }
            else
            {
                return config.titleBar.height;
            }
        }
        return -1;
    }

    bool TitleBar::usesCustomTopBorderResizing() const
    {
#ifdef Q_OS_WIN
        // On Win < 10 we're not overlapping the titlebar, removing it works fine there.
        // On Win < 10 we use native resizing of the top border.
        return QSysInfo::windowsVersion() >= QSysInfo::WV_WINDOWS10;
#else
        return true;
#endif
    }

    void TitleBar::checkEnableMouseTracking()
    {
        // We don't get a mouse release when detaching a dock widget, so much do it with a workaround
        if (!hasMouseTracking())
        {
            if (!isLeftButtonDown() && (!isInDockWidget() || isInFloatingDockWidget()))
            {
                setMouseTracking(true);
                updateMouseCursor(QCursor::pos());
            }
            else
            {
                m_enableMouseTrackingTimer.start();
            }
        }
    }

    QWidget* TitleBar::dockWidget() const
    {
        // If this titlebar is being used in a dock widget, returns that dockwidget
        // The result can either be a QDockWidget or a QDockWidgetGroupWindow
        if (auto dock = qobject_cast<QDockWidget*>(actualTopLevelFor(const_cast<TitleBar*>(this))))
        {
            return dock;
        }
        else if (isInDockWidgetWindowGroup())
        {
            return window();
        }

        return nullptr;
    }

    bool TitleBar::isInDockWidget() const
    {
        return dockWidget() != nullptr || isInDockWidgetWindowGroup();
    }

    bool TitleBar::isInFloatingDockWidget() const
    {
        if (QWidget *win = window())
        {
            return qobject_cast<QDockWidget*>(win)
                || strcmp(win->metaObject()->className(), "QDockWidgetGroupWindow") == 0;
        }

        return false;
    }

    /**
     * Setup our context menu for the main editor window and floating title bars
     */
    void TitleBar::updateStandardContextMenu()
    {
        if (!m_contextMenu)
        {
            m_contextMenu = new QMenu(this);

            m_restoreMenuAction = m_contextMenu->addAction(tr("Restore"));
            QIcon restoreIcon;
            restoreIcon.addFile(":/stylesheet/img/titlebarmenu/restore.png");
            restoreIcon.addFile(":/stylesheet/img/titlebarmenu/restore_disabled.png", QSize(), QIcon::Disabled);

            m_restoreMenuAction->setIcon(restoreIcon);
            connect(m_restoreMenuAction, &QAction::triggered, this, &TitleBar::handleMaximize);

            m_moveMenuAction = m_contextMenu->addAction(tr("Move"));
            connect(m_moveMenuAction, &QAction::triggered, this, &TitleBar::handleMoveRequest);

            m_sizeMenuAction = m_contextMenu->addAction(tr("Size"));
            connect(m_sizeMenuAction, &QAction::triggered, this, &TitleBar::handleSizeRequest);

            m_minimizeMenuAction = m_contextMenu->addAction(tr("Minimize"));
            QIcon minimizeIcon;
            minimizeIcon.addFile(":/stylesheet/img/titlebarmenu/minimize.png");
            minimizeIcon.addFile(":/stylesheet/img/titlebarmenu/minimize_disabled.png", QSize(), QIcon::Disabled);

            m_minimizeMenuAction->setIcon(minimizeIcon);
            connect(m_minimizeMenuAction, &QAction::triggered, this, &TitleBar::handleMinimize);

            m_maximizeMenuAction = m_contextMenu->addAction(tr("Maximize"));
            QIcon maximizeIcon;
            maximizeIcon.addFile(":/stylesheet/img/titlebarmenu/maximize.png");
            maximizeIcon.addFile(":/stylesheet/img/titlebarmenu/maximize_disabled.png", QSize(), QIcon::Disabled);

            m_maximizeMenuAction->setIcon(QIcon(maximizeIcon));
            connect(m_maximizeMenuAction, &QAction::triggered, this, &TitleBar::handleMaximize);

            m_contextMenu->addSeparator();

            m_closeMenuAction = m_contextMenu->addAction(tr("Close"));
            QIcon closeIcon;
            closeIcon.addFile(":/stylesheet/img/titlebarmenu/close.png", QSize());
            closeIcon.addFile(":/stylesheet/img/titlebarmenu/close_disabled.png", QSize(), QIcon::Disabled);

            m_closeMenuAction->setIcon(QIcon(closeIcon));
            m_closeMenuAction->setShortcut(QString("Alt+F4"));
            connect(m_closeMenuAction, &QAction::triggered, this, &TitleBar::handleClose);
        }

        QWidget *topLevelWidget = window();
        if (!topLevelWidget)
        {
            // Defensive, doesn't happen
            return;
        }

        m_restoreMenuAction->setEnabled(buttonIsEnabled(DockBarButton::MaximizeButton) && isMaximized());
        m_moveMenuAction->setEnabled(!isMaximized());
        const bool isFixedSize = topLevelWidget->minimumSize() == topLevelWidget->maximumSize();
        m_sizeMenuAction->setEnabled(!isFixedSize && !isMaximized());
        m_minimizeMenuAction->setEnabled(buttonIsEnabled(DockBarButton::MinimizeButton));
        m_maximizeMenuAction->setEnabled(buttonIsEnabled(DockBarButton::MaximizeButton) && !isMaximized());
        m_closeMenuAction->setEnabled(buttonIsEnabled(DockBarButton::CloseButton));

    }

    /**
     * Setup our context menu for all docked panes
     */
    void TitleBar::updateDockedContextMenu()
    {
        if (!m_contextMenu)
        {
            m_contextMenu = new QMenu(this);

            // Action to close our dock widget, and leave the text blank since
            // it will be dynamically set using the title of the dock widget
            m_closeMenuAction = m_contextMenu->addAction(QString());
            QObject::connect(m_closeMenuAction, &QAction::triggered, this, &TitleBar::handleClose);

            // Unused in this context, but still here for consistency
            m_closeGroupMenuAction = m_contextMenu->addAction(tr("Close Tab Group"));

            // Separate the close actions from the undock actions
            m_contextMenu->addSeparator();

            // Action to undock our dock widget, and leave the text blank since
            // it will be dynamically set using the title of the dock widget
            m_undockMenuAction = m_contextMenu->addAction(QString());
            QObject::connect(m_undockMenuAction, &QAction::triggered, this, &TitleBar::undockAction);

            // Unused in this context, but still here for consistency
            m_undockGroupMenuAction = m_contextMenu->addAction(tr("Undock Tab Group"));
        }

        // Update the menu labels for the close/undock actions
        QString titleLabel = title();
        m_closeMenuAction->setText(tr("Close %1").arg(titleLabel));
        m_undockMenuAction->setText(tr("Undock %1").arg(titleLabel));

        // Don't enable the undock action if this dock widget is the only pane
        // in a floating window or if it isn't docked in one of our dock main windows
        StyledDockWidget* dockWidgetParent = qobject_cast<StyledDockWidget*>(parentWidget());
        DockMainWindow* dockMainWindow = nullptr;
        if (dockWidgetParent)
        {
            dockMainWindow = qobject_cast<DockMainWindow*>(dockWidgetParent->parentWidget());
        }
        m_undockMenuAction->setEnabled(dockMainWindow && dockWidgetParent && !dockWidgetParent->isSingleFloatingChild());

        // The group actions are always disabled, they are only provided for
        // menu consistency with the tab bar
        m_closeGroupMenuAction->setEnabled(false);
        m_undockGroupMenuAction->setEnabled(false);
    }

    void TitleBar::mousePressEvent(QMouseEvent* ev)
    {
        // use QCursor::pos(); in scenarios with multiple screens and different scale factors,
        // it's much more reliable about actually reporting a global position.
        QPoint globalPos = QCursor::pos();

        if (canResize() && (isTopResizeArea(globalPos) || isLeftResizeArea(globalPos) || isRightResizeArea(globalPos)))
        {
            m_resizingTop = isTopResizeArea(globalPos);
            m_resizingLeft = isLeftResizeArea(globalPos);
            m_resizingRight = isRightResizeArea(globalPos);
        }
        else if (canDragWindow())
        {
            QWidget *topLevel = window();
            auto topLevelWidth = topLevel->width();
            m_dragPos = topLevel->mapFromGlobal(globalPos);
            m_relativeDragPos = (double)m_dragPos.x() / ((double)topLevel->width());

            topLevelWidth++;
        }
        else
        {
            if (!isInFloatingDockWidget())
            {
                // Workaround Qt internal crash when detaching docked window group. Crashes if tracking enabled
                setMouseTracking(false);
                m_enableMouseTrackingTimer.start();
            }

            QWidget::mousePressEvent(ev);
        }
    }

    void TitleBar::mouseReleaseEvent(QMouseEvent* ev)
    {
        m_resizingTop = false;
        m_resizingLeft = false;
        m_resizingRight = false;
        m_pendingRepositioning = false;
        m_dragPos = QPoint();
        QWidget::mouseReleaseEvent(ev);
    }

    QWindow* TitleBar::topLevelWindow() const
    {
        if (QWidget* topLevelWidget = window())
        {
            // TitleBar can be native instead of alien so calling this->window() would return
            // an unrelated window.
            return topLevelWidget->windowHandle();
        }

        return nullptr;
    }

    bool TitleBar::isTopResizeArea(const QPoint &globalPos) const
    {
        if (window() != parentWidget() && !isInDockWidgetWindowGroup())
        {
            // The immediate parent of the TitleBar must be a top level
            // if it's not then we're docked and we're not interested in resizing the top.
            return false;
        }

        if (QWindow* topLevelWin = topLevelWindow())
        {
            QPoint pt = mapFromGlobal(globalPos);
            const bool fixedHeight = topLevelWin->maximumHeight() == topLevelWin->minimumHeight();
            const bool maximized = topLevelWin->windowState() == Qt::WindowMaximized;
            return !maximized && !fixedHeight && pt.y() < DockBar::ResizeTopMargin;
        }

        return false;
    }

    bool TitleBar::isLeftResizeArea(const QPoint& globalPos) const
    {
#ifdef Q_OS_MAC
        if (window() != parentWidget() && !isInDockWidgetWindowGroup())
        {
            // The immediate parent of the TitleBar must be a top level
            // if it's not then we're docked and we're not interested in resizing the top.
            return false;
        }

        if (QWindow* topLevelWin = topLevelWindow())
        {
            QPoint pt = mapFromGlobal(globalPos);
            const bool fixedWidth = topLevelWin->maximumWidth() == topLevelWin->minimumWidth();
            const bool maximized = topLevelWin->windowState() == Qt::WindowMaximized;
            return !maximized && !fixedWidth && pt.x() < DockBar::ResizeTopMargin;
        }

        return false;
#else
        Q_UNUSED(globalPos);
        return false;
#endif
    }

    bool TitleBar::isRightResizeArea(const QPoint& globalPos) const
    {
#ifdef Q_OS_MAC
        if (window() != parentWidget() && !isInDockWidgetWindowGroup())
        {
            // The immediate parent of the TitleBar must be a top level
            // if it's not then we're docked and we're not interested in resizing the top.
            return false;
        }

        if (QWindow* topLevelWin = topLevelWindow())
        {
            QPoint pt = mapFromGlobal(globalPos);
            const bool fixedWidth = topLevelWin->maximumWidth() == topLevelWin->minimumWidth();
            const bool maximized = topLevelWin->windowState() == Qt::WindowMaximized;
            return !maximized && !fixedWidth && pt.x() > width() - DockBar::ResizeTopMargin;
        }

        return false;
#else
        Q_UNUSED(globalPos);
        return false;
#endif
    }

    QRect TitleBar::draggableRect() const
    {
        // This is rect() - the button rect, so we can enable aero-snap dragging in that space
        QRect r = rect();
        r.setWidth(m_firstButton->x() - layout()->spacing());
        return r;
    }

    bool TitleBar::canResize() const
    {
        const QWidget *w = window();
        if (!w)
        {
            return false;
        }

        if (isInDockWidget() && !isInFloatingDockWidget())
        {
            // We return false for all embedded dock widgets
            return false;
        }

        return w && usesCustomTopBorderResizing() &&
            w->minimumHeight() < w->maximumHeight() && !w->isMaximized();
    }

    void TitleBar::updateMouseCursor(const QPoint& globalPos)
    {
        if (!usesCustomTopBorderResizing())
        {
            return;
        }

        bool usesResizeCursor = false;
        switch (cursor().shape()) {
        case Qt::SizeVerCursor:
        case Qt::SizeHorCursor:
        case Qt::SizeFDiagCursor:
        case Qt::SizeBDiagCursor:
            usesResizeCursor = true;
            break;
        default:
            usesResizeCursor = false;
            break;
        }

        if (!usesResizeCursor)
        {
            m_originalCursor = cursor().shape();
        }

        if (isTopResizeArea(globalPos) && isLeftResizeArea(globalPos))
        {
            setCursor(Qt::SizeFDiagCursor);
        }
        else if (isTopResizeArea(globalPos) && isRightResizeArea(globalPos))
        {
            setCursor(Qt::SizeBDiagCursor);
        }
        else if (isLeftResizeArea(globalPos) || isRightResizeArea(globalPos))
        {
            setCursor(Qt::SizeHorCursor);
        }
        else if (isTopResizeArea(globalPos))
        {
            setCursor(Qt::SizeVerCursor);
        }
        else
        {
            setCursor(m_originalCursor);
        }
    }

    void TitleBar::resizeWindow(const QPoint& globalPos)
    {
        QWindow *w = topLevelWindow();
        QRect geo = w->geometry();

        // maxGeo has all sides of the rectangle expanded as far as allowed
        const QRect maxGeo = QRect(QPoint(geo.right() - w->maximumWidth(),
                                          geo.bottom() - w->maximumHeight()),
                                   QPoint(geo.left() + w->maximumWidth(),
                                          geo.top() + w->maximumHeight()));
        // same for minGeo, we just have to take care that the size doesn't become "negative"
        const QRect minGeo = QRect(QPoint(m_resizingLeft ? geo.right() - w->minimumWidth() : geo.left(),
                                          m_resizingTop ? geo.bottom() - w->minimumHeight() : geo.top()),
                                   QPoint(m_resizingRight ? geo.left() + w->minimumWidth() : geo.right(),
                                          geo.bottom()));

        if (m_resizingTop)
        {
            geo.setTop(qBound(maxGeo.top(), globalPos.y(), minGeo.top()));
        }
        if (m_resizingLeft)
        {
            geo.setLeft(qBound(maxGeo.left(), globalPos.x(), minGeo.left()));
        }
        if (m_resizingRight)
        {
            geo.setRight(qBound(minGeo.right(), globalPos.x(), maxGeo.right()));
        }

        geo = geo.intersected(maxGeo);
        geo = geo.united(minGeo);

        if (geo != w->geometry())
        {
            w->setGeometry(geo);
        }
    }

    void TitleBar::dragWindow(const QPoint& globalPos)
    {
        QWidget* topLevel = window();
        if (!topLevel)
        {
            return;
        }

        if (isMaximized())
        {
            m_pendingRepositioning = true;

            // have to refresh this value, as the width might have changed
            m_lastLocalPosX = topLevel->mapFromGlobal(globalPos).x() / (topLevel->width() * 1.0);

            handleMaximize();
            return;
        }

        // Workaround QTBUG-47543
        if (QSysInfo::windowsVersion() == QSysInfo::WV_WINDOWS10)
        {
            update();
        }

        if (m_pendingRepositioning)
        {
            // This is the case when moving a maximized window, window is restored and placed at 0,0
            // or whatever is the top-left corner of your screen.
            // We try to maintain the titlebar at the same relative position as when you clicked it.
            // So, if you click on the beginning of a maximized titlebar, it will be restored and
            // your mouse continues to be at the beginning of the titlebar.
            // m_lastLocalPosX holds the place you clicked as a percentage of the width, because
            // width will be smaller when you restore.
            if (topLevel)
            {
                const int offset = static_cast<int>(m_lastLocalPosX) * width();
                topLevel->move(globalPos - QPoint(offset, 0));
                m_dragPos.setX(offset);
            }
        }
        else
        {
            // This is the normal case, we move the window
            QWindow* wind = topLevel->windowHandle();

            if (wind->width() < m_dragPos.x())
            {
                // The window was resized while we were dragging to a screen with different (dpi) scale factor
                // It shrunk, so calculate a new sensible drag pos, because the old is out of screen
                m_dragPos.setX(static_cast<int>(m_relativeDragPos) * wind->width());
            }

            // (Don't cache the margins, they are be different when moving to screens with different scale factors)
            const QPoint newPoint = globalPos - (m_dragPos + QPoint(wind->frameMargins().left(), wind->frameMargins().top()));
            topLevel->move(newPoint);
        }

        m_pendingRepositioning = false;
    }

    bool TitleBar::isTitleBarForDockWidget() const
    {
        return qobject_cast<StyledDockWidget*>(parent()) != nullptr;
    }

    void TitleBar::mouseMoveEvent(QMouseEvent* ev)
    {
        // use QCursor::pos(); in scenarios with multiple screens and different scale factors,
        // it's much more reliable about actually reporting a global position.
        QPoint globalPos = QCursor::pos();

        if (isResizingWindow())
        {
            resizeWindow(globalPos);
        }
        else if (isDraggingWindow())
        {
            dragWindow(globalPos);
        }
        else
        {
            updateMouseCursor(globalPos);
            QWidget::mouseMoveEvent(ev);
            m_pendingRepositioning = false;
        }
    }

    void TitleBar::mouseDoubleClickEvent(QMouseEvent*)
    {
        StyledDockWidget* dockWidgetParent = qobject_cast<StyledDockWidget*>(parentWidget());
        if (m_buttons.contains(DockBarButton::MaximizeButton) || (dockWidgetParent && dockWidgetParent->isFloating()))
        {
            handleMaximize();
        }
    }

    void TitleBar::timerEvent(QTimerEvent*)
    {
        fixEnabled();
    }

    bool TitleBar::isInDockWidgetWindowGroup() const
    {
        // DockWidgetGroupWindow is not exposed as a public API class from Qt, so we have to check for it
        // based on the className instead.
        return window() && strcmp(window()->metaObject()->className(), "QDockWidgetGroupWindow") == 0;
    }

    void TitleBar::fixEnabled()
    {
        StyledDockWidget* dockWidgetParent = qobject_cast<StyledDockWidget*>(parentWidget());
        QWidget* groupWindowParent = dockWidgetParent ? dockWidgetParent->parentWidget() : nullptr;

        // DockWidgetGroupWindow has issues. It renders the TitleBar when floating (and only when it's floating), but renders everything else over top of it.
        // In that case, the TitleBar still gets mouse clicks, so if it's under a menu bar, the menu bar doesn't work.
        // The following is a workaround
        bool shouldBeLowered = false;
        if (isInDockWidgetWindowGroup() && groupWindowParent)
        {
            shouldBeLowered = true;
        }

        // if we're in a group, our title bar should never be over top of anything else
        // But! we don't want to muck with the order in any other case, because it could screw something up.
        // So we only lower it ever.

        if (shouldBeLowered)
        {
            lower();
        }
    }

    /**
     * Handle button press signals from our DockBarButtons based on their type
     */
    void TitleBar::handleButtonClicked(const DockBarButton::WindowDecorationButton type)
    {
        switch (type)
        {
        case DockBarButton::CloseButton:
            handleClose();
            break;
        case DockBarButton::MinimizeButton:
            handleMinimize();
            break;
        case DockBarButton::MaximizeButton:
            handleMaximize();
            break;
        }
    }

    bool TitleBar::isMaximized() const
    {
        return window() && window()->isMaximized();
    }

    static QVector<DockBarButton::WindowDecorationButton> findDisabledButtons(QLayout* layout)
    {
        QVector<DockBarButton::WindowDecorationButton> disabledButtons;
        int itemsInLayout = layout->count();
        for (int i = 0; i < itemsInLayout; i++)
        {
            QLayoutItem* item = layout->itemAt(i);
            DockBarButton* dockBarButton = qobject_cast<DockBarButton*>(item->widget());
            if (dockBarButton && !dockBarButton->isEnabled())
            {
                disabledButtons.push_back(dockBarButton->buttonType());
            }
        }

        return disabledButtons;
    }

    void TitleBar::setupButtons(bool useDividerButtons /*= true */)
    {
        // Before we do anything else, figure out if any existing buttons were disabled.
        // Only trust items already in the layout.
        QVector<DockBarButton::WindowDecorationButton> disabledButtons = findDisabledButtons(m_buttonsLayout);
        
        // Remove the old buttons from our layout.
        const auto oldButtons = m_buttonsContainer->findChildren<QWidget*>();
        for (auto button : oldButtons)
        {
            button->hide();
            m_buttonsLayout->removeWidget(button);

            // Use QObject::deleteLater to make this function safe to call whilst the application is
            // being polished/unpolished.
            button->deleteLater();
        }

        m_firstButton = nullptr;

        for (auto buttonType : m_buttons)
        {
            QWidget* w = nullptr;
            if (buttonType == DockBarButton::DividerButton)
            {
                if (!useDividerButtons)
                {
                    continue;
                }
                w = new ButtonDivider(m_buttonsContainer);
            }
            else
            {
                // Use the dark style of buttons for the titlebars on floating
                // dock widget containers since they are a lighter color
                StyledDockWidget* dockWidgetParent = qobject_cast<StyledDockWidget*>(parentWidget());
                bool isDarkStyle = dockWidgetParent && dockWidgetParent->isFloating();

                DockBarButton* button = new DockBarButton(buttonType, m_buttonsContainer, isDarkStyle);
                QObject::connect(button, &DockBarButton::buttonPressed, this, &TitleBar::handleButtonClicked);
                w = button;

                if (disabledButtons.contains(buttonType))
                {
                    button->setDisabled(true);
                }
            }

            if (!m_firstButton)
            {
                m_firstButton = w;
            }

            m_buttonsLayout->addWidget(w);
        }
    }

    bool TitleBar::isLeftButtonDown() const
    {
        return (QApplication::mouseButtons() & Qt::LeftButton) != 0;
    }

    bool TitleBar::canDragWindow() const
    {
        // Dock widgets use the internal Qt drag implementation
        return m_dragEnabled;
    }

    /**
     * Helper function to determine if we are currently resizing our title bar
     * from the top of our widget
     */
    bool TitleBar::isResizingWindow() const
    {
        return isLeftButtonDown() && (m_resizingTop || m_resizingLeft || m_resizingRight) && canResize();
    }

    /**
     * Helper function to determine if we are currently in the state of click+dragging
     * our title bar to reposition it
     */
    bool TitleBar::isDraggingWindow() const
    {
        return isLeftButtonDown() && !m_dragPos.isNull() && canDragWindow();
    }

    void TitleBar::setButtons(WindowDecorationButtons buttons)
    {
        if (buttons != m_buttons)
        {
            m_buttons = buttons;
            setupButtons();
        }
    }

    int TitleBar::numButtons() const
    {
        return m_buttons.size();
    }

    void TitleBar::setForceInactive(bool force)
    {
        if (m_forceInactive != force)
        {
            m_forceInactive = force;
            update();
            emit forceInactiveChanged(m_forceInactive);
        }
    }

#include <Components/Titlebar.moc>
} // namespace AzQtComponents

