
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
#include <GraphCanvas/Widgets/EditorContextMenu/ContextMenuActions/SceneMenuActions/SceneContextMenuActions.h>

#include <GraphCanvas/Components/SceneBus.h>
#include <GraphCanvas/Utils/GraphUtils.h>

namespace GraphCanvas
{
    ///////////////////////////////////
    // RemoveUnusedElementsMenuAction
    ///////////////////////////////////
    
    RemoveUnusedElementsMenuAction::RemoveUnusedElementsMenuAction(QObject* parent)
        : SceneContextMenuAction("All", parent)
    {
        setToolTip("Removes all of the unused elements from the active graph");
    }
    
    void RemoveUnusedElementsMenuAction::RefreshAction(const GraphId& graphId, const AZ::EntityId& targetId)
    {
        setEnabled(true);
    }

    bool RemoveUnusedElementsMenuAction::IsInSubMenu() const
    {
        return true;
    }

    AZStd::string RemoveUnusedElementsMenuAction::GetSubMenuPath() const
    {
        return "Remove Unused";
    }

    ContextMenuAction::SceneReaction RemoveUnusedElementsMenuAction::TriggerAction(const GraphId& graphId, const AZ::Vector2& scenePos)
    {
        SceneRequestBus::Event(graphId, &SceneRequests::RemoveUnusedElements);
        return SceneReaction::PostUndo;
    }

    ////////////////////////////////
    // RemoveUnusedNodesMenuAction
    ////////////////////////////////

    RemoveUnusedNodesMenuAction::RemoveUnusedNodesMenuAction(QObject* parent)
        : SceneContextMenuAction("Nodes", parent)
    {
        setToolTip("Removes all of the unused nodes from the active graph");
    }

    void RemoveUnusedNodesMenuAction::RefreshAction(const GraphId& graphId, const AZ::EntityId& targetId)
    {
        setEnabled(true);
    }

    bool RemoveUnusedNodesMenuAction::IsInSubMenu() const
    {
        return true;
    }

    AZStd::string RemoveUnusedNodesMenuAction::GetSubMenuPath() const
    {
        return "Remove Unused";
    }

    ContextMenuAction::SceneReaction RemoveUnusedNodesMenuAction::TriggerAction(const GraphId& graphId, const AZ::Vector2& scenePos)
    {
        SceneRequestBus::Event(graphId, &SceneRequests::RemoveUnusedNodes);
        return SceneReaction::PostUndo;
    }
}