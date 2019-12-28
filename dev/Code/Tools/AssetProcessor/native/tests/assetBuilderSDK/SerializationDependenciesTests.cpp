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
#include <AzCore/Asset/AssetManagerComponent.h>
#include <AzCore/Component/Entity.h>
#include <AssetBuilderSDK/SerializationDependencies.h>
#include <Tests/SerializeContextFixture.h>

namespace SerializationDependencyTests
{
    class ClassWithAssetId
    {
    public:
        AZ_RTTI(ClassWithAssetId, "{F6970E05-890B-4E5D-A944-1F58E9751922}");

        virtual ~ClassWithAssetId() {}
        
        static void Reflect(AZ::ReflectContext* reflection)
        {
            AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
            if (serializeContext)
            {
                serializeContext->Class<ClassWithAssetId>()
                    ->Field("m_assetId", &ClassWithAssetId::m_assetId);
            }
        }

        AZ::Data::AssetId m_assetId;
    };

    class ClassWithAsset
    {
    public:
        AZ_RTTI(ClassWithAsset, "{D2BCF9BF-3E64-4942-8AFB-BD3E8453CB52}");

        virtual ~ClassWithAsset() {}
        
        static void Reflect(AZ::ReflectContext* reflection)
        {
            AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
            if (serializeContext)
            {
                serializeContext->Class<ClassWithAsset>()
                    ->Field("m_asset", &ClassWithAsset::m_asset);
            }
        }

        AZ::Data::Asset<AZ::Data::AssetData> m_asset;
    };

    class SimpleAssetMock : public AzFramework::SimpleAssetReferenceBase
    {
    public:
        AZ_RTTI(SimpleAssetMock, "{AA2CDA39-A357-441D-BABA-B1AD3C3A8083}", AzFramework::SimpleAssetReferenceBase);
                
        static void Reflect(AZ::ReflectContext* reflection)
        {
            AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
            if (serializeContext)
            {
                serializeContext->Class<SimpleAssetMock, AzFramework::SimpleAssetReferenceBase>();
            }
        }

        AZ::Data::AssetType GetAssetType() const override
        {
            // Use an arbitrary ID for the asset type.
            return AZ::Data::AssetType("{03FD33E2-DA2F-4021-A266-0DC9714FF84D}");
        }
        virtual const char* GetFileFilter() const
        {
            return nullptr;
        }
    };

    class ClassWithSimpleAsset
    {
    public:
        AZ_RTTI(ClassWithSimpleAsset, "{F4F50653-692C-46F8-A9B0-73C19523E56A}");

        virtual ~ClassWithSimpleAsset() {}
        
        static void Reflect(AZ::ReflectContext* reflection)
        {
            AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
            if (serializeContext)
            {
                serializeContext->Class<ClassWithSimpleAsset>()
                    ->Field("m_simpleAsset", &ClassWithSimpleAsset::m_simpleAsset);
            }
        }

        SimpleAssetMock m_simpleAsset;
    };

    class SerializationDependenciesTests
        : public UnitTest::SerializeContextFixture
        , public UnitTest::TraceBusRedirector
    {
    protected:
        void SetUp() override
        {
            SerializeContextFixture::SetUp();
            AZ::Debug::TraceMessageBus::Handler::BusConnect();

            AZ::Data::AssetId::Reflect(m_serializeContext);
            AZ::Data::AssetData::Reflect(m_serializeContext);
            AzFramework::SimpleAssetReferenceBase::Reflect(m_serializeContext);
            ClassWithAssetId::Reflect(m_serializeContext);
            ClassWithAsset::Reflect(m_serializeContext);
            SimpleAssetMock::Reflect(m_serializeContext);
            ClassWithSimpleAsset::Reflect(m_serializeContext);
        }

        void TearDown() override
        {
            AZ::Debug::TraceMessageBus::Handler::BusDisconnect();
            UnitTest::SerializeContextFixture::TearDown();
        }

    };

    bool FindAssetIdInProductDependencies(const AZStd::vector<AssetBuilderSDK::ProductDependency>& productDependencies, const AZ::Data::AssetId& assetId)
    {
        for (const auto& productDependency : productDependencies)
        {
            if (productDependency.m_dependencyId == assetId)
            {
                return true;
            }
        }
        return false;
    }

    TEST_F(SerializationDependenciesTests, GatherProductDependencies_NullData_NoCrash)
    {
        AZStd::vector<AssetBuilderSDK::ProductDependency> productDependencies;
        AssetBuilderSDK::ProductPathDependencySet productPathDependencySet;
        // Using a known type for the nullptr instead of a void* so the template resolves properly for the call.
        ClassWithAssetId* nullClass = nullptr;
        AZ_TEST_START_TRACE_SUPPRESSION;
        bool gatherResult = AssetBuilderSDK::GatherProductDependencies(*m_serializeContext, nullClass, productDependencies, productPathDependencySet);
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        ASSERT_FALSE(gatherResult);
        ASSERT_EQ(productDependencies.size(), 0);
        ASSERT_EQ(productPathDependencySet.size(), 0);
    }

    TEST_F(SerializationDependenciesTests, GatherProductDependencies_HasValidAssetId_AssetIdFound)
    {
        AZStd::vector<AssetBuilderSDK::ProductDependency> productDependencies;
        AssetBuilderSDK::ProductPathDependencySet productPathDependencySet;
        ClassWithAssetId classWithAssetId;
        classWithAssetId.m_assetId = AZ::Data::AssetId("{3008D6F9-1E56-4699-95F9-91A3758A964E}", 33);
        bool gatherResult = AssetBuilderSDK::GatherProductDependencies(*m_serializeContext, &classWithAssetId, productDependencies, productPathDependencySet);
        
        ASSERT_TRUE(gatherResult);
        ASSERT_EQ(productDependencies.size(), 1);
        ASSERT_TRUE(FindAssetIdInProductDependencies(productDependencies, classWithAssetId.m_assetId));
        ASSERT_EQ(productPathDependencySet.size(), 0);
    }

    TEST_F(SerializationDependenciesTests, GatherProductDependencies_HasNullAssetId_NoDependencyEmitted)
    {
        AZStd::vector<AssetBuilderSDK::ProductDependency> productDependencies;
        AssetBuilderSDK::ProductPathDependencySet productPathDependencySet;
        ClassWithAssetId classWithAssetId;
        bool gatherResult = AssetBuilderSDK::GatherProductDependencies(*m_serializeContext, &classWithAssetId, productDependencies, productPathDependencySet);

        ASSERT_TRUE(gatherResult);
        ASSERT_EQ(productDependencies.size(), 0);
        ASSERT_EQ(productPathDependencySet.size(), 0);
    }

    TEST_F(SerializationDependenciesTests, GatherProductDependencies_HasValidAsset_AssetIdFound)
    {
        AZStd::vector<AssetBuilderSDK::ProductDependency> productDependencies;
        AssetBuilderSDK::ProductPathDependencySet productPathDependencySet;
        ClassWithAsset classWithAsset;
        AZ::Data::AssetId testAssetId("{CAAC5458-0738-43F6-A2BD-4E315C64BFD3}", 71);
        classWithAsset.m_asset = AZ::Data::Asset<AZ::Data::AssetData>(
            testAssetId,
            azrtti_typeid<AZ::Data::AssetData>());
        bool gatherResult = AssetBuilderSDK::GatherProductDependencies(*m_serializeContext, &classWithAsset, productDependencies, productPathDependencySet);

        ASSERT_TRUE(gatherResult);
        ASSERT_EQ(productDependencies.size(), 1);
        ASSERT_TRUE(FindAssetIdInProductDependencies(productDependencies, testAssetId));
        ASSERT_EQ(productPathDependencySet.size(), 0);
    }

    TEST_F(SerializationDependenciesTests, GatherProductDependencies_HasNullAsset_NoDependencyEmitted)
    {
        AZStd::vector<AssetBuilderSDK::ProductDependency> productDependencies;
        AssetBuilderSDK::ProductPathDependencySet productPathDependencySet;
        ClassWithAsset classWithAsset;
        AZ::Data::AssetId testAssetId;
        testAssetId.SetInvalid(); // Make it clear that this is an invalid ID.
        classWithAsset.m_asset = AZ::Data::Asset<AZ::Data::AssetData>(
            testAssetId,
            azrtti_typeid<AZ::Data::AssetData>());
        bool gatherResult = AssetBuilderSDK::GatherProductDependencies(*m_serializeContext, &classWithAsset, productDependencies, productPathDependencySet);

        ASSERT_TRUE(gatherResult);
        ASSERT_EQ(productDependencies.size(), 0);
        ASSERT_EQ(productPathDependencySet.size(), 0);
    }

    TEST_F(SerializationDependenciesTests, GatherProductDependencies_HasValidSimpleAsset_AssetPathFound)
    {
        AZStd::vector<AssetBuilderSDK::ProductDependency> productDependencies;
        AssetBuilderSDK::ProductPathDependencySet productPathDependencySet;
        ClassWithSimpleAsset classWithSimpleAsset;
        const AZStd::string expectedAssetPath("TestAssetPathString.txt");
        classWithSimpleAsset.m_simpleAsset.SetAssetPath(expectedAssetPath.c_str());
        
        bool gatherResult = AssetBuilderSDK::GatherProductDependencies(*m_serializeContext, &classWithSimpleAsset, productDependencies, productPathDependencySet);

        ASSERT_TRUE(gatherResult);
        ASSERT_EQ(productDependencies.size(), 0);
        ASSERT_EQ(productPathDependencySet.size(), 1);
        ASSERT_TRUE(productPathDependencySet.begin()->m_dependencyPath.compare(expectedAssetPath) == 0);
        ASSERT_TRUE(productPathDependencySet.begin()->m_dependencyType == AssetBuilderSDK::ProductPathDependencyType::ProductFile);
    }

    TEST_F(SerializationDependenciesTests, GatherProductDependencies_HasEmptyStringSimpleAsset_NoDependencyEmitted)
    {
        AZStd::vector<AssetBuilderSDK::ProductDependency> productDependencies;
        AssetBuilderSDK::ProductPathDependencySet productPathDependencySet;
        ClassWithSimpleAsset classWithSimpleAsset;
        classWithSimpleAsset.m_simpleAsset.SetAssetPath("");

        bool gatherResult = AssetBuilderSDK::GatherProductDependencies(*m_serializeContext, &classWithSimpleAsset, productDependencies, productPathDependencySet);

        ASSERT_TRUE(gatherResult);
        ASSERT_EQ(productDependencies.size(), 0);
        ASSERT_EQ(productPathDependencySet.size(), 0);
    }
}
