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

#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Quaternion.h>

#include <GraphCanvas/Components/NodePropertyDisplay/VectorDataInterface.h>

#include "ScriptCanvasDataInterface.h"

namespace ScriptCanvasEditor
{
    class ScriptCanvasQuaternionDataInterface
        : public ScriptCanvasDataInterface<GraphCanvas::VectorDataInterface>
    {
    public:
        AZ_CLASS_ALLOCATOR(ScriptCanvasQuaternionDataInterface, AZ::SystemAllocator, 0);
        ScriptCanvasQuaternionDataInterface(const AZ::EntityId& nodeId, const ScriptCanvas::SlotId& slotId)
            : ScriptCanvasDataInterface(nodeId, slotId)
        {
            ConvertToEulerValues();
        }
        
        ~ScriptCanvasQuaternionDataInterface() = default;
        
        int GetElementCount() const override
        {
            return 3;
        }
        
        double GetValue(int index) const override
        {
            if (index < GetElementCount())
            {
                const ScriptCanvas::Datum* object = GetSlotObject();

                if (object)
                {
                    const AZ::Quaternion* retVal = object->GetAs<AZ::Quaternion>();

                    if (retVal)
                    {
                        return aznumeric_cast<double>(static_cast<float>(m_eulerAngles.GetElement(index)));
                    }
                }
            }
            
            return 0.0;
        }

        void SetValue(int index, double value) override
        {
            if (index < GetElementCount())
            {
                ScriptCanvas::Datum* object = GetSlotObject();

                if (object)
                {
                    AZ::Quaternion* currentQuat = const_cast<AZ::Quaternion*>(object->GetAs<AZ::Quaternion>());
                    
                    m_eulerAngles.SetElement(index,value);

                    AZ::Transform eulerRepresentation = AZ::ConvertEulerDegreesToTransform(m_eulerAngles);
                    AZ::Quaternion newValue = AZ::Quaternion::CreateFromTransform(eulerRepresentation);

                    (*currentQuat) = static_cast<AZ::Quaternion>(newValue);      
                    
                    PostUndoPoint();
                    PropertyGridRequestBus::Broadcast(&PropertyGridRequests::RefreshPropertyGrid);
                }
            }
        }        
        
        const char* GetLabel(int index) const override
        {
            switch (index)
            {
            case 0:
                return "P";
            case 1:
                return "Y";
            case 2:
                return "R";
            default:
                return "???";
            }
        }
        
        AZStd::string GetStyle() const override
        {
            return "vectorized";
        }
        
        AZStd::string GetElementStyle(int index) const override
        {
            return AZStd::string::format("quat_%i", index);
        }
        
        const char* GetSuffix(int index) const override
        {
            return " deg";
        }

        void OnInputChanged(const ScriptCanvas::SlotId& slotId) override
        {
            if (slotId == GetSlotId())
            {
                ConvertToEulerValues();
            }

            ScriptCanvasDataInterface::OnInputChanged(slotId);
        }

    private:
        void ConvertToEulerValues()
        {
            const ScriptCanvas::Datum* object = GetSlotObject();

            if (object)
            {
                const AZ::Quaternion* quat = object->GetAs<AZ::Quaternion>();
            
                if (quat)
                {
                    m_eulerAngles = AZ::ConvertTransformToEulerDegrees(AZ::Transform::CreateFromQuaternion((*quat)));
                }
            }
        }

        AZ::Vector3 m_eulerAngles;
    };
}