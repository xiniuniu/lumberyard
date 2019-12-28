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

#include <AzCore/PlatformDef.h>
#include <AzTest_Traits_Platform.h>
#include <list>

AZ_PUSH_DISABLE_WARNING(4389 4800, "-Wunknown-warning-option"); // 'int' : forcing value to bool 'true' or 'false' (performance warning).
#undef strdup // platform.h in CryCommon changes this define which is required by googletest
#include <gtest/gtest.h>
#include <gmock/gmock.h>
AZ_POP_DISABLE_WARNING;

#if defined(HAVE_BENCHMARK)
#include <benchmark/benchmark.h>
#endif

#include <AzCore/Memory/OSAllocator.h>

#define AZTEST_DLL_PUBLIC AZ_DLL_EXPORT
#define AZTEST_EXPORT extern "C" AZTEST_DLL_PUBLIC

namespace AZ
{
    namespace Test
    {
        //! Forward declarations
        class Platform;

        /*!
        * Implement this interface to define the environment setup and teardown functions.
        */
        class ITestEnvironment
            : public ::testing::Environment
        {
        public:
            virtual ~ITestEnvironment()
            {}

            void SetUp() override final
            {
                SetupEnvironment();
            }

            void TearDown() override final
            {
                TeardownEnvironment();
            }

        protected:
            virtual void SetupEnvironment() = 0;
            virtual void TeardownEnvironment() = 0;
        };

        extern ::testing::Environment* sTestEnvironment;

        /*!
        * Monolithic builds will have all the environments available.  Keep a mapping to run the desired envs.
        */
        class TestEnvironmentRegistry
        {
        public:
            TestEnvironmentRegistry(std::vector<ITestEnvironment*> a_envs, const std::string& a_module_name, bool a_unit)
                : m_module_name(a_module_name)
                , m_envs(a_envs)
                , m_unit(a_unit)
            {
                s_envs.push_back(this);
            }

            const std::string m_module_name;
            std::vector<ITestEnvironment*> m_envs;
            bool m_unit;

            static std::vector<TestEnvironmentRegistry*> s_envs;

        private:
            TestEnvironmentRegistry& operator=(const TestEnvironmentRegistry& tmp);
        };

        /*!
        * Empty implementation of ITestEnvironment.
        */
        class EmptyTestEnvironment final
            : public ITestEnvironment
        {
        public:
            virtual ~EmptyTestEnvironment()
            {}

        protected:
            void SetupEnvironment() override
            {}
            void TeardownEnvironment() override
            {}
        };

        void addTestEnvironment(ITestEnvironment* env);
        void addTestEnvironments(std::vector<ITestEnvironment*> envs);
        void excludeIntegTests();
        void runOnlyIntegTests();
        
        //! A hook that can be used to read any other misc parameters (such as --suite) and remove them before google sees them.
        //! Note that this modifies argc and argv to delete the parameters it consumes.
        void ApplyGlobalParameters(int* argc, char** argv);
        void printUnusedParametersWarning(int argc, char** argv);

        /*!
        * Main method for running tests from an executable.
        */
        class AzUnitTestMain final
        {
        public:
            AzUnitTestMain(std::vector<AZ::Test::ITestEnvironment*> envs)
                : m_returnCode(0)
                , m_envs(envs)
            {}

            bool Run(int argc, char** argv);
            bool Run(const char* commandLine);
            int ReturnCode() const { return m_returnCode; }

        private:
            int m_returnCode;
            std::vector<ITestEnvironment*> m_envs;
        };

        //! Run tests in a single library by loading it dynamically and executing the exported symbol,
        //! passing main-like parameters (argc, argv) from the (real or artificial) command line.
        int RunTestsInLib(Platform& platform, const std::string& lib, const std::string& symbol, int& argc, char** argv);

#if defined(HAVE_BENCHMARK)
        static constexpr const char* s_benchmarkEnvironmentName = "BenchmarkEnvironment";

        // BenchmarkEnvironment is a base that can be implemented to used to perform global initialization and teardown
        // for a module
        class BenchmarkEnvironmentBase
        {
        public:
            virtual ~BenchmarkEnvironmentBase() = default;

            virtual void SetUp()
            {
            }
            virtual void TearDown()
            {
            }
        };

        class BenchmarkEnvironmentRegistry
        {
        public:
            BenchmarkEnvironmentRegistry() = default;
            BenchmarkEnvironmentRegistry(const BenchmarkEnvironmentRegistry&) = delete;
            BenchmarkEnvironmentRegistry& operator=(const BenchmarkEnvironmentRegistry&) = delete;

            void AddBenchmarkEnvironment(std::unique_ptr<BenchmarkEnvironmentBase> env)
            {
                m_envs.push_back(std::move(env));
            }

            std::vector<std::unique_ptr<BenchmarkEnvironmentBase>>& GetBenchmarkEnvironments()
            {
                return m_envs;
            }

        private:
            std::vector<std::unique_ptr<BenchmarkEnvironmentBase>> m_envs;
        };

        /*
         * Creates a BenchmarkEnvironment using the specified template type and registers it with the BenchmarkEnvironmentRegister
         * @param T template argument that must have BenchmarkEnvironmentBase as a base class
         * @return returns a reference to the created BenchmarkEnvironment
         */
        template<typename T>
        T& RegisterBenchmarkEnvironment()
        {
            static_assert(std::is_base_of<BenchmarkEnvironmentBase, T>::value, "Supplied benchmark environment must be derived from BenchmarkEnvironmentBase");

            static AZ::EnvironmentVariable<AZ::Test::BenchmarkEnvironmentRegistry> s_benchmarkRegistry;
            if (!s_benchmarkRegistry)
            {
                s_benchmarkRegistry = AZ::Environment::CreateVariable<AZ::Test::BenchmarkEnvironmentRegistry>(s_benchmarkEnvironmentName);
            }

            auto benchmarkEnv{ new T };
            s_benchmarkRegistry->AddBenchmarkEnvironment(std::unique_ptr<BenchmarkEnvironmentBase>{ benchmarkEnv });
            return *benchmarkEnv;
        }
#endif
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        //! listener class to capture and print test output for embedded platforms
        class OutputEventListener : public ::testing::EmptyTestEventListener
        {
        public:
            std::list<std::string> resultList;
            
            void OnTestEnd(const ::testing::TestInfo& test_info)
            {
                std::string result;
                if (test_info.result()->Failed())
                {
                    result = "Fail";
                }
                else
                {
                    result = "Pass";
                }
                std::string formattedResult = "[GTEST][" + result + "] " + test_info.test_case_name() + " " + test_info.name() + "\n";
                resultList.emplace_back(formattedResult);
            }

            void OnTestProgramEnd(const ::testing::UnitTest& unit_test)
            {
                for (std::string testResults : resultList)
                {
                    AZ_Printf("", testResults.c_str());
                }
                if (unit_test.current_test_info())
                {
                    AZ_Printf("", "[GTEST] %s completed %u tests with u% failed test cases.", unit_test.current_test_info()->name(), unit_test.total_test_count(), unit_test.failed_test_case_count());
                }
            }
        };
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    } // Test
} // AZ

#define AZ_UNIT_TEST_HOOK_NAME AzRunUnitTests
#define AZ_INTEG_TEST_HOOK_NAME AzRunIntegTests

#if !defined(AZ_MONOLITHIC_BUILD)
// Environments should be declared dynamically, framework will handle deletion of resources
#define AZ_UNIT_TEST_HOOK(...)                                                                          \
    AZTEST_EXPORT int AZ_UNIT_TEST_HOOK_NAME(int argc, char** argv)                                     \
    {                                                                                                   \
        ::testing::InitGoogleMock(&argc, argv);                                                         \
        if (AZ_TRAIT_AZTEST_ATTACH_RESULT_LISTENER)                                                     \
        {                                                                                               \
            ::testing::TestEventListeners& listeners = testing::UnitTest::GetInstance()->listeners();   \
            listeners.Append(new AZ::Test::OutputEventListener);                                        \
        }                                                                                               \
        AZ::Test::excludeIntegTests();                                                                  \
        AZ::Test::ApplyGlobalParameters(&argc, argv);                                                   \
        AZ::Test::printUnusedParametersWarning(argc, argv);                                             \
        AZ::Test::addTestEnvironments({__VA_ARGS__});                                                   \
        int result = RUN_ALL_TESTS();                                                                   \
        return result;                                                                                  \
    }

// Environments should be declared dynamically, framework will handle deletion of resources
#define AZ_INTEG_TEST_HOOK(...)                                                     \
    AZTEST_EXPORT int AZ_INTEG_TEST_HOOK_NAME(int argc, char** argv)                \
    {                                                                               \
        ::testing::InitGoogleMock(&argc, argv);                                     \
        AZ::Test::runOnlyIntegTests();                                              \
        AZ::Test::ApplyGlobalParameters(&argc, argv);                               \
        AZ::Test::printUnusedParametersWarning(argc, argv);                         \
        AZ::Test::addTestEnvironments({__VA_ARGS__});                               \
        int result = RUN_ALL_TESTS();                                               \
        return result;                                                              \
    }

#if defined(HAVE_BENCHMARK)
#define AZ_BENCHMARK_HOOK() \
AZTEST_EXPORT int AzRunBenchmarks(int argc, char** argv) \
{ \
    auto benchmarkEnvRegistry = AZ::Environment::FindVariable<AZ::Test::BenchmarkEnvironmentRegistry>(AZ::Test::s_benchmarkEnvironmentName); \
    std::vector<std::unique_ptr<AZ::Test::BenchmarkEnvironmentBase>>* benchmarkEnvs = benchmarkEnvRegistry ? &(benchmarkEnvRegistry->GetBenchmarkEnvironments()) : nullptr; \
    if (benchmarkEnvs != nullptr) \
    { \
        for (std::unique_ptr<AZ::Test::BenchmarkEnvironmentBase>& benchmarkEnv : *benchmarkEnvs) \
        { \
            if (benchmarkEnv) \
            { \
                benchmarkEnv->SetUp(); \
            } \
        }\
    } \
    ::benchmark::Initialize(&argc, argv); \
    ::benchmark::RunSpecifiedBenchmarks(); \
    if (benchmarkEnvs != nullptr) \
    { \
        for (auto benchmarkEnvIter = benchmarkEnvs->rbegin(); benchmarkEnvIter != benchmarkEnvs->rend(); ++benchmarkEnvIter) \
        { \
            std::unique_ptr<AZ::Test::BenchmarkEnvironmentBase>& benchmarkEnv = *benchmarkEnvIter; \
            if (benchmarkEnv) \
            { \
                benchmarkEnv->TearDown(); \
            } \
        }\
    } \
    return 0; \
}
#else // !HAVE_BENCHMARK
#define AZ_BENCHMARK_HOOK()
#endif // HAVE_BENCHMARK
#else // monolithic build

#undef GTEST_MODULE_NAME_
#define GTEST_MODULE_NAME_ AZ_MODULE_NAME
#define AZTEST_CONCAT_(a, b) a ## _ ## b
#define AZTEST_CONCAT(a, b) AZTEST_CONCAT_(a, b)

#define AZ_UNIT_TEST_HOOK_REGISTRY_NAME AZTEST_CONCAT(AZ_UNIT_TEST_HOOK_NAME, Registry)
#define AZ_INTEG_TEST_HOOK_REGISTRY_NAME AZTEST_CONCAT(AZ_INTEG_TEST_HOOK_NAME, Registry)

#define AZ_UNIT_TEST_HOOK(...)                                                      \
            static AZ::Test::TestEnvironmentRegistry* AZ_UNIT_TEST_HOOK_REGISTRY_NAME =\
                new( AZ_OS_MALLOC(sizeof(AZ::Test::TestEnvironmentRegistry),           \
                                  alignof(AZ::Test::TestEnvironmentRegistry)))         \
              AZ::Test::TestEnvironmentRegistry({ __VA_ARGS__ }, AZ_MODULE_NAME, true);         

#define AZ_INTEG_TEST_HOOK(...)                                                     \
            static AZ::Test::TestEnvironmentRegistry* AZ_INTEG_TEST_HOOK_REGISTRY_NAME =\
                new( AZ_OS_MALLOC(sizeof(AZ::Test::TestEnvironmentRegistry),           \
                                  alignof(AZ::Test::TestEnvironmentRegistry)))         \
              AZ::Test::TestEnvironmentRegistry({ __VA_ARGS__ }, AZ_MODULE_NAME, false); 
#endif // AZ_MONOLITHIC_BUILD

// Declares a visible external symbol which identifies an executable as containing tests
#define DECLARE_AZ_UNIT_TEST_MAIN() AZTEST_EXPORT int ContainsAzUnitTestMain() { return 1; }

// Attempts to invoke the unit test main function if appropriate flags are present,
// otherwise simply continues launch as normal.
#define INVOKE_AZ_UNIT_TEST_MAIN(...)                                               \
    do {                                                                            \
        AZ::Test::AzUnitTestMain unitTestMain({__VA_ARGS__});                       \
        if (unitTestMain.Run(argc, argv))                                           \
        {                                                                           \
            return unitTestMain.ReturnCode();                                       \
        }                                                                           \
    } while (0); // safe multi-line macro - creates a single statement

// Some implementations use a commandLine rather than argc/argv
#define INVOKE_AZ_UNIT_TEST_MAIN_COMMAND_LINE(...)                                  \
    do {                                                                            \
        AZ::Test::AzUnitTestMain unitTestMain({__VA_ARGS__});                       \
        if (unitTestMain.Run(commandLine))                                          \
        {                                                                           \
            return unitTestMain.ReturnCode();                                       \
        }                                                                           \
    } while (0); // safe multi-line macro - creates a single statement

// Convenience macro for prepending Integ_ for an integration test that doesn't need a fixture
#define INTEG_TEST(test_case_name, test_name) GTEST_TEST(Integ_##test_case_name, test_name)

// Convenience macro for prepending Integ_ for an integration test that uses a fixture
#define INTEG_TEST_F(test_fixture, test_name) GTEST_TEST_(Integ_##test_fixture, test_name, test_fixture, ::testing::internal::GetTypeId<test_fixture>())

// Avoid accidentally being managed by CryMemory, or problems with new/delete when
// AZ allocators are not ready or properly un/initialized.
#define AZ_TEST_CLASS_ALLOCATOR(Class_)                                 \
    void* operator new (size_t size)                                    \
    {                                                                   \
        return AZ_OS_MALLOC(size, AZStd::alignment_of<Class_>::value);  \
    }                                                                   \
    void operator delete(void* ptr)                                     \
    {                                                                   \
        AZ_OS_FREE(ptr);                                                \
    }
