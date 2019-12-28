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

#include "AnnotationsHeaderView_Events.hxx"
#include "AnnotationsDataView_Events.hxx"
#include "Annotations.hxx"
#include <QtWidgets/QGridLayout>

namespace Driller
{
    static const int k_contractedSize = 18;
    static const int k_textWidth = 128;

    AnnotationHeaderView_Events::AnnotationHeaderView_Events(QWidget* parent, Qt::WindowFlags flags)
        : QWidget(parent, flags)
        , m_ptrAnnotations(NULL)
    {
        this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        this->setFixedHeight(k_contractedSize);
        this->setAutoFillBackground(true);

        QHBoxLayout* mainLayout = new QHBoxLayout(this);
        this->setLayout(mainLayout);

        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
    }

    void AnnotationHeaderView_Events::OnScrubberFrameUpdate(FrameNumberType newFrame)
    {
        if (m_ptrDataView)
        {
            m_ptrDataView->OnScrubberFrameUpdate(newFrame);
        }
    }

    QSize AnnotationHeaderView_Events::sizeHint() const
    {
        return QSize(0, k_contractedSize);
    }

    void AnnotationHeaderView_Events::ControllerSizeChanged(QSize newSize)
    {
        (void)newSize;
    }

    AnnotationHeaderView_Events::~AnnotationHeaderView_Events()
    {
    }

    void AnnotationHeaderView_Events::AttachToAxis(AnnotationsProvider* ptrAnnotations, Charts::Axis* target)
    {
        m_ptrAnnotations = ptrAnnotations;
        m_ptrDataView = aznew AnnotationsDataView_Events(this, ptrAnnotations);

        connect(m_ptrDataView, SIGNAL(InformOfMouseOverAnnotation(const Annotation&)), this, SIGNAL(InformOfMouseOverAnnotation(const Annotation&)));
        connect(m_ptrDataView, SIGNAL(InformOfClickAnnotation(const Annotation&)), this, SIGNAL(InformOfClickAnnotation(const Annotation&)));
        connect(m_ptrAnnotations, SIGNAL(AnnotationDataInvalidated()), this, SLOT(RefreshView()));

        layout()->addWidget(m_ptrDataView);
        m_ptrDataView->AttachToAxis(target);
    }

    void AnnotationHeaderView_Events::RefreshView()
    {
        m_ptrDataView->update();
    }
}

#include <Woodpecker/Driller/Annotations/AnnotationsHeaderView_Events.moc>