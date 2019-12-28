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

#include <AzQtComponents/Components/AutoCustomWindowDecorations.h>
#include <AzQtComponents/Components/WindowDecorationWrapper.h>

#include <QWidget>
#include <QFileDialog>
#include <QColorDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QDockWidget>
#include <QApplication>

#if AZ_TRAIT_OS_PLATFORM_APPLE
# include <QMacNativeWidget>
#endif

using namespace AzQtComponents;

static bool widgetHasCustomWindowDecorations(const QWidget* w)
{
    if (!w)
    {
        return false;
    }

    auto wrapper = qobject_cast<WindowDecorationWrapper*>(w->parentWidget());
    if (!wrapper)
    {
        return false;
    }

    // Simply having a decoration wrapper parent doesn't mean the widget has decorations.
    return wrapper->guest() == w;
}

static bool isQWinWidget(const QWidget* w)
{
    // We can't include the QWinWidget header from AzQtComponents, so use metaobject.
    const QMetaObject* mo = w->metaObject()->superClass();
    return mo && (strcmp(mo->className(), "QWinWidget") == 0);
}

static bool widgetShouldHaveCustomDecorations(const QWidget* w, AutoCustomWindowDecorations::Mode mode)
{
    if (!w || qobject_cast<const WindowDecorationWrapper*>(w) ||
        qobject_cast<const QDockWidget*>(w) ||
        qobject_cast<const QFileDialog*>(w) || // QFileDialog is native
#if AZ_TRAIT_OS_PLATFORM_APPLE
        (qobject_cast<const QColorDialog*>(w) && !qobject_cast<const QColorDialog*>(w)->testOption(QColorDialog::DontUseNativeDialog)) || // QColorDialog might be native on macOS
        qobject_cast<const QMacNativeWidget*>(w) ||
#endif
        w->property("HasNoWindowDecorations").toBool() || // Allows decorations to be disabled
        isQWinWidget(w))
    {
        // If wrapper itself, don't recurse.
        // If QDockWidget then also return false, they are styled with QDockWidget::setTitleBarWidget() instead.
        return false;
    }

    if (!(w->windowFlags() & Qt::Window))
    {
        return false;
    }

    if ((w->windowFlags() & Qt::Popup) == Qt::Popup || (w->windowFlags() & Qt::FramelessWindowHint))
    {
        return false;
    }

    if (mode == AutoCustomWindowDecorations::Mode_None)
    {
        return false;
    }
    else if (mode == AutoCustomWindowDecorations::Mode_AnyWindow)
    {
        return true;
    }
    else if (mode == AutoCustomWindowDecorations::Mode_Whitelisted)
    {
        // Don't put QDockWidget here, it uses QDockWidget::setTitleBarWidget() instead.
        return qobject_cast<const QMessageBox*>(w) || qobject_cast<const QInputDialog*>(w);
    }

    return false;
}

AutoCustomWindowDecorations::AutoCustomWindowDecorations(QObject* parent)
    : QObject(parent)
{
    qApp->installEventFilter(this);
}

void AutoCustomWindowDecorations::ensureCustomWindowDecorations(QWidget* w)
{
    if (widgetShouldHaveCustomDecorations(w, m_mode) && !widgetHasCustomWindowDecorations(w))
    {
        auto wrapper = new WindowDecorationWrapper(WindowDecorationWrapper::OptionAutoAttach |
                WindowDecorationWrapper::OptionAutoTitleBarButtons, w->parentWidget());

        w->setParent(wrapper, w->windowFlags());
    }
}

void AutoCustomWindowDecorations::setMode(AutoCustomWindowDecorations::Mode mode)
{
    m_mode = mode;
}

bool AutoCustomWindowDecorations::eventFilter(QObject* watched, QEvent* ev)
{
    if (ev->type() == QEvent::Show)
    {
        if (auto w = qobject_cast<QWidget*>(watched))
        {
            if (strcmp(w->metaObject()->className(), "QDockWidgetGroupWindow") != 0)
            {
                ensureCustomWindowDecorations(w);
            }
        }
    }

    return QObject::eventFilter(watched, ev);
}

