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

#include <AssetBuilderSDK/AssetBuilderSDK.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzFramework/Asset/SimpleAsset.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzCore/std/string/regex.h>

namespace AZ
{
    class SerializeContext;
}

namespace AssetBuilderSDK
{
    bool UpdateDependenciesFromClassData(
        const AZ::SerializeContext& serializeContext,
        void* instancePointer,
        const AZ::SerializeContext::ClassData* classData,
        const AZ::SerializeContext::ClassElement* classElement,
        AZStd::unordered_set<AZ::Data::AssetId>& productDependencySet,
        ProductPathDependencySet& productPathDependencySet,
        bool enumerateChildren)
    {
        if(classData == nullptr)
        {
            return false;
        }
        if (classData->m_typeId == AZ::GetAssetClassId())
        {
            auto* asset = reinterpret_cast<AZ::Data::Asset<AZ::Data::AssetData>*>(instancePointer);

            if (asset->GetId().IsValid())
            {
                productDependencySet.emplace(asset->GetId());
            }
        }
        else if (classData->m_typeId == azrtti_typeid<AZ::Data::AssetId>())
        {
            auto* assetId = reinterpret_cast<AZ::Data::AssetId*>(instancePointer);

            if (assetId->IsValid())
            {
                productDependencySet.emplace(*assetId);
            }
        }
        else if (classData->m_azRtti && classData->m_azRtti->IsTypeOf(azrtti_typeid<AzFramework::SimpleAssetReferenceBase>()))
        {
            auto* asset = reinterpret_cast<AzFramework::SimpleAssetReferenceBase*>(instancePointer);

            if (!asset->GetAssetPath().empty())
            {
                AZStd::string filePath = asset->GetAssetPath();
                AZStd::string fileExtension;
                if (!AzFramework::StringFunc::Path::GetExtension(filePath.c_str(), fileExtension))
                {
                    // GetFileFilter can return either
                    // 1) one file extension like "*.fileExtension"
                    // 2) one file extension like "fileExtension"
                    // 3) a semi colon separated list of file extensions like  "*.fileExtension1; *.fileExtension2"
                    // Please note that if file extension is missing from the path and we get a list of semicolon separated file extensions
                    // we will extract the first file extension and use that. 
                    fileExtension = asset->GetFileFilter();
                    AZStd::regex fileExtensionRegex("^(?:\\*\\.)?(\\w+);?");
                    AZStd::smatch match;
                    if (AZStd::regex_search(fileExtension, match, fileExtensionRegex))
                    {
                        fileExtension = match[1];
                        AzFramework::StringFunc::Path::ReplaceExtension(filePath, fileExtension.c_str());
                    }
                }

                    productPathDependencySet.emplace(filePath, AssetBuilderSDK::ProductPathDependencyType::ProductFile);
            }
        }
        else if(enumerateChildren)
        {

            auto beginCallback = [&serializeContext, &productDependencySet, &productPathDependencySet, enumerateChildren](void* instancePointer, const AZ::SerializeContext::ClassData* classData, const AZ::SerializeContext::ClassElement* classElement)
            {
                // EnumerateInstance calls are already recursive, so no need to keep going, set enumerateChildren to false.
                return UpdateDependenciesFromClassData(serializeContext, instancePointer, classData, classElement, productDependencySet, productPathDependencySet, false);
            };
            AZ::SerializeContext::EnumerateInstanceCallContext callContext(
                beginCallback,
                {},
                &serializeContext,
                AZ::SerializeContext::ENUM_ACCESS_FOR_READ,
                nullptr
            );

            return serializeContext.EnumerateInstance(&callContext, instancePointer, classData->m_typeId, classData, classElement);
        }
        return true;
    }

    void fillDependencyVectorFromSet(
        AZStd::vector<AssetBuilderSDK::ProductDependency>& productDependencies,
        AZStd::unordered_set<AZ::Data::AssetId>& productDependencySet)
    {
        productDependencies.reserve(productDependencySet.size());

        for (const auto& assetId : productDependencySet)
        {
            constexpr int flags = 0;
            productDependencies.emplace_back(assetId, flags);
        }
    }

    bool GatherProductDependenciesForFile(
        AZ::SerializeContext& serializeContext,
        const AZStd::string& filePath,
        AZStd::vector<AssetBuilderSDK::ProductDependency>& productDependencies,
        ProductPathDependencySet& productPathDependencySet)
    {
        AZ::IO::FileIOStream fileStream;
        if (!fileStream.Open(filePath.c_str(), AZ::IO::OpenMode::ModeRead | AZ::IO::OpenMode::ModeBinary))
        {
            return false;
        }
        AZStd::unordered_set<AZ::Data::AssetId> productDependencySet;

        // UpdateDependenciesFromClassData is also looking for assets. In some cases, the assets may not be ready to use
        // in UpdateDependenciesFromClassData, and have an invalid asset ID. This asset filter will be called with valid, ready to use assets,
        // but it's only called on assets and not other supported types, and it's only available when loading the file, and not on an in-memory stream.
        AZ::ObjectStream::FilterDescriptor assetReadyFilterDescriptor([&productDependencySet, &productPathDependencySet](const AZ::Data::Asset<AZ::Data::AssetData>& asset)
        {
            if (asset.GetId().IsValid())
            {
                productDependencySet.emplace(asset.GetId());
            }
            return true;
        });

        if (!AZ::ObjectStream::LoadBlocking(&fileStream, serializeContext, [&productDependencySet, &productPathDependencySet](void* instancePointer, const AZ::Uuid& classId, const AZ::SerializeContext* callbackSerializeContext)
        {
            auto classData = callbackSerializeContext->FindClassData(classId);
            // LoadBlocking only enumerates the topmost level objects, so call UpdateDependenciesFromClassData with enumerateChildren set.
            UpdateDependenciesFromClassData(*callbackSerializeContext, instancePointer, classData, nullptr, productDependencySet, productPathDependencySet, true);
            return true;
        }, assetReadyFilterDescriptor))
        {
            return false;
        }
        fillDependencyVectorFromSet(productDependencies, productDependencySet);
        return true;
    }

    template<class T>
    bool GatherProductDependencies(
        AZ::SerializeContext& serializeContext,
        T* obj,
        AZStd::vector<AssetBuilderSDK::ProductDependency>& productDependencies,
        ProductPathDependencySet& productPathDependencySet,
        const AZStd::function<bool(
            const AZ::SerializeContext& serializeContext,
            void* instancePointer,
            const AZ::SerializeContext::ClassData* classData,
            const AZ::SerializeContext::ClassElement* classElement,
            AZStd::unordered_set<AZ::Data::AssetId>& productDependencySet,
            ProductPathDependencySet& productPathDependencySet,
            bool enumerateChildren)>& handler = &AssetBuilderSDK::UpdateDependenciesFromClassData)
    {
        if (obj == nullptr)
        {
            AZ_Error("AssetBuilderSDK", false, "Cannot gather product dependencies for null data.");
            return false;
        }

        // start with a set to make it easy to avoid duplicate entries.
        AZStd::unordered_set<AZ::Data::AssetId> productDependencySet;
        auto beginCallback = [&serializeContext, &productDependencySet, &productPathDependencySet, handler](void* instancePointer, const AZ::SerializeContext::ClassData* classData, const AZ::SerializeContext::ClassElement* classElement)
        {
            // EnumerateObject already visits every element, so no need to enumerate farther, set enumerateChildren to false.
            return handler(serializeContext, instancePointer, classData, classElement, productDependencySet, productPathDependencySet, false);
        };
        bool enumerateResult = serializeContext.EnumerateObject(obj, beginCallback, {}, AZ::SerializeContext::ENUM_ACCESS_FOR_READ, nullptr);
        fillDependencyVectorFromSet(productDependencies, productDependencySet);
        return enumerateResult;
    }
}
