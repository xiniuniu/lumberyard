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

// include the required headers
#include <AzCore/std/containers/vector.h>
#include <MCore/Source/Array.h>
#include <MCore/Source/MemoryFile.h>
#include <MCore/Source/Endian.h>
#include <MCore/Source/Quaternion.h>
#include <MCore/Source/Color.h>
#include <EMotionFX/Source/WaveletSkeletalMotion.h>
#include <EMotionFX/Source/MorphTarget.h>
#include <EMotionFX/Source/Actor.h>
#include <EMotionFX/Source/Importer/SharedFileFormatStructs.h>
#include <EMotionFX/Source/Importer/MotionFileFormat.h>


// forward declaration
namespace MCore
{
    class CommandManager;
}


// forward declaration
namespace EMotionFX
{
    class Actor;
    class Node;
    class NodeGroup;
    class NodeLimitAttribute;
    class Material;
    class Mesh;
    class MorphTarget;
    class AnimGraph;
    class MotionEventTable;
    class Motion;
    class MotionSet;
    class SkeletalSubMotion;
    class MorphSubMotion;
}


namespace ExporterLib
{
    // windows platform endian type
    #define EXPLIB_PLATFORM_ENDIAN MCore::Endian::ENDIAN_LITTLE

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Helpers
    //////////////////////////////////////////////////////////////////////////////////////////////////////////

    // helpers
    //void FixSkeleton(Actor* actor, MCore::Array<MCore::Matrix>* outDeltaMatrices = nullptr);
    void CopyVector2(EMotionFX::FileFormat::FileVector2& to, const AZ::Vector2& from);
    void CopyVector(EMotionFX::FileFormat::FileVector3& to, const AZ::PackedVector3f& from);
    void CopyQuaternion(EMotionFX::FileFormat::FileQuaternion& to, MCore::Quaternion from);
    void Copy16BitQuaternion(EMotionFX::FileFormat::File16BitQuaternion& to, MCore::Quaternion from);
    void Copy16BitQuaternion(EMotionFX::FileFormat::File16BitQuaternion& to, MCore::Compressed16BitQuaternion from);
    void CopyColor(const MCore::RGBAColor& from, EMotionFX::FileFormat::FileColor& to);

    // endian conversion
    void ConvertUnsignedInt(uint32* value, MCore::Endian::EEndianType targetEndianType);
    void ConvertInt(int* value, MCore::Endian::EEndianType targetEndianType);
    void ConvertUnsignedShort(uint16* value, MCore::Endian::EEndianType targetEndianType);
    void ConvertFloat(float* value, MCore::Endian::EEndianType targetEndianType);
    void ConvertFileChunk(EMotionFX::FileFormat::FileChunk* value, MCore::Endian::EEndianType targetEndianType);
    void ConvertFileColor(EMotionFX::FileFormat::FileColor* value, MCore::Endian::EEndianType targetEndianType);
    void ConvertFileVector2(EMotionFX::FileFormat::FileVector2* value, MCore::Endian::EEndianType targetEndianType);
    void ConvertFileVector3(EMotionFX::FileFormat::FileVector3* value, MCore::Endian::EEndianType targetEndianType);
    void ConvertFile16BitVector3(EMotionFX::FileFormat::File16BitVector3* value, MCore::Endian::EEndianType targetEndianType);
    void ConvertFileQuaternion(EMotionFX::FileFormat::FileQuaternion* value, MCore::Endian::EEndianType targetEndianType);
    void ConvertFile16BitQuaternion(EMotionFX::FileFormat::File16BitQuaternion* value, MCore::Endian::EEndianType targetEndianType);
    void ConvertFileMotionEvent(EMotionFX::FileFormat::FileMotionEvent* value, MCore::Endian::EEndianType targetEndianType);
    void ConvertFileMotionEventTable(EMotionFX::FileFormat::FileMotionEventTrack* value, MCore::Endian::EEndianType targetEndianType);
    void ConvertRGBAColor(MCore::RGBAColor* value, MCore::Endian::EEndianType targetEndianType);
    void ConvertVector3(AZ::PackedVector3f* value, MCore::Endian::EEndianType targetEndianType);

    uint32 GetFileHighVersion();
    uint32 GetFileLowVersion();
    const char* GetCompilationDate();

    /**
     * Save the given string to a file. It will first save an integer determining the number of characters which follow and
     * then the actual string data.
     * @param textToSave The string to save.
     * @param file The file stream to save the string to.
     */
    void SaveString(const AZStd::string& textToSave, MCore::Stream* file, MCore::Endian::EEndianType targetEndianType);
    void SaveAzString(const AZStd::string& textToSave, MCore::Stream* file, MCore::Endian::EEndianType targetEndianType);

    /**
     * Return the size in bytes the string chunk will have.
     * @param text The string to check the chunk size for.
     * @return The size the string chunk will have in bytes.
     */
    uint32 GetStringChunkSize(const AZStd::string& text);
    size_t GetAzStringChunkSize(const AZStd::string& text);

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Actors
    //////////////////////////////////////////////////////////////////////////////////////////////////////////

    // nodes
    void SaveNodes(MCore::Stream* file, EMotionFX::Actor* actor, MCore::Endian::EEndianType targetEndianType);
    void SaveNodeGroup(MCore::Stream* file, EMotionFX::NodeGroup* nodeGroup, MCore::Endian::EEndianType targetEndianType);
    void SaveNodeGroups(MCore::Stream* file, const MCore::Array<EMotionFX::NodeGroup*>& nodeGroups, MCore::Endian::EEndianType targetEndianType);
    void SaveNodeGroups(MCore::Stream* file, EMotionFX::Actor* actor, MCore::Endian::EEndianType targetEndianType);
    void SaveNodeMotionSources(MCore::Stream* file, EMotionFX::Actor* actor, MCore::Array<EMotionFX::Actor::NodeMirrorInfo>* mirrorInfo, MCore::Endian::EEndianType targetEndianType);
    void SaveAttachmentNodes(MCore::Stream* file, EMotionFX::Actor* actor, MCore::Endian::EEndianType targetEndianType);
    void SaveAttachmentNodes(MCore::Stream* file, EMotionFX::Actor* actor, const AZStd::vector<uint16>& attachmentNodes, MCore::Endian::EEndianType targetEndianType);

    // materials
    void SaveMaterials(MCore::Stream* file, MCore::Array<EMotionFX::Material*>& materials, uint32 lodLevel, MCore::Endian::EEndianType targetEndianType);
    void SaveMaterials(MCore::Stream* file, EMotionFX::Actor* actor, uint32 lodLevel, MCore::Endian::EEndianType targetEndianType);
    void SaveMaterials(MCore::Stream* file, EMotionFX::Actor* actor, MCore::Endian::EEndianType targetEndianType);

    // meshes & skins
    void SaveMesh(MCore::Stream* file, EMotionFX::Mesh* mesh, uint32 nodeIndex, bool isCollisionMesh, uint32 lodLevel, MCore::Endian::EEndianType targetEndianType);
    void SaveSkin(MCore::Stream* file, EMotionFX::Mesh* mesh, uint32 nodeIndex, bool isCollisionMesh, uint32 lodLevel, MCore::Endian::EEndianType targetEndianType);
    void SaveSkins(MCore::Stream* file, EMotionFX::Actor* actor, MCore::Endian::EEndianType targetEndianType);
    void SaveMeshes(MCore::Stream* file, EMotionFX::Actor* actor, MCore::Endian::EEndianType targetEndianType);

    // morph targets
    void SaveMorphTarget(MCore::Stream* file, EMotionFX::Actor* actor, EMotionFX::MorphTarget* inputMorphTarget, uint32 lodLevel, MCore::Endian::EEndianType targetEndianType);
    void SaveMorphTargets(MCore::Stream* file, EMotionFX::Actor* actor, uint32 lodLevel, MCore::Endian::EEndianType targetEndianType);
    void SaveMorphTargets(MCore::Stream* file, EMotionFX::Actor* actor, MCore::Endian::EEndianType targetEndianType);
    bool AddMorphTarget(EMotionFX::Actor* actor, MCore::MemoryFile* file, const AZStd::string& morphTargetName, uint32 captureMode, uint32 phonemeSets, float rangeMin, float rangeMax, uint32 geomLODLevel);

    // actors
    const char* GetActorExtension(bool includingDot = true);
    void SaveActorHeader(MCore::Stream* file, MCore::Endian::EEndianType targetEndianType);
    void SaveActorFileInfo(MCore::Stream* file, uint32 numLODLevels, uint32 motionExtractionNodeIndex, uint32 retargetRootNodeIndex, const char* sourceApp, const char* orgFileName, const char* actorName, MCore::Distance::EUnitType unitType, MCore::Endian::EEndianType targetEndianType);

    void SaveActor(MCore::MemoryFile* file, EMotionFX::Actor* actor, MCore::Endian::EEndianType targetEndianType);
    bool SaveActor(AZStd::string& filename, EMotionFX::Actor* actor, MCore::Endian::EEndianType targetEndianType);

    // deformable attachments
    void SaveDeformableAttachments(const char* fileNameWithoutExtension, EMotionFX::Actor* actor, MCore::Endian::EEndianType targetEndianType, MCore::CommandManager* commandManager = nullptr);

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Shared By All Motion Types
    //////////////////////////////////////////////////////////////////////////////////////////////////////////

    void SaveMotionEvents(MCore::Stream* file, EMotionFX::MotionEventTable* motionEventTable, MCore::Endian::EEndianType targetEndianType);
    const char* GetSkeletalMotionExtension(bool includingDot = true);

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Skeletal Motions
    //////////////////////////////////////////////////////////////////////////////////////////////////////////

    void OptimizeSkeletalSubMotion(EMotionFX::SkeletalSubMotion* subMotion, bool optimizePos, bool optimizeRot, bool optimizeScale, float maxPosError, float maxRotError, float maxScaleError);
    void AddSortedKey(EMotionFX::SkeletalSubMotion* subMotion, float time, const AZ::Vector3& position, const MCore::Quaternion& rotation, const AZ::Vector3& scale);
    // call this before optimization !!!
    void ConformSkeletalMotion(EMotionFX::SkeletalMotion* motion, EMotionFX::Actor* actor, const MCore::Array<MCore::Matrix>& deltaMatrices);
    void FixTransformation(const MCore::Array<MCore::Matrix>& deltaMatrices, uint32 nodeIndex, uint32 parentNodeIndex, const AZ::Vector3& inPosition, const MCore::Quaternion& inRotation, const AZ::Vector3& inScale, AZ::Vector3* outPosition, MCore::Quaternion* outRotation, AZ::Vector3* outScale);

    void SaveSkeletalMotionHeader(MCore::Stream* file, EMotionFX::Motion* motion, MCore::Endian::EEndianType targetEndianType);
    void SaveSkeletalMotionFileInfo(MCore::Stream* file, EMotionFX::SkeletalMotion* motion, MCore::Endian::EEndianType targetEndianType);
    void SaveSkeletalMotion(MCore::Stream* file, EMotionFX::SkeletalMotion* motion, MCore::Endian::EEndianType targetEndianType, bool onlyAnimatedMorphs);
    void SaveSkeletalMotion(AZStd::string& filename, EMotionFX::SkeletalMotion* motion, MCore::Endian::EEndianType targetEndianType, bool onlyAnimatedMorphs);
    void SaveWaveletSkeletalMotion(MCore::Stream* file, EMotionFX::WaveletSkeletalMotion* motion, MCore::Endian::EEndianType targetEndianType);
    void SaveMorphSubMotions(MCore::Stream* file, EMotionFX::SkeletalMotion* motion, MCore::Endian::EEndianType targetEndianType, bool onlyAnimated);

    // TODO: not a nice function yet, no file processor, ignoring the file processor passes, change later on
    bool ConvertToWaveletSkeletalMotion(const AZStd::string& fileName, EMotionFX::WaveletSkeletalMotion::Settings* settings, MCore::Endian::EEndianType targetEndianType);

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Morphing
    //////////////////////////////////////////////////////////////////////////////////////////////////////////

    EMotionFX::MorphSubMotion* GetMorphSubMotionByName(EMotionFX::SkeletalMotion* motion, const AZStd::string& name);
    EMotionFX::MorphSubMotion* CreateAddMorphSubMotion(EMotionFX::SkeletalMotion* motion, const AZStd::string& subMotionName);

    const char* GetMorphSubMotionName(EMotionFX::MorphSubMotion* subMotion);
    void OptimizeMorphSubMotions(EMotionFX::SkeletalMotion* motion, float maxError = 0.001f);
    void AddSortedKey(EMotionFX::MorphSubMotion* subMotion, float time, float value);
} // namespace ExporterLib