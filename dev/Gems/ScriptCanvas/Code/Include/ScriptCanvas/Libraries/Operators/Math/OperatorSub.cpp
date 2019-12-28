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

#include "OperatorSub.h"
#include <ScriptCanvas/Libraries/Core/MethodUtility.h>
#include <ScriptCanvas/Core/Contracts/MathOperatorContract.h>
#include <AzCore/Math/MathUtils.h>

namespace ScriptCanvas
{
    namespace Nodes
    {
        namespace Operators
        {
            template<typename Type>
            struct OperatorSubImpl
            {
                Type operator()(const Type& a, const Datum& b)
                {
                    const Type* dataB = b.GetAs<Type>();

                    if (dataB)
                    {
                        return a - (*dataB);
                    }
                    else
                    {
                        return a;
                    }
                }
            };

            template <>
            struct OperatorSubImpl<Data::ColorType>
            {
                Data::ColorType operator()(const Data::ColorType& lhs, const Datum& rhs)
                {
                    const AZ::Color* dataB = rhs.GetAs<AZ::Color>();

                    if (dataB)
                    {
                        // TODO: clamping should happen at the AZ::Color level, not here - but it does not.
                        float a = AZ::GetClamp<float>(lhs.GetA(), 0.f, 1.f) - AZ::GetClamp<float>(dataB->GetA(), 0.f, 1.f);
                        float r = AZ::GetClamp<float>(lhs.GetR(), 0.f, 1.f) - AZ::GetClamp<float>(dataB->GetR(), 0.f, 1.f);
                        float g = AZ::GetClamp<float>(lhs.GetG(), 0.f, 1.f) - AZ::GetClamp<float>(dataB->GetG(), 0.f, 1.f);
                        float b = AZ::GetClamp<float>(lhs.GetB(), 0.f, 1.f) - AZ::GetClamp<float>(dataB->GetB(), 0.f, 1.f);

                        return AZ::Color(r, g, b, a);
                    }
                    else
                    {
                        return lhs;
                    }
                }
            };

            template <>
            struct OperatorSubImpl<Data::Matrix3x3Type>
            {
                Data::Matrix3x3Type operator()(const Data::Matrix3x3Type& lhs, const Datum& rhs)
                {
                    const AZ::Matrix3x3* dataRhs = rhs.GetAs<AZ::Matrix3x3>();

                    if (dataRhs)
                    {
                        return AZ::Matrix3x3::CreateFromColumns(lhs.GetColumn(0) - dataRhs->GetColumn(0), lhs.GetColumn(1) - dataRhs->GetColumn(1), lhs.GetColumn(2) - dataRhs->GetColumn(2));
                    }

                    return lhs;
                }
            };

            template <>
            struct OperatorSubImpl<Data::Matrix4x4Type>
            {
                Data::Matrix4x4Type operator()(const Data::Matrix4x4Type& lhs, const Datum& rhs)
                {
                    const AZ::Matrix4x4* dataRhs = rhs.GetAs<AZ::Matrix4x4>();
                    if (dataRhs)
                    {
                        return AZ::Matrix4x4::CreateFromColumns(lhs.GetColumn(0) - dataRhs->GetColumn(0), lhs.GetColumn(1) - dataRhs->GetColumn(1), lhs.GetColumn(2) - dataRhs->GetColumn(2), lhs.GetColumn(3) - dataRhs->GetColumn(3));
                    }

                    return lhs;
                }
            };

            void OperatorSub::Operator(Data::eType type, const ArithmeticOperands& operands, Datum& result)
            {
                switch (type)
                {
                case Data::eType::Number:
                    OperatorEvaluator::Evaluate<Data::NumberType>(OperatorSubImpl<Data::NumberType>(), operands, result);
                    break;
                case Data::eType::Color:
                    OperatorEvaluator::Evaluate<Data::ColorType>(OperatorSubImpl<Data::ColorType>(), operands, result);
                    break;
                case Data::eType::Vector2:
                    OperatorEvaluator::Evaluate<Data::Vector2Type>(OperatorSubImpl<Data::Vector2Type>(), operands, result);
                    break;
                case Data::eType::Vector3:
                    OperatorEvaluator::Evaluate<Data::Vector3Type>(OperatorSubImpl<Data::Vector3Type>(), operands, result);
                    break;
                case Data::eType::Vector4:
                    OperatorEvaluator::Evaluate<Data::Vector4Type>(OperatorSubImpl<Data::Vector4Type>(), operands, result);
                    break;
                case Data::eType::Matrix3x3:
                    OperatorEvaluator::Evaluate<Data::Matrix3x3Type>(OperatorSubImpl<Data::Matrix3x3Type>(), operands, result);
                    break;
                case Data::eType::Matrix4x4:
                    OperatorEvaluator::Evaluate<Data::Matrix4x4Type>(OperatorSubImpl<Data::Matrix4x4Type>(), operands, result);
                    break;
                default:
                    AZ_Assert(false, "Subtraction operator not defined for type: %s", Data::ToAZType(type).ToString<AZStd::string>().c_str());
                    break;
                }
            }
        }
    }
}

#include <Include/ScriptCanvas/Libraries/Operators/Math/OperatorSub.generated.cpp>