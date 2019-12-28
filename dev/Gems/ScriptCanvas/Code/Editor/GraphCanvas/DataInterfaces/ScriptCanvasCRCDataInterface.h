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

#include <GraphCanvas/Components/NodePropertyDisplay/StringDataInterface.h>

#include "ScriptCanvasDataInterface.h"

#include <ScriptCanvas/Bus/EditorScriptCanvasBus.h>

namespace ScriptCanvasEditor
{
    class ScriptCanvasCRCDataInterface
        : public ScriptCanvasDataInterface<GraphCanvas::StringDataInterface>
    {
    public:
        AZ_CLASS_ALLOCATOR(ScriptCanvasCRCDataInterface, AZ::SystemAllocator, 0);
        ScriptCanvasCRCDataInterface(const AZ::EntityId& nodeId, const ScriptCanvas::SlotId& slotId)
            : ScriptCanvasDataInterface(nodeId, slotId)
        {
        }

        ~ScriptCanvasCRCDataInterface() = default;

        // StringDataInterface
        AZStd::string GetString() const override
        {
            AZStd::string retVal;

            const ScriptCanvas::Datum* object = GetSlotObject();

            if (object && object->GetAs<AZ::Crc32>())
            {
                AZ::Crc32 crcValue = (*object->GetAs<AZ::Crc32>());
                EditorGraphRequestBus::EventResult(retVal, GetScriptCanvasGraphEntityId(), &EditorGraphRequests::DecodeCrc, crcValue);

                if (retVal.empty() && crcValue != AZ::Crc32())
                {
                    AZ_Warning("ScriptCanvas", false, "Unknown CRC value. Cannot display cached string.");
                    retVal = AZStd::string::format("0x%X", crcValue);
                }
            }

            return retVal;
        }
        
        void SetString(const AZStd::string& value) override
        {
            ScriptCanvas::Datum* object = GetSlotObject();

            if (object)
            {
                AZ::Crc32 newCrc = AZ::Crc32(value.c_str());
                AZ::Crc32 oldCrc = (*object->GetAs<AZ::Crc32>());

                if (oldCrc != newCrc)
                {
                    EditorGraphRequestBus::Event(GetScriptCanvasGraphEntityId(), &EditorGraphRequests::RemoveCrcCache, oldCrc);
                    EditorGraphRequestBus::Event(GetScriptCanvasGraphEntityId(), &EditorGraphRequests::AddCrcCache, newCrc, value);

                    object->Set(newCrc);

                    PostUndoPoint();
                    PropertyGridRequestBus::Broadcast(&PropertyGridRequests::RefreshPropertyGrid);
                }
            }
        }
        ////
    };
}
