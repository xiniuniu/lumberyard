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

#include "OperatorMul.h"
#include <ScriptCanvas/Libraries/Core/MethodUtility.h>
#include <ScriptCanvas/Core/Contracts/MathOperatorContract.h>
#include <AzCore/Math/MathUtils.h>
#include <ScriptCanvas/Data/NumericData.h>

namespace ScriptCanvas
{
    namespace Nodes
    {
        namespace Operators
        {
            template<typename Type>
            struct OperatorMulImpl
            {
                Type operator()(const Type& a, const Datum& b)
                {
                    const Type* dataB = b.GetAs<Type>();

                    if (dataB)
                    {
                        return a * (*dataB);
                    }
                    else
                    {
                        return a;
                    }
                }
            };

            template <>
            struct OperatorMulImpl<Data::ColorType>
            {
                Data::ColorType operator()(const Data::ColorType& lhs, const Datum& rhs)
                {
                    const AZ::Color* dataB = rhs.GetAs<AZ::Color>();

                    if (dataB)
                    {
                        // TODO: clamping should happen at the AZ::Color level, not here - but it does not.
                        float a = AZ::GetClamp<float>(lhs.GetA(), 0.f, 1.f) * AZ::GetClamp<float>(dataB->GetA(), 0.f, 1.f);
                        float r = AZ::GetClamp<float>(lhs.GetR(), 0.f, 1.f) * AZ::GetClamp<float>(dataB->GetR(), 0.f, 1.f);
                        float g = AZ::GetClamp<float>(lhs.GetG(), 0.f, 1.f) * AZ::GetClamp<float>(dataB->GetG(), 0.f, 1.f);
                        float b = AZ::GetClamp<float>(lhs.GetB(), 0.f, 1.f) * AZ::GetClamp<float>(dataB->GetB(), 0.f, 1.f);

                        return AZ::Color(r, g, b, a);
                    }
                    else
                    {
                        return lhs;
                    }
                }
            };

            AZStd::unordered_set< Data::Type > OperatorMul::GetSupportedNativeDataTypes() const
            {
                return {
                    Data::Type::Number(),
                    Data::Type::Quaternion(),
                    Data::Type::Transform(),
                    Data::Type::Matrix3x3(),
                    Data::Type::Matrix4x4()
                };
            }

            void OperatorMul::Operator(Data::eType type, const ArithmeticOperands& operands, Datum& result)
            {
                AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::ScriptCanvas);

                switch (type)
                {
                case Data::eType::Number:
                    OperatorEvaluator::Evaluate<Data::NumberType>(OperatorMulImpl<Data::NumberType>(), operands, result);
                    break;
                case Data::eType::Quaternion:
                    OperatorEvaluator::Evaluate<Data::QuaternionType>(OperatorMulImpl<Data::QuaternionType>(), operands, result);
                    break;
                case Data::eType::Transform:
                    OperatorEvaluator::Evaluate<Data::TransformType>(OperatorMulImpl<Data::TransformType>(), operands, result);
                    break;
                case Data::eType::Matrix3x3:
                    OperatorEvaluator::Evaluate<Data::Matrix3x3Type>(OperatorMulImpl<Data::Matrix3x3Type>(), operands, result);
                    break;
                case Data::eType::Matrix4x4:
                    OperatorEvaluator::Evaluate<Data::Matrix4x4Type>(OperatorMulImpl<Data::Matrix4x4Type>(), operands, result);
                    break; 
                default:
                    AZ_Assert(false, "Multiplication operator not defined for type: %s", Data::ToAZType(type).ToString<AZStd::string>().c_str());
                    break;
                }
            }

            void OperatorMul::InitializeDatum(Datum* datum, const ScriptCanvas::Data::Type& dataType)
            {
                switch (dataType.GetType())
                {
                case Data::eType::Number:
                    datum->Set(ScriptCanvas::Data::One());
                    break;
                case Data::eType::Vector2:
                    datum->Set(Data::Vector2Type::CreateOne());
                    break;
                case Data::eType::Vector3:
                    datum->Set(Data::Vector3Type::CreateOne());
                    break;
                case Data::eType::Vector4:
                    datum->Set(Data::Vector4Type::CreateOne());
                    break;
                case Data::eType::Quaternion:
                    datum->Set(Data::QuaternionType::CreateIdentity());
                    break;
                case Data::eType::Matrix3x3:
                    datum->Set(Data::Matrix3x3Type::CreateIdentity());
                    break;
                case Data::eType::Matrix4x4:
                    datum->Set(Data::Matrix4x4Type::CreateIdentity());
                    break;
                default:
                    break;
                };
            }            

            bool OperatorMul::IsValidArithmeticSlot(const SlotId& slotId) const
            {
                const Datum* datum = GetInput(slotId);

                if (datum)
                {
                    switch (datum->GetType().GetType())
                    {
                    case Data::eType::Number:
                        return !AZ::IsClose((*datum->GetAs<Data::NumberType>()), Data::NumberType(1.0), ScriptCanvas::Data::ToleranceEpsilon());
                    case Data::eType::Quaternion:
                        return !datum->GetAs<Data::QuaternionType>()->IsIdentity();
                    default:
                        break;
                    }
                }

                return (datum != nullptr);
            }

            void OperatorMul::OnResetDatumToDefaultValue(Datum* datum)
            {
                Data::Type displayType = GetDisplayType(GetArithmeticDynamicTypeGroup());
                
                if (displayType.IsValid())
                {
                    InitializeDatum(datum, displayType);
                }
            }
        }
    }
}

#include <Include/ScriptCanvas/Libraries/Operators/Math/OperatorMul.generated.cpp>