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
#include "CryLegacy_precompiled.h"
#include <AzTest/AzTest.h>
#include <Mocks/ITimerMock.h>
#include <Mocks/ICryPakMock.h>
#include <AzCore/Memory/OSAllocator.h>
#include <AzCore/Memory/SystemAllocator.h>
// The AWS Native SDK AWSAllocator triggers a warning due to accessing members of std::allocator directly.
// AWSAllocator.h(70): warning C4996: 'std::allocator<T>::pointer': warning STL4010: Various members of std::allocator are deprecated in C++17.
// Use std::allocator_traits instead of accessing these members directly.
// You can define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING or _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS to acknowledge that you have received this warning.

#include <AzCore/PlatformDef.h>
AZ_PUSH_DISABLE_WARNING(4251 4996, "-Wunknown-warning-option")
#include <aws/core/http/HttpClientFactory.h>
AZ_POP_DISABLE_WARNING

using ::testing::NiceMock;

class CryLegacyTestEnvironment
    : public AZ::Test::ITestEnvironment
{
public:
    AZ_TEST_CLASS_ALLOCATOR(CryLegacyTestEnvironment);

    virtual ~CryLegacyTestEnvironment()
    {
    }

protected:
    struct MockHolder
    {
        AZ_TEST_CLASS_ALLOCATOR(MockHolder);

        NiceMock<TimerMock> timer;
        NiceMock<CryPakMock> pak;
    };

    void SetupEnvironment() override
    {
        AZ::AllocatorInstance<AZ::OSAllocator>::Create();
        AZ::AllocatorInstance<AZ::SystemAllocator>::Create();
        AZ::AllocatorInstance<AZ::LegacyAllocator>::Create();
        AZ::AllocatorInstance<CryStringAllocator>::Create();
        Aws::Http::InitHttp();

        // Mocks need to be destroyed before the allocators are destroyed, 
        // but if they are member variables, they get destroyed *after*
        // TeardownEnvironment when this Environment class is destroyed
        // by the GoogleTest framework.
        //
        // Mocks also do not have any public destroy or release method to
        // manage their lifetime, so this solution manages the lifetime
        // and ordering via the heap.
        m_mocks = new MockHolder();
        m_stubEnv.pTimer = &m_mocks->timer;
        m_stubEnv.pCryPak = &m_mocks->pak;
        gEnv = &m_stubEnv;
    }

    void TeardownEnvironment() override
    {
        // Destroy mocks before AZ allocators
        delete m_mocks;

        Aws::Http::CleanupHttp();
        AZ::AllocatorInstance<CryStringAllocator>::Destroy();
        AZ::AllocatorInstance<AZ::LegacyAllocator>::Destroy();
        AZ::AllocatorInstance<AZ::SystemAllocator>::Destroy();
        AZ::AllocatorInstance<AZ::OSAllocator>::Destroy();
    }

private:
    SSystemGlobalEnvironment m_stubEnv;
    MockHolder* m_mocks;
};

AZ_UNIT_TEST_HOOK(new CryLegacyTestEnvironment)
AZ_INTEG_TEST_HOOK();

TEST(CryAnimationSanityTest, Sanity)
{
    EXPECT_EQ(1, 1);
}
