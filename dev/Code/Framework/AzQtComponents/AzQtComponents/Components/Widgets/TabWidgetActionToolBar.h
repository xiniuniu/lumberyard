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

#include <QWidget>

class QPushButton;

namespace AzQtComponents
{
    /**
     * A widget displaying QAction objects using QPushButton widgets.
     * It binds to the parent events and filter action events to create, change and remove
     * buttons according when an action is added, changed or removed on the parent widget.
     */
    class AZ_QT_COMPONENTS_API TabWidgetActionToolBar
        : public QWidget
    {
        Q_OBJECT

    public:
        explicit TabWidgetActionToolBar(QWidget* parent = nullptr);

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        friend class TabWidget;
        QHash<QAction*, QPushButton*> m_actionButtons;

        void removeWidgetFromLayout(QWidget* widget);
    };

} // namespace AzQtComponents
