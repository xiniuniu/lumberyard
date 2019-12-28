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
#include <QApplication>
#include <QClipboard>
#include <QMimeData>

#include <GraphCanvas/Widgets/EditorContextMenu/ContextMenuActions/EditMenuActions/EditContextMenuActions.h>

#include <GraphCanvas/Components/SceneBus.h>

namespace GraphCanvas
{
    ////////////////////////////////
    // CutGraphSelectionMenuAction
    ////////////////////////////////

    CutGraphSelectionMenuAction::CutGraphSelectionMenuAction(QObject* parent)
        : EditContextMenuAction("Cut", parent)
    {
    }

    ContextMenuAction::SceneReaction CutGraphSelectionMenuAction::TriggerAction(const GraphId& graphId, const AZ::Vector2& scenePos)
    {
        SceneRequestBus::Event(graphId, &SceneRequests::CutSelection);
        return SceneReaction::PostUndo;
    }

    /////////////////////////////////
    // CopyGraphSelectionMenuAction
    /////////////////////////////////

    CopyGraphSelectionMenuAction::CopyGraphSelectionMenuAction(QObject* parent)
        : EditContextMenuAction("Copy", parent)
    {
    }

    ContextMenuAction::SceneReaction CopyGraphSelectionMenuAction::TriggerAction(const GraphId& graphId, const AZ::Vector2& scenePos)
    {
        SceneRequestBus::Event(graphId, &SceneRequests::CopySelection);
        return SceneReaction::PostUndo;
    }

    //////////////////////////////////
    // PasteGraphSelectionMenuAction
    //////////////////////////////////

    PasteGraphSelectionMenuAction::PasteGraphSelectionMenuAction(QObject* parent)
        : EditContextMenuAction("Paste", parent)
    {
    }

    void PasteGraphSelectionMenuAction::RefreshAction(const GraphId& graphId, const AZ::EntityId& targetId)
    {
        AZStd::string mimeType;
        SceneRequestBus::EventResult(mimeType, graphId, &SceneRequests::GetCopyMimeType);

        bool isPasteable = QApplication::clipboard()->mimeData()->hasFormat(mimeType.c_str());
        setEnabled(isPasteable);
    }

    ContextMenuAction::SceneReaction PasteGraphSelectionMenuAction::TriggerAction(const GraphId& graphId, const AZ::Vector2& scenePos)
    {
        SceneRequestBus::Event(graphId, &SceneRequests::PasteAt, QPoint(scenePos.GetX(), scenePos.GetY()));
        return SceneReaction::PostUndo;
    }
    
    ///////////////////////////////////
    // DeleteGraphSelectionMenuAction
    ///////////////////////////////////

    DeleteGraphSelectionMenuAction::DeleteGraphSelectionMenuAction(QObject* parent)
        : EditContextMenuAction("Delete", parent)
    {
    }

    ContextMenuAction::SceneReaction DeleteGraphSelectionMenuAction::TriggerAction(const GraphId& graphId, const AZ::Vector2& scenePos)
    {
        SceneRequestBus::Event(graphId, &SceneRequests::DeleteSelection);
        return SceneReaction::PostUndo;
    }

    //////////////////////////////////////
    // DuplicateGraphSelectionMenuAction
    //////////////////////////////////////

    DuplicateGraphSelectionMenuAction::DuplicateGraphSelectionMenuAction(QObject* parent)
        : EditContextMenuAction("Duplicate", parent)
    {
    }

    ContextMenuAction::SceneReaction DuplicateGraphSelectionMenuAction::TriggerAction(const GraphId& graphId, const AZ::Vector2& scenePos)
    {
        SceneRequestBus::Event(graphId, &SceneRequests::DuplicateSelectionAt, QPoint(scenePos.GetX(), scenePos.GetY()));
        return SceneReaction::PostUndo;
    }
}