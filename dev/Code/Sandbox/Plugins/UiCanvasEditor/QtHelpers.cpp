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

#include "EditorCommon.h"

namespace QtHelpers
{
    AZ::Vector2 QPointFToVector2(const QPointF& point)
    {
        return AZ::Vector2(point.x(), point.y());
    }

    AZ::Vector2 MapGlobalPosToLocalVector2(const QWidget* widget, const QPoint& pos)
    {
        QPoint localPos = widget->mapFromGlobal(pos);
        return QPointFToVector2(localPos);
    }

    bool IsGlobalPosInWidget(const QWidget* widget, const QPoint& pos)
    {
        QPoint localPos = widget->mapFromGlobal(pos);
        const QSize& size = widget->size();
        bool inWidget = (localPos.x() >= 0 && localPos.x() < size.width() && localPos.y() >= 0 && localPos.y() < size.height());
        return inWidget;
    }

}   // namespace QtHelpers
