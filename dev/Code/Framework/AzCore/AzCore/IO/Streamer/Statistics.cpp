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

#include <AzCore/IO/Streamer/Statistics.h>

namespace AZ
{
    namespace IO
    {
        Statistic Statistic::CreateFloat(AZStd::string_view owner, AZStd::string_view name, double value)
        {
            Statistic result;
            result.m_owner = owner;
            result.m_name = name;
            result.m_value.m_floatingPoint = value;
            result.m_type = Type::FloatingPoint;
            return result;
        }

        Statistic Statistic::CreateInteger(AZStd::string_view owner, AZStd::string_view name, s64 value)
        {
            Statistic result;
            result.m_owner = owner;
            result.m_name = name;
            result.m_value.m_integer = value;
            result.m_type = Type::Integer;
            return result;
        }

        Statistic Statistic::CreatePercentage(AZStd::string_view owner, AZStd::string_view name, double value)
        {
            Statistic result;
            result.m_owner = owner;
            result.m_name = name;
            result.m_value.m_floatingPoint = value;
            result.m_type = Type::Percentage;
            return result;
        }

        Statistic::Statistic(const Statistic& rhs)
            : m_owner(rhs.m_owner)
            , m_name(rhs.m_name)
            , m_type(rhs.m_type)
        {
            memcpy(&m_value, &rhs.m_value, sizeof(m_value));
        }

        Statistic::Statistic(Statistic&& rhs)
            : m_owner(AZStd::move(rhs.m_owner))
            , m_name(AZStd::move(rhs.m_name))
            , m_type(rhs.m_type)
        {
            memcpy(&m_value, &rhs.m_value, sizeof(m_value));
        }

        Statistic& Statistic::operator=(const Statistic& rhs)
        {
            if (this != &rhs)
            {
                m_owner = rhs.m_owner;
                m_name = rhs.m_name;
                m_type = rhs.m_type;
                memcpy(&m_value, &rhs.m_value, sizeof(m_value));
            }
            return *this;
        }

        Statistic& Statistic::operator=(Statistic&& rhs)
        {
            if (this != &rhs)
            {
                m_owner = AZStd::move(rhs.m_owner);
                m_name = AZStd::move(rhs.m_name);
                m_type = rhs.m_type;
                memcpy(&m_value, &rhs.m_value, sizeof(m_value));
            }
            return *this;
        }

        AZStd::string_view Statistic::GetOwner() const
        {
            return m_owner;
        }

        AZStd::string_view Statistic::GetName() const
        {
            return m_name;
        }

        Statistic::Type Statistic::GetType() const
        {
            return m_type;
        }

        double Statistic::GetFloatValue() const
        {
            AZ_Assert(m_type == Type::FloatingPoint, "Trying to get a floating point value from a statistic that doesn't store a floating point value.");
            return m_value.m_floatingPoint;
        }

        s64 Statistic::GetIntegerValue() const
        {
            AZ_Assert(m_type == Type::Integer, "Trying to get a integer value from a statistic that doesn't store a integer value.");
            return m_value.m_integer;
        }

        double Statistic::GetPercentage() const
        {
            AZ_Assert(m_type == Type::Percentage, "Trying to get a percentage value from a statistic that doesn't store a percentage value.");
            return m_value.m_floatingPoint * 100.0;
        }
    } // namespace IO
} // namespace AZ