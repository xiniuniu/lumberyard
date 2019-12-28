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

#include <AzQtComponents/AzQtComponentsAPI.h>
#include <QFrame>

namespace AzQtComponents
{
    class AZ_QT_COMPONENTS_API ColorGrid
        : public QFrame
    {
        Q_OBJECT
        Q_PROPERTY(qreal hue READ hue WRITE setHue NOTIFY hueChanged);
        Q_PROPERTY(qreal saturation READ saturation WRITE setSaturation NOTIFY saturationChanged);
        Q_PROPERTY(qreal value READ value WRITE setValue NOTIFY valueChanged);

    public:
        explicit ColorGrid(QWidget* parent = nullptr);
        ~ColorGrid() override;

        qreal hue() const;
        qreal saturation() const;
        qreal value() const;
        qreal defaultVForHsMode() const;

        enum class Mode
        {
            SaturationValue,
            HueSaturation,
        };

        void setMode(Mode mode);
        Mode mode() const;

    public Q_SLOTS:
        void setHue(qreal hue);
        void setSaturation(qreal saturation);
        void setValue(qreal value);
        void setDefaultVForHsMode(qreal value);

    Q_SIGNALS:
        void gridPressed();
        void hueChanged(qreal hue);
        void saturationChanged(qreal saturation);
        void valueChanged(qreal value);
        void gridReleased();

    protected:
        void paintEvent(QPaintEvent* e) override;
        void mouseMoveEvent(QMouseEvent* e) override;
        void mousePressEvent(QMouseEvent* e) override;
        void mouseReleaseEvent(QMouseEvent* e) override;
        void resizeEvent(QResizeEvent* e) override;

    private:
        void initPixmap();
        void handleLeftButtonEvent(const QPoint& p);

        struct HSV
        {
            qreal hue;
            qreal saturation;
            qreal value;
        };
        HSV positionToColor(const QPoint& pos) const;

        QPoint cursorCenter() const;

        Mode m_mode;

        qreal m_hue;
        qreal m_saturation;
        qreal m_value;
        qreal m_defaultVForHsMode;

        QPixmap m_pixmap;
    };
} // namespace AzQtComponents
