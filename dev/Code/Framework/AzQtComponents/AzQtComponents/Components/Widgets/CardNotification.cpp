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
#include <AzQtComponents/Components/Widgets/CardNotification.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace AzQtComponents
{
    CardNotification::CardNotification(QWidget* parent, const QString& title, const QIcon& icon)
        : QFrame(parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

        QFrame* headerFrame = new QFrame(this);
        headerFrame->setObjectName("HeaderFrame");
        headerFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

        // icon widget
        QLabel* iconLabel = new QLabel(headerFrame);
        iconLabel->setObjectName("Icon");
        iconLabel->setPixmap(icon.pixmap(icon.availableSizes().front()));

        // title widget
        QLabel* titleLabel = new QLabel(title, headerFrame);
        titleLabel->setObjectName("Title");
        titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        titleLabel->setWordWrap(true);

        QHBoxLayout* headerLayout = new QHBoxLayout(headerFrame);
        headerLayout->setSizeConstraint(QLayout::SetMinimumSize);
        headerLayout->addWidget(iconLabel);
        headerLayout->addWidget(titleLabel);
        headerFrame->setLayout(headerLayout);

        m_featureLayout = new QVBoxLayout(this);
        m_featureLayout->setSizeConstraint(QLayout::SetMinimumSize);
        m_featureLayout->addWidget(headerFrame);
        setLayout(m_featureLayout);
    }

    void CardNotification::addFeature(QWidget* feature)
    {
        feature->setParent(this);
        m_featureLayout->addWidget(feature);
    }

    QPushButton* CardNotification::addButtonFeature(QString buttonText)
    {
        QPushButton* featureButton = new QPushButton(buttonText, this);

        addFeature(featureButton);

        return featureButton;
    }
}

