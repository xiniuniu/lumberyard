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

#include "UnsupportedDataAggregator.hxx"
#include <Woodpecker/Driller/Unsupported/UnsupportedDataAggregator.moc>

#include "UnsupportedEvents.h"

#include <AzCore/std/containers/queue.h>

namespace Driller
{
    UnsupportedDataAggregator::UnsupportedDataAggregator(AZ::u32 drillerId)
        : m_parser(drillerId)
        , Aggregator(0)
    {
        m_parser.SetAggregator(this);
    }

    float UnsupportedDataAggregator::ValueAtFrame(FrameNumberType frame)
    {
        const float maxEventsPerFrame = 500.0f;
        float numEventsPerFrame = static_cast<float>(NumOfEventsAtFrame(frame));
        return AZStd::GetMin(numEventsPerFrame / maxEventsPerFrame, 1.0f) * 2.0f - 1.0f;
    }

    QColor UnsupportedDataAggregator::GetColor() const
    {
        return QColor(40, 40, 40);
    }

    QString UnsupportedDataAggregator::GetName() const
    {
        char buf[64];
        azsnprintf(buf, AZ_ARRAY_SIZE(buf), "Id: 0x%08x", m_parser.GetDrillerId());
        return buf;
    }

    QString UnsupportedDataAggregator::GetChannelName() const
    {
        return ChannelName();
    }

    QString UnsupportedDataAggregator::GetDescription() const
    {
        return QString("Unsupported driller");
    }

    QString UnsupportedDataAggregator::GetToolTip() const
    {
        return QString("Unknown Driller");
    }

    AZ::Uuid UnsupportedDataAggregator::GetID() const
    {
        return AZ::Uuid("{368D6FB2-9A92-4DFE-8DB4-4F106194BA6F}");
    }

    QWidget* UnsupportedDataAggregator::DrillDownRequest(FrameNumberType frame)
    {
        (void)frame;
        return NULL;
    }
    void UnsupportedDataAggregator::OptionsRequest()
    {
    }
} // namespace Driller
