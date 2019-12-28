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

#include <PhysX_precompiled.h>

#include <AzFramework/StringFunc/StringFunc.h>
#include <AzToolsFramework/Debug/TraceContext.h>
#include <SceneAPI/SceneCore/Containers/Utilities/Filters.h>
#include <SceneAPI/SceneCore/Containers/Utilities/SceneGraphUtilities.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMeshData.h>
#include <SceneAPI/SceneCore/Events/ExportEventContext.h>
#include <SceneAPI/SceneCore/Events/ExportProductList.h>
#include <SceneAPI/SceneCore/Utilities/FileUtilities.h>
#include <SceneAPI/SceneCore/Utilities/Reporting.h>
#include <SceneAPI/SceneCore/Containers/Views/SceneGraphChildIterator.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMaterialData.h>

#include <PhysX/MeshAsset.h>
#include <AzFramework/Physics/Material.h>
#include <Source/Pipeline/MeshAssetHandler.h>
#include <Source/Pipeline/MeshExporter.h>
#include <Source/Pipeline/MeshGroup.h>
#include <Source/Utils.h>

#include <PxPhysicsAPI.h>

#include <Cry_Math.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/Math/Matrix3x3.h>
#include <AzCore/XML/rapidxml.h>
#include <GFxFramework/MaterialIO/Material.h>

// A utility macro helping set/clear bits in a single line
#define SET_BITS(flags, condition, bits) flags = (condition) ? ((flags) | (bits)) : ((flags) & ~(bits))

using namespace AZ::SceneAPI;

namespace PhysX
{
    namespace Pipeline
    {
        namespace SceneContainers = AZ::SceneAPI::Containers;
        namespace SceneEvents = AZ::SceneAPI::Events;
        namespace SceneUtil = AZ::SceneAPI::Utilities;

        static physx::PxDefaultAllocator pxDefaultAllocatorCallback;
        static const char* const s_defaultSurfaceType = "mat_default";

        /// Implementation of the PhysX error callback interface directing errors to ErrorWindow output.
        ///
        static class PxExportErrorCallback
            : public physx::PxErrorCallback
        {
        public:
            virtual void reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line)
            {
                AZ_TracePrintf(AZ::SceneAPI::Utilities::ErrorWindow, "PxErrorCode %i: %s (line %i in %s)", code, message, line, file);
            }
        } pxDefaultErrorCallback;

        MeshExporter::MeshExporter()
        {
            BindToCall(&MeshExporter::ProcessContext);
        }

        void MeshExporter::Reflect(AZ::ReflectContext* context)
        {
            AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
            if (serializeContext)
            {
                serializeContext->Class<MeshExporter, AZ::SceneAPI::SceneCore::ExportingComponent>()->Version(2);
            }
        }

        namespace Utils
        {
            AZ::u16 GetMaterialIndex(const AZStd::string& name, AZStd::vector<Physics::MaterialConfiguration>& materials)
            {
                for (AZ::u16 i = 0; i < materials.size(); ++i)
                {
                    if (materials[i].m_surfaceType == name)
                    {
                        return i;
                    }
                }

                Physics::MaterialConfiguration newConfiguration;
                newConfiguration.m_surfaceType = name;

                materials.push_back(newConfiguration);

                return materials.size() - 1;
            }

            void BuildMaterialToSurfaceTypeMap(const AZStd::string& materialFilename,
                AZStd::unordered_map<AZStd::string, AZStd::string>& materialToSurfaceTypeMap)
            {
                AZ::IO::SystemFile mtlFile;
                bool fileOpened = mtlFile.Open(materialFilename.c_str(), AZ::IO::SystemFile::SF_OPEN_READ_ONLY);
                if (fileOpened && mtlFile.Length() != 0)
                {
                    //Read material override file into a buffer
                    AZStd::vector<char> buffer(mtlFile.Length());
                    mtlFile.Read(mtlFile.Length(), buffer.data());
                    mtlFile.Close();

                    //Apparently in rapidxml if 'parse_no_data_nodes' isn't set it creates both value and data nodes 
                    //with the data nodes having precedence such that updating values doesn't work. 
                    AZ::rapidxml::xml_document<char>  document;
                    document.parse<AZ::rapidxml::parse_no_data_nodes>(buffer.data());

                    //Parse MTL file for materials and/or submaterials. 
                    AZ::rapidxml::xml_node<char>* rootMaterialNode = document.first_node(AZ::GFxFramework::MaterialExport::g_materialString);

                    AZ::rapidxml::xml_node<char>* subMaterialNode = rootMaterialNode->first_node(AZ::GFxFramework::MaterialExport::g_subMaterialString);

                    if (subMaterialNode)
                    {
                        for (AZ::rapidxml::xml_node<char>* materialNode = subMaterialNode->first_node(AZ::GFxFramework::MaterialExport::g_materialString);
                            materialNode;
                            materialNode = materialNode->next_sibling(AZ::GFxFramework::MaterialExport::g_materialString))
                        {
                            AZ::rapidxml::xml_attribute<char>* nameAttribute = materialNode->first_attribute(AZ::GFxFramework::MaterialExport::g_nameString);
                            if (nameAttribute)
                            {
                                AZStd::string materialName = nameAttribute->value();
                                AZStd::string surfaceTypeName = s_defaultSurfaceType;

                                AZ::rapidxml::xml_attribute<char>* surfaceTypeNode = materialNode->first_attribute("SurfaceType");
                                if (surfaceTypeNode && surfaceTypeNode->value_size() != 0)
                                {
                                    surfaceTypeName = surfaceTypeNode->value();
                                }

                                materialToSurfaceTypeMap[materialName] = surfaceTypeName;
                            }
                            else
                            {
                                AZ_TracePrintf(AZ::SceneAPI::Utilities::ErrorWindow, "A SubMaterial without Name found in the .mtl file: %s", materialFilename.c_str());
                            }
                        }
                    }
                    else
                    {
                        AZ_TracePrintf(AZ::SceneAPI::Utilities::ErrorWindow, "No SubMaterial node in the .mtl file: %s", materialFilename.c_str());
                    }
                }
                else
                {
                    AZ_TracePrintf(AZ::SceneAPI::Utilities::ErrorWindow, "Unable to open .mtl file: %s", materialFilename.c_str());
                }
            }

            void ResolveMaterialSlotsSoftNamingConvention(AZStd::vector<Physics::MaterialConfiguration>& originalMaterials, 
                AZStd::vector<AZStd::string>& outSlots, 
                const AZStd::unordered_map<AZStd::string, AZStd::string>& materialToSurfaceTypeMap)
            {

                outSlots.clear();
                outSlots.reserve(originalMaterials.size());

                int slotCounter = 0;

                for (auto& materialConfiguration : originalMaterials)
                {
                    // MaterialConfiguration::surfaceType stores the material name assigned in DCC initially
                    AZStd::string materialName = materialConfiguration.m_surfaceType;

                    AZStd::string surfaceType = s_defaultSurfaceType;
                    
                    // Here we assign the actual engine surface type based on the material name
                    auto materialToSurfaceIt = materialToSurfaceTypeMap.find(materialName);
                    if (materialToSurfaceIt != materialToSurfaceTypeMap.end())
                    {
                        surfaceType = materialToSurfaceIt->second;
                    }

                    outSlots.emplace_back(materialName);

                    // Remove the mat_ prefix since the material library generated from surface types doesn't have it.
                    if (surfaceType.find("mat_") == 0)
                    {
                        surfaceType = surfaceType.substr(4);
                    }

                    // Save the actual surface type now
                    materialConfiguration.m_surfaceType = surfaceType;

                    slotCounter++;
                }
            }

            bool ValidateCookedTriangleMesh(void* assetData, AZ::u32 assetDataSize)
            {
                physx::PxDefaultMemoryInputData inpStream(static_cast<physx::PxU8*>(assetData), assetDataSize);
                physx::PxTriangleMesh* triangleMesh = PxGetPhysics().createTriangleMesh(inpStream);

                bool success = triangleMesh != nullptr;
                triangleMesh->release();
                return success;
            }

            bool ValidateCookedConvexMesh(void* assetData, AZ::u32 assetDataSize)
            {
                physx::PxDefaultMemoryInputData inpStream(static_cast<physx::PxU8*>(assetData), assetDataSize);
                physx::PxConvexMesh* convexMesh = PxGetPhysics().createConvexMesh(inpStream);

                bool success = convexMesh != nullptr;
                convexMesh->release();
                return success;
            }

            AZStd::vector<AZStd::string> GenerateLocalNodeMaterialMap(const AZ::SceneAPI::Containers::SceneGraph& graph, const AZ::SceneAPI::Containers::SceneGraph::NodeIndex& nodeIndex)
            {
                AZStd::vector<AZStd::string> materialNames;

                auto view = AZ::SceneAPI::Containers::Views::MakeSceneGraphChildView<AZ::SceneAPI::Containers::Views::AcceptEndPointsOnly>(
                    graph, 
                    nodeIndex, 
                    graph.GetContentStorage().begin(), 
                    true
                );

                for (auto it = view.begin(), itEnd = view.end(); it != itEnd; ++it)
                {
                    if ((*it) && (*it)->RTTI_IsTypeOf(AZ::SceneAPI::DataTypes::IMaterialData::TYPEINFO_Uuid()))
                    {
                        AZStd::string nodeName = graph.GetNodeName(graph.ConvertToNodeIndex(it.GetHierarchyIterator())).GetName();
                        materialNames.push_back(nodeName);
                    }
                }

                return materialNames;
            }
        }

        static void AccumulateMeshes(
            const AZStd::shared_ptr<const AZ::SceneAPI::DataTypes::IMeshData>& meshToExport, 
            const AZ::Transform& worldTransform, 
            const AZStd::vector<AZStd::string>& localFbxMaterialsList,
            AZStd::vector<Vec3>& vertices, AZStd::vector<AZ::u32>& indices, 
            AZStd::vector<physx::PxMaterialTableIndex>& faceMaterialIndices, 
            AZStd::vector<Physics::MaterialConfiguration>& materialConfigruations)
        {
            // append the vertices
            AZ::u32 vertexCount = meshToExport->GetVertexCount();
            AZ::u32 vertOffset = vertices.size();
            vertices.resize(vertices.size() + vertexCount);

            for (int i = 0; i < vertexCount; ++i)
            {
                AZ::Vector3 pos = meshToExport->GetPosition(i);
                pos = worldTransform * pos;
                vertices[vertOffset + i] = Vec3(pos.GetX(), pos.GetY(), pos.GetZ());
            }

            // append the indices
            int faceCount = meshToExport->GetFaceCount();
            AZ::u32 indexOffset = indices.size();
            AZ::u32 faceIndexOffset = faceMaterialIndices.size();

            indices.resize(indices.size() + faceCount * 3);
            faceMaterialIndices.resize(faceMaterialIndices.size() + faceCount);

            for (int i = 0; i < faceCount; ++i)
            {
                AZ::SceneAPI::DataTypes::IMeshData::Face face = meshToExport->GetFaceInfo(i);
                indices[indexOffset + i*3] = vertOffset + face.vertexIndex[0];
                indices[indexOffset + i*3 + 1] = vertOffset + face.vertexIndex[1];
                indices[indexOffset + i*3 + 2] = vertOffset + face.vertexIndex[2];

                int materialId = meshToExport->GetFaceMaterialId(i);
                if (materialId < localFbxMaterialsList.size())
                {
                    auto& materialName = localFbxMaterialsList[materialId];
                    AZ::u16 materialIndex = Utils::GetMaterialIndex(materialName, materialConfigruations);
                    faceMaterialIndices[faceIndexOffset + i] = materialIndex;
                }
            }
        }

        static physx::PxMeshMidPhase::Enum GetMidPhaseStructureType(const AZStd::string& platformIdentifier)
        {
            // Use by default 3.4 since 3.3 is being deprecated (despite being default)
            physx::PxMeshMidPhase::Enum ret = physx::PxMeshMidPhase::eBVH34;

            // Fallback to 3.3 on Android and iOS platforms since they don't support SSE2, which is required for 3.4
            if (platformIdentifier == "es3" || platformIdentifier == "ios")
            {
                ret = physx::PxMeshMidPhase::eBVH33;
            }
            return ret;
        }

        // used by CGFMeshAssetBuilderWorker.cpp to generate .pxmesh from cgf
        bool CookPhysxTriangleMesh(
            const AZStd::vector<Vec3>& vertices,
            const AZStd::vector<AZ::u32>& indices,
            const AZStd::vector<AZ::u16>& faceMaterials,
            AZStd::vector<AZ::u8>* output,
            const MeshGroup& meshGroup,
            const AZStd::string& platformIdentifier
        ) {
            bool cookingSuccessful = false;
            AZStd::string cookingResultErrorCodeString;
            bool shouldExportAsConvex = meshGroup.GetExportAsConvex();

            physx::PxCookingParams pxCookingParams = physx::PxCookingParams(physx::PxTolerancesScale());

            pxCookingParams.buildGPUData = false;
            pxCookingParams.midphaseDesc.setToDefault(GetMidPhaseStructureType(platformIdentifier));

            if (shouldExportAsConvex)
            {
                if (meshGroup.GetCheckZeroAreaTriangles())
                {
                    pxCookingParams.areaTestEpsilon = meshGroup.GetAreaTestEpsilon();
                }

                pxCookingParams.planeTolerance = meshGroup.GetPlaneTolerance();
                pxCookingParams.gaussMapLimit = meshGroup.GetGaussMapLimit();
            }
            else
            {
                pxCookingParams.midphaseDesc.mBVH34Desc.numPrimsPerLeaf = meshGroup.GetNumTrisPerLeaf();
                pxCookingParams.meshWeldTolerance = meshGroup.GetMeshWeldTolerance();
                pxCookingParams.buildTriangleAdjacencies = meshGroup.GetBuildTriangleAdjacencies();
                pxCookingParams.suppressTriangleMeshRemapTable = meshGroup.GetSuppressTriangleMeshRemapTable();

                if (meshGroup.GetWeldVertices())
                {
                    pxCookingParams.meshPreprocessParams |= physx::PxMeshPreprocessingFlag::eWELD_VERTICES;
                }

                if (meshGroup.GetDisableCleanMesh())
                {
                    pxCookingParams.meshPreprocessParams |= physx::PxMeshPreprocessingFlag::eDISABLE_CLEAN_MESH;
                }

                if (meshGroup.GetForce32BitIndices())
                {
                    pxCookingParams.meshPreprocessParams |= physx::PxMeshPreprocessingFlag::eFORCE_32BIT_INDICES;
                }
            }

            physx::PxCooking* pxCooking = PxCreateCooking(PX_PHYSICS_VERSION, PxGetFoundation(), pxCookingParams);
            AZ_Assert(pxCooking, "Failed to create PxCooking");

            physx::PxBoundedData strideData;
            strideData.count = vertices.size();
            strideData.stride = sizeof(Vec3);
            strideData.data = vertices.data();

            physx::PxDefaultMemoryOutputStream cookedMeshData;

            if (shouldExportAsConvex)
            {
                physx::PxConvexMeshDesc convexDesc;
                convexDesc.points = strideData;
                convexDesc.flags = physx::PxConvexFlag::eCOMPUTE_CONVEX;

                SET_BITS(convexDesc.flags, meshGroup.GetUse16bitIndices(), physx::PxConvexFlag::e16_BIT_INDICES);
                SET_BITS(convexDesc.flags, meshGroup.GetCheckZeroAreaTriangles(), physx::PxConvexFlag::eCHECK_ZERO_AREA_TRIANGLES);
                SET_BITS(convexDesc.flags, meshGroup.GetQuantizeInput(), physx::PxConvexFlag::eQUANTIZE_INPUT);
                SET_BITS(convexDesc.flags, meshGroup.GetUsePlaneShifting(), physx::PxConvexFlag::ePLANE_SHIFTING);
                SET_BITS(convexDesc.flags, meshGroup.GetBuildGPUData(), physx::PxConvexFlag::eGPU_COMPATIBLE);
                SET_BITS(convexDesc.flags, meshGroup.GetShiftVertices(), physx::PxConvexFlag::eSHIFT_VERTICES);

                physx::PxConvexMeshCookingResult::Enum convexCookingResultCode = physx::PxConvexMeshCookingResult::eSUCCESS;

                cookingSuccessful = 
                    pxCooking->cookConvexMesh(convexDesc, cookedMeshData, &convexCookingResultCode)
                    && Utils::ValidateCookedConvexMesh(cookedMeshData.getData(), cookedMeshData.getSize());

                cookingResultErrorCodeString = PhysX::Utils::ConvexCookingResultToString(convexCookingResultCode);
            }
            else
            {
                physx::PxTriangleMeshDesc meshDesc;
                meshDesc.points = strideData;

                meshDesc.triangles.count = indices.size() / 3;
                meshDesc.triangles.stride = sizeof(AZ::u32) * 3;
                meshDesc.triangles.data = indices.data();

                meshDesc.materialIndices.stride = sizeof(AZ::u16);
                meshDesc.materialIndices.data = faceMaterials.data();

                physx::PxTriangleMeshCookingResult::Enum trimeshCookingResultCode = physx::PxTriangleMeshCookingResult::eSUCCESS;

                cookingSuccessful = 
                    pxCooking->cookTriangleMesh(meshDesc, cookedMeshData, &trimeshCookingResultCode) 
                    && Utils::ValidateCookedTriangleMesh(cookedMeshData.getData(), cookedMeshData.getSize());

                cookingResultErrorCodeString = PhysX::Utils::TriMeshCookingResultToString(trimeshCookingResultCode);
            }

            if (cookingSuccessful)
            {
                output->insert(output->end(), cookedMeshData.getData(), cookedMeshData.getData() + cookedMeshData.getSize());
            }
            else
            {
                AZ_TracePrintf(AZ::SceneAPI::Utilities::ErrorWindow, "Cooking Mesh failed: %s", cookingResultErrorCodeString.c_str());
            }

            pxCooking->release();
            return cookingSuccessful;
        }

        static AZ::SceneAPI::Events::ProcessingResult WritePhysx(
            AZ::SceneAPI::Events::ExportEventContext& context,
            const AZStd::vector<AZ::u8>& cookedMeshData,
            const AZStd::vector<Physics::MaterialConfiguration>& materials,
            const MeshGroup& meshGroup
        ) {
            SceneEvents::ProcessingResult result = SceneEvents::ProcessingResult::Ignored;

            AZStd::string assetName = meshGroup.GetName();
            AZStd::string filename = SceneUtil::FileUtilities::CreateOutputFileName(assetName, context.GetOutputDirectory(), "physx");

            bool canStartWritingToFile = !filename.empty() && SceneUtil::FileUtilities::EnsureTargetFolderExists(filename);

            if (canStartWritingToFile)
            {
                result = SceneEvents::ProcessingResult::Success;

                // copy data into destination
                // ToBeDeprecated
            }

            return result;
        }

        static AZ::SceneAPI::Events::ProcessingResult WritePxmesh(
            AZ::SceneAPI::Events::ExportEventContext& context,
            const AZStd::vector<AZ::u8>& physxData,
            const AZStd::vector<Physics::MaterialConfiguration> &materials,
            const MeshGroup& meshGroup
        ) {
            SceneEvents::ProcessingResult result = SceneEvents::ProcessingResult::Ignored;

            AZStd::string assetName = meshGroup.GetName();
            AZStd::string filename = SceneUtil::FileUtilities::CreateOutputFileName(assetName, context.GetOutputDirectory(), MeshAssetHandler::s_assetFileExtension);

            bool canStartWritingToFile = !filename.empty() && SceneUtil::FileUtilities::EnsureTargetFolderExists(filename);
            
            if (canStartWritingToFile)
            {
                const AZ::SceneAPI::Events::ExportProductList& exportProductList = context.GetProductList();
                const AZ::SceneAPI::Containers::Scene& scene = context.GetScene();

                AZStd::string materialFilename = scene.GetSourceFilename();
                AzFramework::StringFunc::Path::ReplaceExtension(materialFilename, ".mtl");

                result = SceneEvents::ProcessingResult::Success;

                MeshAssetCookedData cookedData;
                cookedData.m_isConvexMesh = meshGroup.GetExportAsConvex();
                cookedData.m_cookedPxMeshData = physxData;

                if (!cookedData.m_isConvexMesh)
                {
                    cookedData.m_materialsData = materials;

                    // Read the information about surface type for each material from the .mtl file
                    AZStd::unordered_map<AZStd::string, AZStd::string> materialToSurfaceTypeMap;
                    Utils::BuildMaterialToSurfaceTypeMap(materialFilename, materialToSurfaceTypeMap);
                    
                    // Assign the surface types into the materials data
                    Utils::ResolveMaterialSlotsSoftNamingConvention(cookedData.m_materialsData, cookedData.m_materialSlots, materialToSurfaceTypeMap);
                }

                if (PhysX::Utils::WriteCookedMeshToFile(filename, cookedData))
                {
                    AZStd::string productUuidString = meshGroup.GetId().ToString<AZStd::string>();
                    AZ::Uuid productUuid = AZ::Uuid::CreateData(productUuidString.data(), productUuidString.size() * sizeof(productUuidString[0]));

                    context.GetProductList().AddProduct(AZStd::move(filename), productUuid, AZ::AzTypeInfo<MeshAsset>::Uuid());
                }
                else
                {
                    AZ_TracePrintf(AZ::SceneAPI::Utilities::ErrorWindow, "Unable to write to a file for a PhysX mesh asset. Filename: %s", filename.c_str());
                    result = SceneEvents::ProcessingResult::Failure;
                }
            }
            else
            {
                AZ_TracePrintf(AZ::SceneAPI::Utilities::ErrorWindow, "Unable to create a file for a PhysX mesh asset. AssetName: %s, filename: %s", assetName.c_str(), filename.c_str());
                result = SceneEvents::ProcessingResult::Failure;
            }

            return result;
        }

        SceneEvents::ProcessingResult MeshExporter::ProcessContext(SceneEvents::ExportEventContext& context) const
        {
            AZ_TraceContext("Exporter", "PhysX");

            SceneEvents::ProcessingResultCombiner result;

            const AZ::SceneAPI::Containers::Scene& scene = context.GetScene();
            const AZ::SceneAPI::Containers::SceneGraph& graph = scene.GetGraph();

            const SceneContainers::SceneManifest& manifest = context.GetScene().GetManifest();

            SceneContainers::SceneManifest::ValueStorageConstData valueStorage = manifest.GetValueStorage();
            auto view = SceneContainers::MakeExactFilterView<MeshGroup>(valueStorage);

            for (const MeshGroup& pxMeshGroup : view)
            {
                AZStd::vector<Vec3> accumulatedVertices;
                AZStd::vector<vtx_idx> accumulatedIndices;
                AZStd::vector<AZ::u16> accumulatedFaceMaterialIndicies;
                AZStd::vector<Physics::MaterialConfiguration> accumulatedMaterialConfigurations;

                AZStd::string groupName = pxMeshGroup.GetName();

                AZ_TraceContext("Group Name", groupName);

                const auto& sceneNodeSelectionList = pxMeshGroup.GetSceneNodeSelectionList();

                auto selectedNodeCount = sceneNodeSelectionList.GetSelectedNodeCount();

                for (size_t i = 0; i < selectedNodeCount; i++)
                {
                    auto nodeIndex = graph.Find(sceneNodeSelectionList.GetSelectedNode(i));
                    auto nodeMesh = azrtti_cast<const AZ::SceneAPI::DataTypes::IMeshData*>(*graph.ConvertToStorageIterator(nodeIndex));

                    AZStd::vector<AZStd::string> localFbxMaterialsList = Utils::GenerateLocalNodeMaterialMap(graph, nodeIndex);
                    const AZ::Transform worldTransform = SceneUtil::BuildWorldTransform(graph, nodeIndex);

                    if (nodeMesh)
                    {
                        AccumulateMeshes(
                            nodeMesh,
                            worldTransform,
                            localFbxMaterialsList,
                            accumulatedVertices,
                            accumulatedIndices,
                            accumulatedFaceMaterialIndicies,
                            accumulatedMaterialConfigurations
                        );
                    }
                }

                if (accumulatedVertices.size())
                {
                    AZStd::vector<AZ::u8> physxData;
                    bool success = CookPhysxTriangleMesh(accumulatedVertices, accumulatedIndices, accumulatedFaceMaterialIndicies, &physxData, pxMeshGroup, context.GetPlatformIdentifier());
                    if (success)
                    {
                        result += WritePxmesh(context, physxData, accumulatedMaterialConfigurations, pxMeshGroup);
                        result += WritePhysx(context, physxData, accumulatedMaterialConfigurations, pxMeshGroup);
                    }
                    else
                    {
                        result = SceneEvents::ProcessingResult::Failure;
                        AZ_TracePrintf(AZ::SceneAPI::Utilities::ErrorWindow, "PhysX Mesh group didn't have any vertices to cook");
                    }
                }
            }

            return result.GetResult();
        }
    }
}
