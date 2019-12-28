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

#include <AzCore/Serialization/Json/JsonSerializationResult.h>
#include <AzCore/Serialization/Json/JsonSerialization.h>
#include <AzCore/std/algorithm.h>

namespace AZ
{
    namespace JsonSerializationResult
    {
        namespace Internal
        {
            template<typename StringType>
            void AppendToString(AZ::JsonSerializationResult::ResultCode code, 
                StringType& target, AZStd::string_view path)
            {
                if (code.GetTask() == static_cast<Tasks>(0))
                {
                    target.append("The result code wasn't initialized");
                    return;
                }

                target.append("The operation ");
                switch (code.GetProcessing())
                {
                case Processing::Halted:
                    target.append("has halted during ");
                    break;
                case Processing::Altered:
                    target.append("has taken an alternative approach for ");
                    break;
                case Processing::PartialAlter:
                    target.append("has taken a partially alternative approach for ");
                    break;
                case Processing::Completed:
                    target.append("has completed ");
                    break;
                default:
                    target.append("has unknown processing status for ");
                    break;
                }

                switch (code.GetTask())
                {
                case Tasks::RetrieveInfo:
                    target.append("a retrieve info operation ");
                    break;
                case Tasks::CreateDefault:
                    target.append("a create default operation ");
                    break;
                case Tasks::Convert:
                    target.append("a convert operation ");
                    break;
                case Tasks::ReadField:
                    target.append("a read field operation ");
                    break;
                case Tasks::WriteValue:
                    target.append("a write value operation ");
                    break;
                default:
                    target.append("an unknown operation ");
                    break;
                }

                if (!path.empty())
                {
                    target.append("for '");
                    target.append(path.begin(), path.end());
                    target.append("' ");
                }

                switch (code.GetOutcome())
                {
                case Outcomes::Success:
                    target.append("which resulted in success");
                    break;
                case Outcomes::DefaultsUsed:
                    target.append("by using only default values");
                    break;
                case Outcomes::PartialDefaults:
                    target.append("by using one or more default values");
                    break;
                case Outcomes::Skipped:
                    target.append("because a field or value was skipped");
                    break;
                case Outcomes::Unavailable:
                    target.append("because the target was unavailable");
                    break;
                case Outcomes::Unsupported:
                    target.append("because the action was unsupported");
                    break;
                case Outcomes::TypeMismatch:
                    target.append("because the source and target are unrelated types");
                    break;
                case Outcomes::Unknown:
                    target.append("because information was missing");
                    break;
                case Outcomes::Catastrophic:
                    target.append("because a catastrophic issue was encountered");
                    break;
                default:
                    break;
                }
            }
        } // namespace JsonSerializationResultInternal

        ResultCode::ResultCode(Tasks task)
            : m_code(0)
        {
            m_options.m_task = task;
        }

        ResultCode::ResultCode(uint32_t code)
            : m_code(code)
        {}

        ResultCode::ResultCode(Tasks task, Outcomes outcome)
        {
            m_options.m_task = task;
            switch (outcome)
            {
            case Outcomes::Success:          // fall through
            case Outcomes::Skipped:          // fall through
            case Outcomes::DefaultsUsed:     // fall through
            case Outcomes::PartialDefaults:
                m_options.m_processing = Processing::Completed;
                break;
            case Outcomes::Unavailable:      // fall through
            case Outcomes::Unsupported:
                m_options.m_processing = Processing::Altered;
                break;
            case Outcomes::TypeMismatch:     // fall through
            case Outcomes::Unknown:          // fall through
            case Outcomes::Catastrophic:     // fall through
            default:
                m_options.m_processing = Processing::Halted;
                break;
            }
            m_options.m_outcome = outcome;
        }

        ResultCode ResultCode::Success(Tasks task)
        {
            return ResultCode(task, Outcomes::Success);
        }
        
        ResultCode ResultCode::Default(Tasks task)
        {
            return ResultCode(task, Outcomes::DefaultsUsed);
        }

        ResultCode ResultCode::PartialDefault(Tasks task)
        {
            return ResultCode(task, Outcomes::PartialDefaults);
        }

        bool ResultCode::HasDoneWork() const
        {
            return m_options.m_outcome != static_cast<Outcomes>(0);
        }

        ResultCode& ResultCode::Combine(ResultCode other)
        {
            *this = Combine(*this, other);
            return *this;
        }

        ResultCode& ResultCode::Combine(const Result& other)
        {
            *this = Combine(*this, other.GetResultCode());
            return *this;
        }

        ResultCode ResultCode::Combine(ResultCode lhs, ResultCode rhs)
        {
            ResultCode result = ResultCode(AZStd::max(lhs.m_code, rhs.m_code));

            if ((lhs.m_options.m_outcome == Outcomes::Success && rhs.m_options.m_outcome == Outcomes::DefaultsUsed) ||
                (lhs.m_options.m_outcome == Outcomes::DefaultsUsed && rhs.m_options.m_outcome == Outcomes::Success))
            {
                result.m_options.m_outcome = Outcomes::PartialDefaults;
            }

            if ((lhs.m_options.m_processing == Processing::Completed && rhs.m_options.m_processing == Processing::Altered) ||
                (lhs.m_options.m_processing == Processing::Altered && rhs.m_options.m_processing == Processing::Completed))
            {
                result.m_options.m_processing = Processing::PartialAlter;
            }

            return result;
        }

        Tasks ResultCode::GetTask() const
        {
            return m_options.m_task;
        }

        Processing ResultCode::GetProcessing() const
        {
            return m_options.m_processing == static_cast<Processing>(0) ?
                Processing::Completed : m_options.m_processing;
        }

        Outcomes ResultCode::GetOutcome() const
        {
            return m_options.m_outcome == static_cast<Outcomes>(0) ?
                Outcomes::DefaultsUsed : m_options.m_outcome;
        }

        void ResultCode::AppendToString(AZ::OSString& target, AZStd::string_view path) const
        {
            Internal::AppendToString(*this, target, path);
        }

        void ResultCode::AppendToString(AZStd::string& target, AZStd::string_view path) const
        {
            Internal::AppendToString(*this, target, path);
        }

        AZStd::string ResultCode::ToString(AZStd::string_view path) const
        {
            AZStd::string result;
            AppendToString(result, path);
            return result;
        }

        AZ::OSString ResultCode::ToOSString(AZStd::string_view path) const
        {
            AZ::OSString result;
            AppendToString(result, path);
            return result;
        }


        Result::Result(const JsonDeserializerSettings& settings, AZStd::string_view message,
            ResultCode result, AZStd::string_view path)
            : m_result(settings.m_reporting(message, result, path)) {}

        Result::Result(const JsonDeserializerSettings& settings, AZStd::string_view message,
            Tasks task, Outcomes outcome, AZStd::string_view path)
            : m_result(settings.m_reporting(message, ResultCode(task, outcome), path)) {}

        Result::Result(const JsonSerializerSettings& settings, AZStd::string_view message,
            ResultCode result, AZStd::string_view path)
            : m_result(settings.m_reporting(message, result, path)) {}

        Result::Result(const JsonSerializerSettings& settings, AZStd::string_view message,
            Tasks task, Outcomes outcome, AZStd::string_view path)
            : m_result(settings.m_reporting(message, ResultCode(task, outcome), path)) {}

        Result::operator ResultCode() const
        { 
            return m_result;
        }

        ResultCode Result::GetResultCode() const
        {
            return m_result;
        }
    } // namespace JsonSerializationResult
} // namespace AZ
