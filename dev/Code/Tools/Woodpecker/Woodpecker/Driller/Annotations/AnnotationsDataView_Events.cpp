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

#include "stdafx.h"

#include "AnnotationsDataView_Events.hxx"
#include "AnnotationsHeaderView_Events.hxx"
#include "Annotations.hxx"
#include <Woodpecker/Driller/Axis.hxx>

namespace Driller
{
    static const float arrow_width = 8.0f;

    AnnotationsDataView_Events::AnnotationsDataView_Events(AnnotationHeaderView_Events* header, AnnotationsProvider* annotations)
        : QWidget(header)
        , m_ptrHeaderView(header)
        , m_ptrAnnotations(annotations)
        , m_ptrAxis(NULL)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setFixedHeight(18);
        setAutoFillBackground(false);
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        setMouseTracking(true);
        m_CurrentFrameNumber = 0;
    }

    void AnnotationsDataView_Events::AttachToAxis(Charts::Axis* pAxis)
    {
        if (m_ptrAxis)
        {
            disconnect(m_ptrAxis, SIGNAL(destroyed(QObject*)), this, SLOT(OnAxisDestroyed()));
            disconnect(m_ptrAxis, SIGNAL(Invalidated()), this, SLOT(OnAxisInvalidated()));
        }

        m_ptrAxis = pAxis;
        if (pAxis)
        {
            connect(m_ptrAxis, SIGNAL(destroyed(QObject*)), this, SLOT(OnAxisDestroyed()));
            connect(m_ptrAxis, SIGNAL(Invalidated()), this, SLOT(OnAxisInvalidated()));
        }
    }

    void AnnotationsDataView_Events::OnAxisDestroyed()
    {
        m_ptrAxis = NULL;
        update();
    }

    void AnnotationsDataView_Events::OnAxisInvalidated()
    {
        update();
    }

    AnnotationsDataView_Events::~AnnotationsDataView_Events()
    {
    }

    void AnnotationsDataView_Events::paintEvent(QPaintEvent* event)
    {
        (void)event;

        m_ClickableAreas.clear();

        // scan for annotations



        // fill with black
        QPainter painter(this);
        painter.fillRect(rect(), Qt::black);

        if (!m_ptrAxis)
        {
            return;
        }

        if (!m_ptrAxis->GetValid())
        {
            return;
        }

        QRectF drawRange = rect();
        // adjust for inset:

        drawRange.adjust(2.0f, 0.0f, -4.0f, 0.0f);
        float leftEdge = (float)drawRange.left();
        float drawRangeWidth = (float)drawRange.width();

        AZ::s64 eventIndexStart = (AZ::s64)m_ptrAxis->GetWindowMin();
        AZ::s64 eventIndexEnd = (AZ::s64)m_ptrAxis->GetWindowMax() + 1;
        float eventIndexRange = ((float)m_ptrAxis->GetWindowMax() - (float)m_ptrAxis->GetWindowMin()); // this is the domain range


        if (eventIndexRange <= 0.0f)
        {
            return;
        }

        float oneEventWidthInPixels = drawRangeWidth / eventIndexRange;
        float halfEventWidth = oneEventWidthInPixels * 0.5f;

        // find the first event within that range:
        QPen fatPen(QColor(255, 255, 255, 255));
        fatPen.setWidth(2);
        fatPen.setCapStyle(Qt::FlatCap);

        AnnotationsProvider::ConstAnnotationIterator it = m_ptrAnnotations->GetFirstAnnotationForFrame(m_CurrentFrameNumber);
        AnnotationsProvider::ConstAnnotationIterator endIt = m_ptrAnnotations->GetEnd();

        // now keep going until we hit the end of the range:
        while (it != endIt)
        {
            if (it->GetEventIndex() >= eventIndexEnd)
            {
                break;
            }

            if (it->GetEventIndex() < eventIndexStart)
            {
                ++it;
                continue; // we're within the zoom
            }

            // transform that event ID into the window domain:


            float eventRatio = ((float)it->GetEventIndex() - m_ptrAxis->GetWindowMin()) / eventIndexRange;

            float center = floorf(leftEdge + (drawRangeWidth * eventRatio));

            center += (float)drawRange.left();
            center += halfEventWidth;

            QPainterPath newPath;
            QPolygonF newPolygon;
            newPolygon << QPointF(center - arrow_width, 1.0f) << QPointF(center, drawRange.height() - 1.0f) << QPointF(center + arrow_width, 1.0f);
            newPath.addPolygon(newPolygon);
            newPath.closeSubpath();

            if (m_eventsToHighlight.find(it->GetEventIndex()) != m_eventsToHighlight.end())
            {
                painter.setPen(fatPen);
                painter.setBrush(m_ptrAnnotations->GetColorForChannel(it->GetChannelCRC()));
            }
            else
            {
                painter.setPen(QColor(0, 0, 0, 0));
                painter.setBrush(m_ptrAnnotations->GetColorForChannel(it->GetChannelCRC()));
            }
            painter.drawPath(newPath);
            m_ClickableAreas[it->GetEventIndex()] = newPath;
            ++it;
        }
    }

    void AnnotationsDataView_Events::mouseMoveEvent(QMouseEvent* event)
    {
        AZStd::unordered_set<AZ::s64> newEventsToHighlight;

        for (auto it = m_ClickableAreas.begin(); it != m_ClickableAreas.end(); ++it)
        {
            if (it->second.contains(event->pos()))
            {
                auto annot = m_ptrAnnotations->GetAnnotationForEvent(it->first);
                if (annot != m_ptrAnnotations->GetEnd())
                {
                    newEventsToHighlight.insert(annot->GetEventIndex());
                    emit InformOfMouseOverAnnotation(*annot);
                }
            }
        }

        bool doUpdate = false;
        // did our highlight change?
        for (auto it = newEventsToHighlight.begin(); it != newEventsToHighlight.end(); ++it)
        {
            if (m_eventsToHighlight.find(*it) == m_eventsToHighlight.end())
            {
                doUpdate = true;
                break;
            }
        }

        // did our highlight change?
        if (!doUpdate)
        {
            for (auto it = m_eventsToHighlight.begin(); it != m_eventsToHighlight.end(); ++it)
            {
                if (newEventsToHighlight.find(*it) == newEventsToHighlight.end())
                {
                    doUpdate = true;
                    break;
                }
            }
        }

        if (doUpdate)
        {
            newEventsToHighlight.swap(m_eventsToHighlight);
            update();
        }

        // find the first annotation within a margin:
        event->ignore();
    }

    void AnnotationsDataView_Events::mousePressEvent(QMouseEvent* event)
    {
        for (auto it = m_ClickableAreas.begin(); it != m_ClickableAreas.end(); ++it)
        {
            if (it->second.contains(event->pos()))
            {
                auto annot = m_ptrAnnotations->GetAnnotationForEvent(it->first);
                if (annot != m_ptrAnnotations->GetEnd())
                {
                    emit InformOfClickAnnotation(*annot);
                }
            }
        }
        event->ignore();
    }


    void AnnotationsDataView_Events::mouseReleaseEvent(QMouseEvent* event)
    {
        event->ignore();
    }

    void AnnotationsDataView_Events::OnScrubberFrameUpdate(FrameNumberType newFramenumber)
    {
        if (newFramenumber != m_CurrentFrameNumber)
        {
            m_CurrentFrameNumber = newFramenumber;
            // we don't update here because we wait for the new range to be set
            //update();
        }
    }
}

#include <Woodpecker/Driller/Annotations/AnnotationsDataView_Events.moc>
