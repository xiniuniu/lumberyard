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

#include "ComponentModeTestDoubles.h"

#include <AzCore/Serialization/SerializeContext.h>

#ifdef AZ_TESTS_ENABLED

namespace AzToolsFramework
{
    namespace ComponentModeFramework
    {
        AZ_CLASS_ALLOCATOR_IMPL(PlaceHolderComponentMode, AZ::SystemAllocator, 0)
        AZ_CLASS_ALLOCATOR_IMPL(AnotherPlaceHolderComponentMode, AZ::SystemAllocator, 0)
        AZ_CLASS_ALLOCATOR_IMPL(OverrideMouseInteractionComponentMode, AZ::SystemAllocator, 0)

        void PlaceholderEditorComponent::Reflect(AZ::ReflectContext* context)
        {
            if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
            {
                serializeContext->Class<PlaceholderEditorComponent>()
                    ->Version(0)
                    ->Field("ComponentMode", &PlaceholderEditorComponent::m_componentModeDelegate);
            }
        }

        void PlaceholderEditorComponent::Activate()
        {
            EditorComponentBase::Activate();

            m_componentModeDelegate.ConnectWithSingleComponentMode<
                PlaceholderEditorComponent, PlaceHolderComponentMode>(
                    AZ::EntityComponentIdPair(GetEntityId(), GetId()), nullptr);
        }

        void PlaceholderEditorComponent::Deactivate()
        {
            EditorComponentBase::Deactivate();

            m_componentModeDelegate.Disconnect();
        }

        void AnotherPlaceholderEditorComponent::Reflect(AZ::ReflectContext* context)
        {
            if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
            {
                serializeContext->Class<AnotherPlaceholderEditorComponent>()
                    ->Version(0)
                    ->Field("ComponentMode", &AnotherPlaceholderEditorComponent::m_componentModeDelegate);
            }
        }

        void AnotherPlaceholderEditorComponent::Activate()
        {
            EditorComponentBase::Activate();

            m_componentModeDelegate.ConnectWithSingleComponentMode<
                AnotherPlaceholderEditorComponent, PlaceHolderComponentMode>(
                    AZ::EntityComponentIdPair(GetEntityId(), GetId()), nullptr);
        }

        void AnotherPlaceholderEditorComponent::Deactivate()
        {
            EditorComponentBase::Deactivate();

            m_componentModeDelegate.Disconnect();
        }

        void DependentPlaceholderEditorComponent::Reflect(AZ::ReflectContext* context)
        {
            if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
            {
                serializeContext->Class<DependentPlaceholderEditorComponent>()
                    ->Version(0)
                    ->Field("ComponentMode", &DependentPlaceholderEditorComponent::m_componentModeDelegate);
            }
        }

        void DependentPlaceholderEditorComponent::Activate()
        {
            EditorComponentBase::Activate();

            // connect the ComponentMode delegate to this entity/component id pair
            m_componentModeDelegate.Connect<DependentPlaceholderEditorComponent>(AZ::EntityComponentIdPair(GetEntityId(), GetId()), nullptr);
            // setup the ComponentMode(s) to add for the editing of this Component (in this case Spline and Tube ComponentModes)
            m_componentModeDelegate.SetAddComponentModeCallback([this](const AZ::EntityComponentIdPair& entityComponentIdPair)
            {
                using namespace AzToolsFramework::ComponentModeFramework;

                // builder for PlaceHolderComponentMode for DependentPlaceholderEditorComponent
                const auto placeholdComponentModeBuilder =
                    CreateComponentModeBuilder<DependentPlaceholderEditorComponent, PlaceHolderComponentMode>(
                        entityComponentIdPair);

                // must have AnotherPlaceholderEditorComponent when using DependentPlaceholderEditorComponent
                const auto componentId = GetEntity()->FindComponent<AnotherPlaceholderEditorComponent>()->GetId();
                const auto anotherPlaceholdComponentModeBuilder =
                    CreateComponentModeBuilder<AnotherPlaceholderEditorComponent, AnotherPlaceHolderComponentMode>(
                        AZ::EntityComponentIdPair(GetEntityId(), componentId));

                // aggregate builders
                const auto entityAndComponentModeBuilder =
                    EntityAndComponentModeBuilders(
                        GetEntityId(), { placeholdComponentModeBuilder, anotherPlaceholdComponentModeBuilder });

                // updates modes to add when entering ComponentMode
                ComponentModeSystemRequestBus::Broadcast(
                    &ComponentModeSystemRequests::AddComponentModes, entityAndComponentModeBuilder);
            });
        }

        void DependentPlaceholderEditorComponent::Deactivate()
        {
            EditorComponentBase::Deactivate();
            m_componentModeDelegate.Disconnect();
        }

        ComponentModeActionSignalNotificationChecker::ComponentModeActionSignalNotificationChecker(int busId)
        {
            ComponentModeActionSignalNotificationBus::Handler::BusConnect(busId);
        }

        ComponentModeActionSignalNotificationChecker::~ComponentModeActionSignalNotificationChecker()
        {
            ComponentModeActionSignalNotificationBus::Handler::BusDisconnect();
        }

        void ComponentModeActionSignalNotificationChecker::OnActionTriggered()
        {
            m_counter++;
        }

        PlaceHolderComponentMode::PlaceHolderComponentMode(
            const AZ::EntityComponentIdPair& entityComponentIdPair, AZ::Uuid componentType)
            : EditorBaseComponentMode(entityComponentIdPair, componentType)
        {
            ComponentModeActionSignalRequestBus::Handler::BusConnect(entityComponentIdPair);
        }

        PlaceHolderComponentMode::~PlaceHolderComponentMode()
        {
            ComponentModeActionSignalRequestBus::Handler::BusDisconnect();
        }

        AZStd::vector<AzToolsFramework::ActionOverride> PlaceHolderComponentMode::PopulateActionsImpl()
        {
            const AZ::Crc32 placeHolderComponentModeAction = AZ_CRC("com.amazon.action.placeholder.test", 0x77a171ec);

            return AZStd::vector<AzToolsFramework::ActionOverride>
            {
                // setup an event to notify us when an action fires
                AzToolsFramework::ActionOverride()
                    .SetUri(placeHolderComponentModeAction)
                    .SetKeySequence(QKeySequence(Qt::Key_Space))
                    .SetTitle("Test action")
                    .SetTip("This is a test action")
                    .SetEntityComponentIdPair(AZ::EntityComponentIdPair(GetEntityId(), GetComponentId()))
                    .SetCallback([this]()
                {
                    ComponentModeActionSignalNotificationBus::Event(
                        m_componentModeActionSignalNotificationBusId, &ComponentModeActionSignalNotifications::OnActionTriggered);
                })
            };
        }

        void PlaceHolderComponentMode::SetComponentModeActionNotificationBusToNotify(const int busId)
        {
            m_componentModeActionSignalNotificationBusId = busId;
        }

        AnotherPlaceHolderComponentMode::AnotherPlaceHolderComponentMode(
            const AZ::EntityComponentIdPair& entityComponentIdPair, AZ::Uuid componentType)
            : EditorBaseComponentMode(entityComponentIdPair, componentType) {}

        OverrideMouseInteractionComponentMode::OverrideMouseInteractionComponentMode(
            const AZ::EntityComponentIdPair& entityComponentIdPair, AZ::Uuid componentType)
            : EditorBaseComponentMode(entityComponentIdPair, componentType) {}

    } // namespace ComponentModeFramework
} // namespace AzToolsFramework

#endif // AZ_TESTS_ENABLED
