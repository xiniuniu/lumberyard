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

#include "OperatorClear.h"

#include <ScriptCanvas/Core/Contracts/SupportsMethodContract.h>
#include <ScriptCanvas/Libraries/Core/MethodUtility.h>
#include <ScriptCanvas/Core/Core.h>

#include <ScriptCanvas/Utils/SerializationUtils.h>

namespace ScriptCanvas
{
    namespace Nodes
    {
        namespace Operators
        {
            bool OperatorClear::OperatorClearVersionConverter(AZ::SerializeContext& serializeContext, AZ::SerializeContext::DataElementNode& rootElement)
            {
                if (rootElement.GetVersion() < Version::RemoveOperatorBase)
                {
                    // Remove OperatorBase
                    if (!SerializationUtils::RemoveBaseClass(serializeContext, rootElement))
                    {
                        return false;
                    }
                }

                return true;
            }

            void OperatorClear::OnInputSignal(const SlotId& slotId)
            {
                const SlotId inSlotId = OperatorBaseProperty::GetInSlotId(this);

                if (slotId != inSlotId)
                {
                    return;
                }

                SlotId sourceSlotId = OperatorClearProperty::GetSourceSlotId(this);

                if (const Datum* containerDatum = GetInput(sourceSlotId))
                {
                    if (Datum::IsValidDatum(containerDatum))
                    {
                        AZ::Outcome<Datum, AZStd::string> clearOutcome = BehaviorContextMethodHelper::CallMethodOnDatum(*containerDatum, "Clear");
                        if (!clearOutcome.IsSuccess())
                        {
                            SCRIPTCANVAS_REPORT_ERROR((*this), "Failed to call Clear on container: %s", clearOutcome.GetError().c_str());
                            return;
                        }

                        // Push the source container as an output to support chaining
                        PushOutput(*containerDatum, *OperatorClearProperty::GetContainerSlot(this));
                    }
                }

                SignalOutput(GetSlotId("Out"));
            }
        }
    }
}

#include <Include/ScriptCanvas/Libraries/Operators/Containers/OperatorClear.generated.cpp>