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
#include <Material/MaterialPythonFuncs.h>
#include <AzCore/RTTI/BehaviorContext.h>

namespace MaterialPythonFuncsUnitTests
{

    class MaterialPythonBindingsFixture
        : public testing::Test
    {
    public:
        AzToolsFramework::ToolsApplication m_app;

        void SetUp() override
        {
            AzFramework::Application::Descriptor appDesc;
            appDesc.m_enableDrilling = false;

            m_app.Start(appDesc);
            m_app.RegisterComponentDescriptor(AzToolsFramework::MaterialPythonFuncsHandler::CreateDescriptor());
        }

        void TearDown() override
        {
            m_app.Stop();
        }
    };

    TEST_F(MaterialPythonBindingsFixture, MaterialEditorCommands_ApiExists)
    {
        AZ::BehaviorContext* behaviorContext = m_app.GetBehaviorContext();
        ASSERT_TRUE(behaviorContext);

        EXPECT_TRUE(behaviorContext->m_methods.find("create") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("create_multi") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("convert_to_multi") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("duplicate_current") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("merge_selection") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("delete_current") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("get_submaterial") != behaviorContext->m_methods.end());

        EXPECT_TRUE(behaviorContext->m_methods.find("get_property_as_bool") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("get_property_as_color") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("get_property_as_float") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("get_property_as_int") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("get_property_as_string") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("get_property_as_vector3") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("get_property_as_vector4") != behaviorContext->m_methods.end());
        //EXPECT_TRUE(behaviorContext->m_methods.find("get_property") != behaviorContext->m_methods.end());
            
        EXPECT_TRUE(behaviorContext->m_methods.find("set_property_from_bool") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("set_property_from_color") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("set_property_from_float") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("set_property_from_int") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("set_property_from_string") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("set_property_from_vector3") != behaviorContext->m_methods.end());
        EXPECT_TRUE(behaviorContext->m_methods.find("set_property_from_vector4") != behaviorContext->m_methods.end());
        //EXPECT_TRUE(behaviorContext->m_methods.find("set_property") != behaviorContext->m_methods.end());
    }
}