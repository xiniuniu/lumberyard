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
#include "StdAfx.h"
#include <AzTest/AzTest.h>
#include <AzCore/Memory/AllocatorScope.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <Mocks/ICryPakMock.h>

using ::testing::NiceMock;

class EditorCoreTestEnvironment
    : public AZ::Test::ITestEnvironment
{
public:
    AZ_TEST_CLASS_ALLOCATOR(EditorCoreTestEnvironment);

    virtual ~EditorCoreTestEnvironment()
    {
    }

protected:
    void SetupEnvironment() override
    {
        m_allocatorScope.ActivateAllocators();
        m_cryPak = new NiceMock<CryPakMock>();

        // Initialize the fileIO
        AZ::IO::FileIOBase::SetInstance(&m_fileIO);

        // Setup gEnv with the systems/mocks we will be using for the unit tests
        m_stubEnv.pCryPak = m_cryPak;
        m_stubEnv.pFileIO = &m_fileIO;
        gEnv = &m_stubEnv;
    }

    void TeardownEnvironment() override
    {
        delete m_cryPak;
        m_allocatorScope.DeactivateAllocators();
    }

private:
    AZ::AllocatorScope<AZ::OSAllocator, AZ::SystemAllocator, AZ::LegacyAllocator, CryStringAllocator> m_allocatorScope;
    SSystemGlobalEnvironment m_stubEnv;
    AZ::IO::LocalFileIO m_fileIO;
    NiceMock<CryPakMock>* m_cryPak;
};

AZ_UNIT_TEST_HOOK(new EditorCoreTestEnvironment);

TEST(EditorCoreSanityTest, Sanity)
{
    EXPECT_EQ(1, 1);
}
