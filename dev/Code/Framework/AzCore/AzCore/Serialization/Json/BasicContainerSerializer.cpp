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

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/Json/BasicContainerSerializer.h>
#include <AzCore/Serialization/Json/JsonSerialization.h>
#include <AzCore/Serialization/Json/StackedString.h>
#include <AzCore/std/string/osstring.h>
#include <AzCore/Memory/SystemAllocator.h>

namespace AZ
{
    AZ_CLASS_ALLOCATOR_IMPL(JsonBasicContainerSerializer, SystemAllocator, 0);

    JsonSerializationResult::Result JsonBasicContainerSerializer::Load(void* outputValue, const Uuid& outputValueTypeId,
        const rapidjson::Value& inputValue, StackedString& path, const JsonDeserializerSettings& settings)
    {
        namespace JSR = JsonSerializationResult; // Used to remove name conflicts in AzCore in uber builds.

        AZ_UNUSED(outputValueTypeId);
        AZ_Assert(outputValue, "Expected a valid pointer to load from json value.");

        switch (inputValue.GetType())
        {
        case rapidjson::kArrayType:
            return LoadContainer(outputValue, outputValueTypeId, inputValue, path, settings);

        case rapidjson::kObjectType: // fall through
        case rapidjson::kNullType: // fall through
        case rapidjson::kStringType: // fall through
        case rapidjson::kFalseType: // fall through
        case rapidjson::kTrueType: // fall through
        case rapidjson::kNumberType:
            return JSR::Result(settings, "Unsupported type. Basic containers can only be read from an array.",
                JSR::Tasks::ReadField, JSR::Outcomes::Unsupported, path);

        default:
            return JSR::Result(settings, "Unknown json type encountered for deserialization from a basic container.",
                JSR::Tasks::ReadField, JSR::Outcomes::Unknown, path);
        }
    }

    JsonSerializationResult::Result JsonBasicContainerSerializer::Store(rapidjson::Value& outputValue, rapidjson::Document::AllocatorType& allocator,
        const void* inputValue, const void* /*defaultValue*/, const Uuid& valueTypeId, StackedString& path, const JsonSerializerSettings& settings)
    {
        namespace JSR = JsonSerializationResult; // Used to remove name conflicts in AzCore in uber builds.

        const SerializeContext::ClassData* containerClass = settings.m_serializeContext->FindClassData(valueTypeId);
        if (!containerClass)
        {
            return JSR::Result(settings, "Unable to retrieve information for definition of the basic container instance.",
                JSR::Tasks::RetrieveInfo, JSR::Outcomes::Unsupported, path);
        }

        SerializeContext::IDataContainer* container = containerClass->m_container;
        if (!container)
        {
            return JSR::Result(settings, "Unable to retrieve information for representation of the basic container instance.",
                JSR::Tasks::RetrieveInfo, JSR::Outcomes::Unsupported, path);
        }

        rapidjson::Value array;
        array.SetArray();
        size_t index = 0;
        JSR::ResultCode retVal(JSR::Tasks::WriteValue);
        auto elementCallback = [this, &array, &retVal, &index, &allocator, &path, &settings]
            (void* elementPtr, const Uuid& elementId, const SerializeContext::ClassData*, const SerializeContext::ClassElement* classElement)
        {
            Flags flags = classElement->m_flags & SerializeContext::ClassElement::Flags::FLG_POINTER ?
                Flags::ResolvePointer : Flags::None;
            flags |= Flags::ReplaceDefault;
            
            ScopedStackedString subPath(path, index);
            index++;

            rapidjson::Value storedValue;
            JSR::ResultCode result = ContinueStoring(storedValue, allocator, elementPtr, nullptr,
                elementId, subPath, settings, flags);
            if (result.GetProcessing() == JSR::Processing::Halted)
            {
                retVal = JSR::Result(settings, "Failed to store data for element in basic container.", result, path).GetResultCode();
                return false;
            }

            array.PushBack(AZStd::move(storedValue), allocator);
            retVal.Combine(result);    
            return true;
        };
        container->EnumElements(const_cast<void*>(inputValue), elementCallback);

        if (retVal.GetProcessing() == JSR::Processing::Halted)
        {
            return JSR::Result(settings, "Processing of basic container was halted.", retVal, path);
        }

        if (settings.m_keepDefaults)
        {
            outputValue = AZStd::move(array);
            if (retVal.HasDoneWork())
            {
                AZ_Assert(retVal.GetOutcome() != JSR::Outcomes::DefaultsUsed,
                    "Basic container serialized with 'keep defaults' but still got default values.");
                AZ_Assert(retVal.GetOutcome() != JSR::Outcomes::PartialDefaults,
                    "Basic container serialized with 'keep defaults' but still got partial default values.");
                return JSR::Result(settings, "Content written to basic container.", retVal, path);
            }
            else
            {
                return JSR::Result(settings, "Empty array written because the provided basic container is empty.", 
                    JSR::ResultCode::Success(JSR::Tasks::WriteValue), path);
            }
        }

        if (retVal.GetOutcome() != JSR::Outcomes::DefaultsUsed)
        {
            rapidjson::SizeType arraySize = array.Size();
            outputValue = AZStd::move(array);

            AZStd::string_view message = arraySize == 0 ?
                "No values written because the basic container was empty." :
                "Content written to basic container.";
            return JSR::Result(settings, message, retVal, path);
        }
        else
        {
            return JSR::Result(settings, "No content written to basic container because only defaults were found.", 
                JSR::ResultCode::Default(JSR::Tasks::WriteValue), path);
        }
    }

    JsonSerializationResult::Result JsonBasicContainerSerializer::LoadContainer(void* outputValue, const Uuid& outputValueTypeId, const rapidjson::Value& inputValue,
        StackedString& path, const JsonDeserializerSettings& settings)
    {
        namespace JSR = JsonSerializationResult; // Used to remove name conflicts in AzCore in uber builds.

        const SerializeContext::ClassData* containerClass = settings.m_serializeContext->FindClassData(outputValueTypeId);
        if (!containerClass)
        {
            return JSR::Result(settings, "Unable to retrieve information for definition of the basic container.",
                JSR::Tasks::RetrieveInfo, JSR::Outcomes::Unsupported, path);
        }

        SerializeContext::IDataContainer* container = containerClass->m_container;
        if (!container)
        {
            return JSR::Result(settings, "Unable to retrieve container meta information for the basic container.",
                JSR::Tasks::RetrieveInfo, JSR::Outcomes::Unsupported, path);
        }

        const SerializeContext::ClassElement* classElement = nullptr;
        auto typeEnumCallback = [&classElement](const Uuid&, const SerializeContext::ClassElement* genericClassElement)
        {
            AZ_Assert(!classElement, "There are multiple class elements registered for a basic container where only one was expected.");
            classElement = genericClassElement;
            return true;
        };
        container->EnumTypes(typeEnumCallback);
        AZ_Assert(classElement, "No class element found for the type in the basic container.");

        Flags flags = classElement->m_flags & SerializeContext::ClassElement::Flags::FLG_POINTER ?
            Flags::ResolvePointer : Flags::None;

        JSR::ResultCode retVal(JSR::Tasks::ReadField);
        size_t containerSize = container->Size(outputValue);
        rapidjson::SizeType arraySize = inputValue.Size();
        for (rapidjson::SizeType i = 0; i < arraySize; ++i)
        {
            ScopedStackedString subPath(path, i);

            size_t expectedSize = container->Size(outputValue) + 1;
            void* elementAddress = container->ReserveElement(outputValue, classElement);
            if (!elementAddress)
            {
                return JSR::Result(settings, "Failed to allocate an item in the basic container.",
                    JSR::ResultCode(JSR::Tasks::ReadField, JSR::Outcomes::Catastrophic), path);
            }
            if (classElement->m_flags & SerializeContext::ClassElement::Flags::FLG_POINTER)
            {
                *reinterpret_cast<void**>(elementAddress) = nullptr;
            }
            
            JSR::ResultCode result = ContinueLoading(elementAddress, classElement->m_typeId, inputValue[i], subPath, settings, flags);
            if (result.GetProcessing() == JSR::Processing::Halted)
            {
                container->FreeReservedElement(outputValue, elementAddress, settings.m_serializeContext);
                return JSR::Result(settings, "Failed to read element for basic container.", retVal, subPath);
            }
            else if (result.GetProcessing() == JSR::Processing::Altered)
            {
                container->FreeReservedElement(outputValue, elementAddress, settings.m_serializeContext);
                retVal.Combine(result);
            }
            else
            {
                container->StoreElement(outputValue, elementAddress);
                if (container->Size(outputValue) != expectedSize)
                {
                    retVal.Combine(JSR::Result(settings, "Unable to store element to basic container.",
                        JSR::Tasks::ReadField, JSR::Outcomes::Unavailable, subPath));
                }
                else
                {
                    retVal.Combine(result);
                }
            } 
        }

        if (!retVal.HasDoneWork() && inputValue.Empty())
        {
            return JSR::Result(settings, "No values provided for basic container.",
                JSR::ResultCode::Success(JSR::Tasks::ReadField), path);
        }

        size_t addedCount = container->Size(outputValue) - containerSize;
        AZStd::string_view message =
            addedCount >= arraySize ? "Successfully read basic container.":
            addedCount == 0 ? "Unable to read data for basic container." :
            "Partially read data for basic container.";
        return JSR::Result(settings, message, retVal, path);
    }
} // namespace AZ
