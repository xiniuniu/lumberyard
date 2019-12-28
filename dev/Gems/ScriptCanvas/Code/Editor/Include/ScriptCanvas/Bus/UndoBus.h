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

#include <AzCore/EBus/EBus.h>
#include <AzCore/std/any.h>
#include <AzCore/std/string/string_view.h>
#include <AzToolsFramework/Undo/UndoSystem.h>

#include <GraphCanvas/Types/EntitySaveData.h>
#include <ScriptCanvas/Core/GraphData.h>
#include <ScriptCanvas/Variable/VariableData.h>

namespace ScriptCanvasEditor
{
    enum class UndoGraphCommand
    {
        ChangeItem,
        AddItem,
        RemoveItem
    };

    struct UndoData
    {
        AZ_TYPE_INFO(UndoData, "{12561F1F-2806-4BCB-BDC5-B2F2B568A139}");
        AZ_CLASS_ALLOCATOR(UndoData, AZ::SystemAllocator, 0);
        UndoData() = default;

        static void Reflect(AZ::ReflectContext* reflectContext)
        {
            if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(reflectContext))
            {
                serializeContext->Class<UndoData>()
                    ->Version(2)
                    ->Field("m_graphData", &UndoData::m_graphData)
                    ->Field("m_variableData", &UndoData::m_variableData)
                    ->Field("m_visualSaveData",&UndoData::m_visualSaveData)
                    ;
            }
        }

        ScriptCanvas::GraphData             m_graphData;
        ScriptCanvas::VariableData          m_variableData;
        AZStd::unordered_map< AZ::EntityId, GraphCanvas::EntitySaveDataContainer* > m_visualSaveData;
    };

    class UndoCache;
    class UndoRequests
        : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        /*!
        * Retrieves the undo cache for storing Script Canvas data
        */
        virtual UndoCache* GetActiveSceneUndoCache() = 0;

        /*!
        * Retrieves the undo cache for specific Script Canvas entity
        */
        virtual UndoCache* GetSceneUndoCache(AZ::EntityId sceneId) = 0;

        /*!
        * Start batch undo so we can add multiple undo to one undo step.
        */
        virtual void BeginUndoBatch(AZStd::string_view label) = 0;

        /*!
        * End current undo batch and save it to undo stack.
        */
        virtual void EndUndoBatch() = 0;

        /*!
        * Add a undo to stack. it could be added directly to stack or added to the current batch.
        */
        virtual void AddUndo(AzToolsFramework::UndoSystem::URSequencePoint* seqPoint) = 0;

        virtual void AddGraphItemChangeUndo(AZ::Entity* scriptCanvasEntity, AZStd::string_view undoLabel) = 0;
        virtual void AddGraphItemAdditionUndo(AZ::Entity* scriptCanvasEntity, AZStd::string_view undoLabel) = 0;
        virtual void AddGraphItemRemovalUndo(AZ::Entity* scriptCanvasEntity, AZStd::string_view undoLabel)  = 0;

        virtual void Undo() = 0;
        virtual void Redo() = 0;
        virtual void Reset() = 0;

        virtual UndoData CreateUndoData(AZ::EntityId entityId) = 0;
    };
    using UndoRequestBus = AZ::EBus<UndoRequests>;

    class UndoNotifications
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////

        virtual void OnCanUndoChanged(bool canUndo) {}
        virtual void OnCanRedoChanged(bool canRedo) {}

    };
    using UndoNotificationBus = AZ::EBus<UndoNotifications>;

}
