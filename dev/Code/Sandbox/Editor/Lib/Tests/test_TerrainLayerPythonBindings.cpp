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
#include <AzTest/AzTest.h>
#include <Util/EditorUtils.h>
#include <AzCore/base.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Debug/TraceMessageBus.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <AzToolsFramework/Application/ToolsApplication.h>
#include <Terrain/PythonTerrainLayerFuncs.h>
#include <AzCore/RTTI/BehaviorContext.h>

namespace TerrainFuncsUnitTests
{

    class TerrainLayerPythonBindingsFixture
        : public testing::Test
    {
    public:
        AzToolsFramework::ToolsApplication m_app;

        void SetUp() override
        {
            AzFramework::Application::Descriptor appDesc;
            appDesc.m_enableDrilling = false;

            m_app.Start(appDesc);
            m_app.RegisterComponentDescriptor(AzToolsFramework::TerrainLayerPythonFuncsHandler::CreateDescriptor());
        }

        void TearDown() override
        {
            m_app.Stop();
        }
    };

    TEST_F(TerrainLayerPythonBindingsFixture, TerrainCommands_ApiExists)
    {
        AZ::BehaviorContext* behaviorContext = m_app.GetBehaviorContext();
        ASSERT_TRUE(behaviorContext);

        EXPECT_TRUE(behaviorContext->m_methods.find("get_tile_count_x") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("get_tile_count_y") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("get_tile_resolution") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("set_tile_resolution") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("set_tile_count") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("get_color_at") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("import_megaterrain") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("export_megaterrain") != behaviorContext->m_methods.end());
    }
}
