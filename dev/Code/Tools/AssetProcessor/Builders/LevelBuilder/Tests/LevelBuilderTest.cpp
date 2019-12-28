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

#include <AzTest/AzTest.h>
#include <Source/LevelBuilderWorker.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/UserSettings/UserSettingsComponent.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzToolsFramework/Application/ToolsApplication.h>
#include <QCoreApplication>
#include "AZTestShared/Utils/Utils.h"
#include "AzCore/Slice/SliceAssetHandler.h"

using namespace LevelBuilder;
using namespace AZ;
using namespace AssetBuilderSDK;

class MockSimpleAsset
{
public:
    AZ_TYPE_INFO(MockSimpleAsset, "{A8A04FF5-1D58-450D-8FD4-2641F290B918}");

    static const char* GetFileFilter()
    {
        return "*.txt;";
    }
};

class SecondMockSimpleAsset
{
public:
    AZ_TYPE_INFO(SecondMockSimpleAsset, "{A443123A-FD95-45F6-9767-35B17DA2072F}");

    static const char* GetFileFilter()
    {
        return "*.txt;*.txt1;*.txt2";
    }
};

class ThirdMockSimpleAsset
{
public:
    AZ_TYPE_INFO(ThirdMockSimpleAsset, "{0298F78B-76EF-47CE-8812-B0BC80060016}");

    static const char* GetFileFilter()
    {
        return "txt";
    }
};

namespace AZ
{
    AZ_TYPE_INFO_SPECIALIZE(AzFramework::SimpleAssetReference<MockSimpleAsset>, "{2845879D-3F35-4DF1-910A-477C5A2C72EA}")
    AZ_TYPE_INFO_SPECIALIZE(AzFramework::SimpleAssetReference<SecondMockSimpleAsset>, "{0F69D7B1-D639-49E3-BB7F-CB52D9144F15}")
    AZ_TYPE_INFO_SPECIALIZE(AzFramework::SimpleAssetReference<ThirdMockSimpleAsset>, "{88920F68-8056-43C0-8B39-429660C32564}")
}

struct MockSimpleAssetRefComponent
    : public AZ::Component
{
    AZ_COMPONENT(MockSimpleAssetRefComponent, "{7A37EE69-707B-435F-8B8C-B347C454DC6B}");

    static void Reflect(ReflectContext* reflection)
    {
        SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(reflection);

        AzFramework::SimpleAssetReference<MockSimpleAsset>::Register(*serializeContext);
        AzFramework::SimpleAssetReference<SecondMockSimpleAsset>::Register(*serializeContext);
        AzFramework::SimpleAssetReference<ThirdMockSimpleAsset>::Register(*serializeContext);

        if (serializeContext)
        {
            serializeContext->Class<MockSimpleAssetRefComponent, AZ::Component>()
                ->Field("asset", &MockSimpleAssetRefComponent::m_asset)
                ->Field("secondAsset", &MockSimpleAssetRefComponent::m_secondAsset)
                ->Field("thirdAsset", &MockSimpleAssetRefComponent::m_thirdAsset);
        }
    }

    void Activate() override {}
    void Deactivate() override {}

    AzFramework::SimpleAssetReference<MockSimpleAsset> m_asset;
    AzFramework::SimpleAssetReference<SecondMockSimpleAsset> m_secondAsset;
    AzFramework::SimpleAssetReference<ThirdMockSimpleAsset> m_thirdAsset;
};

class LevelBuilderTest
    : public ::testing::Test
    , public UnitTest::TraceBusRedirector
{
protected:
    void SetUp() override
    {
        m_app.Start(m_descriptor);
        AZ::UserSettingsComponentRequestBus::Broadcast(&AZ::UserSettingsComponentRequests::DisableSaveOnFinalize);
        AZ::Debug::TraceMessageBus::Handler::BusConnect();

        const char* dir = m_app.GetExecutableFolder();

        AZ::IO::FileIOBase::GetInstance()->SetAlias("@root@", dir);

        auto* serializeContext = m_app.GetSerializeContext();

        m_simpleAssetRefDescriptor = MockSimpleAssetRefComponent::CreateDescriptor();
        m_simpleAssetRefDescriptor->Reflect(serializeContext);
    }

    void TearDown() override
    {
        delete m_simpleAssetRefDescriptor;

        m_app.Stop();
        AZ::Debug::TraceMessageBus::Handler::BusDisconnect();
    }

    void TestFailureCase(AZStd::string_view fileName)
    {
        IO::FileIOStream fileStream;
        LevelBuilderWorker worker;
        ProductPathDependencySet productDependencies;

        ASSERT_TRUE(OpenTestFile(fileName, fileStream));
        ASSERT_FALSE(worker.PopulateLevelDataDependenciesHelper(&fileStream, productDependencies));
        ASSERT_EQ(productDependencies.size(), 0);
    }

    AZStd::string GetTestFileAliasedPath(AZStd::string_view fileName)
    {
        constexpr char testFileFolder[] = "@root@/../Code/Tools/AssetProcessor/Builders/LevelBuilder/Tests/";
        return AZStd::string::format("%s%.*s", testFileFolder, fileName.size(), fileName.data());
    }


    AZStd::string GetTestFileFullPath(AZStd::string_view fileName)
    {
        AZStd::string aliasedPath = GetTestFileAliasedPath(fileName);
        char resolvedPath[AZ_MAX_PATH_LEN];
        AZ::IO::FileIOBase::GetInstance()->ResolvePath(aliasedPath.c_str(), resolvedPath, AZ_MAX_PATH_LEN);
        return AZStd::string(resolvedPath);
    }

    bool OpenTestFile(AZStd::string_view fileName, IO::FileIOStream& fileStream)
    {
        AZStd::string aliasedPath = GetTestFileAliasedPath(fileName);
        return fileStream.Open(aliasedPath.c_str(), IO::OpenMode::ModeRead | IO::OpenMode::ModeBinary);
    }

    AzToolsFramework::ToolsApplication m_app;
    AZ::ComponentApplication::Descriptor m_descriptor;
    AZ::ComponentDescriptor* m_simpleAssetRefDescriptor;
};

TEST_F(LevelBuilderTest, TestAudioControl_MultipleDependencies)
{
    AZ::IO::FileIOBase::GetInstance()->SetAlias("@devassets@", GetTestFileFullPath("audiocontrol_test1").c_str());

    LevelBuilderWorker worker;
    ProductPathDependencySet productDependencies;

    worker.PopulateLevelAudioControlDependenciesHelper("testlevel", productDependencies);
    AZ::IO::FileIOBase::GetInstance()->ClearAlias("@devassets@");

    ASSERT_THAT(productDependencies,
        testing::UnorderedElementsAre(
            ProductPathDependency("libs/gameaudio/wwise/global.xml", AssetBuilderSDK::ProductPathDependencyType::ProductFile),
            ProductPathDependency("libs/gameaudio/wwise/default_controls.xml", AssetBuilderSDK::ProductPathDependencyType::ProductFile),
            ProductPathDependency("libs/gameaudio/wwise/levels/testlevel/testcontrol.xml", AssetBuilderSDK::ProductPathDependencyType::ProductFile),
            ProductPathDependency("libs/gameaudio/wwise/levels/testlevel/very_specific_testcontrol.xml", AssetBuilderSDK::ProductPathDependencyType::ProductFile)
        )
    );
}

TEST_F(LevelBuilderTest, TestAudioControl_OnlyGlobalControls_MultipleDependencies)
{
    AZ::IO::FileIOBase::GetInstance()->SetAlias("@devassets@", GetTestFileFullPath("audiocontrol_test2").c_str());

    LevelBuilderWorker worker;
    ProductPathDependencySet productDependencies;

    worker.PopulateLevelAudioControlDependenciesHelper("testlevel", productDependencies);
    AZ::IO::FileIOBase::GetInstance()->ClearAlias("@devassets@");

    ASSERT_THAT(productDependencies,
        testing::UnorderedElementsAre(
            ProductPathDependency("libs/gameaudio/wwise/global.xml", AssetBuilderSDK::ProductPathDependencyType::ProductFile),
            ProductPathDependency("libs/gameaudio/wwise/default_controls.xml", AssetBuilderSDK::ProductPathDependencyType::ProductFile)
        )
    );
}

TEST_F(LevelBuilderTest, TestAudioControl_OnlyLevelControls_SingleDependency)
{
    AZ::IO::FileIOBase::GetInstance()->SetAlias("@devassets@", GetTestFileFullPath("audiocontrol_test3").c_str());

    LevelBuilderWorker worker;
    ProductPathDependencySet productDependencies;

    worker.PopulateLevelAudioControlDependenciesHelper("testlevel", productDependencies);
    AZ::IO::FileIOBase::GetInstance()->ClearAlias("@devassets@");

    ASSERT_THAT(productDependencies,
        testing::UnorderedElementsAre(
            ProductPathDependency("libs/gameaudio/wwise/levels/testlevel/testcontrol.xml", AssetBuilderSDK::ProductPathDependencyType::ProductFile)
        )
    );
}

TEST_F(LevelBuilderTest, TestAudioControl_NoAudioControls_NoDependencies)
{
    AZ::IO::FileIOBase::GetInstance()->SetAlias("@devassets@", GetTestFileFullPath("audiocontrol_test4").c_str());

    LevelBuilderWorker worker;
    ProductPathDependencySet productDependencies;

    worker.PopulateLevelAudioControlDependenciesHelper("testlevel", productDependencies);
    AZ::IO::FileIOBase::GetInstance()->ClearAlias("@devassets@");

    ASSERT_EQ(productDependencies.size(), 0);
}

TEST_F(LevelBuilderTest, TestLevelData_RequestOptionalLevelDependencies_ExpectedDependenciesAdded)
{
    LevelBuilderWorker worker;
    ProductPathDependencySet productDependencies;

    const AZStd::string levelPath("some/levelName/");
    AZStd::string levelPak;
    AzFramework::StringFunc::Path::Join(levelPath.c_str(), "level.pak", levelPak);

    worker.PopulateOptionalLevelDependencies(levelPak, productDependencies);

    AZStd::string coverPath;
    AzFramework::StringFunc::Path::Join(levelPath.c_str(), "terrain/cover.ctc", coverPath);

    AZStd::string preloadlibsPath;
    AzFramework::StringFunc::Path::Join(levelPath.c_str(), "preloadlibs.txt", preloadlibsPath);

    AZStd::string levelParticlesPath;
    AzFramework::StringFunc::Path::Join(levelPath.c_str(), "levelparticles.xml", levelParticlesPath);

    AZStd::string occluderPath;
    AzFramework::StringFunc::Path::Join(levelPath.c_str(), "occluder.ocm", occluderPath);

    AZStd::string levelCfgPath;
    AzFramework::StringFunc::Path::Join(levelPath.c_str(), "level.cfg", levelCfgPath);

    AZStd::string autoResourceList;
    AzFramework::StringFunc::Path::Join(levelPath.c_str(), "auto_resourcelist.txt", autoResourceList);

    AZStd::string mergedMeshesList;
    AzFramework::StringFunc::Path::Join(levelPath.c_str(), "terrain/merged_meshes_sectors/mmrm_used_meshes.lst", mergedMeshesList);

    AZStd::string levelXmlPath;
    AzFramework::StringFunc::Path::Join(levelPath.c_str(), "levelName.xml", levelXmlPath);

    ASSERT_THAT(productDependencies,
        testing::UnorderedElementsAre(
            ProductPathDependency(coverPath.c_str(), AssetBuilderSDK::ProductPathDependencyType::SourceFile),
            ProductPathDependency(preloadlibsPath.c_str(), AssetBuilderSDK::ProductPathDependencyType::SourceFile),
            ProductPathDependency(levelParticlesPath.c_str(), AssetBuilderSDK::ProductPathDependencyType::SourceFile),
            ProductPathDependency(occluderPath.c_str(), AssetBuilderSDK::ProductPathDependencyType::SourceFile),
            ProductPathDependency(levelCfgPath.c_str(), AssetBuilderSDK::ProductPathDependencyType::SourceFile),
            ProductPathDependency(autoResourceList.c_str(), AssetBuilderSDK::ProductPathDependencyType::SourceFile),
            ProductPathDependency(mergedMeshesList.c_str(), AssetBuilderSDK::ProductPathDependencyType::SourceFile),
            ProductPathDependency(levelXmlPath.c_str(), AssetBuilderSDK::ProductPathDependencyType::SourceFile))
    );
}

TEST_F(LevelBuilderTest, TestLevelData_MultipleDependencies)
{
    // Tests processing a leveldata.xml file containing multiple dependencies
    // Should output 5 dependencies

    IO::FileIOStream fileStream;
    
    ASSERT_TRUE(OpenTestFile("leveldata_test1.xml", fileStream));

    LevelBuilderWorker worker;
    ProductPathDependencySet productDependencies;

    ASSERT_TRUE(worker.PopulateLevelDataDependenciesHelper(&fileStream, productDependencies));
    ASSERT_THAT(productDependencies, 
        testing::UnorderedElementsAre(
            ProductPathDependency("materials/natural/terrain/am_mud1.mtl", AssetBuilderSDK::ProductPathDependencyType::ProductFile),
            ProductPathDependency("materials/natural/terrain/am_grass1.mtl", AssetBuilderSDK::ProductPathDependencyType::ProductFile),
            ProductPathDependency("materials/natural/terrain/am_rockcliff1.mtl", AssetBuilderSDK::ProductPathDependencyType::ProductFile),
            ProductPathDependency("materials/natural/terrain/am_path_hexagon.mtl", AssetBuilderSDK::ProductPathDependencyType::ProductFile),
            ProductPathDependency("materials/natural/terrain/am_rockground.mtl", AssetBuilderSDK::ProductPathDependencyType::ProductFile))
        );
}

TEST_F(LevelBuilderTest, TestLevelData_NoDependencies)
{
    // Tests processing a leveldata.xml file that has no dependencies
    // Should output 0 dependencies

    IO::FileIOStream fileStream;

    ASSERT_TRUE(OpenTestFile("leveldata_test2.xml", fileStream));

    LevelBuilderWorker worker;
    ProductPathDependencySet productDependencies;

    ASSERT_TRUE(worker.PopulateLevelDataDependenciesHelper(&fileStream, productDependencies));
    ASSERT_EQ(productDependencies.size(), 0);
}

TEST_F(LevelBuilderTest, TestLevelData_InvalidStream)
{
    // Tests passing an invalid stream in
    // Should output 0 dependencies and return false

    LevelBuilderWorker worker;
    ProductPathDependencySet productDependencies;

    ASSERT_FALSE(worker.PopulateLevelDataDependenciesHelper(nullptr, productDependencies));
    ASSERT_EQ(productDependencies.size(), 0);
}

TEST_F(LevelBuilderTest, TestLevelData_EmptyFile)
{
    // Tests processing a leveldata.xml file that is empty
    // Should output 0 dependencies and return false

    TestFailureCase("leveldata_test3.xml");
}

TEST_F(LevelBuilderTest, TestLevelData_NoSurfaceTypes)
{
    // Tests processing a leveldata.xml file that contains no surface types
    // Should output 0 dependencies and return false

    TestFailureCase("leveldata_test4.xml");
}

TEST_F(LevelBuilderTest, TestLevelData_NoLevelData)
{
    // Tests processing a leveldata.xml file that contains no level data
    // Should output 0 dependencies and return false

    TestFailureCase("leveldata_test5.xml");
}

TEST_F(LevelBuilderTest, TestLevelData_NonXMLData)
{
    // Tests processing a leveldata.xml file that is not an xml file
    // Should output 0 dependencies and return false

    TestFailureCase("leveldata_test6.xml");
}

TEST_F(LevelBuilderTest, TestLevelData_MalformedXMLData)
{
    // Tests processing a leveldata.xml file that contains malformed XML
    // Should output 0 dependencies and return false

    TestFailureCase("leveldata_test7.xml");
}

//////////////////////////////////////////////////////////////////////////

TEST_F(LevelBuilderTest, TestMission_MultipleDependencies)
{
    // Tests processing a mission_*.xml file containing multiple dependencies and no Cloud texture
    // Should output 3 dependencies

    IO::FileIOStream fileStream;

    ASSERT_TRUE(OpenTestFile("mission_mission0_test1.xml", fileStream));

    LevelBuilderWorker worker;
    ProductPathDependencySet productDependencies;

    ASSERT_TRUE(worker.PopulateMissionDependenciesHelper(&fileStream, productDependencies));
    ASSERT_THAT(productDependencies, testing::UnorderedElementsAre(
        ProductPathDependency{ "EngineAssets/Materials/Sky/Sky.mtl", AssetBuilderSDK::ProductPathDependencyType::ProductFile},
        ProductPathDependency{ "EngineAssets/Materials/Water/Ocean_default.mtl", AssetBuilderSDK::ProductPathDependencyType::ProductFile},
        ProductPathDependency{ "Textures/Skys/Night/half_moon.dds", AssetBuilderSDK::ProductPathDependencyType::ProductFile}));
}

TEST_F(LevelBuilderTest, TestMission_NoSkyBox)
{
    // Tests processing a mission_*.xml file with no skybox settings
    // Should output 0 dependencies and return false

    TestFailureCase("mission_mission0_test2.xml");
}

TEST_F(LevelBuilderTest, TestMission_NoOcean)
{
    // Tests processing a mission_*.xml file with no ocean settings
    // Should output 0 dependencies and return false

    TestFailureCase("mission_mission0_test3.xml");
}

TEST_F(LevelBuilderTest, TestMission_NoMoon)
{
    // Tests processing a mission_*.xml file with no moon settings
    // Should output 2 dependencies

    IO::FileIOStream fileStream;

    ASSERT_TRUE(OpenTestFile("mission_mission0_test4.xml", fileStream));

    LevelBuilderWorker worker;
    ProductPathDependencySet productDependencies;

    ASSERT_TRUE(worker.PopulateMissionDependenciesHelper(&fileStream, productDependencies));
    ASSERT_THAT(productDependencies, testing::UnorderedElementsAre(
        ProductPathDependency{ "EngineAssets/Materials/Sky/Sky.mtl", AssetBuilderSDK::ProductPathDependencyType::ProductFile },
        ProductPathDependency{ "EngineAssets/Materials/Water/Ocean_default.mtl", AssetBuilderSDK::ProductPathDependencyType::ProductFile }));
}

TEST_F(LevelBuilderTest, TestMission_NoEnvironment)
{
    // Tests processing a mission_*.xml file with no environment settings
    // Should output 0 dependencies and return false

    TestFailureCase("mission_mission0_test5.xml");
}

TEST_F(LevelBuilderTest, TestMission_EmptyFile)
{
    // Tests processing an empty mission_*.xml
    // Should output 0 dependencies and return false

    TestFailureCase("mission_mission0_test6.xml");
}

TEST_F(LevelBuilderTest, TestMission_CloudShadow)
{
    // Tests processing a mission_*.xml file with cloud shadow texture set
    // Should output 4 dependencies and return true

    using namespace AssetBuilderSDK;

    IO::FileIOStream fileStream;

    ASSERT_TRUE(OpenTestFile("mission_mission0_test7.xml", fileStream));

    LevelBuilderWorker worker;
    ProductPathDependencySet productDependencies;

    ASSERT_TRUE(worker.PopulateMissionDependenciesHelper(&fileStream, productDependencies));
    ASSERT_THAT(productDependencies, testing::UnorderedElementsAre(
        ProductPathDependency{ "EngineAssets/Materials/Sky/Sky.mtl", AssetBuilderSDK::ProductPathDependencyType::ProductFile },
        ProductPathDependency{ "EngineAssets/Materials/Water/Ocean_default.mtl", AssetBuilderSDK::ProductPathDependencyType::ProductFile },
        ProductPathDependency{ "Textures/Skys/Night/half_moon.dds", AssetBuilderSDK::ProductPathDependencyType::ProductFile },
        ProductPathDependency{ "textures/terrain/ftue_megatexture_02.dds", AssetBuilderSDK::ProductPathDependencyType::ProductFile }));
}

TEST_F(LevelBuilderTest, DynamicSlice_NoAssetReferences_HasNoProductDependencies)
{
    LevelBuilderWorker worker;
    AZStd::vector<ProductDependency> productDependencies;
    ProductPathDependencySet productPathDependencies;

    AZ::IO::FileIOStream fileStream;
    ASSERT_TRUE(OpenTestFile("levelSlice_noAssetReferences.entities_xml", fileStream));

    worker.PopulateLevelSliceDependenciesHelper(&fileStream, productDependencies, productPathDependencies);
    ASSERT_EQ(productDependencies.size(), 0);
    ASSERT_EQ(productPathDependencies.size(), 0);
}

TEST_F(LevelBuilderTest, DynamicSlice_HasAssetReference_HasCorrectProductDependency)
{
    LevelBuilderWorker worker;
    AZStd::vector<ProductDependency> productDependencies;
    ProductPathDependencySet productPathDependencies;

    AZ::IO::FileIOStream fileStream;
    ASSERT_TRUE(OpenTestFile("levelSlice_oneAssetRef.entities_xml", fileStream));

    worker.PopulateLevelSliceDependenciesHelper(&fileStream, productDependencies, productPathDependencies);
    ASSERT_EQ(productPathDependencies.size(), 0);
    ASSERT_EQ(productDependencies.size(), 1);
    ASSERT_EQ(productDependencies[0].m_dependencyId.m_guid, AZ::Uuid("A8970A25-5043-5519-A927-F180E7D6E8C1"));
    ASSERT_EQ(productDependencies[0].m_dependencyId.m_subId, 1);
}

void BuildSliceWithSimpleAssetReference(const AZStd::vector<AZStd::string>& filePaths, AZStd::vector<ProductDependency>& productDependencies, ProductPathDependencySet& productPathDependencies)
{
    auto* assetComponent = aznew MockSimpleAssetRefComponent;

    assetComponent->m_asset.SetAssetPath(filePaths[0].c_str());
    assetComponent->m_secondAsset.SetAssetPath(filePaths[1].c_str());
    assetComponent->m_thirdAsset.SetAssetPath(filePaths[2].c_str());

    auto sliceAsset = AZ::Test::CreateSliceFromComponent(assetComponent, AZ::Data::AssetId(AZ::Uuid::CreateRandom(), 0));

    AZ::SliceAssetHandler assetHandler;
    assetHandler.SetSerializeContext(nullptr);

    AZStd::vector<char> charBuffer;
    AZ::IO::ByteContainerStream<AZStd::vector<char>> charStream(&charBuffer);
    assetHandler.SaveAssetData(sliceAsset, &charStream);

    charStream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);

    LevelBuilderWorker worker;

    worker.PopulateLevelSliceDependenciesHelper(&charStream, productDependencies, productPathDependencies);
}

TEST_F(LevelBuilderTest, DynamicSlice_HasPopulatedSimpleAssetReference_HasCorrectProductDependency)
{
    AZStd::vector<ProductDependency> productDependencies;
    ProductPathDependencySet productPathDependencies;
    AZStd::vector<AZStd::string> filePaths = { "some/test/path.txt", "", "" };
    BuildSliceWithSimpleAssetReference(filePaths, productDependencies, productPathDependencies);
    ASSERT_EQ(productDependencies.size(), 0);
    ASSERT_EQ(productPathDependencies.size(), 1);
    ASSERT_EQ(productPathDependencies.begin()->m_dependencyPath, filePaths[0]);
}

TEST_F(LevelBuilderTest, DynamicSlice_HasPopulatedSimpleAssetReferencesNoExtension_HasCorrectProductDependency)
{
    AZStd::vector<ProductDependency> productDependencies;
    ProductPathDependencySet productPathDependencies;
    AZStd::vector<AZStd::string> filePaths = { "some/test/path0", "some/test/path1", "some/test/path2" };
    BuildSliceWithSimpleAssetReference(filePaths, productDependencies, productPathDependencies);
    ASSERT_EQ(productDependencies.size(), 0);
    ASSERT_EQ(productPathDependencies.size(), 3);

    ASSERT_THAT(productPathDependencies, testing::UnorderedElementsAre(
        ProductPathDependency{ "some/test/path0.txt", AssetBuilderSDK::ProductPathDependencyType::ProductFile },
        ProductPathDependency{ "some/test/path1.txt", AssetBuilderSDK::ProductPathDependencyType::ProductFile },
        ProductPathDependency{ "some/test/path2.txt", AssetBuilderSDK::ProductPathDependencyType::ProductFile }));
}

TEST_F(LevelBuilderTest, DynamicSlice_HasEmptySimpleAssetReference_HasNoProductDependency)
{
    AZStd::vector<ProductDependency> productDependencies;
    ProductPathDependencySet productPathDependencies;
    AZStd::vector<AZStd::string> filePaths = { "", "", "" };
    BuildSliceWithSimpleAssetReference(filePaths, productDependencies, productPathDependencies);
    ASSERT_EQ(productDependencies.size(), 0);
    ASSERT_EQ(productPathDependencies.size(), 0);
}

TEST_F(LevelBuilderTest, VegetationMap_HasFileReference_HasCorrectProductDependency)
{
    LevelBuilderWorker worker;
    ProductPathDependencySet productPathDependencies;

    AZ::IO::FileIOStream fileStream;
    ASSERT_TRUE(OpenTestFile("VegetationMap_oneFileReferences.dat", fileStream));

    worker.PopulateVegetationMapDataDependenciesHelper(&fileStream, productPathDependencies);
    ASSERT_EQ(productPathDependencies.size(), 1);
    ASSERT_THAT(productPathDependencies, testing::UnorderedElementsAre(
        ProductPathDependency{ "dummytestbasefolder/dummytestfolder/dummygeometryfile.cgf", AssetBuilderSDK::ProductPathDependencyType::ProductFile }));
}

TEST_F(LevelBuilderTest, VegetationMap_HasNoFileReference_HasCorrectProductDependency)
{
    LevelBuilderWorker worker;
    ProductPathDependencySet productPathDependencies;

    AZ::IO::FileIOStream fileStream;
    ASSERT_TRUE(OpenTestFile("VegetationMap_noFileReferences.dat", fileStream));

    worker.PopulateVegetationMapDataDependenciesHelper(&fileStream, productPathDependencies);
    ASSERT_EQ(productPathDependencies.size(), 0);
}

AZ_UNIT_TEST_HOOK();
