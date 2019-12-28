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

#include <QCoreApplication>
#include <QFont>
#include <QGraphicsItem>
#include <QGraphicsSceneEvent>
#include <QPainter>
#include <qwidget.h>

#include <AzCore/Serialization/EditContext.h>

#include <Widgets/GraphCanvasLabel.h>

#include <GraphCanvas/Editor/GraphCanvasProfiler.h>
#include <GraphCanvas/tools.h>
#include <GraphCanvas/Styling/StyleHelper.h>

namespace GraphCanvas
{
    /////////////////////
    // GraphCanvasLabel
    /////////////////////

    GraphCanvasLabel::GraphCanvasLabel(QGraphicsItem* parent)
        : QGraphicsWidget(parent)
        , m_defaultAlignment(Qt::AlignVCenter | Qt::AlignHCenter)
        , m_elide(true)
        , m_wrap(false)
        , m_maximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX)
        , m_minimumSize(0,0)
        , m_wrapMode(WrapMode::MaximumWidth)
    {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        setGraphicsItem(this);
        setFlag(ItemIsMovable, false);
    }

    void GraphCanvasLabel::SetFontColor(const QColor& color)
    {
        m_styleHelper.AddAttributeOverride(Styling::Attribute::Color, color);

        update();
    }

    void GraphCanvasLabel::ClearFontColor()
    {
        m_styleHelper.RemoveAttributeOverride(Styling::Attribute::Color);

        update();
    }

    void GraphCanvasLabel::SetLabel(const AZStd::string& label, const AZStd::string& translationContext, const AZStd::string& translationKey)
    {
        TranslationKeyedString keyedString(label, translationContext, translationKey);
        SetLabel(keyedString);
    }

    void GraphCanvasLabel::SetLabel(const TranslationKeyedString& value)
    {
        AZStd::string displayString = value.GetDisplayString();

        if (m_labelText.compare(QString(displayString.c_str())))
        {
            m_labelText = Tools::qStringFromUtf8(displayString);

            UpdateDisplayText();
            RefreshDisplay();
        }
    }

    void GraphCanvasLabel::SetSceneStyle(const AZ::EntityId& sceneId, const char* style)
    {
        m_styleHelper.SetScene(sceneId);
        m_styleHelper.SetStyle(style);
        UpdateDisplayText();
        RefreshDisplay();
    }

    void GraphCanvasLabel::SetStyle(const AZ::EntityId& entityId, const char* styleElement)
    {
        m_styleHelper.SetStyle(entityId, styleElement);
        UpdateDisplayText();
        RefreshDisplay();
    }

    void GraphCanvasLabel::RefreshDisplay()
    {
        UpdateDesiredBounds();
        updateGeometry();
        update();
    }

    void GraphCanvasLabel::SetWrapMode(WrapMode wrapMode)
    {
        if (m_wrapMode != wrapMode)
        {
            m_wrapMode = wrapMode;

            UpdateDesiredBounds();
            UpdateDisplayText();
            RefreshDisplay();
        }
    }

    QRectF GraphCanvasLabel::GetDisplayedSize() const
    {
        return m_displayedSize;
    }

    void GraphCanvasLabel::SetElide(bool elide)
    {
        if (m_elide != elide)
        {
            m_elide = elide;
            RefreshDisplay();
        }
    }

    void GraphCanvasLabel::SetWrap(bool wrap)
    {
        if (m_wrap != wrap)
        {
            m_wrap = wrap;
            RefreshDisplay();
        }
    }

    void GraphCanvasLabel::SetDefaultAlignment(Qt::Alignment defaultAlignment)
    {
        m_defaultAlignment = defaultAlignment;
        update();
    }

    Styling::StyleHelper& GraphCanvasLabel::GetStyleHelper()
    {
        return m_styleHelper;
    }

    const Styling::StyleHelper& GraphCanvasLabel::GetStyleHelper() const
    {
        return m_styleHelper;
    }

    bool GraphCanvasLabel::event(QEvent* qEvent)
    {
        if (qEvent->type() == QEvent::GraphicsSceneResize)
        {
            UpdateDisplayText();
        }

        return QGraphicsWidget::event(qEvent);
    }

    void GraphCanvasLabel::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget /*= nullptr*/)
    {
        GRAPH_CANVAS_DETAILED_PROFILE_FUNCTION();
        AZ_Warning("GraphCanvasLabel", !(m_elide && m_wrap), "GraphCanvasLabel doesn't support eliding text and word wrapping at the same time.");

        painter->save();

        // Background
        {
            qreal borderRadius = m_styleHelper.GetAttribute(Styling::Attribute::BorderRadius, 0);

            m_displayedSize = boundingRect();

            if (borderRadius == 0)
            {
                painter->fillRect(m_displayedSize, m_styleHelper.GetBrush(Styling::Attribute::BackgroundColor));

                if (m_styleHelper.HasAttribute(Styling::Attribute::BorderWidth))
                {
                    QPen restorePen = painter->pen();
                    painter->setPen(m_styleHelper.GetBorder());
                    painter->drawRect(m_displayedSize);
                    painter->setPen(restorePen);
                }
            }
            else
            {
                QPainterPath path;
                path.addRoundedRect(m_displayedSize, borderRadius, borderRadius);
                painter->fillPath(path, m_styleHelper.GetBrush(Styling::Attribute::BackgroundColor));

                if (m_styleHelper.HasAttribute(Styling::Attribute::BorderWidth))
                {
                    QPen restorePen = painter->pen();
                    painter->setPen(m_styleHelper.GetBorder());
                    painter->drawPath(path);
                    painter->setPen(restorePen);
                }
            }
        }

        // Text.
        if (!m_labelText.isEmpty())
        {
            qreal padding = m_styleHelper.GetAttribute(Styling::Attribute::Padding, 2.0f);

            QRectF innerBounds = m_displayedSize;
            innerBounds = innerBounds.adjusted(padding, padding, -padding, -padding);

            painter->setPen(m_styleHelper.GetColor(Styling::Attribute::Color));
            painter->setBrush(QBrush());
            painter->setFont(m_styleHelper.GetFont());

            Qt::Alignment textAlignment = m_styleHelper.GetTextAlignment(m_defaultAlignment);

            QTextOption opt(textAlignment);
            opt.setFlags(QTextOption::IncludeTrailingSpaces);

            if (m_wrap)
            {
                opt.setWrapMode(QTextOption::WordWrap);
            }
            else
            {
                opt.setWrapMode(QTextOption::NoWrap);
            }

            painter->drawText(innerBounds, m_displayText, opt);
        }

        painter->restore();

        QGraphicsWidget::paint(painter, option, widget);
    }

    QSizeF GraphCanvasLabel::sizeHint(Qt::SizeHint which, const QSizeF& constraint /*= QSizeF()*/) const
    {
        switch (which)
        {
        case Qt::MinimumSize:
            return m_minimumSize;
        case Qt::PreferredSize:
            return m_desiredBounds.size();
        case Qt::MaximumSize:
            return m_maximumSize;
        default:
            return QGraphicsWidget::sizeHint(which, constraint);
        }

        return QGraphicsWidget::sizeHint(which, constraint);
    }

    void GraphCanvasLabel::UpdateDisplayText()
    {
        qreal padding = m_styleHelper.GetAttribute(Styling::Attribute::Padding, 2.0f);
        QFontMetrics metrics(m_styleHelper.GetFont());

        QRectF innerBounds = boundingRect();
        innerBounds = innerBounds.adjusted(padding, padding, -padding, -padding);

        if (m_elide)
        {
            QStringList newlines = m_labelText.split('\n');

            m_displayText.clear();

            bool needsNewline = false;

            for (const QString& currentLine : newlines)
            {
                if (needsNewline)
                {
                    m_displayText.append('\n');
                }

                m_displayText = m_displayText.append(metrics.elidedText(currentLine, Qt::TextElideMode::ElideRight, innerBounds.width()));
                needsNewline = true;
            }
        }
        else
        {
            m_displayText = m_labelText;
        }
    }

    void GraphCanvasLabel::UpdateDesiredBounds()
    {
        prepareGeometryChange();
        qreal padding = m_styleHelper.GetAttribute(Styling::Attribute::Padding, 2.0f);

        QFontMetricsF metrics = QFontMetricsF(m_styleHelper.GetFont());

        int flags = m_defaultAlignment;
        if (m_wrap)
        {
            flags = flags | Qt::TextWordWrap;
        }

        flags = flags & ~Qt::TextSingleLine;

        m_maximumSize = m_styleHelper.GetMaximumSize();

        QSizeF sizeClamp = maximumSize();

        if (m_wrapMode == WrapMode::BoundingWidth)
        {
            sizeClamp.setWidth(boundingRect().size().width());
        }

        QRectF maxInnerBounds(rect().topLeft(), sizeClamp);
        maxInnerBounds = maxInnerBounds.adjusted(padding, padding, -padding, -padding);

        // Horizontal Padding:
        // 1 padding for the left side
        // 1 padding for the right side
        //
        // Vertical Padding:
        // 1 padding for the top
        // 1 padding for the bottom.
        m_desiredBounds = metrics.boundingRect(maxInnerBounds, flags, m_labelText).adjusted(0.0f, 0.0f, padding * 2.0f, padding * 2.0f);
        
        // Seem so be an off by 1 error here. So adding one in each direction to my desired size to stop text from being clipped.
        m_desiredBounds.adjust(-1.0, -1.0, 1.0, 1.0);

        m_minimumSize = m_styleHelper.GetMinimumSize(m_desiredBounds.size());

        updateGeometry();
        setCacheMode(QGraphicsItem::CacheMode::DeviceCoordinateCache);
    }
}
