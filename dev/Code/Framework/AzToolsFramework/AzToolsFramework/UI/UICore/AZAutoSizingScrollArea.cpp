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

#include "AZAutoSizingScrollArea.hxx"

#include <qscrollbar.h>

namespace AzToolsFramework
{

    AZAutoSizingScrollArea::AZAutoSizingScrollArea(QWidget* parent)
        : QScrollArea(parent)        
    {    
    }

    // this code was copied from the regular implementation of the same function in QScrollArea, but converted
    // the private calls to public calls and removed the cache.
    QSize AZAutoSizingScrollArea::sizeHint() const
    {
        int initialSize = 2 * frameWidth();
        QSize sizeHint(initialSize, initialSize);
        
        if (widget())
        {
            sizeHint += this->widgetResizable() ? widget()->sizeHint() : widget()->size();
        }
        else
        {
            // If we don't have a widget, we want to reserve some space visually for ourselves.            
            int fontHeight = fontMetrics().height();
            sizeHint += QSize(2 * fontHeight, 2 * fontHeight);
        }

        if (verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOn)
        {
            sizeHint.setWidth(sizeHint.width() + verticalScrollBar()->sizeHint().width());
        }

        if (horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOn)
        {
            sizeHint.setHeight(sizeHint.height() + horizontalScrollBar()->sizeHint().height());
        }

        return sizeHint;
    }
}

#include <UI/UICore/AZAutoSizingScrollArea.moc>
