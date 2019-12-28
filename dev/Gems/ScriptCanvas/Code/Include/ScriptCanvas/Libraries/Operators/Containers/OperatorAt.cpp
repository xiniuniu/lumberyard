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

#include "OperatorAt.h"

#include <ScriptCanvas/Core/Contracts/SupportsMethodContract.h>
#include <ScriptCanvas/Libraries/Core/MethodUtility.h>
#include <ScriptCanvas/Core/Core.h>

namespace ScriptCanvas
{
    namespace Nodes
    {
        namespace Operators
        {
            void OperatorAt::ConfigureContracts(SourceType sourceType, AZStd::vector<ContractDescriptor>& contractDescs)
            {
                if (sourceType == SourceType::SourceInput)
                {
                    ContractDescriptor supportsMethodContract;
                    supportsMethodContract.m_createFunc = [this]() -> SupportsMethodContract* { return aznew SupportsMethodContract("At"); };
                    contractDescs.push_back(AZStd::move(supportsMethodContract));
                }
            }

            void OperatorAt::OnSourceTypeChanged()
            {
                if (Data::IsVectorContainerType(GetSourceAZType()))
                {
                    // Add the INDEX as the INPUT slot
                    {
                        DataSlotConfiguration slotConfiguration;

                        slotConfiguration.m_name = "Index";
                        slotConfiguration.m_displayGroup = GetSourceDisplayGroup();
                        slotConfiguration.SetType(Data::Type::Number());
                        slotConfiguration.SetConnectionType(ConnectionType::Input);

                        m_inputSlots.insert(AddSlot(slotConfiguration));
                    }

                    // Add the OUTPUT slots, most of the time there will only be one
                    {
                        Data::Type type = Data::FromAZType(m_sourceTypes[0]);

                        DataSlotConfiguration slotConfiguration;

                        slotConfiguration.m_name = Data::GetName(type);
                        slotConfiguration.m_displayGroup = GetSourceDisplayGroup();
                        slotConfiguration.SetType(type);
                        slotConfiguration.SetConnectionType(ConnectionType::Output);

                        m_outputSlots.insert(AddSlot(slotConfiguration));
                    }
                }
                else if (Data::IsMapContainerType(GetSourceAZType()))
                {
                    AZStd::vector<AZ::Uuid> types = ScriptCanvas::Data::GetContainedTypes(GetSourceAZType());

                    // Only add the KEY as INPUT slot
                    {
                        ScriptCanvas::Data::Type dataType = Data::FromAZType(types[0]);

                        DataSlotConfiguration slotConfiguration;

                        slotConfiguration.m_name = Data::GetName(dataType);
                        slotConfiguration.SetType(dataType);
                        slotConfiguration.SetConnectionType(ConnectionType::Input);

                        m_inputSlots.insert(AddSlot(slotConfiguration));
                    }

                    // Only add the VALUE as the OUTPUT slot
                    {
                        Data::Type type = Data::FromAZType(types[1]);

                        DataSlotConfiguration slotConfiguration;

                        slotConfiguration.m_name = Data::GetName(type);
                        slotConfiguration.m_toolTip = "The value at the specified index";
                        slotConfiguration.SetType(type);
                        slotConfiguration.SetConnectionType(ConnectionType::Output);

                        m_outputSlots.insert(AddSlot(slotConfiguration));
                    }
                }
            }

            void OperatorAt::InvokeOperator()
            {
                Slot* inputSlot = GetFirstInputSourceSlot();                

                if (inputSlot)
                {
                    SlotId sourceSlotId = inputSlot->GetId();
                    const Datum* containerDatum = GetInput(sourceSlotId);
                    
                    if (Datum::IsValidDatum(containerDatum))
                    {
                        const Datum* inputKeyDatum = GetInput(*m_inputSlots.begin());
                        AZ::Outcome<Datum, AZStd::string> valueOutcome = BehaviorContextMethodHelper::CallMethodOnDatumUnpackOutcomeSuccess(*containerDatum, "At", *inputKeyDatum);
                        if (!valueOutcome.IsSuccess())
                        {
                            SCRIPTCANVAS_REPORT_ERROR((*this), "Failed to get key in container: %s", valueOutcome.GetError().c_str());
                            return;
                        }

                        if (Data::IsVectorContainerType(containerDatum->GetType()))
                        {
                            PushOutput(valueOutcome.TakeValue(), *GetSlot(*m_outputSlots.begin()));
                        }
                        else if (Data::IsSetContainerType(containerDatum->GetType()) || Data::IsMapContainerType(containerDatum->GetType()))
                        {
                            Datum keyDatum = valueOutcome.TakeValue();
                            if (keyDatum.Empty())
                            {
                                SCRIPTCANVAS_REPORT_ERROR((*this), "Behavior Context call failed; unable to retrieve element from container.");
                                return;
                            }

                            PushOutput(keyDatum, *GetSlot(*m_outputSlots.begin()));
                        }
                    }
                }

                SignalOutput(GetSlotId("Out"));
            }

            void OperatorAt::OnInputSignal(const SlotId& slotId)
            {
                const SlotId inSlotId = OperatorBaseProperty::GetInSlotId(this);
                if (slotId == inSlotId)
                {
                    InvokeOperator();
                }
            }
        }
    }
}

#include <Include/ScriptCanvas/Libraries/Operators/Containers/OperatorAt.generated.cpp>