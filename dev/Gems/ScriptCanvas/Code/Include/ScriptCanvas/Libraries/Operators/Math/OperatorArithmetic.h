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

#include <AzCore/Serialization/SerializeContext.h>

#include <ScriptCanvas/Libraries/Operators/Operator.h>
#include <ScriptCanvas/Data/Data.h>
#include <Include/ScriptCanvas/Libraries/Operators/Math/OperatorArithmetic.generated.h>

namespace ScriptCanvas
{
    namespace Nodes
    {
        namespace Operators
        {
            inline namespace OperatorEvaluator
            {
                template <typename ResultType, typename OperatorFunctor>
                static void Evaluate(OperatorFunctor&& operatorFunctor, const OperatorBase::OperatorOperands& operands, Datum& result)
                {
                    OperatorBase::OperatorOperands::const_iterator operandIter = operands.begin();

                    ResultType resultType;

                    while (operandIter != operands.end())
                    {
                        const ResultType* type = (*operandIter)->GetAs<ResultType>();

                        if (type)
                        {
                            resultType = (*type);
                            break;
                        }
                    }

                    for (++operandIter; operandIter != operands.end(); ++operandIter)
                    {
                        resultType = AZStd::invoke(operatorFunctor, resultType, (*(*operandIter)));
                    }

                    result = Datum(resultType);
                }
            }

            class OperatorArithmetic : public Node
            {
            public:
                ScriptCanvas_Node(OperatorArithmetic,
                    ScriptCanvas_Node::Name("OperatorArithmetic")
                    ScriptCanvas_Node::Uuid("{FE0589B0-F835-4CD5-BBD3-86510CBB985B}")
                    ScriptCanvas_Node::Description("")
                    ScriptCanvas_Node::Version(1, OperatorArithmeticVersionConverter)
                    ScriptCanvas_Node::Category("Operators")
                );

                // Need to update code gen to deal with enums.
                // Will do that in a separate pass for now still want to track versions using enums
                enum Version
                {
                    InitialVersion = 0,
                    RemoveOperatorBase,

                    Current
                };

                typedef AZStd::vector<const Datum*> ArithmeticOperands;

                static bool OperatorArithmeticVersionConverter(AZ::SerializeContext& serializeContext, AZ::SerializeContext::DataElementNode& rootElement);

                OperatorArithmetic() = default;

                AZ::Crc32 GetArithmeticExtensionId() const { return AZ_CRC("AddnewValueExtension", 0xea20301c); }
                AZ::Crc32 GetArithmeticDynamicTypeGroup() const { return AZ_CRC("ArithmeticGroup", 0x4271e41f); }
                AZStd::string GetArithmeticDisplayGroup() const { return "ArithmeticGroup"; }

                virtual AZStd::string_view OperatorFunction() const { return ""; }
                virtual AZStd::unordered_set< Data::Type > GetSupportedNativeDataTypes() const
                {
                    return {
                        Data::Type::Number(),
                        Data::Type::Vector2(),
                        Data::Type::Vector3(),
                        Data::Type::Vector4(),
                        Data::Type::Color(),
                        Data::Type::Quaternion(),
                        Data::Type::Transform(),
                        Data::Type::Matrix3x3(),
                        Data::Type::Matrix4x4()
                    };
                }
                
                // Node
                void OnDynamicGroupDisplayTypeChanged(const AZ::Crc32& dynamicGroup, const Data::Type& dataType) override final;
                void OnInputSignal(const SlotId& slotId) override;
                
                void OnConfigured() override;
                void OnInit() override;

                bool IsNodeExtendable() const override;
                int GetNumberOfExtensions() const override;
                ExtendableSlotConfiguration GetExtensionConfiguration(int extensionIndex) const override;

                SlotId HandleExtension(AZ::Crc32 extensionId) override;

                bool CanDeleteSlot(const SlotId& slotId) const override;
                ////

                void Evaluate(const ArithmeticOperands&, Datum&);

                virtual void Operator(Data::eType type, const ArithmeticOperands& operands, Datum& result);
                virtual void InitializeDatum(Datum* datum, const Data::Type& dataType);
                virtual void InvokeOperator();

                ScriptCanvas_In(ScriptCanvas_In::Name("In", ""));
                ScriptCanvas_Out(ScriptCanvas_Out::Name("Out", ""));

            protected:

                SlotId CreateSlot(AZStd::string_view name, AZStd::string_view toolTip, ConnectionType connectionType);

                void UpdateArithmeticSlotNames();
                void SetSourceNames(const AZStd::string& inputName, const AZStd::string& outputName);

                virtual bool IsValidArithmeticSlot(const SlotId& slotId) const;

                // Contains the list of slots that have, or have the potential, to have values which will impact
                // the specific arithmetic operation.
                //
                // Used at run time to try to avoid invoking extra operator calls for no-op operations
                AZStd::vector< SlotId > m_applicableInputSlots;

                SlotId m_outputSlot;
            };
            
            // Deprecated class. Only here for version conversion. Do not use.
            class OperatorArithmeticUnary : public OperatorArithmetic
            {
            public:

                ScriptCanvas_Node(OperatorArithmeticUnary,
                    ScriptCanvas_Node::Name("OperatorArithmeticUnary")
                    ScriptCanvas_Node::Uuid("{4B68DF49-35DE-48CF-BCE3-F892CCF2639D}")
                    ScriptCanvas_Node::Description("")
                    ScriptCanvas_Node::Version(0)
                    ScriptCanvas_Node::Category("Operators/Math")
                );

                OperatorArithmeticUnary()
                    : OperatorArithmetic()
                {
                }
            };
        }
    }
}