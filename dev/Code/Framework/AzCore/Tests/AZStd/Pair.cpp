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

#include <AzCore/std/containers/compressed_pair.h>
#include "UserTypes.h"

namespace UnitTest
{
    //////////////////////////////////////////////////////////////////////////
    // Fixtures

    // Fixture for non-typed tests
    template<typename TestConfig>
    class CompressedPairTest
        : public AllocatorsFixture
    {
    protected:
        void SetUp() override
        {
            AllocatorsFixture::SetUp();
        }

        void TearDown() override
        {
            AllocatorsFixture::TearDown();
        }
    };

    template<typename TestConfig>
    class CompressedPairSizeTest
        : public CompressedPairTest<TestConfig>
    {};

    namespace CompressedPairInternal
    {
        struct EmptyStruct
        {};

        struct EmptyStructNonDerived
        {};

        struct FinalEmptyStruct final
        {};

        struct DerivedFromEmptyStruct
            : EmptyStruct
        {

        };

        struct DerivedWithDataFromEmptyStruct
            : EmptyStruct
        {
            uint32_t m_value{};
        };
    }

    template<typename T1, typename T2, size_t MaxExpectedSize>
    struct CompressedPairTestConfig
    {
        using first_type = T1;
        using second_type = T2;
        static constexpr size_t max_expected_size = MaxExpectedSize;
    };

    constexpr size_t pairSize = sizeof(AZStd::compressed_pair<CompressedPairInternal::EmptyStruct, int32_t>);
    using CompressedPairTestConfigs = ::testing::Types<
        CompressedPairTestConfig<CompressedPairInternal::EmptyStruct, CompressedPairInternal::FinalEmptyStruct, 1>
        , CompressedPairTestConfig<CompressedPairInternal::EmptyStruct, int32_t, 4>
        , CompressedPairTestConfig<CompressedPairInternal::EmptyStruct, CompressedPairInternal::EmptyStructNonDerived, 1>
        , CompressedPairTestConfig<int32_t, CompressedPairInternal::EmptyStruct, 4>
        , CompressedPairTestConfig<int32_t, int32_t, 8>
    >;
    TYPED_TEST_CASE(CompressedPairTest, CompressedPairTestConfigs);

    using CompressedPairSizeTestConfigs = ::testing::Types<
        CompressedPairTestConfig<CompressedPairInternal::EmptyStruct, CompressedPairInternal::FinalEmptyStruct, 1>
        , CompressedPairTestConfig<CompressedPairInternal::EmptyStruct, CompressedPairInternal::EmptyStructNonDerived, 1>
        , CompressedPairTestConfig<CompressedPairInternal::EmptyStruct, int32_t, 4>
        , CompressedPairTestConfig<CompressedPairInternal::EmptyStructNonDerived, CompressedPairInternal::FinalEmptyStruct, 1>
        , CompressedPairTestConfig<CompressedPairInternal::EmptyStructNonDerived, CompressedPairInternal::DerivedFromEmptyStruct, 1>
        , CompressedPairTestConfig<CompressedPairInternal::FinalEmptyStruct, int32_t, 8>
        , CompressedPairTestConfig<CompressedPairInternal::FinalEmptyStruct, CompressedPairInternal::FinalEmptyStruct, 2>
        , CompressedPairTestConfig<CompressedPairInternal::FinalEmptyStruct, CompressedPairInternal::DerivedWithDataFromEmptyStruct, 8>
        , CompressedPairTestConfig<CompressedPairInternal::DerivedWithDataFromEmptyStruct, CompressedPairInternal::EmptyStructNonDerived, 4>
        , CompressedPairTestConfig<CompressedPairInternal::DerivedWithDataFromEmptyStruct, CompressedPairInternal::DerivedWithDataFromEmptyStruct, 8>
        , CompressedPairTestConfig<int32_t, int32_t, 8>
    >;
    TYPED_TEST_CASE(CompressedPairSizeTest, CompressedPairSizeTestConfigs);

    TYPED_TEST(CompressedPairTest, CompressedPairDefaultConstructorSucceeds)
    {
        AZStd::compressed_pair<typename TypeParam::first_type, typename TypeParam::second_type> testPair;
        (void)testPair;
    }

    TYPED_TEST(CompressedPairTest, CompressedPairFirstElementConstructorSucceeds)
    {
        AZStd::compressed_pair<typename TypeParam::first_type, typename TypeParam::second_type> testPair(typename TypeParam::first_type{});
        (void)testPair;
    }

    TYPED_TEST(CompressedPairTest, CompressedPairSecondElementConstructorSucceeds)
    {
        AZStd::compressed_pair<typename TypeParam::first_type, typename TypeParam::second_type> testPair(AZStd::skip_element_tag{}, typename TypeParam::second_type{});
        (void)testPair;
    }

    TYPED_TEST(CompressedPairTest, CompressedPairPiecewiseElementConstructorSucceeds)
    {
        AZStd::compressed_pair<typename TypeParam::first_type, typename TypeParam::second_type> testPair(AZStd::piecewise_construct_t{}, std::tuple<>{}, std::tuple<>{});
        (void)testPair;
    }

    TYPED_TEST(CompressedPairSizeTest, CompressedPairUsesEmptyBaseOptimizationForClassSize)
    {
        static_assert(sizeof(TypeParam) <= TypeParam::max_expected_size, "Compressed Pair is not expected size");
    }
}
