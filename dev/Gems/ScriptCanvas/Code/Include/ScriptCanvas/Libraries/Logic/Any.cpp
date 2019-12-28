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

#include "Any.h"

#include <Include/ScriptCanvas/Libraries/Logic/Any.generated.cpp>

namespace ScriptCanvas
{
    namespace Nodes
    {
        namespace Logic
        {
            bool Any::AnyNodeVersionConverter(AZ::SerializeContext& serializeContext, AZ::SerializeContext::DataElementNode& rootElement)
            {
                if (rootElement.GetVersion() < Version::RemoveInputsContainers)
                {
                    rootElement.RemoveElementByName(AZ::Crc32("m_inputs"));
                }

                return true;
            }

            void Any::OnInit()
            {
                AZStd::vector< const Slot* > slots = GetAllSlotsByDescriptor(ScriptCanvas::SlotDescriptors::ExecutionIn());

                if (slots.empty())
                {
                    AddInputSlot();
                }
            }
            
            void Any::OnInputSignal(const SlotId& slotId)
            {
                const SlotId outSlotId = AnyProperty::GetOutSlotId(this);
                SignalOutput(outSlotId);
            }

            bool Any::IsNodeExtendable() const
            {
                return true;
            }

            int Any::GetNumberOfExtensions() const
            {
                return 1;
            }

            ExtendableSlotConfiguration Any::GetExtensionConfiguration(int extensionIndex) const
            {
                ExtendableSlotConfiguration extendableConfiguration;

                extendableConfiguration.m_name = "Add Input";
                extendableConfiguration.m_tooltip = "Adds a new input to the Any Node";

                // DisplayGroup Taken from GraphCanvas
                extendableConfiguration.m_displayGroup = "SlotGroup_Execution";

                extendableConfiguration.m_connectionType = ConnectionType::Input;
                extendableConfiguration.m_identifier = GetInputExtensionId();

                return extendableConfiguration;
            }

            SlotId Any::HandleExtension(AZ::Crc32 extensionId)
            {
                if (extensionId == GetInputExtensionId())
                {
                    return AddInputSlot();
                }

                return SlotId();
            }

            bool Any::CanDeleteSlot(const SlotId& slotId) const
            {
                auto slots = GetAllSlotsByDescriptor(SlotDescriptors::ExecutionIn());
                return slots.size() > 1;
            }

            void Any::OnSlotRemoved(const SlotId& slotId)
            {
                FixupStateNames();
            }

            AZStd::string Any::GenerateInputName(int counter)
            {
                return AZStd::string::format("Input %i", counter);
            }

            SlotId Any::AddInputSlot()
            {
                auto inputSlots = GetAllSlotsByDescriptor(SlotDescriptors::ExecutionIn());
                int inputCount = static_cast<int>(inputSlots.size());

                ExecutionSlotConfiguration  slotConfiguration(GenerateInputName(inputCount), ConnectionType::Input);
                return AddSlot(slotConfiguration);
            }

            void Any::FixupStateNames()
            {
                auto inputSlots = GetAllSlotsByDescriptor(SlotDescriptors::ExecutionIn());
                for (int i = 0; i < inputSlots.size(); ++i)
                {
                    Slot* slot = GetSlot(inputSlots[i]->GetId());

                    if (slot)
                    {
                        slot->Rename(GenerateInputName(i));
                    }
                }
            }
        }
    }
}