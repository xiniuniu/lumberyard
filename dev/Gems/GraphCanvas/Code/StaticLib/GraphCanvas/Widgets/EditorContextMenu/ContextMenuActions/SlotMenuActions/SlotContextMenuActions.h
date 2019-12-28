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

#include <AzCore/Memory/SystemAllocator.h>

#include <GraphCanvas/Widgets/EditorContextMenu/ContextMenuActions/SlotMenuActions/SlotContextMenuAction.h>

namespace GraphCanvas
{
    class AddSlotMenuAction
        : public SlotContextMenuAction
    {
    public:
        AZ_CLASS_ALLOCATOR(AddSlotMenuAction, AZ::SystemAllocator, 0);

        AddSlotMenuAction(QObject* parent);
        virtual ~AddSlotMenuAction() = default;

        void RefreshAction(const GraphId& graphId, const AZ::EntityId& targetId) override;
        SceneReaction TriggerAction(const GraphId& graphId, const AZ::Vector2& scenePos) override;

    private:
        AZ::EntityId m_targetId;
    };

    class RemoveSlotMenuAction
        : public SlotContextMenuAction
    {
    public:
        AZ_CLASS_ALLOCATOR(RemoveSlotMenuAction, AZ::SystemAllocator, 0);

        RemoveSlotMenuAction(QObject* parent);
        virtual ~RemoveSlotMenuAction() = default;

        void RefreshAction(const GraphId& graphId, const AZ::EntityId& targetId) override;
        SceneReaction TriggerAction(const GraphId& graphId, const AZ::Vector2& scenePos) override;

    private:
        AZ::EntityId m_targetId;
    };


    class ClearConnectionsMenuAction
        : public SlotContextMenuAction
    {
    public:
        AZ_CLASS_ALLOCATOR(ClearConnectionsMenuAction, AZ::SystemAllocator, 0);
        
        ClearConnectionsMenuAction(QObject* parent);
        virtual ~ClearConnectionsMenuAction() = default;

        void RefreshAction(const GraphId& graphId, const AZ::EntityId& targetId) override;
        SceneReaction TriggerAction(const GraphId& graphId, const AZ::Vector2& scenePos) override;
    
    private:
        AZ::EntityId m_targetId;
    };

    //////////////////////
    // Data Slot Actions
    //////////////////////

    class ResetToDefaultValueMenuAction
        : public SlotContextMenuAction
    {
    public:
        AZ_CLASS_ALLOCATOR(ResetToDefaultValueMenuAction, AZ::SystemAllocator, 0);

        ResetToDefaultValueMenuAction(QObject* parent);
        virtual ~ResetToDefaultValueMenuAction() = default;

        void RefreshAction(const GraphId& graphId, const AZ::EntityId& targetId) override;
        SceneReaction TriggerAction(const GraphId& graphId, const AZ::Vector2& scenePos) override;

    private:
        AZ::EntityId m_targetId;
    };
}