/*
* All or portions of this file Copyright(c) Amazon.com, Inc.or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution(the "License").All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file.Do not
* remove or modify any license notices.This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include "SchemaBuilderWorker.h"
#include "SchemaUtils.h"

#include <AzFramework/FileFunc/FileFunc.h>

namespace CopyDependencyBuilder
{
    SchemaBuilderWorker::SchemaBuilderWorker()
        : CopyDependencyBuilderWorker("xmlschema", true, true)
    {
    }

    void SchemaBuilderWorker::RegisterBuilderWorker()
    {
        AssetBuilderSDK::AssetBuilderDesc xmlSchemaBuilderDescriptor;
        xmlSchemaBuilderDescriptor.m_name = "SchemaBuilderWorker";
        xmlSchemaBuilderDescriptor.m_patterns.push_back(AssetBuilderSDK::AssetBuilderPattern("*.xmlschema", AssetBuilderSDK::AssetBuilderPattern::PatternType::Wildcard));
        xmlSchemaBuilderDescriptor.m_busId = azrtti_typeid<SchemaBuilderWorker>();
        xmlSchemaBuilderDescriptor.m_version = 2;
        xmlSchemaBuilderDescriptor.m_createJobFunction =
            AZStd::bind(&SchemaBuilderWorker::CreateJobs, this, AZStd::placeholders::_1, AZStd::placeholders::_2);
        xmlSchemaBuilderDescriptor.m_processJobFunction =
            AZStd::bind(&SchemaBuilderWorker::ProcessJob, this, AZStd::placeholders::_1, AZStd::placeholders::_2);

        BusConnect(xmlSchemaBuilderDescriptor.m_busId);
        AssetBuilderSDK::AssetBuilderBus::Broadcast(&AssetBuilderSDK::AssetBuilderBusTraits::RegisterBuilderInformation, xmlSchemaBuilderDescriptor);
    }

    void SchemaBuilderWorker::UnregisterBuilderWorker()
    {
        BusDisconnect();
    }

    AZ::Outcome<AZStd::vector<AZStd::string>, AZStd::string> SchemaBuilderWorker::GetSourcesToReprocess(
        const AssetBuilderSDK::ProcessJobRequest& request) const
    {
        AZStd::vector<AZStd::string> reverseSourceDependences;

        // Iterate through each file in the asset safe folder and check whether it matches the schema matching rules
        AzFramework::XmlSchemaAsset schemaAsset;
        if (!AZ::Utils::LoadObjectFromFileInPlace(request.m_fullPath, schemaAsset))
        {
            return AZ::Failure(AZStd::string::format("Failed to load schema file: %s.", request.m_fullPath.c_str()));
        }

        if (schemaAsset.GetMatchingRules().size() == 0)
        {
            return AZ::Failure(AZStd::string::format("Matching rules are missing."));
        }

        if (schemaAsset.GetDependencySearchRules().size() == 0 && !schemaAsset.UseAZSerialization())
        {
            return AZ::Failure(AZStd::string::format("Dependency search rules are missing."));
        }


        bool result;
        AZStd::vector<AZStd::string> assetSafeFolders;
        AzToolsFramework::AssetSystemRequestBus::BroadcastResult(result, &AzToolsFramework::AssetSystem::AssetSystemRequest::GetAssetSafeFolders, assetSafeFolders);
        if (!result)
        {
            return AZ::Failure(AZStd::string("Failed to get asset safe folders."));
        }

        for (const AZStd::string& assetSafeFolder : assetSafeFolders)
        {
            auto findFilesResult = AzFramework::FileFunc::FindFilesInPath(assetSafeFolder, "*", true);
            if (!findFilesResult.IsSuccess())
            {
                continue;
            }

            for (const AZStd::string& sourceAssetPath : findFilesResult.TakeValue())
            {
                if (SourceFileDependsOnSchema(schemaAsset, sourceAssetPath))
                {
                    reverseSourceDependences.emplace_back(sourceAssetPath);
                }
            }
        }

        return AZ::Success(reverseSourceDependences);
    }

    bool SchemaBuilderWorker::ParseProductDependencies(
        const AssetBuilderSDK::ProcessJobRequest& /*request*/,
        AZStd::vector<AssetBuilderSDK::ProductDependency>& /*productDependencies*/,
        AssetBuilderSDK::ProductPathDependencySet& /*pathDependencies*/)
    {
        return true;
    }

    AZ::Data::AssetType SchemaBuilderWorker::GetAssetType(const AZStd::string& /*fileName*/) const
    {
        return azrtti_typeid<AzFramework::XmlSchemaAsset>();
    }
}
