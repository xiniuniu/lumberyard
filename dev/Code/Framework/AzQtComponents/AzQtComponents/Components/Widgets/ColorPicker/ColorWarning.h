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
#include <AzCore/Math/Color.h>
#include <QWidget>
#include <QString>

class QLabel;

namespace AzQtComponents
{

    class Swatch;

    class AZ_QT_COMPONENTS_API ColorWarning
        : public QWidget
    {
        Q_OBJECT //AUTOMOC

        Q_PROPERTY(Mode mode READ mode WRITE setMode)
        Q_PROPERTY(AZ::Color color READ color WRITE setColor)
        Q_PROPERTY(QString message READ message WRITE setMessage)

    public:
        enum class Mode
        {
            Warning,
            Error
        };

        explicit ColorWarning(QWidget* parent = nullptr);
        explicit ColorWarning(Mode mode, const AZ::Color& color, const QString& message, QWidget* parent = nullptr);
        ~ColorWarning() override;

        Mode mode() const;
        void setMode(Mode mode);

        AZ::Color color() const;
        void setColor(const AZ::Color& color);

        const QString& message() const;
        void setMessage(const QString& message);

        void set(Mode mode, const QString& message);
        void clear();

    private:
        Mode m_mode = Mode::Warning;
        AZ::Color m_color;
        QString m_message;

        QLabel* m_iconLabel;
        Swatch* m_swatch;
        QLabel* m_messageLabel;
    };

} // namespace AzQtComponents
