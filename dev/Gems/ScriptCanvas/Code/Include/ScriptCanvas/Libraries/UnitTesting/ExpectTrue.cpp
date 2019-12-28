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

#include "ExpectTrue.h"

#include "UnitTestBus.h"

namespace ScriptCanvas
{
    namespace Nodes
    {
        namespace UnitTesting
        {
            void ExpectTrue::OnInit()
            {
                // DYNAMIC_SLOT_VERSION_CONVERTER
                auto candidateSlot = GetSlot(GetSlotId("Candidate"));

                if (candidateSlot && !candidateSlot->IsDynamicSlot())
                {
                    candidateSlot->SetDynamicDataType(DynamicDataType::Any);
                }

                auto referenceSlot = GetSlot(GetSlotId("Reference"));

                if (referenceSlot && !referenceSlot->IsDynamicSlot())
                {
                    referenceSlot->SetDynamicDataType(DynamicDataType::Any);
                }
                ////
            }

            void ExpectTrue::OnInputSignal(const SlotId& slotId)
            {
                const auto value = GetInput(GetSlotId("Candidate"))->GetAs<Data::BooleanType>();
                const auto report = GetInput(GetSlotId("Report"))->GetAs<Data::StringType>();

                ScriptCanvas::UnitTesting::Bus::Event
                          ( GetGraphId()
                          , &ScriptCanvas::UnitTesting::BusTraits::ExpectTrue
                          , *value
                          , *report);
                
                SignalOutput(GetSlotId("Out"));
            }
        } // namespace UnitTesting
    } // namespace Nodes
} // namespace ScriptCanvas

#include <Include/ScriptCanvas/Libraries/UnitTesting/ExpectTrue.generated.cpp>
