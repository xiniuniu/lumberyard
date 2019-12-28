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
#include <GraphCanvas/Utils/QtDrawingUtils.h>

#include <GraphCanvas/Components/StyleBus.h>
#include <GraphCanvas/Styling/StyleHelper.h>

namespace GraphCanvas
{
    ///////////////////
    // QtDrawingUtils
    ///////////////////
    void QtDrawingUtils::FillArea(QPainter& painter, const QRectF& drawArea, const Styling::StyleHelper& styleHelper)
    {
        Styling::PaletteStyle paletteStyle = styleHelper.GetAttribute(Styling::Attribute::PaletteStyle, Styling::PaletteStyle::Solid);
        QBrush background = styleHelper.GetBrush(Styling::Attribute::BackgroundColor);

        painter.fillRect(drawArea, background);

        if (paletteStyle == Styling::PaletteStyle::CandyStripe)
        {
            CandyStripeConfiguration configuration = styleHelper.GetCandyStripeConfiguration();
            CandyStripeArea(painter, drawArea, configuration);
        }
        else if (paletteStyle == Styling::PaletteStyle::PatternFill)
        {
            PatternedFillGenerator generator = styleHelper.GetPatternedFillGenerator();
            PatternFillArea(painter, drawArea, generator);
        }
    }

    void QtDrawingUtils::CandyStripeArea(QPainter& painter, const QRectF& drawArea, const CandyStripeConfiguration& candyStripeConfiguration)
    {
        int width = drawArea.width();
        int height = drawArea.height();

        int stripeSize = width / candyStripeConfiguration.m_minStripes;

        if (stripeSize >= candyStripeConfiguration.m_maximumSize)
        {
            stripeSize = candyStripeConfiguration.m_maximumSize;
        }

        int complimentAngle = abs(candyStripeConfiguration.m_stripeAngle);

        int skewOffset = height * tan(AZ::DegToRad(complimentAngle));

        if (candyStripeConfiguration.m_stripeAngle < 0)
        {
            skewOffset = -skewOffset;
        }
        
        // Create the template points for our pin stripe
        QPointF backPointBottom(0, height);
        QPointF backPointTop(skewOffset, 0);

        QPointF forwardPointBottom = backPointBottom + QPointF(stripeSize, 0);
        QPointF forwardPointTop = backPointTop + QPointF(stripeSize, 0);

        int totalStripeStep = forwardPointTop.x() - backPointBottom.x();

        // Starting with an offset of -0.5 * totalStripeStep just to make it look reasonable
        // when it steps in(looks nice starting roughly in the middle, vs starting with an entire stripe
        // on the node title.
        int initialOffset = -(totalStripeStep * 0.5f) + candyStripeConfiguration.m_initialOffset;

        QBrush stripeBrush(candyStripeConfiguration.m_stripeColor);

        if (stripeSize == 0)
        {
            stripeSize = 1;
        }

        // We want to ensure we cover the entire drawable space with striping, so we
        // want to start at -totalStripeStep just to ensure we cover all of our bases.
        // Once there we can offset ourselves to make it align in a aesthetically pleasing way
        for (int x = -totalStripeStep + initialOffset; x < width; x += (2 * stripeSize))
        {
            QPointF offset = drawArea.topLeft() + QPointF(x, 0);

            QPainterPath path;

            path.moveTo(backPointBottom + offset);
            path.lineTo(backPointTop + offset);
            path.lineTo(forwardPointTop + offset);
            path.lineTo(forwardPointBottom + offset);
            path.closeSubpath();

            painter.fillPath(path, stripeBrush);
        }
    }

    void PatternFillHelper(QPainter& painter, const QRectF& area, const QPixmap& pattern, const PatternFillConfiguration& patternFillConfiguration)
    {
        int rowCount = 0;

        int oddOffset = pattern.width() * patternFillConfiguration.m_oddRowOffsetPercent;
        int evenOffset = pattern.width() * patternFillConfiguration.m_evenRowOffsetPercent;

        int currentX = area.left() - evenOffset;
        int currentY = area.top();

        int stepX = AZStd::max(pattern.width(), 1);
        int stepY = AZStd::max(pattern.height(), 1);

        while (currentY <= area.bottom())
        {
            painter.drawPixmap(currentX, currentY, pattern);

            currentX += stepX;

            if (currentX > area.right())
            {
                ++rowCount;
                currentX = area.left();

                if (rowCount % 2 == 0)
                {
                    currentX -= evenOffset;
                }
                else
                {
                    currentX -= oddOffset;
                }

                currentY += stepY;
            }
        }
    }

    void QtDrawingUtils::PatternFillArea(QPainter& painter, const QRectF& area, const QPixmap& pattern, const PatternFillConfiguration& patternFillConfiguration)
    {
        int scaleFactor = 1;

        if (patternFillConfiguration.m_minimumTileRepetitions > 1)
        {
            int numRepeats = area.width() / pattern.width();

            if (numRepeats < patternFillConfiguration.m_minimumTileRepetitions)
            {
                scaleFactor = std::ceil(pattern.width() / std::ceil((area.width()/patternFillConfiguration.m_minimumTileRepetitions)));
            }
        }

        if (scaleFactor == 1)
        {
            PatternFillHelper(painter, area, pattern, patternFillConfiguration);
        }
        else
        {
            QPixmap scaledPattern = pattern.scaledToWidth(std::floor(pattern.width() / scaleFactor));
            PatternFillHelper(painter, area, scaledPattern, patternFillConfiguration);
        }
    }

    void QtDrawingUtils::PatternFillArea(QPainter& painter, const QRectF& area, const PatternedFillGenerator& patternedFillGenerator)
    {
        const QPixmap* pixmap = nullptr;

        if (!patternedFillGenerator.m_palettes.empty())
        {
            StyleManagerRequestBus::EventResult(pixmap, patternedFillGenerator.m_editorId, &StyleManagerRequests::CreatePatternPixmap, patternedFillGenerator.m_palettes, patternedFillGenerator.m_id);
        }
        else
        {
            StyleManagerRequestBus::EventResult(pixmap, patternedFillGenerator.m_editorId, &StyleManagerRequests::CreateColoredPatternPixmap, patternedFillGenerator.m_colors, patternedFillGenerator.m_id);
        }

        if (pixmap)
        {
            PatternFillArea(painter, area, (*pixmap), patternedFillGenerator.m_configuration);
        }
    }
}
