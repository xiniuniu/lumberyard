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

#include <AzCore/Component/Entity.h>
#include <AzCore/UnitTest/TestTypes.h>

#include <AzFramework/Application/Application.h>
#include <AzFramework/Entity/EntityContext.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/Application/ToolsApplication.h>
#include <AzToolsFramework/UI/PropertyEditor/EntityIdQLabel.hxx>
#include <AzToolsFramework/Viewport/ActionBus.h>

#include <QtTest/QtTest>

using namespace AzToolsFramework;

namespace UnitTest
{

    // Test widget to store a EntityIdQLabel
    class TestWidget
        : public QWidget
    {
    public:
        explicit TestWidget(QWidget* parent = nullptr)
            : QWidget(nullptr)
        {
            AZ_UNUSED(parent);
            // ensure TestWidget can intercept and filter any incoming events itself
            installEventFilter(this);

            m_testLabel = new EntityIdQLabel(this);
        }

        EntityIdQLabel* m_testLabel = nullptr;

    };

    // Used to simulate a system implementing the EditorRequests bus to validate that the double click will
    // result in a GoToSelectedEntitiesInViewports event
    class EditorRequestHandlerTest : AzToolsFramework::EditorRequests::Bus::Handler
    {
    public:
        EditorRequestHandlerTest()
        {
            AzToolsFramework::EditorRequests::Bus::Handler::BusConnect();
        }

        ~EditorRequestHandlerTest()
        {
            AzToolsFramework::EditorRequests::Bus::Handler::BusDisconnect();
        }

        void BrowseForAssets(AssetBrowser::AssetSelectionModel& /*selection*/) override {}
        int GetIconTextureIdFromEntityIconPath(const AZStd::string& entityIconPath) override { AZ_UNUSED(entityIconPath);  return 0; }
        bool DisplayHelpersVisible() override { return false; }

        void GoToSelectedEntitiesInViewports() override 
        {
            m_wentToSelectedEntitiesInViewport = true;
        }

        bool m_wentToSelectedEntitiesInViewport = false;

    };

    // Fixture to support testing EntityIdQLabel functionality
    class EntityIdQLabelTest
        : public AllocatorsTestFixture
    {
    public:
        void SetUp() override
        {
            m_app.Start(AzFramework::Application::Descriptor());
        }

        void TearDown() override
        {
            m_app.Stop();
        }

        TestWidget* m_widget = nullptr;

    private:

        ToolsApplication m_app;

    };

    TEST_F(EntityIdQLabelTest, DoubleClickEntitySelectionTest)
    {
        AZ::Entity* entity = aznew AZ::Entity();
        ASSERT_TRUE(entity != nullptr);
        entity->Init();
        entity->Activate();

        AZ::EntityId entityId = entity->GetId();
        ASSERT_TRUE(entityId.IsValid());

        TestWidget* widget = new TestWidget(nullptr);
        ASSERT_TRUE(widget != nullptr);
        ASSERT_TRUE(widget->m_testLabel != nullptr);

        widget->m_testLabel->setFocus();
        widget->m_testLabel->SetEntityId(entityId, {});

        EditorRequestHandlerTest editorRequestHandler;

        // Simulate double clicking the label
        QTest::mouseDClick(widget->m_testLabel, Qt::LeftButton);

        // If successful we expect the label's entity to be selected.
        EntityIdList selectedEntities;
        ToolsApplicationRequestBus::BroadcastResult(selectedEntities, &ToolsApplicationRequests::GetSelectedEntities);

        EXPECT_FALSE(selectedEntities.empty()) << "Double clicking on an EntityIdQLabel should select the entity";
        EXPECT_TRUE(selectedEntities[0] == entityId) << "The selected entity is not the one that was double clicked";
        
        selectedEntities.clear();
        ToolsApplicationRequestBus::Broadcast(&ToolsApplicationRequests::SetSelectedEntities, selectedEntities);

        widget->m_testLabel->SetEntityId(AZ::EntityId(), {});

        ToolsApplicationRequestBus::BroadcastResult(selectedEntities, &ToolsApplicationRequests::GetSelectedEntities);
        EXPECT_TRUE(selectedEntities.empty()) << "Double clicking on an EntityIdQLabel with an invalid entity ID shouldn't change anything";

        EXPECT_TRUE(editorRequestHandler.m_wentToSelectedEntitiesInViewport) << "Double clicking an EntityIdQLabel should result in a GoToSelectedEntitiesInViewports call";

        delete entity;
        delete widget;
    }
}