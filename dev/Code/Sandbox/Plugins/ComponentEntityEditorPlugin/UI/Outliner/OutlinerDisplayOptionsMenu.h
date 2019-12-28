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

#include <QScopedPointer>
#include <QWidget>

namespace Ui
{
    class OutlinerDisplayOptions;
}

namespace EntityOutliner
{
    enum class DisplaySortMode : unsigned char
    {
        Manually,
        AtoZ,
        ZtoA
    };

    enum class DisplayOption : unsigned char
    {
        AutoScroll,
        AutoExpand
    };

    class DisplayOptionsMenu 
        : public QMenu
    {
        Q_OBJECT

    public:
        DisplayOptionsMenu(QWidget* parent = nullptr);
        ~DisplayOptionsMenu() = default;

    signals:
        void OnSortModeChanged(DisplaySortMode sortMode);
        void OnOptionToggled(DisplayOption option, bool enabled);

    private:
        void OnSortModeSelected(int sortMode);

        void OnAutoScrollToggle(int state);
        void OnAutoExpandToggle(int state);


        QScopedPointer<Ui::OutlinerDisplayOptions> m_ui;
    };
}
