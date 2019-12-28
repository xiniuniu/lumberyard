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

#include "OperatorSize.h"
#include <ScriptCanvas/Libraries/Core/MethodUtility.h>
#include <ScriptCanvas/Core/Contracts/SupportsMethodContract.h>

#include <ScriptCanvas/Utils/SerializationUtils.h>

namespace ScriptCanvas
{
    namespace Nodes
    {
        namespace Operators
        {
            /////////////////
            // OperatorSize
            /////////////////

            bool OperatorSize::OperatorSizeVersionConverter(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement)
            {
                // Remove the now unnecessary OperatorBase class from the inheritance chain.
                if (classElement.GetVersion() < 1)
                {
                    AZ::SerializeContext::DataElementNode* operatorBaseClass = classElement.FindSubElement(AZ::Crc32("BaseClass1"));

                    if (operatorBaseClass == nullptr)
                    {
                        return false;
                    }

                    int nodeElementIndex = operatorBaseClass->FindElement(AZ_CRC("BaseClass1", 0xd4925735));

                    if (nodeElementIndex < 0)
                    {
                        return false;
                    }

                    // The DataElementNode is being copied purposefully in this statement to clone the data
                    AZ::SerializeContext::DataElementNode baseNodeElement = operatorBaseClass->GetSubElement(nodeElementIndex);

                    classElement.RemoveElementByName(AZ::Crc32("BaseClass1"));
                    classElement.AddElement(baseNodeElement);                    
                }

                return true;
            }

            void OperatorSize::OnInit()
            {
                // Version Conversion away from Operator Base
                if (HasSlots())
                {
                    const Slot* slot = GetSlot(OperatorSizeProperty::GetSizeSlotId(this));
                    if (slot == nullptr)
                    {
                        ConfigureSlots();
                    }
                }
                ////
            }

            void OperatorSize::OnInputSignal(const SlotId& slotId)
            {
                const SlotId inSlotId = OperatorSizeProperty::GetInSlotId(this);
                if (slotId == inSlotId)
                {
                    bool pushedSize = false;

                    SlotId sourceSlotId = OperatorSizeProperty::GetSourceSlotId(this);
                    SlotId sizeSlotId = OperatorSizeProperty::GetSizeSlotId(this);

                    const Datum* containerDatum = GetInput(sourceSlotId);

                    if (Datum::IsValidDatum(containerDatum))
                    {
                        // Get the size of the container
                        auto sizeOutcome = BehaviorContextMethodHelper::CallMethodOnDatum(*containerDatum, "Size");
                        if (!sizeOutcome)
                        {
                            SCRIPTCANVAS_REPORT_ERROR((*this), "Failed to get size of container: %s", sizeOutcome.GetError().c_str());
                            return;
                        }

                        // Index
                        Datum sizeResult = sizeOutcome.TakeValue();
                        const size_t* sizePtr = sizeResult.GetAs<size_t>();

                        PushOutput(sizeResult, *GetSlot(sizeSlotId));                        
                    }
                    else
                    {
                        Datum zero(0);
                        PushOutput(zero, *GetSlot(sizeSlotId));
                    }

                    SignalOutput(OperatorSizeProperty::GetOutSlotId(this));
                }
            }
        }
    }
}

#include <Include/ScriptCanvas/Libraries/Operators/Containers/OperatorSize.generated.cpp>