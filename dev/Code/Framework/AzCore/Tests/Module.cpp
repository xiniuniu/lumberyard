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

#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/Module/Module.h>
#include <AzCore/PlatformIncl.h>
#include <AzCore/Module/ModuleManagerBus.h>
#include <AzCore/Memory/AllocationRecords.h>
#include <AzCore/UnitTest/TestTypes.h>
#include "ModuleTestBus.h"

#if AZ_TRAIT_TEST_SUPPORT_DLOPEN
#include <dlfcn.h>
#endif

using namespace AZ;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::Matcher;
using ::testing::_;

namespace UnitTest
{
    static const AZ::Uuid AZCoreTestsDLLModuleId{ "{99C6BF95-847F-4EEE-BB60-9B26D02FF577}" };

    class SystemComponentRequests
        : public AZ::EBusTraits
    {
    public:
        virtual bool IsConnected() = 0;
    };
    using SystemComponentRequestBus = AZ::EBus<SystemComponentRequests>;

    class SystemComponentFromModule
        : public AZ::Component
        , protected SystemComponentRequestBus::Handler
    {
    public:
        AZ_COMPONENT(SystemComponentFromModule, "{7CDDF71F-4D9E-41B0-8F82-4FFA86513809}")

        void Activate() override
        {
            SystemComponentRequestBus::Handler::BusConnect();
        }
        void Deactivate() override
        {
            SystemComponentRequestBus::Handler::BusDisconnect();
        }

        static void Reflect(AZ::ReflectContext* reflectContext)
        {
            if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(reflectContext))
            {
                serializeContext->Class<SystemComponentFromModule, AZ::Component>()
                    ;
            }
        }

    protected:
        bool IsConnected() override
        {
            return true;
        }
    };

    class StaticModule
        : public Module
        , public ModuleTestRequestBus::Handler
    {
    public:
        static bool s_loaded;
        static bool s_reflected;

        StaticModule()
        {
            s_loaded = true;
            ModuleTestRequestBus::Handler::BusConnect();
            m_descriptors.insert(m_descriptors.end(), {
                SystemComponentFromModule::CreateDescriptor(),
            });
        }

        ~StaticModule()
        {
            ModuleTestRequestBus::Handler::BusDisconnect();
            s_loaded = false;
        }

        //void Reflect(ReflectContext*) override
        //{
         //   s_reflected = true;
        //}

        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList{
                azrtti_typeid<SystemComponentFromModule>(),
            };
        }

        const char* GetModuleName() override
        {
            return "StaticModule";
        }
    };

    bool StaticModule::s_loaded = false;
    bool StaticModule::s_reflected = false;

    void AZCreateStaticModules(AZStd::vector<AZ::Module*>& modulesOut)
    {
        modulesOut.push_back(new UnitTest::StaticModule());
    }

    TEST(ModuleManager, Test)
    {
        {
            ComponentApplication app;

            // Create application descriptor
            ComponentApplication::Descriptor appDesc;
            appDesc.m_memoryBlocksByteSize = 10 * 1024 * 1024;
            appDesc.m_recordingMode = Debug::AllocationRecords::RECORD_FULL;

            // AZCoreTestDLL will load as a dynamic module
            appDesc.m_modules.push_back();
            DynamicModuleDescriptor& dynamicModuleDescriptor = appDesc.m_modules.back();
            dynamicModuleDescriptor.m_dynamicLibraryPath = "AZCoreTestDLL";

            // StaticModule will load via AZCreateStaticModule(...)

            // Start up application
            ComponentApplication::StartupParameters startupParams;
            startupParams.m_createStaticModulesCallback = AZCreateStaticModules;
            Entity* systemEntity = app.Create(appDesc, startupParams);
            EXPECT_NE(nullptr, systemEntity);
            systemEntity->Init();
            systemEntity->Activate();

            // Check that StaticModule was loaded and reflected
            EXPECT_TRUE(StaticModule::s_loaded);
            // AZ_TEST_ASSERT(StaticModule::s_reflected);

            { // Query both modules via the ModuleTestRequestBus
                EBusAggregateResults<const char*> moduleNames;
                EBUS_EVENT_RESULT(moduleNames, ModuleTestRequestBus, GetModuleName);

                EXPECT_TRUE(moduleNames.values.size() == 2);
                bool foundStaticModule = false;
                bool foundDynamicModule = false;
                for (const char* moduleName : moduleNames.values)
                {
                    if (strcmp(moduleName, "DllModule") == 0)
                    {
                        foundDynamicModule = true;
                    }
                    if (strcmp(moduleName, "StaticModule") == 0)
                    {
                        foundStaticModule = true;
                    }
                }
                EXPECT_TRUE(foundDynamicModule);
                EXPECT_TRUE(foundStaticModule);
            }

            // Check that system component from module was added
            bool isComponentAround = false;
            SystemComponentRequestBus::BroadcastResult(isComponentAround, &SystemComponentRequestBus::Events::IsConnected);
            EXPECT_TRUE(isComponentAround);

            {
                // Find the dynamic module
                const ModuleData* systemLoadedModule = nullptr;
                ModuleManagerRequestBus::Broadcast(&ModuleManagerRequestBus::Events::EnumerateModules, [&systemLoadedModule](const ModuleData& moduleData) {
                    if (azrtti_typeid(moduleData.GetModule()) == AZCoreTestsDLLModuleId)
                    {
                        systemLoadedModule = &moduleData;
                        return false;
                    }
                    else
                    {
                        return true;
                    }
                });
                ASSERT_NE(nullptr, systemLoadedModule);

                ModuleManagerRequests::LoadModuleOutcome loadResult = AZ::Failure(AZStd::string("Failed to connect to ModuleManagerRequestBus"));

                // Load the module
                ModuleManagerRequestBus::BroadcastResult(loadResult, &ModuleManagerRequestBus::Events::LoadDynamicModule, "AZCoreTestDLL", ModuleInitializationSteps::ActivateEntity, true);
                ASSERT_TRUE(loadResult.IsSuccess());

                // Capture the handle
                AZStd::shared_ptr<ModuleData> moduleHandle = AZStd::move(loadResult.GetValue());

                // Validate that the pointer is the same as the one the system loaded
                EXPECT_EQ(systemLoadedModule, moduleHandle.get());

                // Load the module again
                ModuleManagerRequestBus::BroadcastResult(loadResult, &ModuleManagerRequestBus::Events::LoadDynamicModule, "AZCoreTestDLL", ModuleInitializationSteps::ActivateEntity, true);
                ASSERT_TRUE(loadResult.IsSuccess());

                // Validate that the pointers from the load calls are the same
                EXPECT_EQ(moduleHandle.get(), loadResult.GetValue().get());
            }

            // shut down application (deletes Modules, unloads DLLs)
            app.Destroy();
        }

        EXPECT_FALSE(StaticModule::s_loaded);

        bool isComponentAround = false;
        SystemComponentRequestBus::BroadcastResult(isComponentAround, &SystemComponentRequestBus::Events::IsConnected);
        EXPECT_FALSE(isComponentAround);
    }

    TEST(ModuleManager, SequentialLoadTest)
    {
        {
            ComponentApplication app;

            // Start up application
            ComponentApplication::Descriptor appDesc;
            ComponentApplication::StartupParameters startupParams;
            Entity* systemEntity = app.Create(appDesc, startupParams);

            EXPECT_NE(nullptr, systemEntity);
            systemEntity->Init();
            systemEntity->Activate();

            {
                // this scope exists to clear memory before app is destroyed.
                ModuleManagerRequests::LoadModuleOutcome loadResult = AZ::Failure(AZStd::string("Failed to connect to ModuleManagerRequestBus"));

                // Create the module
                ModuleManagerRequestBus::BroadcastResult(loadResult, &ModuleManagerRequestBus::Events::LoadDynamicModule, "AZCoreTestDLL", ModuleInitializationSteps::None, true);
                ASSERT_TRUE(loadResult.IsSuccess());

                // Find the dynamic module
                const ModuleData* systemLoadedModule = nullptr;
                ModuleManagerRequestBus::Broadcast(&ModuleManagerRequestBus::Events::EnumerateModules, [&systemLoadedModule](const ModuleData& moduleData)
                {
                    // Because the module was loaded with ModuleInitializationSteps::None, it should be the only one that doesn't have a module class
                    if (!moduleData.GetModule())
                    {
                        systemLoadedModule = &moduleData;
                        return false;
                    }
                    else
                    {
                        return true;
                    }
                });

                // Test that the module exists, but is empty
                ASSERT_NE(nullptr, systemLoadedModule);
                EXPECT_EQ(nullptr, systemLoadedModule->GetDynamicModuleHandle());
                EXPECT_EQ(nullptr, systemLoadedModule->GetModule());
                EXPECT_EQ(nullptr, systemLoadedModule->GetEntity());

                // Capture the handle
                AZStd::shared_ptr<ModuleData> moduleHandle = AZStd::move(loadResult.GetValue());

                // Validate that the pointer is the same as the one the system loaded
                EXPECT_EQ(systemLoadedModule, moduleHandle.get());

                // Load the module
                ModuleManagerRequestBus::BroadcastResult(loadResult, &ModuleManagerRequestBus::Events::LoadDynamicModule, "AZCoreTestDLL", ModuleInitializationSteps::Load, true);
                ASSERT_TRUE(loadResult.IsSuccess());

                // Validate that the pointers from the load calls are the same
                EXPECT_EQ(moduleHandle.get(), loadResult.GetValue().get());

                EXPECT_NE(nullptr, systemLoadedModule->GetDynamicModuleHandle());
                EXPECT_EQ(nullptr, systemLoadedModule->GetModule());
                EXPECT_EQ(nullptr, systemLoadedModule->GetEntity());

                // Create the module class
                ModuleManagerRequestBus::BroadcastResult(loadResult, &ModuleManagerRequestBus::Events::LoadDynamicModule, "AZCoreTestDLL", ModuleInitializationSteps::CreateClass, true);
                ASSERT_TRUE(loadResult.IsSuccess());
                EXPECT_EQ(moduleHandle.get(), loadResult.GetValue().get());

                EXPECT_NE(nullptr, systemLoadedModule->GetDynamicModuleHandle());
                EXPECT_NE(nullptr, systemLoadedModule->GetModule());
                EXPECT_EQ(nullptr, systemLoadedModule->GetEntity());

                // Activate the system entity
                ModuleManagerRequestBus::BroadcastResult(loadResult, &ModuleManagerRequestBus::Events::LoadDynamicModule, "AZCoreTestDLL", ModuleInitializationSteps::ActivateEntity, true);
                ASSERT_TRUE(loadResult.IsSuccess());
                EXPECT_EQ(moduleHandle.get(), loadResult.GetValue().get());

                EXPECT_NE(nullptr, systemLoadedModule->GetDynamicModuleHandle());
                EXPECT_NE(nullptr, systemLoadedModule->GetModule());
                EXPECT_NE(nullptr, systemLoadedModule->GetEntity());
            }

            // shut down application (deletes Modules, unloads DLLs)
            app.Destroy();
        }
    }


// the following tests only run on the following platforms which support module loading and unloading
// as these platforms expand we can always use traits to include the ones that can do so:
#if AZ_TRAIT_TEST_SUPPORT_MODULE_LOADING
    // this class just attaches to the PrintF stream and watches for a specific message
    // to appear.
    class PrintFCollector : public AZ::Debug::TraceMessageBus::Handler
    {
    public:
        PrintFCollector(const char* stringToWatchFor)
            :m_stringToWatchFor(stringToWatchFor)
        {
            BusConnect();
            
        }
        bool OnPrintf(const char* window, const char* message) override
        {
            if (
                ((window)&&(strstr(window, m_stringToWatchFor.c_str()))) ||
                ((message)&&(strstr(message, m_stringToWatchFor.c_str())))
                )
            {
                m_foundWhatWeWereWatchingFor = true;
            }
            return false;
        }

        ~PrintFCollector()
        {
            BusDisconnect();
        }

        bool m_foundWhatWeWereWatchingFor = false;
        AZ::OSString m_stringToWatchFor;
    };

    TEST(ModuleManager, OwnerInitializesAndDeinitializesTest)
    {
        // in this test, we make sure that a module is always initialized even if the operating
        // system previously loaded it (due to static linkage or other reason)
        // and that when it is initialized in this manner, it is also deinitialized when the owner
        // unloads it (even if the operating system still has a handle to it).
        // note that the above test already tests repeated loads and unloads, so there is no
        // need to test that here.
        
        ComponentApplication app;

        // Start up application
        ComponentApplication::Descriptor appDesc;
        ComponentApplication::StartupParameters startupParams;
        Entity* systemEntity = app.Create(appDesc, startupParams);

        ASSERT_NE(nullptr, systemEntity);
        systemEntity->Init();
        systemEntity->Activate();
        
        // we open a scope here to make sure any heap allocations made by local variables during this test
        // are destroyed before we try to stop the app.
        { 
            // we will use the fact that DynamicModuleHandle resolves paths to operating system specific
            // paths without actually calling Load(), and capture the final name it uses to load modules so that we
            // can manually load it ourselves.
            AZ::OSString finalPath;
            
            {
                auto handle = DynamicModuleHandle::Create("AZCoreTestDLL");
                finalPath = handle->GetFilename();
            }

            // now that we know the true name of the module in a way that it could be loaded by the operating system, 
            // we need to actually load the module using the operating system loader so that its "already loaded" by OS.

            {
#if AZ_TRAIT_TEST_SUPPORT_LOADLIBRARY
                // expect the module to not currently be loaded.
                EXPECT_EQ(nullptr, GetModuleHandleA(finalPath.c_str())); 
                HMODULE mod = ::LoadLibraryA(finalPath.c_str());
                ASSERT_NE(nullptr, mod);
#elif AZ_TRAIT_TEST_SUPPORT_DLOPEN
                void* pHandle = dlopen(finalPath.c_str(), RTLD_NOW);
                ASSERT_NE(nullptr, pHandle);
#endif 

                // now that the operating system has an open handle to it, we load it using the 
                // AZ functions, and make sure that the AZ library correctly attaches even though
                // the OS already has it open:
                PrintFCollector watchForDestruction("UninitializeDynamicModule called");
                PrintFCollector watchForCreation("InitializeDynamicModule called");
                {
                    auto handle = DynamicModuleHandle::Create("AZCoreTestDLL");
                    handle->Load(true);
                    EXPECT_TRUE(watchForCreation.m_foundWhatWeWereWatchingFor); // should not destroy until we leave scope.
                                                                                // steal the file path (which will be resolved with per-platform extensions like DLL or SO.
                    EXPECT_FALSE(watchForDestruction.m_foundWhatWeWereWatchingFor); // should not destroy until we leave scope.

                    PrintFCollector watchForCreationSecondTime("InitializeDynamicModule called");
                    auto handle2 = DynamicModuleHandle::Create("AZCoreTestDLL");
                    handle2->Load(true);
                    // this should NOT have initialized it again:
                    EXPECT_FALSE(watchForCreationSecondTime.m_foundWhatWeWereWatchingFor); // should not destroy until we leave scope.
                }
                EXPECT_TRUE(watchForDestruction.m_foundWhatWeWereWatchingFor); // we have left scope, destroy should have occurred.

         // drop the operating systems attachment to the module:
#if AZ_TRAIT_TEST_SUPPORT_LOADLIBRARY
                ::FreeLibrary(mod);
#elif AZ_TRAIT_TEST_SUPPORT_DLOPEN
                dlclose(pHandle);
#endif // platform switch statement 
            }
        }
        
        // shut down application (deletes Modules, unloads DLLs)
        app.Destroy();
    }

    class TestCalculateBinFolderClass : public ComponentApplication
    {
    public:
        TestCalculateBinFolderClass(const char* testExePath) :
            ComponentApplication()
        {
            azstrcpy(this->m_exeDirectory, AZ_ARRAY_SIZE(this->m_exeDirectory), testExePath);
        }

        void TestCalculateBinFolder()
        {
            this->PlatformCalculateBinFolder();
        }

        MOCK_CONST_METHOD1(CheckPathForEngineMarker, bool(const char*));

        bool m_bFileExists;
    };


#define TEST_BINFOLDER  "bin64vc141"

    TEST(ModuleManager, TestCalculateBinFolder)
    {
        // Note: all the paths used in this test has the path separators normalized to forward slashes, since this is
        // what the ComponentApplication will do automatically when it detects the exe path on initialize

        {   // Standard case (for win/mac) where the bin folder exists based on the build variant and built on the local host
            TestCalculateBinFolderClass app("c:/lyengine/dev/" TEST_BINFOLDER);

            EXPECT_CALL(app, CheckPathForEngineMarker(StrEq("c:/lyengine/dev")))
                        .WillRepeatedly(Return(true));

            app.TestCalculateBinFolder();

            EXPECT_STREQ(TEST_BINFOLDER, app.GetBinFolder());
        }
        {
            // Case where executable folder is at the root (c:) along with the engine marker
            TestCalculateBinFolderClass app("c:");

            EXPECT_CALL(app, CheckPathForEngineMarker(StrEq("")))
                        .WillRepeatedly(Return(true));

            app.TestCalculateBinFolder();

            EXPECT_STREQ("", app.GetBinFolder());
        }
        {
            // Case where the engine marker cannot be found and we start from the cdrive root
            TestCalculateBinFolderClass app("c:");

            EXPECT_CALL(app, CheckPathForEngineMarker(Matcher<const char*>(_)))
                        .WillRepeatedly(Return(false));

            app.TestCalculateBinFolder();

            EXPECT_STREQ("", app.GetBinFolder());
        }
        {
            // Case where the engine marker cannot be found and we start from a valid path
            TestCalculateBinFolderClass app("c:/lyengine/dev/" TEST_BINFOLDER);

            EXPECT_CALL(app, CheckPathForEngineMarker(Matcher<const char*>(_)))
                        .WillRepeatedly(Return(false));

            app.TestCalculateBinFolder();

            // This should not happen, but we should see a warning that it could not be found
            EXPECT_STREQ("", app.GetBinFolder());
        }
        {
            // Case the exe path is one level deeper than the bin64 folder
            TestCalculateBinFolderClass app("c:/lyengine/dev/" TEST_BINFOLDER "/rc/");

            EXPECT_CALL(app, CheckPathForEngineMarker(StrEq("c:/lyengine/dev/" TEST_BINFOLDER)))
                        .WillRepeatedly(Return(false));
            EXPECT_CALL(app, CheckPathForEngineMarker(StrEq("c:/lyengine/dev")))
                        .WillRepeatedly(Return(true));

            app.TestCalculateBinFolder();

            EXPECT_STREQ(TEST_BINFOLDER, app.GetBinFolder());
        }
    }
#endif // AZ_TRAIT_TEST_SUPPORT_MODULE_LOADING

} // namespace UnitTest
