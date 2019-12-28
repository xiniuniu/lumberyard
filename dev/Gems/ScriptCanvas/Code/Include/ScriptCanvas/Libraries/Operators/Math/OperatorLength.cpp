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

#include "OperatorLength.h"
#include <ScriptCanvas/Libraries/Core/MethodUtility.h>
#include <ScriptCanvas/Core/Contracts/MathOperatorContract.h>
#include <AzCore/Math/MathUtils.h>

#include <ScriptCanvas/Utils/SerializationUtils.h>

namespace ScriptCanvas
{
    namespace Nodes
    {
        namespace Operators
        {
            bool OperatorLength::OperatorLengthConverter(AZ::SerializeContext& serializeContext, AZ::SerializeContext::DataElementNode& rootElement)
            {
                if (rootElement.GetVersion() < Version::RemoveOperatorBase)
                {
                    if (!SerializationUtils::RemoveBaseClass(serializeContext, rootElement))
                    {
                        return false;
                    }

                    if (!SerializationUtils::RemoveBaseClass(serializeContext, rootElement))
                    {
                        return false;
                    }

                    rootElement.RemoveElementByName(AZ::Crc32("BaseClass2"));
                }

                return true;
            }

            void OperatorLength::OnInit()
            {
                Slot* slot = GetSlotByName("Length");

                if (slot == nullptr)
                {
                    ConfigureSlots();
                }
            }

            void OperatorLength::OnInputSignal(const SlotId& slotId)
            {
                if (slotId != OperatorLengthProperty::GetInSlotId(this))
                {
                    return;
                }

                Data::Type type = GetDisplayType(AZ::Crc32("SourceGroup"));

                if (!type.IsValid())
                {
                    return;
                }

                Datum result;
                const Datum* operand = GetInput(OperatorLengthProperty::GetSourceSlotId(this));                

                switch (type.GetType())
                {
                case Data::eType::Vector2:
                {
                    const AZ::Vector2* vector = operand->GetAs<AZ::Vector2>();
                    result = Datum(vector->GetLength());
                }
                break;
                case Data::eType::Vector3:
                {
                    const AZ::Vector3* vector = operand->GetAs<AZ::Vector3>();
                    result = Datum(vector->GetLength());
                }
                break;
                case Data::eType::Vector4:
                {
                    const AZ::Vector4* vector = operand->GetAs<AZ::Vector4>();
                    result = Datum(vector->GetLength());
                }
                break;
                case Data::eType::Quaternion:
                {
                    const AZ::Quaternion* vector = operand->GetAs<AZ::Quaternion>();
                    result = Datum(vector->GetLength());
                }
                break;
                default:
                    AZ_Assert(false, "Length operator not defined for type: %s", Data::ToAZType(type).ToString<AZStd::string>().c_str());
                    break;
                }

                PushOutput(result, (*OperatorLengthProperty::GetLengthSlot(this)));
                SignalOutput(OperatorLengthProperty::GetOutSlotId(this));
            }
        }
    }
}

#include <Include/ScriptCanvas/Libraries/Operators/Math/OperatorLength.generated.cpp>