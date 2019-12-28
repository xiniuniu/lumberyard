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
#include "ImGuiMainWindow.h"
#include "QtViewPaneManager.h"
#include "ImGuiViewport.h"
#include <AzToolsFramework/API/ToolsApplicationAPI.h>

using namespace ImGui;

ImGuiMainWindow::ImGuiMainWindow()
    : QMainWindow()
    , m_ui(new Ui::ImGuiMainWindow)
    , m_viewport(new ImGuiViewportWidget(this))
{
    m_ui->setupUi(this);

    //@rky: Add the ImGuiViewport to this window
    m_viewport->setFocusPolicy(Qt::StrongFocus);
    m_ui->gridLayout->addWidget(m_viewport, 0, 0, 1, 1);
}

#include <Editor/ImGuiMainWindow.moc>
