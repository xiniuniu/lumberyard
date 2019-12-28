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

#include "MathOperatorContract.h"
#include <ScriptCanvas/Core/Slot.h>
#include <ScriptCanvas/Core/Node.h>

namespace ScriptCanvas
{
    void MathOperatorContract::SetSupportedNativeTypes(const AZStd::unordered_set< Data::Type >& nativeTypes)
    {
        m_supportedNativeTypes = nativeTypes;
    }

    void MathOperatorContract::SetSupportedOperator(AZStd::string_view operatorString)
    {
        m_supportedOperator = operatorString;
    }

    bool MathOperatorContract::HasOperatorFunction() const
    {
        return m_supportedOperator.empty();
    }

    AZ::Outcome<void, AZStd::string> MathOperatorContract::OnEvaluate(const Slot& sourceSlot, const Slot& targetSlot) const
    {
        // Check that the type in the target slot is one of the built in math functions
        AZ::Entity* targetSlotEntity{};
        AZ::ComponentApplicationBus::BroadcastResult(targetSlotEntity, &AZ::ComponentApplicationRequests::FindEntity, targetSlot.GetNodeId());
        auto dataNode = targetSlotEntity ? AZ::EntityUtils::FindFirstDerivedComponent<Node>(targetSlotEntity) : nullptr;
        if (dataNode)
        {
            const Data::Type& dataType = dataNode->GetSlotDataType(targetSlot.GetId());            

            if (dataType == Data::Type::Invalid())
            {
                // For right now we don't want to let dynamic slots connect to each other since the updating mechanism
                // doesn't work for passing along type updating.
                if (targetSlot.IsDynamicSlot())
                {
                    for (const auto& supportedDataType : m_supportedNativeTypes)
                    {
                        if (targetSlot.IsTypeMatchFor(supportedDataType))
                        {
                            return AZ::Success();
                        }
                    }
                }
            }
            else
            {
                return EvaluateForType(dataType);
            }
        }

        return AZ::Failure(AZStd::string::format("Type does not support the method: %s", m_supportedOperator.c_str()));
    }

    AZ::Outcome<void, AZStd::string> MathOperatorContract::OnEvaluateForType(const Data::Type& dataType) const
    {
        if (dataType != Data::Type::Invalid())
        {
            if (m_supportedNativeTypes.count(dataType) != 0)
            {
                // This supports math operators
                return AZ::Success();
            }
        }

        AZ::BehaviorContext* behaviorContext(nullptr);
        AZ::ComponentApplicationBus::BroadcastResult(behaviorContext, &AZ::ComponentApplicationRequests::GetBehaviorContext);
        if (!behaviorContext)
        {
            AZ_Assert(false, "A behavior context is required!");
            return AZ::Failure(AZStd::string::format("No Behavior Context"));
        }

        // Finally if we're not sure if the type supports the operator, check if it has the operator's method
        const AZ::TypeId azType = Data::ToAZType(dataType);

        const auto classIter(behaviorContext->m_typeToClassMap.find(azType));
        if (classIter == behaviorContext->m_typeToClassMap.end())
        {
            return AZ::Failure(AZStd::string::format("Behavior Context does not contain reflection for type provided: %s", azType.ToString<AZStd::string>().c_str()));
        }

        AZ::BehaviorClass* behaviorClass = classIter->second;
        if (behaviorClass)
        {
            if (behaviorClass->m_methods.find(m_supportedOperator) != behaviorClass->m_methods.end())
            {
                return AZ::Success();
            }
        }

        return AZ::Failure(AZStd::string::format("%s does not support the method: %s", ScriptCanvas::Data::GetName(dataType), m_supportedOperator.c_str()));
    }

    void MathOperatorContract::Reflect(AZ::ReflectContext* reflection)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection))
        {
            serializeContext->Class<MathOperatorContract, Contract>()
                ->Version(1)
                ->Field("OperatorType", &MathOperatorContract::m_supportedOperator)
                ->Field("NativeTypes", &MathOperatorContract::m_supportedNativeTypes)
                ;
        }
    }
}
