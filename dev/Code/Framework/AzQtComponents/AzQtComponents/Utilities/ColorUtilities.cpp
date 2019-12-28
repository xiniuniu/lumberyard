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

#include <AzQtComponents/Utilities/ColorUtilities.h>
#include <AzQtComponents/Utilities/Conversions.h>
#include <AzQtComponents/Components/Style.h>
#include <cmath>
#include <QPainter>
#include <AzCore/Math/MathUtils.h>
#include <assert.h>

namespace AzQtComponents
{
    QColor AdjustGamma(const QColor& color, qreal gamma)
    {
        const QColor rgb = color.toRgb();
        const auto r = std::pow(rgb.redF(), 1.0 / gamma);
        const auto g = std::pow(rgb.greenF(), 1.0 / gamma);
        const auto b = std::pow(rgb.blueF(), 1.0 / gamma);
        const auto a = rgb.alphaF();
        return toQColor(r, g, b, a);
    }

    AZ::Color AdjustGamma(const AZ::Color& color, float gamma)
    {
        const float r = std::pow(color.GetR(), 1.0f / gamma);
        const float g = std::pow(color.GetG(), 1.0f / gamma);
        const float b = std::pow(color.GetB(), 1.0f / gamma);
        const float a = color.GetA();
        return AZ::Color(r, g, b, a);
    }

    QBrush MakeAlphaBrush(const QColor& color, qreal gamma)
    {
        QColor adjusted = gamma == 1.0f ? color : AdjustGamma(color, gamma);

        QPixmap alpha = Style::cachedPixmap(QStringLiteral(":/stylesheet/img/UI20/alpha-background.png"));
        QPainter filler(&alpha);
        filler.fillRect(alpha.rect(), adjusted);

        return QBrush(alpha);
    }

    QBrush MakeAlphaBrush(const AZ::Color& color, float gamma)
    {
        return MakeAlphaBrush(ToQColor(color), gamma);
    }

    bool AreClose(const AZ::Color& left, const AZ::Color& right)
    {
        // the two values can't be off more than a single 8 bit rounded unit
        const float tolerance = 1.0f / 255.0f;

        return left.IsClose(right, tolerance);
    }

} // namespace AzQtComponents



