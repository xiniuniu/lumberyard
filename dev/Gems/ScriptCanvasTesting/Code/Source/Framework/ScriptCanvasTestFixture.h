/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright EntityRef license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#pragma once

#include "ScriptCanvasTestApplication.h"
#include "ScriptCanvasTestNodes.h"
#include "EntityRefTests.h"
#include "ScriptCanvasTestUtilities.h"
#include <ScriptCanvas/Core/Graph.h>
#include <ScriptCanvas/Core/SlotConfigurationDefaults.h>

#include <AzTest/AzTest.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Driller/Driller.h>
#include <AzCore/Memory/MemoryDriller.h>
#include <ScriptCanvas/ScriptCanvasGem.h>
#include <AzCore/Memory/MemoryComponent.h>
#include <AzCore/Asset/AssetManagerComponent.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <ScriptCanvas/SystemComponent.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzCore/std/containers/vector.h>

#include <AzCore/Component/ComponentApplicationBus.h>

#define SC_EXPECT_DOUBLE_EQ(candidate, reference) EXPECT_NEAR(candidate, reference, 0.001)
#define SC_EXPECT_FLOAT_EQ(candidate, reference) EXPECT_NEAR(candidate, reference, 0.001f)

namespace ScriptCanvasTests
{

    class ScriptCanvasTestFixture
        : public ::testing::Test
    {
    public:
        static AZStd::atomic_bool s_asyncOperationActive;

    protected:

        static ScriptCanvasTests::Application* s_application;

        static void SetUpTestCase()
        {
            s_allocatorSetup.SetupAllocator();

            s_asyncOperationActive = false;

            if (s_application == nullptr)
            {
                AZ::ComponentApplication::StartupParameters appStartup;
                s_application = aznew ScriptCanvasTests::Application();

                {
                    ScriptCanvasEditor::TraceSuppressionBus::Broadcast(&ScriptCanvasEditor::TraceSuppressionRequests::SuppressPrintf, true);
                    AZ::ComponentApplication::Descriptor descriptor;
                    descriptor.m_enableDrilling = false;
                    descriptor.m_useExistingAllocator = true;

                    AZ::DynamicModuleDescriptor dynamicModuleDescriptor;
                    dynamicModuleDescriptor.m_dynamicLibraryPath = "Gem.GraphCanvas.Editor.875b6fcbdeea44deaae7984ad9bb6cdc.v0.1.0";
                    descriptor.m_modules.push_back(dynamicModuleDescriptor);
                    dynamicModuleDescriptor.m_dynamicLibraryPath = "Gem.ScriptCanvasGem.Editor.869a0d0ec11a45c299917d45c81555e6.v0.1.0";
                    descriptor.m_modules.push_back(dynamicModuleDescriptor);

                    s_application->Start(descriptor, appStartup);
                    ScriptCanvasEditor::TraceSuppressionBus::Broadcast(&ScriptCanvasEditor::TraceSuppressionRequests::SuppressPrintf, false);
                }
            }

            AZ::IO::FileIOBase* fileIO = AZ::IO::FileIOBase::GetInstance();
            AZ_Assert(fileIO, "SC unit tests require filehandling");

            if (!fileIO->GetAlias("@engroot@"))
            {
                const char* engineRoot = nullptr;
                AzFramework::ApplicationRequests::Bus::BroadcastResult(engineRoot, &AzFramework::ApplicationRequests::GetEngineRoot);
                AZ_Assert(engineRoot, "null engine root");
                fileIO->SetAlias("@engroot@", engineRoot);
            }

            s_setupSucceeded = fileIO->GetAlias("@engroot@") != nullptr;
            
            AZ::TickBus::AllowFunctionQueuing(true);
        }

        static void TearDownTestCase()
        {
            // don't hang on to dangling assets
            AZ::Data::AssetManager::Instance().DispatchEvents();

            if (AZ::IO::FileIOBase* fileIO = AZ::IO::FileIOBase::GetInstance())
            {
                fileIO->DestroyPath(k_tempCoreAssetDir);
            }

            if (s_application)
            {
                s_application->Stop();
                delete s_application;
                s_application = nullptr;
            }

            s_allocatorSetup.TeardownAllocator();
        }

        template<class T>
        void RegisterComponentDescriptor()
        {
            AZ::ComponentDescriptor* descriptor = T::CreateDescriptor();

            auto insertResult = m_descriptors.insert(descriptor);

            if (insertResult.second)
            {
                GetApplication()->RegisterComponentDescriptor(descriptor);
            }
        }

        void SetUp() override
        {
            ASSERT_TRUE(s_setupSucceeded) << "ScriptCanvasTestFixture set up failed, unit tests can't work properly";
            m_serializeContext = s_application->GetSerializeContext();
            m_behaviorContext = s_application->GetBehaviorContext();
            AZ_Assert(AZ::IO::FileIOBase::GetInstance(), "File IO was not properly installed");

            RegisterComponentDescriptor<TestNodes::TestResult>();
            RegisterComponentDescriptor<TestNodes::ConfigurableUnitTestNode>();

            m_numericVectorType = ScriptCanvas::Data::Type::BehaviorContextObject(azrtti_typeid<AZStd::vector<ScriptCanvas::Data::NumberType>>());
            m_stringToNumberMapType = ScriptCanvas::Data::Type::BehaviorContextObject(azrtti_typeid<AZStd::unordered_map<ScriptCanvas::Data::StringType, ScriptCanvas::Data::NumberType>>());

            m_dataSlotConfigurationType = ScriptCanvas::Data::Type::BehaviorContextObject(azrtti_typeid<ScriptCanvas::DataSlotConfiguration>());

            ScriptUnitTestEventHandler::Reflect(m_serializeContext);
            ScriptUnitTestEventHandler::Reflect(m_behaviorContext);
        }

        void TearDown() override
        {
            ASSERT_TRUE(s_setupSucceeded) << "ScriptCanvasTestFixture set up failed, unit tests can't work properly";

            m_serializeContext->EnableRemoveReflection();
            m_behaviorContext->EnableRemoveReflection();

            ScriptUnitTestEventHandler::Reflect(m_serializeContext);
            ScriptUnitTestEventHandler::Reflect(m_behaviorContext);

            m_serializeContext->DisableRemoveReflection();
            m_behaviorContext->DisableRemoveReflection();

            for (AZ::ComponentDescriptor* componentDescriptor : m_descriptors)
            {
                GetApplication()->UnregisterComponentDescriptor(componentDescriptor);
            }

            m_descriptors.clear();

            delete m_graph;
            m_graph = nullptr;
        }

        ScriptCanvas::Graph* CreateGraph()
        {
            if (m_graph == nullptr)
        {
                m_graph = aznew ScriptCanvas::Graph();
                m_graph->Init();
            }

            return m_graph;
        }

        TestNodes::ConfigurableUnitTestNode* CreateConfigurableNode(AZStd::string entityName = "ConfigurableNodeEntity")
        {
            AZ::Entity* configurableNodeEntity = new AZ::Entity(entityName.c_str());         
            auto configurableNode = configurableNodeEntity->CreateComponent<TestNodes::ConfigurableUnitTestNode>();

            configurableNodeEntity->Init();
            configurableNodeEntity->Activate();

            if (m_graph == nullptr)
            {
                CreateGraph();
            }

            m_graph->AddNode(configurableNodeEntity->GetId());

            return configurableNode;
        }

        void ReportErrors(ScriptCanvas::Graph* graph, bool expectErrors = false, bool expectIrrecoverableErrors = false)
        {
            EXPECT_EQ(graph->IsInErrorState(), expectErrors);
            EXPECT_EQ(graph->IsInIrrecoverableErrorState(), expectIrrecoverableErrors);

            if (graph->IsInErrorState())
            {
                AZ_TracePrintf("UnitTest", "%s\n", AZStd::string(graph->GetLastErrorDescription()).c_str());
            }
        }

        void TestConnectionBetween(ScriptCanvas::Endpoint sourceEndpoint, ScriptCanvas::Endpoint targetEndpoint, bool isValid = true)
        {
            EXPECT_EQ(m_graph->CanConnectionExistBetween(sourceEndpoint, targetEndpoint).IsSuccess(), isValid);
            EXPECT_EQ(m_graph->CanConnectionExistBetween(targetEndpoint, sourceEndpoint).IsSuccess(), isValid);

            EXPECT_EQ(m_graph->CanCreateConnectionBetween(sourceEndpoint, targetEndpoint).IsSuccess(), isValid);
            EXPECT_EQ(m_graph->CanCreateConnectionBetween(targetEndpoint, sourceEndpoint).IsSuccess(), isValid);

            if (isValid)
            {
                EXPECT_TRUE(m_graph->ConnectByEndpoint(sourceEndpoint, targetEndpoint));
            }
        }

        void TestIsConnectionPossible(ScriptCanvas::Endpoint sourceEndpoint, ScriptCanvas::Endpoint targetEndpoint, bool isValid = true)
        {
            EXPECT_EQ(m_graph->CanConnectionExistBetween(sourceEndpoint, targetEndpoint).IsSuccess(), isValid);
            EXPECT_EQ(m_graph->CanConnectionExistBetween(targetEndpoint, sourceEndpoint).IsSuccess(), isValid);

            EXPECT_EQ(m_graph->CanCreateConnectionBetween(sourceEndpoint, targetEndpoint).IsSuccess(), isValid);
            EXPECT_EQ(m_graph->CanCreateConnectionBetween(targetEndpoint, sourceEndpoint).IsSuccess(), isValid);
        }

        void CreateExecutionFlowBetween(AZStd::vector<TestNodes::ConfigurableUnitTestNode*> unitTestNodes)
        {
            ScriptCanvas::Slot* previousOutSlot = nullptr;

            for (TestNodes::ConfigurableUnitTestNode* testNode : unitTestNodes)
            {
                {
                    ScriptCanvas::ExecutionSlotConfiguration inputSlot = ScriptCanvas::CommonSlots::GeneralInSlot();
                    ScriptCanvas::Slot* slot = testNode->AddTestingSlot(inputSlot);

                    if (slot && previousOutSlot)
                    {
                        TestConnectionBetween(previousOutSlot->GetEndpoint(), slot->GetEndpoint());
                    }
                }

                {
                    ScriptCanvas::ExecutionSlotConfiguration outputSlot = ScriptCanvas::CommonSlots::GeneralOutSlot();
                    previousOutSlot = testNode->AddTestingSlot(outputSlot);
                }
            }
        }

        AZStd::vector< ScriptCanvas::Data::Type > GetContainerDataTypes() const
        {
            return { m_numericVectorType, m_stringToNumberMapType };
        }

        ScriptCanvas::Data::Type GetRandomContainerType() const
        {
            AZStd::vector< ScriptCanvas::Data::Type > containerTypes = GetContainerDataTypes();

            // We have no types to randomize. Just return.
            if (containerTypes.empty())
            {
                return m_numericVectorType;
            }

            int randomIndex = rand() % containerTypes.size();

            ScriptCanvas::Data::Type randomType = containerTypes[randomIndex];
            AZ_TracePrintf("ScriptCanvasTestFixture", "RandomContainerType: %s\n", randomType.GetAZType().ToString<AZStd::string>().c_str());
            return randomType;
        }

        AZStd::vector< ScriptCanvas::Data::Type > GetPrimitiveTypes() const
        {
            return{
                ScriptCanvas::Data::Type::AABB(),
                ScriptCanvas::Data::Type::Boolean(),
                ScriptCanvas::Data::Type::Color(),
                ScriptCanvas::Data::Type::CRC(),
                ScriptCanvas::Data::Type::EntityID(),
                ScriptCanvas::Data::Type::Matrix3x3(),
                ScriptCanvas::Data::Type::Matrix4x4(),
                ScriptCanvas::Data::Type::Number(),
                ScriptCanvas::Data::Type::OBB(),
                ScriptCanvas::Data::Type::Plane(),
                ScriptCanvas::Data::Type::Quaternion(),
                ScriptCanvas::Data::Type::String(),
                ScriptCanvas::Data::Type::Transform(),
                ScriptCanvas::Data::Type::Vector2(),
                ScriptCanvas::Data::Type::Vector3(),
                ScriptCanvas::Data::Type::Vector4()
            };
        }

        ScriptCanvas::Data::Type GetRandomPrimitiveType() const
        {
            AZStd::vector< ScriptCanvas::Data::Type > primitiveTypes = GetPrimitiveTypes();

            // We have no types to randomize. Just return.
            if (primitiveTypes.empty())
            {
                return ScriptCanvas::Data::Type::Number();
            }

            int randomIndex = rand() % primitiveTypes.size();

            ScriptCanvas::Data::Type randomType = primitiveTypes[randomIndex];
            AZ_TracePrintf("ScriptCanvasTestFixture", "RandomPrimitiveType: %s\n", randomType.GetAZType().ToString<AZStd::string>().c_str());
            return randomType;
        }

        AZStd::vector< ScriptCanvas::Data::Type > GetBehaviorObjectTypes() const
        {
            return {
                m_dataSlotConfigurationType
            };
        }

        ScriptCanvas::Data::Type GetRandomObjectType() const
        {
            AZStd::vector< ScriptCanvas::Data::Type > objectTypes = GetBehaviorObjectTypes();

            // We have no types to randomize. Just return.
            if (objectTypes.empty())
            {
                return m_dataSlotConfigurationType;
            }

            int randomIndex = rand() % objectTypes.size();

            ScriptCanvas::Data::Type randomType = objectTypes[randomIndex];
            AZ_TracePrintf("ScriptCanvasTestFixture", "RandomObjectType: %s\n", randomType.GetAZType().ToString<AZStd::string>().c_str());
            return randomType;
        }

        AZStd::vector< ScriptCanvas::Data::Type > GetTypes() const
        {
            auto primitives = GetPrimitiveTypes();
            auto containers = GetContainerDataTypes();
            auto objects = GetBehaviorObjectTypes();

            primitives.reserve(containers.size() + objects.size());

            primitives.insert(primitives.end(), containers.begin(), containers.end());
            primitives.insert(primitives.end(), objects.begin(), objects.end());

            return primitives;
        }

        ScriptCanvas::Data::Type GetRandomType() const
        {
            AZStd::vector< ScriptCanvas::Data::Type > types = GetTypes();

            // We have no types to randomize. Just return.
            if (types.empty())
            {
                return m_dataSlotConfigurationType;
            }

            int randomIndex = rand() % types.size();

            ScriptCanvas::Data::Type randomType = types[randomIndex];
            AZ_TracePrintf("ScriptCanvasTestFixture", "RandomType: %s\n", randomType.GetAZType().ToString<AZStd::string>().c_str());
            return randomType;
        }

        AZStd::string GenerateSlotName()
        {
            AZStd::string slotName = AZStd::string::format("Slot %i", m_slotCounter);
            ++m_slotCounter;

            return slotName;
        }

        AZ::SerializeContext* m_serializeContext;
        AZ::BehaviorContext* m_behaviorContext;
        UnitTestEntityContext m_entityContext;

        // Really big(visually) data types just storing here for ease of use in situations.
        ScriptCanvas::Data::Type m_numericVectorType;
        ScriptCanvas::Data::Type m_stringToNumberMapType;

        ScriptCanvas::Data::Type m_dataSlotConfigurationType;

        ScriptCanvas::Graph* m_graph = nullptr;

        int m_slotCounter = 0;

        AZStd::unordered_map< AZ::EntityId, AZ::Entity* > m_entityMap;

    protected:
        static ScriptCanvasTests::Application* GetApplication() { return s_application; }
        
    private:

        static UnitTest::AllocatorsBase s_allocatorSetup;
        static bool s_setupSucceeded;

        AZStd::unordered_set< AZ::ComponentDescriptor* > m_descriptors;
        
    };
}

