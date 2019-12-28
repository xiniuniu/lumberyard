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
#include "stdafx.h"
#include <AzTest/AzTest.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AssetBuilderSDK/AssetBuilderSDK.h>
#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/UserSettings/UserSettingsComponent.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzToolsFramework/Application/ToolsApplication.h>
#include "../CryEngine/Cry3DEngine/CGF/CGFLoader.h"

#include "StatCGFCompiler.h"
#include "MultiplatformConfig.h"
#include <QTemporaryDir>

AZ_UNIT_TEST_HOOK();

TEST(ResourceCompilerPCSanityTest, Sanity)
{
    EXPECT_EQ(1, 1);
}

AZStd::string GetTestAssetRootPath()
{
    char resolvedDir[AZ_MAX_PATH_LEN];
    AZ::IO::FileIOBase::GetInstance()->ResolvePath(
        "@root@/../Code/Tools/RC/ResourceCompilerPC/Tests/TestAssets/SamplesProject/",
        resolvedDir, sizeof(resolvedDir));
    return AZStd::string(resolvedDir);
}


// This is a dummy ResourceCompiler object overload.
// The CGF compiler is heavily dependent upon a ResourceCompiler object existing, so this overload
// just does the minimum amount of work to allow the CGF compiler to query it for default values and
// specific values needed for our test cases.
class ResourceCompilerForTesting
    : public IResourceCompiler
    , public IConfigKeyRegistry
{
public:
    ResourceCompilerForTesting()
    {
        platformInfo.SetName(0, "pc");
        platformInfo.bBigEndian = false;
    }
    virtual ~ResourceCompilerForTesting() {}

    //IConfigKeyRegistry
    virtual void VerifyKeyRegistration(const char* szKey) const {}
    virtual bool HasKeyRegistered(const char* szKey) const { return false; }

    //IResourceCompiler
    virtual void RegisterConvertor(const char* name, IConvertor* conv) {}
    virtual void AddUnitTest(FnRunUnitTests unitTestFunction) {}
    virtual IPakSystem* GetPakSystem() { return nullptr; }
    virtual const ICfgFile* GetIniFile() const { return nullptr; }
    virtual int GetPlatformCount() const { return 1; }
    virtual const PlatformInfo* GetPlatformInfo(int index) const { return &platformInfo; }
    virtual int FindPlatform(const char* name) const { return 0; }
    virtual void AddInputOutputFilePair(const char* inputFilename, const char* outputFilename) {}
    virtual void MarkOutputFileForRemoval(const char* sOutputFilename) {}
    virtual void AddExitObserver(IExitObserver* p) {}
    virtual void RemoveExitObserver(IExitObserver* p) {}
    virtual IRCLog* GetIRCLog() { return nullptr; }
    virtual int GetVerbosityLevel() const { return 0; }
    virtual const SFileVersion& GetFileVersion() const { return fileVersionInfo; }
    virtual const void GetGenericInfo(char* buffer, size_t bufferSize, const char* rowSeparator) const {}
    virtual void RegisterKey(const char* key, const char* helptxt) {}
    virtual const char* GetExePath() const { return nullptr; }
    virtual const char* GetTmpPath() const { return nullptr; }
    virtual const char* GetInitialCurrentDir() const { return nullptr; }
    virtual XmlNodeRef LoadXml(const char* filename) { XmlNodeRef tmp; return tmp; }
    virtual XmlNodeRef CreateXml(const char* tag) { XmlNodeRef tmp; return tmp; }
    virtual bool CompileSingleFileBySingleProcess(const char* filename) { return true; }
    virtual void SetAssetWriter(IAssetWriter* pAssetWriter) {}
    virtual IAssetWriter* GetAssetWriter() const { return nullptr; }
    virtual const char* GetAppRoot() const { return nullptr; }

    SFileVersion fileVersionInfo;
    PlatformInfo platformInfo;
    MultiplatformConfig localMultiConfig;
};

class CGFBuilderTest
    : public ::testing::Test
{
protected:
    AZStd::string GetTemporaryDirectory()
    {
        QString tempPath = m_temporaryDirectory.path();
        return AZStd::string(tempPath.toUtf8().constData());
    }

    void SetUp() override
    {
        AZ::AllocatorInstance<AZ::SystemAllocator>::Create();
        AZ::AllocatorInstance<CryStringAllocator>::Create();

        m_app.reset(aznew AzToolsFramework::ToolsApplication());
        AZ::ComponentApplication::Descriptor desc;
        desc.m_useExistingAllocator = true;
        m_app->Start(desc);

        // Disable saving global user settings when the builder finishes
        AZ::UserSettingsComponentRequestBus::Broadcast(&AZ::UserSettingsComponentRequests::DisableSaveOnFinalize);

        AssetBuilderSDK::InitializeSerializationContext();

        const char* dir = m_app->GetExecutableFolder();

        AZ::IO::FileIOBase::GetInstance()->SetAlias("@root@", dir);

        m_rc = new ResourceCompilerForTesting();
        m_compiler = new CStatCGFCompiler();

        m_rc->localMultiConfig.init(1, 0, m_rc);
        m_rc->localMultiConfig.setActivePlatform(0);
    }

    void TearDown() override
    {
        delete m_compiler;
        delete m_rc;
        m_compiler = nullptr;
        m_rc = nullptr;
        m_app->Stop();
        m_app = nullptr;

        AZ::AllocatorInstance<CryStringAllocator>::Destroy();
        AZ::AllocatorInstance<AZ::SystemAllocator>::Destroy();
    }

    void LoadCompileAndValidateCGF(const AZStd::string& cgfName, const AZStd::string& cgfPath, const AZStd::string& outputPath, AssetBuilderSDK::ProcessJobResponse& response);

    AZStd::unique_ptr<AzToolsFramework::ToolsApplication> m_app;
    QTemporaryDir m_temporaryDirectory;

    CStatCGFCompiler* m_compiler = nullptr;
    ResourceCompilerForTesting* m_rc = nullptr;
};

void CGFBuilderTest::LoadCompileAndValidateCGF(const AZStd::string& cgfName, const AZStd::string& cgfPath, const AZStd::string& outputPath, AssetBuilderSDK::ProcessJobResponse& response)
{
    // Pass in a custom asset root to the CGF compiler because our test assets are not going through AP
    AZStd::string assetRoot(GetTestAssetRootPath());
    
    // Our test assets came from SamplesProject, so emulate that being our game project
    AZStd::string gameFolder = "SamplesProject";

    IConvertContext* const pCC = m_compiler->GetConvertContext();
    pCC->SetMultiplatformConfig(&m_rc->localMultiConfig);
    pCC->SetRC(m_rc);

    pCC->SetForceRecompiling(true);
    pCC->SetConvertorExtension(".cgf");
    pCC->SetSourceFileNameOnly(cgfName.c_str());
    pCC->SetSourceFolder(cgfPath.c_str());
    pCC->SetOutputFolder(outputPath.c_str());

    bool compileSuccess = m_compiler->CompileCGF(response, assetRoot, gameFolder);

    EXPECT_TRUE(compileSuccess);

    bool responseSuccess = m_compiler->WriteResponse(outputPath.c_str(), response, compileSuccess);

    EXPECT_TRUE(responseSuccess);
}

// In this case, we load a basic CGF where the material path is just the name of the material file, 
// so it is assumed that material is in the same folder as the CGF
TEST_F(CGFBuilderTest, CGF_MaterialInSameFolder)
{
    AZStd::string cgfName = "cube_material_same_folder.cgf";

    AZStd::string cgfPath(PathHelpers::Join(GetTestAssetRootPath().c_str(),"Objects/Primitives/"));

    AzFramework::ApplicationRequests::Bus::Broadcast(&AzFramework::ApplicationRequests::Bus::Events::NormalizePath, cgfPath);
    AZStd::string outputPath = GetTemporaryDirectory();

    AZStd::string expectedMaterialPath;
    AzFramework::StringFunc::AssetDatabasePath::Join(cgfPath.c_str(), "primitives_001_mg.mtl", expectedMaterialPath);

    AssetBuilderSDK::ProcessJobResponse response;
    LoadCompileAndValidateCGF(cgfName, cgfPath, outputPath, response);

    ASSERT_EQ(response.m_outputProducts.size(), 1);
    AZStd::string productPath = response.m_outputProducts[0].m_productFileName;
    EXPECT_TRUE(productPath == "cube_material_same_folder.cgf"); // Path would be relative to the AP temp directory, so just the file name itself

    ASSERT_EQ(response.m_outputProducts[0].m_pathDependencies.size(), 1);
    AssetBuilderSDK::ProductPathDependency& dependency = *response.m_outputProducts[0].m_pathDependencies.begin();
    AZStd::string materialPath = dependency.m_dependencyPath;
    EXPECT_TRUE(materialPath == expectedMaterialPath);
    EXPECT_EQ(dependency.m_dependencyType, AssetBuilderSDK::ProductPathDependencyType::SourceFile);
}

// In this case, we load a basic CGF where the material path is absolute from the dev/ folder,
// for example "samplesproject/materials/test.mtl".
TEST_F(CGFBuilderTest, CGF_MaterialInDifferentFolder)
{
    AZStd::string cgfName = "gs_block.cgf";
    AZStd::string cgfPath(PathHelpers::Join(GetTestAssetRootPath().c_str(), "Objects/GettingStartedAssets/"));
    AZStd::string outputPath = GetTemporaryDirectory();

    AssetBuilderSDK::ProcessJobResponse response;
    LoadCompileAndValidateCGF(cgfName, cgfPath, outputPath, response);

    ASSERT_EQ(response.m_outputProducts.size(), 1);
    AZStd::string productPath = response.m_outputProducts[0].m_productFileName;
    EXPECT_TRUE(productPath == "gs_block.cgf"); // Path would be relative to the AP temp directory, so just the file name itself

    ASSERT_EQ(response.m_outputProducts[0].m_pathDependencies.size(), 1);
    AssetBuilderSDK::ProductPathDependency& dependency = *response.m_outputProducts[0].m_pathDependencies.begin();
    AZStd::string materialPath = dependency.m_dependencyPath;
    EXPECT_TRUE(materialPath == "materials/gettingstartedmaterials/gs_block.mtl");
    EXPECT_EQ(dependency.m_dependencyType, AssetBuilderSDK::ProductPathDependencyType::ProductFile);
}

// A simple CGF with 2 lods, outputting to just 1 compiled CGF file
TEST_F(CGFBuilderTest, CGF_WithLOD_NoSplit)
{
    AZStd::string cgfName = "CGF_LOD_Test.cgf";

    AZStd::string cgfPath(PathHelpers::Join(GetTestAssetRootPath().c_str(), "Objects/"));

    AzFramework::ApplicationRequests::Bus::Broadcast(&AzFramework::ApplicationRequests::Bus::Events::NormalizePath, cgfPath);
    AZStd::string outputPath = GetTemporaryDirectory();

    AZStd::string expectedMaterialPath;
    AzFramework::StringFunc::AssetDatabasePath::Join(cgfPath.c_str(), "Grass_Atlas_matGroup.mtl", expectedMaterialPath);

    m_rc->localMultiConfig.setKeyValue(eCP_PriorityLowest, "SplitLODs", "false");

    AssetBuilderSDK::ProcessJobResponse response;
    LoadCompileAndValidateCGF(cgfName, cgfPath, outputPath, response);

    ASSERT_EQ(response.m_outputProducts.size(), 1);
    AZStd::string productPath = response.m_outputProducts[0].m_productFileName;
    EXPECT_TRUE(productPath == "CGF_LOD_Test.cgf"); // Path would be relative to the AP temp directory, so just the file name itself

    ASSERT_EQ(response.m_outputProducts[0].m_pathDependencies.size(), 1);
    AssetBuilderSDK::ProductPathDependency& dependency = *response.m_outputProducts[0].m_pathDependencies.begin();
    AZStd::string materialPath = dependency.m_dependencyPath;
    EXPECT_TRUE(materialPath == expectedMaterialPath);
}

// A simple CGF with 2 lods, outputting to a unique CGF per lod
TEST_F(CGFBuilderTest, CGF_WithLOD_Split)
{
    AZStd::string cgfName = "CGF_LOD_Test.cgf";

    AZStd::string cgfPath(PathHelpers::Join(GetTestAssetRootPath().c_str(), "Objects/"));
    AzFramework::ApplicationRequests::Bus::Broadcast(&AzFramework::ApplicationRequests::Bus::Events::NormalizePath, cgfPath);
    AZStd::string outputPath = GetTemporaryDirectory();

    m_rc->localMultiConfig.setKeyValue(eCP_PriorityLowest, "SplitLODs", "true");

    AssetBuilderSDK::ProcessJobResponse response;
    LoadCompileAndValidateCGF(cgfName, cgfPath, outputPath, response);

    ASSERT_EQ(response.m_outputProducts.size(), 3);
    AZStd::string productPath = response.m_outputProducts[0].m_productFileName;
    EXPECT_TRUE(productPath == "CGF_LOD_Test.cgf");

    productPath = response.m_outputProducts[1].m_productFileName;
    EXPECT_TRUE(productPath == "CGF_LOD_Test_lod1.cgf");

    productPath = response.m_outputProducts[2].m_productFileName;
    EXPECT_TRUE(productPath == "CGF_LOD_Test_lod2.cgf");

    EXPECT_TRUE(response.m_outputProducts[0].m_pathDependencies.size() == 3);
    AZStd::vector<AZStd::string> expectedDependencyPaths = {
        "Grass_Atlas_matGroup.mtl",
        "CGF_LOD_Test_lod1.cgf",
        "CGF_LOD_Test_lod2.cgf"
    };
    
    for (AZStd::string& expectedDependency : expectedDependencyPaths)
    {
        const char* fileName = expectedDependency.c_str();
        AzFramework::StringFunc::AssetDatabasePath::Join(cgfPath.c_str(), fileName, expectedDependency);
    }

    for (const AssetBuilderSDK::ProductPathDependency& dependency : response.m_outputProducts[0].m_pathDependencies)
    {
        ASSERT_TRUE(AZStd::find(expectedDependencyPaths.begin(), expectedDependencyPaths.end(), dependency.m_dependencyPath) != expectedDependencyPaths.end());
    }
}
