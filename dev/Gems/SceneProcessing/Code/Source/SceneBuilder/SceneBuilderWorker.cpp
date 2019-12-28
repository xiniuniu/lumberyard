/*
* All or portions of this file Copyright(c) Amazon.com, Inc.or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
*or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
*WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/containers/set.h>
#include <AzFramework/Application/Application.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzToolsFramework/Debug/TraceContext.h>
#include <SceneAPI/SceneCore/Components/ExportingComponent.h>
#include <SceneAPI/SceneCore/Components/LoadingComponent.h>
#include <SceneAPI/SceneCore/Components/Utilities/EntityConstructor.h>
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Containers/SceneManifest.h>
#include <SceneAPI/SceneCore/Events/AssetImportRequest.h>
#include <SceneAPI/SceneCore/Events/ExportProductList.h>
#include <SceneAPI/SceneCore/Events/ExportEventContext.h>
#include <SceneAPI/SceneCore/Events/SceneSerializationBus.h>
#include <SceneAPI/SceneCore/Utilities/Reporting.h>

#include <AssetBuilderSDK/AssetBuilderSDK.h>
#include <SceneBuilder/SceneBuilderWorker.h>
#include <SceneBuilder/TraceMessageHook.h>

namespace SceneBuilder
{
    void SceneBuilderWorker::ShutDown()
    {
        m_isShuttingDown = true;
    }

    const char* SceneBuilderWorker::GetFingerprint() const
    {
        if (m_cachedFingerprint.empty())
        {
            // put them in an ORDERED set so that changing the reflection
            // or the gems loaded does not invalidate FBX files due to order of reflection changing.
            AZStd::set<AZStd::string> fragments;

            AZ::SerializeContext* context = nullptr;
            AZ::ComponentApplicationBus::BroadcastResult(context, &AZ::ComponentApplicationBus::Events::GetSerializeContext);
            if (context)
            {
                auto callback = [&fragments](const AZ::SerializeContext::ClassData* data, const AZ::Uuid& typeId)
                {
                    AZ_UNUSED(typeId);
                    fragments.insert(AZStd::string::format("[%s:v%i]", data->m_name, data->m_version));
                    return true;
                };

                context->EnumerateDerived(callback, azrtti_typeid<AZ::SceneAPI::SceneCore::ExportingComponent>(), azrtti_typeid<AZ::SceneAPI::SceneCore::ExportingComponent>());
                context->EnumerateDerived(callback, azrtti_typeid<AZ::SceneAPI::SceneCore::LoadingComponent>(), azrtti_typeid<AZ::SceneAPI::SceneCore::LoadingComponent>());
            }

            for (const AZStd::string& element : fragments)
            {
                m_cachedFingerprint.append(element);
            }
        }

        return m_cachedFingerprint.c_str();
    }

    void SceneBuilderWorker::CreateJobs(const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response)
    {
        // Check for shutdown
        if (m_isShuttingDown)
        {
            response.m_result = AssetBuilderSDK::CreateJobsResultCode::ShuttingDown;
            return;
        }

        for (auto& enabledPlatform : request.m_enabledPlatforms)
        {
            AssetBuilderSDK::JobDescriptor descriptor;
            descriptor.m_jobKey = "Scene compilation";
            descriptor.SetPlatformIdentifier(enabledPlatform.m_identifier.c_str());
            descriptor.m_failOnError = true;
            descriptor.m_priority = 11; // more important than static mesh files, since these may control logic (actors and motions specifically)
            descriptor.m_additionalFingerprintInfo = GetFingerprint();
            response.m_createJobOutputs.push_back(descriptor);
        }

        // Adding corresponding material file as a source file dependency
        AssetBuilderSDK::SourceFileDependency sourceFileDependencyInfo;
        AZStd::string relPath = request.m_sourceFile.c_str();
        AzFramework::StringFunc::Path::ReplaceExtension(relPath, "mtl");
        sourceFileDependencyInfo.m_sourceFileDependencyPath = relPath;
        response.m_sourceFileDependencyList.push_back(sourceFileDependencyInfo);

        response.m_result = AssetBuilderSDK::CreateJobsResultCode::Success;
    }

    void SceneBuilderWorker::ProcessJob(const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response)
    {
        using namespace AZ::SceneAPI::Containers;

        // Only used during processing to redirect trace printfs with an warning or error window to the appropriate reporting function.
        TraceMessageHook messageHook;
        
        // Load Scene graph and manifest from the provided path and then initialize them.
        if (m_isShuttingDown)
        {
            AZ_TracePrintf(AZ::SceneAPI::Utilities::LogWindow, "Loading scene was cancelled.\n");
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
            return;
        }

        AZStd::shared_ptr<Scene> scene;
        if (!LoadScene(scene, request, response))
        {
            return;
        }

        // Process the scene.
        if (m_isShuttingDown)
        {
            AZ_TracePrintf(AZ::SceneAPI::Utilities::LogWindow, "Processing scene was cancelled.\n");
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
            return;
        }
        if (!ExportScene(scene, request, response))
        {
            return;
        }

        AZ_TracePrintf(AZ::SceneAPI::Utilities::LogWindow, "Finalizing scene processing.\n");
        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
    }

    AZ::Uuid SceneBuilderWorker::GetUUID()
    {
        return AZ::Uuid::CreateString("{BD8BF658-9485-4FE3-830E-8EC3A23C35F3}");
    }

    void SceneBuilderWorker::PopulateProductDependencies(const AZ::SceneAPI::Events::ExportProduct& exportProduct, const char* watchFolder, AssetBuilderSDK::JobProduct& jobProduct) const
    {
        // Register the product dependencies and path dependencies from the export product to the job product.
        for (const AZ::SceneAPI::Events::ExportProduct& dependency : exportProduct.m_productDependencies)
        {
            jobProduct.m_dependencies.emplace_back(dependency.m_id, 0);
        }
        for (const AZStd::string& pathDependency : exportProduct.m_legacyPathDependencies)
        {
            // SceneCore doesn't have access to AssetBuilderSDK, so it doesn't have access to the 
            //  ProductPathDependency type or the ProductPathDependencyType enum. Exporters registered with the
            //  Scene Builder should report path dependencies on source files as absolute paths, while dependencies 
            //  on product files should be reported as relative paths.
            if (AzFramework::StringFunc::Path::IsRelative(pathDependency.c_str()))
            {
                // Make sure the path is relative to the watch folder. Paths passed in might be using asset database separators.
                //  Convert to system separators for path manipulation.
                AZStd::string normalizedPathDependency = pathDependency;
                AZStd::string normalizedWatchFolder(watchFolder);
                AZStd::string assetRootRelativePath;
                AzFramework::StringFunc::Path::Normalize(normalizedWatchFolder);
                AzFramework::StringFunc::Path::Normalize(normalizedPathDependency);
                AzFramework::StringFunc::Path::Join(normalizedWatchFolder.c_str(), normalizedPathDependency .c_str(), assetRootRelativePath, true);
                AzFramework::ApplicationRequests::Bus::Broadcast(&AzFramework::ApplicationRequests::Bus::Events::MakePathRelative, assetRootRelativePath, watchFolder);
                jobProduct.m_pathDependencies.emplace(assetRootRelativePath, AssetBuilderSDK::ProductPathDependencyType::ProductFile);
            }
            else
            {
                jobProduct.m_pathDependencies.emplace(pathDependency, AssetBuilderSDK::ProductPathDependencyType::SourceFile);
            }
        }
    }

    bool SceneBuilderWorker::LoadScene(AZStd::shared_ptr<AZ::SceneAPI::Containers::Scene>& result,
        const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response)
    {
        using namespace AZ::SceneAPI;
        using namespace AZ::SceneAPI::Containers;
        using namespace AZ::SceneAPI::Events;
        
        AZ_TracePrintf(Utilities::LogWindow, "Loading scene.\n");

        SceneSerializationBus::BroadcastResult(result, &SceneSerializationBus::Events::LoadScene, request.m_fullPath, request.m_sourceFileUUID);
        if (!result)
        {
            AZ_TracePrintf(Utilities::ErrorWindow, "Failed to load scene file.\n");
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return false;
        }
        AZ_TraceContext("Manifest", result->GetManifestFilename());
        if (result->GetManifest().IsEmpty())
        {
            AZ_TracePrintf(Utilities::WarningWindow, "No manifest loaded and not enough information to create a default manifest.\n");
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
            return false; // Still return false as there's no work so should exit.
        }
        
        return true;
    }

    bool SceneBuilderWorker::ExportScene(const AZStd::shared_ptr<AZ::SceneAPI::Containers::Scene>& scene,
        const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response)
    {
        using namespace AZ::SceneAPI;
        using namespace AZ::SceneAPI::Events;
        using namespace AZ::SceneAPI::SceneCore;

        AZ_Assert(scene, "Invalid scene passed for exporting.");
        
        const char* outputFolder = request.m_tempDirPath.c_str();
        const char* platformIdentifier = request.m_jobDescription.GetPlatformIdentifier().c_str();
        AZ_TraceContext("Output folder", outputFolder);
        AZ_TraceContext("Platform", platformIdentifier);
        AZ_TracePrintf(Utilities::LogWindow, "Processing scene.\n");

        AZ_TracePrintf(Utilities::LogWindow, "Creating export entities.\n");
        EntityConstructor::EntityPointer exporter = EntityConstructor::BuildEntity("Scene Exporters", ExportingComponent::TYPEINFO_Uuid());

        ExportProductList productList;
        ProcessingResultCombiner result;
        AZ_TracePrintf(Utilities::LogWindow, "Preparing for export.\n");
        result += Process<PreExportEventContext>(productList, outputFolder, *scene, platformIdentifier);
        AZ_TracePrintf(Utilities::LogWindow, "Exporting...\n");
        result += Process<ExportEventContext>(productList, outputFolder, *scene, platformIdentifier);
        AZ_TracePrintf(Utilities::LogWindow, "Finalizing export process.\n");
        result += Process<PostExportEventContext>(productList, outputFolder, platformIdentifier);

        AZ_TracePrintf(Utilities::LogWindow, "Collecting and registering products.\n");
        for (const ExportProduct& product : productList.GetProducts())
        {
            AZ::u32 subId = BuildSubId(product);
            AZ_TracePrintf(Utilities::LogWindow, "Listed product: %s+0x%08x - %s (type %s)\n", product.m_id.ToString<AZStd::string>().c_str(),
                subId, product.m_filename.c_str(), product.m_assetType.ToString<AZStd::string>().c_str());

            AssetBuilderSDK::JobProduct jobProduct(AZStd::move(product.m_filename), product.m_assetType, subId);
            PopulateProductDependencies(product, request.m_watchFolder.c_str(), jobProduct);

            response.m_outputProducts.emplace_back(jobProduct);
            // Unlike the version in ResourceCompilerScene/SceneCompiler.cpp, this version doesn't need to deal with sub ids that were
            // created before explicit sub ids were added to the SceneAPI.
        }

        switch (result.GetResult())
        {
        case ProcessingResult::Success:
            return true;
        case ProcessingResult::Ignored:
            // While ResourceCompilerScene is still around there's situations where either this builder or RCScene does work but the other not.
            // That used to be a cause for a warning and will be again once RCScene has been removed. It's not possible to detect if either
            // did any work so the warning is disabled for now.
            // AZ_TracePrintf(Utilities::WarningWindow, "Nothing found to convert and export.\n");
            return true;
        case ProcessingResult::Failure:
            AZ_TracePrintf(Utilities::ErrorWindow, "Failure during conversion and exporting.\n");
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return false;
        default:
            AZ_TracePrintf(Utilities::ErrorWindow,
                "Unexpected result from conversion and exporting (%i).\n", result.GetResult());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            return false;
        }

        return true;
    }

    // BuildSubId has an equivalent counterpart in ResourceCompilerScene. Both need to remain the same to avoid problems with sub ids.
    AZ::u32 SceneBuilderWorker::BuildSubId(const AZ::SceneAPI::Events::ExportProduct& product) const
    {
        // Instead of the just the lower 16-bits, use the full 32-bits that are available. There are production examples of
        // uber-fbx files that contain hundreds of meshes that need to be split into individual mesh objects as an example.
        AZ::u32 id = static_cast<AZ::u32>(product.m_id.GetHash());

        if (product.m_lod != AZ::SceneAPI::Events::ExportProduct::s_LodNotUsed)
        {
            AZ::u8 lod = static_cast<AZ::u8>(product.m_lod);
            if (lod > 0xF)
            {
                AZ_TracePrintf(AZ::SceneAPI::Utilities::WarningWindow, "%i is too large to fit in the allotted bits for LOD.\n", static_cast<AZ::u32>(lod));
                lod = 0xF;
            }
            // The product uses lods so mask out the lod bits and set them appropriately.
            id &= ~AssetBuilderSDK::SUBID_MASK_LOD_LEVEL;
            id |= lod << AssetBuilderSDK::SUBID_LOD_LEVEL_SHIFT;
        }

        return id;
    }
} // namespace SceneBuilder
