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

#include <AzToolsFramework/ComponentMode/EditorComponentModeBus.h>
#include <AzToolsFramework/Viewport/ActionBus.h>

namespace AzToolsFramework
{
    namespace ComponentModeFramework
    {
         /// Abstract class to be inherited from by concrete ComponentModes.
         /// Exposes ComponentMode interface and handles some useful common
         /// functionality all ComponentModes require.
        class EditorBaseComponentMode
            : public ComponentMode
            , private ToolsApplicationNotificationBus::Handler
            , private ComponentModeRequestBus::Handler
        {
        public:
            AZ_CLASS_ALLOCATOR_DECL

            /// @cond
            EditorBaseComponentMode(
                const AZ::EntityComponentIdPair& entityComponentIdPair, AZ::Uuid componentType);
            virtual ~EditorBaseComponentMode();
            /// @endcond

            /// ComponentMode interface - populate actions for this ComponentMode.
            /// When PopulateActions is called, if a second action override is found with the
            /// same key, it should override the existing action if one already exists.
            /// (e.g. The 'escape' key will first deselect a vertex, then leave ComponentMode if
            /// an action is added to deselect a vertex when one is selected)
            /// @attention More specific actions come later in the ordering when they are added.
            AZStd::vector<ActionOverride> PopulateActions() final;

        protected:
            /// The EntityId this ComponentMode instance is associated with.
            AZ::EntityId GetEntityId() const { return m_entityComponentIdPair.GetEntityId(); }
            /// The combined Entity and Component Id to uniquely identify a specific Component on a given Entity.
            /// Note: This is required when more than one Component of the same type can exists on an Entity at a time.
            AZ_DEPRECATED(, "GetComponentEntityIdPair() is deprecated, please use GetEntityComponentIdPair()")
            AZ::EntityComponentIdPair GetComponentEntityIdPair() const { return m_entityComponentIdPair; }
            /// The combined Entity and Component Id to uniquely identify a specific Component on a given Entity.
            /// Note: This is required when more than one Component of the same type can exists on an Entity at a time.
            AZ::EntityComponentIdPair GetEntityComponentIdPair() const { return m_entityComponentIdPair; }

            /// The ComponentId this ComponentMode instance is associated with.
            AZ::ComponentId GetComponentId() const final { return m_entityComponentIdPair.GetComponentId(); }
            /// The underlying Component type for this ComponentMode.
            AZ::Uuid GetComponentType() const final { return m_componentType; }

        private:
            AZ_DISABLE_COPY_MOVE(EditorBaseComponentMode)

            /// EditorBaseComponentMode interface
            /// @see To be overridden by derived ComponentModes
            virtual AZStd::vector<ActionOverride> PopulateActionsImpl();

            // ToolsApplicationNotificationBus
            void AfterUndoRedo() override;

            AZ::EntityComponentIdPair m_entityComponentIdPair; ///< Entity and Component Id associated with this ComponentMode.
            AZ::Uuid m_componentType; ///< The underlying type of the Component this ComponentMode is for.
        };
    } // namespace ComponentModeFramework
} // namespace AzToolsFramework