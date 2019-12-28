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

#include "AssetMemoryAnalyzer_precompiled.h"

#include <AzTest/AzTest.h>
#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AssetMemoryAnalyzer/AssetMemoryAnalyzerBus.h>
#include <../Source/AssetMemoryAnalyzer.h>
#include <../Source/AssetMemoryAnalyzerSystemComponent.h>

using namespace AssetMemoryAnalyzer;

static AZ::IAllocator* testalloc = nullptr;

class AssetMemoryAnalyzerTest
    : public UnitTest::AllocatorsTestFixture
{
protected:
    void SetUp() override
    {
        AllocatorsTestFixture::SetUp();
        testalloc = &AZ::AllocatorInstance<AZ::SystemAllocator>::GetAllocator();
        AZ::ComponentApplication::Descriptor desc;
        desc.m_useExistingAllocator = true;
        desc.m_enableDrilling = false;
        m_app = new (&m_appStorage) AZ::ComponentApplication;
        m_systemEntity = m_app->Create(desc);
        m_app->RegisterComponentDescriptor(AssetMemoryAnalyzerSystemComponent::CreateDescriptor());
        m_gemSystemComponent = m_systemEntity->CreateComponent<AssetMemoryAnalyzerSystemComponent>();
        m_systemEntity->Init();
        m_systemEntity->Activate();
    }

    void TearDown() override
    {
        m_app->Destroy();
        m_app->~ComponentApplication();

        AllocatorsTestFixture::TearDown();
    }

    AZStd::aligned_storage_for_t<AZ::ComponentApplication> m_appStorage;
    AZ::ComponentApplication* m_app = nullptr;
    AZ::Entity* m_systemEntity = nullptr;
    AZ::Component* m_gemSystemComponent = nullptr;
};

TEST_F(AssetMemoryAnalyzerTest, BasicTest)
{
    AZStd::shared_ptr<FrameAnalysis> analysis;
    AssetMemoryAnalyzerRequestBus::BroadcastResult(analysis, &AssetMemoryAnalyzerRequests::GetAnalysis);

    EXPECT_FALSE(analysis.get());

    AssetMemoryAnalyzerRequestBus::Broadcast(&AssetMemoryAnalyzerRequests::SetEnabled, true);
    AssetMemoryAnalyzerRequestBus::BroadcastResult(analysis, &AssetMemoryAnalyzerRequests::GetAnalysis);

    ASSERT_TRUE(analysis.get());
#ifndef AZ_TRACK_ASSET_SCOPES
    // No recordings should exist if analysis is disabled
    ASSERT_TRUE(analysis->GetAllocationPoints().empty());
#endif
}

AZ_UNIT_TEST_HOOK();
