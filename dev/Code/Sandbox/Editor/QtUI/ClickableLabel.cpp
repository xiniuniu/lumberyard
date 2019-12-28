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
#include "StdAfx.h"
#include "QtUI/ClickableLabel.h"

#include <QApplication>
#include <QMouseEvent>

ClickableLabel::ClickableLabel(const QString& text, QWidget* parent)
    : QLabel(parent)
    , m_text(text)
    , m_showDecoration(false)
{
    setTextFormat(Qt::RichText);
    setTextInteractionFlags(Qt::TextBrowserInteraction);
}

ClickableLabel::ClickableLabel(QWidget* parent)
    : QLabel(parent)
    , m_showDecoration(false)
{
    setTextFormat(Qt::RichText);
    setTextInteractionFlags(Qt::TextBrowserInteraction);
}

void ClickableLabel::showEvent(QShowEvent* event)
{
    updateFormatting(false);
}

void ClickableLabel::enterEvent(QEvent* ev)
{
    if (!isEnabled())
    {
        return;
    }

    updateFormatting(true);
    QApplication::setOverrideCursor(QCursor(Qt::PointingHandCursor));
    QLabel::enterEvent(ev);
}

void ClickableLabel::leaveEvent(QEvent* ev)
{
    if (!isEnabled())
    {
        return;
    }

    updateFormatting(false);
    QApplication::restoreOverrideCursor();
    QLabel::leaveEvent(ev);
}

void ClickableLabel::setText(const QString& text)
{
    m_text = text;
    QLabel::setText(text);
    updateFormatting(false);
}

void ClickableLabel::setShowDecoration(bool b)
{
    m_showDecoration = b;
    updateFormatting(false);
}

void ClickableLabel::updateFormatting(bool mouseOver)
{
    //FIXME: this should be done differently. Using a style sheet would be easiest.

    QColor c = palette().color(QPalette::WindowText);
    if (mouseOver || m_showDecoration)
    {
        QLabel::setText(QString(R"(<a href="dummy" style="color: %1";>%2</a>)").arg(c.name(), m_text));
    }
    else
    {
        QLabel::setText(m_text);
    }
}

bool ClickableLabel::event(QEvent* e)
{
    if (isEnabled())
    {
        if (e->type() == QEvent::MouseButtonDblClick)
        {
            emit linkActivated(QString());
            return true; //ignore
        }
    }

    return QLabel::event(e);
}

#include <QtUI/ClickableLabel.moc>
