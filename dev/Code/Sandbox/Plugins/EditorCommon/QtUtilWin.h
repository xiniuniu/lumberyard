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

#include <QRect>
#include <QColor>
#include <QPixmap>
#include <QString>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <QWidget>
#include <QWindow>
#include <QApplication>

#ifdef Q_OS_WIN
# include <QtWinExtras/QtWin>
# include <QtGui/qpa/qplatformnativeinterface.h>
# include <objidl.h>
# include <GdiPlus.h>
#endif // Q_OS_WIN

#include "Util/EditorUtils.h"

namespace QtUtil
{

#ifdef Q_OS_WIN
    inline HWND getNativeHandle(QWidget* widget)
    {
        QWindow* window = widget->windowHandle();
        QPlatformNativeInterface* nativeInterface = QGuiApplication::platformNativeInterface();
        return static_cast<HWND>(nativeInterface->nativeResourceForWindow(QByteArrayLiteral("handle"), window));
    }
#endif // Q_OS_WIN

    //! a helper class which captures the HWND of a given widget for the duration of its life cycle.
    //! used mainly to set the parent of popup dialogs like file dialogs which are still MFC classes.
    class QtMFCScopedHWNDCapture
    {
    public:
        QtMFCScopedHWNDCapture(QWidget* source = nullptr)
        {
            if (!source)
            {
                source = qApp->activeWindow();
            }

            if (source)
            {
                m_widget = source;
            }
        }

        ~QtMFCScopedHWNDCapture()
        {
            m_widget = nullptr;
            m_attached = false;
        }

        // this is here so that it will also work for widgets that need parents if we upgrade the file dialog and other dialogs to be widget based
        operator QWidget*()
        {
            return m_widget;
        }

    private:
        bool m_attached = false;
        QWidget* m_widget = nullptr;
    };
}
