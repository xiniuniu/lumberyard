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

#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Components/TransformComponent.h>
#include <AzFramework/Entity/EntityContext.h>
#include <AzTest/AzTest.h>
#include <AzToolsFramework/Application/ToolsApplication.h>
#include <AzToolsFramework/ComponentMode/ComponentModeCollection.h>
#include <AzToolsFramework/ComponentMode/EditorComponentModeBus.h>
#include <AzToolsFramework/ToolsComponents/EditorLockComponent.h>
#include <AzToolsFramework/ToolsComponents/EditorVisibilityComponent.h>
#include <AzToolsFramework/ToolsComponents/TransformComponent.h>
#include <AzToolsFramework/UI/PropertyEditor/EntityPropertyEditor.hxx>
#include <AzToolsFramework/Viewport/ActionBus.h>
#include <AzToolsFramework/ViewportSelection/EditorDefaultSelection.h>
#include <AzToolsFramework/ViewportSelection/EditorInteractionSystemViewportSelectionRequestBus.h>
#include <AzToolsFramework/ViewportSelection/EditorVisibleEntityDataCache.h>
#include <AzCore/UnitTest/TestTypes.h>

#include <AzToolsFramework/UnitTest/AzToolsFrameworkTestHelpers.h>
#include <AzToolsFramework/UnitTest/ComponentModeTestDoubles.h>
#include <AzToolsFramework/UnitTest/ComponentModeTestFixture.h>


#include <QtTest/QtTest>
#include <QApplication>

namespace UnitTest
{
    using namespace AzToolsFramework;
    using namespace AzToolsFramework::ComponentModeFramework;

    TEST_F(ComponentModeTestFixture, BeginEndComponentMode)
    {
        using ::testing::Eq;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        EditorInteractionSystemViewportSelectionRequestBus::Event(
            GetEntityContextId(), &EditorInteractionSystemViewportSelection::SetDefaultHandler);

        QWidget rootWidget;
        ActionOverrideRequestBus::Event(
            GetEntityContextId(), &ActionOverrideRequests::SetupActionOverrideHandler, &rootWidget);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        ComponentModeSystemRequestBus::Broadcast(
            &ComponentModeSystemRequests::BeginComponentMode,
            AZStd::vector<EntityAndComponentModeBuilders>{});

        bool inComponentMode = false;
        ComponentModeSystemRequestBus::BroadcastResult(
            inComponentMode, &ComponentModeSystemRequests::InComponentMode);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_TRUE(inComponentMode);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        ComponentModeSystemRequestBus::Broadcast(
            &ComponentModeSystemRequests::EndComponentMode);

        ComponentModeSystemRequestBus::BroadcastResult(
            inComponentMode, &ComponentModeSystemRequests::InComponentMode);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_FALSE(inComponentMode);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ActionOverrideRequestBus::Event(
            GetEntityContextId(), &ActionOverrideRequests::TeardownActionOverrideHandler);
    }

    TEST_F(ComponentModeTestFixture, TwoComponentsOnSingleEntityWithSameComponentModeBothBegin)
    {
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        // setup default editor interaction model
        EditorInteractionSystemViewportSelectionRequestBus::Event(
            GetEntityContextId(), &EditorInteractionSystemViewportSelection::SetDefaultHandler);

        AZ::Entity* entity = nullptr;
        AZ::EntityId entityId = CreateDefaultEditorEntity("ComponentModeEntity", &entity);

        entity->Deactivate();

        // add two placeholder Components (each with their own Component Mode)
        const AZ::Component* placeholder1 = entity->CreateComponent<PlaceholderEditorComponent>();
        const AZ::Component* placeholder2 = entity->CreateComponent<PlaceholderEditorComponent>();

        entity->Activate();

        // mimic selecting the entity in the viewport (after selection the ComponentModeDelegate
        // connects to the ComponentModeDelegateRequestBus on the entity/component pair address)
        const AzToolsFramework::EntityIdList entityIds = { entityId };
        ToolsApplicationRequestBus::Broadcast(
            &ToolsApplicationRequests::SetSelectedEntities, entityIds);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        // move all selected components into ComponentMode
        // (mimic pressing the 'Edit' button to begin Component Mode)
        ComponentModeSystemRequestBus::Broadcast(
            &ComponentModeSystemRequests::AddSelectedComponentModesOfType,
            AZ::AzTypeInfo<PlaceholderEditorComponent>::Uuid());
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        bool firstComponentModeInstantiated = false;
        ComponentModeSystemRequestBus::BroadcastResult(
            firstComponentModeInstantiated, &ComponentModeSystemRequests::ComponentModeInstantiated,
            AZ::EntityComponentIdPair(entityId, placeholder1->GetId()));

        bool secondComponentModeInstantiated = false;
        ComponentModeSystemRequestBus::BroadcastResult(
            secondComponentModeInstantiated, &ComponentModeSystemRequests::ComponentModeInstantiated,
            AZ::EntityComponentIdPair(entityId, placeholder2->GetId()));

        EXPECT_TRUE(firstComponentModeInstantiated && secondComponentModeInstantiated);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    TEST_F(ComponentModeTestFixture, OneComponentModeBeginsWithTwoComponentsOnSingleEntityEachWithDifferentComponentModes)
    {
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        // setup default editor interaction model
        EditorInteractionSystemViewportSelectionRequestBus::Event(
            GetEntityContextId(), &EditorInteractionSystemViewportSelection::SetDefaultHandler);

        AZ::Entity* entity = nullptr;
        AZ::EntityId entityId = CreateDefaultEditorEntity("ComponentModeEntity", &entity);

        entity->Deactivate();

        // add two placeholder Components (each with their own Component Mode)
        const AZ::Component* placeholder1 = entity->CreateComponent<PlaceholderEditorComponent>();
        const AZ::Component* placeholder2 = entity->CreateComponent<AnotherPlaceholderEditorComponent>();

        entity->Activate();

        // mimic selecting the entity in the viewport (after selection the ComponentModeDelegate
        // connects to the ComponentModeDelegateRequestBus on the entity/component pair address)
        const AzToolsFramework::EntityIdList entityIds = { entityId };
        ToolsApplicationRequestBus::Broadcast(
            &ToolsApplicationRequests::SetSelectedEntities, entityIds);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        // move all selected components into ComponentMode
        // (mimic pressing the 'Edit' button to begin Component Mode)
        ComponentModeSystemRequestBus::Broadcast(
            &ComponentModeSystemRequests::AddSelectedComponentModesOfType,
            AZ::AzTypeInfo<PlaceholderEditorComponent>::Uuid());
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        bool firstComponentModeInstantiated = false;
        ComponentModeSystemRequestBus::BroadcastResult(
            firstComponentModeInstantiated, &ComponentModeSystemRequests::ComponentModeInstantiated,
            AZ::EntityComponentIdPair(entityId, placeholder1->GetId()));

        bool secondComponentModeInstantiated = true;
        ComponentModeSystemRequestBus::BroadcastResult(
            secondComponentModeInstantiated, &ComponentModeSystemRequests::ComponentModeInstantiated,
            AZ::EntityComponentIdPair(entityId, placeholder2->GetId()));

        EXPECT_TRUE(firstComponentModeInstantiated && !secondComponentModeInstantiated);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    TEST_F(ComponentModeTestFixture, TwoComponentsOnSingleEntityWithSameComponentModeDoNotCycle)
    {
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        // setup default editor interaction model
        EditorInteractionSystemViewportSelectionRequestBus::Event(
            GetEntityContextId(), &EditorInteractionSystemViewportSelection::SetDefaultHandler);

        AZ::Entity* entity = nullptr;
        AZ::EntityId entityId = CreateDefaultEditorEntity("ComponentModeEntity", &entity);

        entity->Deactivate();

        // add two placeholder Components (each with their own Component Mode)
        const AZ::Component* placeholder1 = entity->CreateComponent<PlaceholderEditorComponent>();
        AZ_UNUSED(placeholder1);
        const AZ::Component* placeholder2 = entity->CreateComponent<PlaceholderEditorComponent>();
        AZ_UNUSED(placeholder2);

        entity->Activate();

        // mimic selecting the entity in the viewport (after selection the ComponentModeDelegate
        // connects to the ComponentModeDelegateRequestBus on the entity/component pair address)
        const AzToolsFramework::EntityIdList entityIds = { entityId };
        ToolsApplicationRequestBus::Broadcast(
            &ToolsApplicationRequests::SetSelectedEntities, entityIds);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        // move all selected components into ComponentMode
        // (mimic pressing the 'Edit' button to begin Component Mode)
        ComponentModeSystemRequestBus::Broadcast(
            &ComponentModeSystemRequests::AddSelectedComponentModesOfType,
            AZ::AzTypeInfo<PlaceholderEditorComponent>::Uuid());

        bool nextModeCycled = true;
        ComponentModeSystemRequestBus::BroadcastResult(
            nextModeCycled, &ComponentModeSystemRequests::SelectNextActiveComponentMode);

        bool previousModeCycled = true;
        ComponentModeSystemRequestBus::BroadcastResult(
            previousModeCycled, &ComponentModeSystemRequests::SelectPreviousActiveComponentMode);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_TRUE(!nextModeCycled && !previousModeCycled);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    TEST_F(ComponentModeTestFixture, TwoComponentsOnSingleEntityWithSameComponentModeHasOnlyOneType)
    {
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        // setup default editor interaction model
        EditorInteractionSystemViewportSelectionRequestBus::Event(
            GetEntityContextId(), &EditorInteractionSystemViewportSelection::SetDefaultHandler);

        AZ::Entity* entity = nullptr;
        AZ::EntityId entityId = CreateDefaultEditorEntity("ComponentModeEntity", &entity);

        entity->Deactivate();

        // add two placeholder Components (each with their own Component Mode)
        const AZ::Component* placeholder1 = entity->CreateComponent<PlaceholderEditorComponent>();
        AZ_UNUSED(placeholder1);
        const AZ::Component* placeholder2 = entity->CreateComponent<PlaceholderEditorComponent>();
        AZ_UNUSED(placeholder2);

        entity->Activate();

        // mimic selecting the entity in the viewport (after selection the ComponentModeDelegate
        // connects to the ComponentModeDelegateRequestBus on the entity/component pair address)
        const AzToolsFramework::EntityIdList entityIds = { entityId };
        ToolsApplicationRequestBus::Broadcast(
            &ToolsApplicationRequests::SetSelectedEntities, entityIds);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        // move all selected components into ComponentMode
        // (mimic pressing the 'Edit' button to begin Component Mode)
        ComponentModeSystemRequestBus::Broadcast(
            &ComponentModeSystemRequests::AddSelectedComponentModesOfType,
            AZ::AzTypeInfo<PlaceholderEditorComponent>::Uuid());
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        bool multipleComponentModeTypes = true;
        ComponentModeSystemRequestBus::BroadcastResult(
            multipleComponentModeTypes, &ComponentModeSystemRequests::HasMultipleComponentTypes);

        EXPECT_FALSE(multipleComponentModeTypes);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    TEST_F(ComponentModeTestFixture, TwoComponentsOnSingleEntityWithDifferentComponentModeHasOnlyOneType)
    {
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        // setup default editor interaction model
        EditorInteractionSystemViewportSelectionRequestBus::Event(
            GetEntityContextId(), &EditorInteractionSystemViewportSelection::SetDefaultHandler);

        AZ::Entity* entity = nullptr;
        AZ::EntityId entityId = CreateDefaultEditorEntity("ComponentModeEntity", &entity);

        entity->Deactivate();

        // add two placeholder Components (each with their own Component Mode)
        const AZ::Component* placeholder1 = entity->CreateComponent<PlaceholderEditorComponent>();
        AZ_UNUSED(placeholder1);
        const AZ::Component* placeholder2 = entity->CreateComponent<AnotherPlaceholderEditorComponent>();
        AZ_UNUSED(placeholder2);

        entity->Activate();

        // mimic selecting the entity in the viewport (after selection the ComponentModeDelegate
        // connects to the ComponentModeDelegateRequestBus on the entity/component pair address)
        const AzToolsFramework::EntityIdList entityIds = { entityId };
        ToolsApplicationRequestBus::Broadcast(
            &ToolsApplicationRequests::SetSelectedEntities, entityIds);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        // move all selected components into ComponentMode
        // (mimic pressing the 'Edit' button to begin Component Mode)
        ComponentModeSystemRequestBus::Broadcast(
            &ComponentModeSystemRequests::AddSelectedComponentModesOfType,
            AZ::AzTypeInfo<AnotherPlaceholderEditorComponent>::Uuid());
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        bool multipleComponentModeTypes = true;
        ComponentModeSystemRequestBus::BroadcastResult(
            multipleComponentModeTypes, &ComponentModeSystemRequests::HasMultipleComponentTypes);

        EXPECT_FALSE(multipleComponentModeTypes);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    TEST_F(ComponentModeTestFixture, TwoComponentsOnSingleEntityWithDependentComponentModesHasTwoTypes)
    {
        using testing::Eq;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        // setup default editor interaction model
        EditorInteractionSystemViewportSelectionRequestBus::Event(
            GetEntityContextId(), &EditorInteractionSystemViewportSelection::SetDefaultHandler);

        AZ::Entity* entity = nullptr;
        AZ::EntityId entityId = CreateDefaultEditorEntity("ComponentModeEntity", &entity);

        entity->Deactivate();

        // add two placeholder Components (each with their own Component Mode)
        const AZ::Component* placeholder1 = entity->CreateComponent<AnotherPlaceholderEditorComponent>();
        AZ_UNUSED(placeholder1);
        // DependentPlaceholderEditorComponent has a Component Mode dependent on AnotherPlaceholderEditorComponent
        const AZ::Component* placeholder2 = entity->CreateComponent<DependentPlaceholderEditorComponent>();
        AZ_UNUSED(placeholder2);

        entity->Activate();

        // mimic selecting the entity in the viewport (after selection the ComponentModeDelegate
        // connects to the ComponentModeDelegateRequestBus on the entity/component pair address)
        const AzToolsFramework::EntityIdList entityIds = { entityId };
        ToolsApplicationRequestBus::Broadcast(
            &ToolsApplicationRequests::SetSelectedEntities, entityIds);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        // move all selected components into ComponentMode
        // (mimic pressing the 'Edit' button to begin Component Mode)
        ComponentModeSystemRequestBus::Broadcast(
            &ComponentModeSystemRequests::AddSelectedComponentModesOfType,
            AZ::AzTypeInfo<DependentPlaceholderEditorComponent>::Uuid());
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        bool multipleComponentModeTypes = false;
        ComponentModeSystemRequestBus::BroadcastResult(
            multipleComponentModeTypes, &ComponentModeSystemRequests::HasMultipleComponentTypes);

        bool secondComponentModeInstantiated = false;
        ComponentModeSystemRequestBus::BroadcastResult(
            secondComponentModeInstantiated, &ComponentModeSystemRequests::ComponentModeInstantiated,
            AZ::EntityComponentIdPair(entityId, placeholder2->GetId()));

        AZ::Uuid activeComponentType = AZ::Uuid::CreateNull();
        ComponentModeSystemRequestBus::BroadcastResult(
            activeComponentType, &ComponentModeSystemRequests::ActiveComponentMode);

        EXPECT_TRUE(multipleComponentModeTypes);
        EXPECT_TRUE(secondComponentModeInstantiated);
        EXPECT_THAT(activeComponentType, Eq(AZ::AzTypeInfo<DependentPlaceholderEditorComponent>::Uuid()));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    TEST_F(ComponentModeTestFixture, TwoComponentsOnSingleEntityWithSameComponentModeBothTriggerSameAction)
    {
        using testing::Eq;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        const auto viewportHandlerBuilder =
            [this](const AzToolsFramework::EditorVisibleEntityDataCache* entityDataCache)
        {
            // create the default viewport (handles ComponentMode)
            AZStd::unique_ptr<EditorDefaultSelection> defaultSelection =
                AZStd::make_unique<EditorDefaultSelection>(entityDataCache);

            // override the phantom widget so we can use out custom test widget
            defaultSelection->SetOverridePhantomWidget(&m_editorActions.m_testWidget);

            return defaultSelection;
        };

        // setup default editor interaction model with the phantom widget overridden
        EditorInteractionSystemViewportSelectionRequestBus::Event(
            GetEntityContextId(), &EditorInteractionSystemViewportSelection::SetHandler, viewportHandlerBuilder);

        AZ::Entity* entity = nullptr;
        AZ::EntityId entityId = CreateDefaultEditorEntity("ComponentModeEntity", &entity);

        entity->Deactivate();

        // add two placeholder Components (each with their own Component Mode)
        const AZ::Component* placeholder1 = entity->CreateComponent<PlaceholderEditorComponent>();
        const AZ::Component* placeholder2 = entity->CreateComponent<PlaceholderEditorComponent>();

        entity->Activate();

        // mimic selecting the entity in the viewport (after selection the ComponentModeDelegate
        // connects to the ComponentModeDelegateRequestBus on the entity/component pair address)
        const AzToolsFramework::EntityIdList entityIds = { entityId };
        ToolsApplicationRequestBus::Broadcast(
            &ToolsApplicationRequests::SetSelectedEntities, entityIds);

        // move all selected components into ComponentMode
        // (mimic pressing the 'Edit' button to begin Component Mode)
        ComponentModeSystemRequestBus::Broadcast(
            &ComponentModeSystemRequests::AddSelectedComponentModesOfType,
            AZ::AzTypeInfo<PlaceholderEditorComponent>::Uuid());

        // Component Modes are now instantiated

        // create a simple signal checker type which implements the ComponentModeActionSignalNotificationBus
        const int checkerBusId = 1234;
        ComponentModeActionSignalNotificationChecker checker(checkerBusId);

        // when a shortcut action happens, we want to send a message to the checker bus
        // internally PlaceHolderComponentMode sets up an action to send an event to
        // ComponentModeActionSignalNotifications::OnActionTriggered - we make sure each
        // Component Mode is will sent the notification to the correct address.
        ComponentModeActionSignalRequestBus::Event(
            AZ::EntityComponentIdPair(entityId, placeholder1->GetId()),
            &ComponentModeActionSignalRequests::SetComponentModeActionNotificationBusToNotify,
            checkerBusId);

        ComponentModeActionSignalRequestBus::Event(
            AZ::EntityComponentIdPair(entityId, placeholder2->GetId()),
            &ComponentModeActionSignalRequests::SetComponentModeActionNotificationBusToNotify,
            checkerBusId);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        // trigger the shortcut for this Component Mode
        QTest::keyPress(&m_editorActions.m_testWidget, Qt::Key_Space);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        // ensure the checker count is what we expect (both Component Modes will notify the
        // ComponentModeActionSignalNotificationChecker connected at the address specified)
        EXPECT_THAT(checker.GetCount(), Eq(2));
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    TEST_F(ComponentModeTestFixture, ShouldIgnoreMouseEventWhenOverridenByComponentMode)
    {
        using OverrideMouseInteractionComponent = TestComponentModeComponent<OverrideMouseInteractionComponentMode>;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        // Setup default editor interaction model.
        EditorInteractionSystemViewportSelectionRequestBus::Event(
            GetEntityContextId(), &EditorInteractionSystemViewportSelection::SetDefaultHandler);

        AZ::Entity* entity = nullptr;
        AZ::EntityId entityId = CreateDefaultEditorEntity("ComponentModeEntity", &entity);

        entity->Deactivate();

        // Add placeholder component which implements component mode.
        const AZ::Component* placeholder1 = entity->CreateComponent<OverrideMouseInteractionComponent>();

        entity->Activate();

        // Mimic selecting the entity in the viewport (after selection the ComponentModeDelegate
        // connects to the ComponentModeDelegateRequestBus on the entity/component pair address)
        SelectEntities({ entityId });
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        // Move all selected components into ComponentMode
        // (mimic pressing the 'Edit' button to begin Component Mode)
        EnterComponentMode<OverrideMouseInteractionComponent>();

        ViewportInteraction::MouseInteractionEvent interactionEvent;
        interactionEvent.m_mouseEvent = ViewportInteraction::MouseEvent::Move;

        // Simulate a mouse event
        bool handled = false;
        AzToolsFramework::EditorInteractionSystemViewportSelectionRequestBus::BroadcastResult(handled,
            &AzToolsFramework::ViewportInteraction::MouseViewportRequests::HandleMouseInteraction,
            interactionEvent);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        // Check it was handled by the component mode.
        EXPECT_TRUE(handled);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    // Test version of EntityPropertyEditor to detect/ensure certain functions were called
    class TestEntityPropertyEditor
        : public AzToolsFramework::EntityPropertyEditor
    {
    public:
        void InvalidatePropertyDisplay(PropertyModificationRefreshLevel level) override;
        bool m_invalidatePropertyDisplayCalled = false;
    };

    void TestEntityPropertyEditor::InvalidatePropertyDisplay(PropertyModificationRefreshLevel level)
    {
        m_invalidatePropertyDisplayCalled = true;
    }

    // Simple fixture to encapsulate a TestEntityPropertyEditor
    class ComponentModePinnedSelectionFixture
        : public ToolsApplicationFixture
    {
    public:
        void SetUpEditorFixtureImpl() override
        {
            EditorInteractionSystemViewportSelectionRequestBus::Event(
                GetEntityContextId(), &EditorInteractionSystemViewportSelection::SetDefaultHandler);

            m_testEntityPropertyEditor = AZStd::make_unique<TestEntityPropertyEditor>();
        }

        AZStd::unique_ptr<TestEntityPropertyEditor> m_testEntityPropertyEditor;
    };

    TEST_F(ComponentModePinnedSelectionFixture, CannotEnterComponentModeWhenEntityIsPinnedButNotSelected)
    {
        using PlaceHolderComponent = TestComponentModeComponent<PlaceHolderComponentMode>;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Given
        AZ::Entity* entity = nullptr;
        AZ::EntityId entityId = CreateDefaultEditorEntity("ComponentModeEntity", &entity);

        entity->Deactivate();

        // Add placeholder component which implements component mode.
        entity->CreateComponent<PlaceHolderComponent>();

        entity->Activate();
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // When
        // select entity
        const auto selectedEntities = AzToolsFramework::EntityIdList { entityId };
        SelectEntities(selectedEntities);

        // pin entity
        m_testEntityPropertyEditor->SetOverrideEntityIds(selectedEntities);

        // deselect entity
        SelectEntities(AzToolsFramework::EntityIdList{});
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Then
        EXPECT_TRUE(m_testEntityPropertyEditor->IsLockedToSpecificEntities());
        EXPECT_TRUE(m_testEntityPropertyEditor->m_invalidatePropertyDisplayCalled);

        bool couldBeginComponentMode =
            AzToolsFramework::ComponentModeFramework::CouldBeginComponentModeWithEntity(entityId);

        EXPECT_FALSE(couldBeginComponentMode);
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }
} // namespace UnitTest
