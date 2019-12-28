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

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/Module/DynamicModuleHandle.h>

#include "SystemComponentFixture.h"

namespace AZ
{
    class DynamicModuleHandle;
}

namespace EMotionFX
{

    template<class... Components>
    class InitSceneAPIFixture : public ComponentFixture<Components...>
    {
        using DynamicModuleHandlePtr = AZStd::unique_ptr<AZ::DynamicModuleHandle>;

    protected:
        void Activate() override
        {
            const AZStd::vector<AZStd::string> moduleNames {"SceneCore", "SceneData"};
            for (const AZStd::string& moduleName : moduleNames)
            {
                AZStd::unique_ptr<AZ::DynamicModuleHandle> module = AZ::DynamicModuleHandle::Create(moduleName.c_str());
                ASSERT_TRUE(module) << "EMotionFX Editor unit tests failed to create " << moduleName.c_str() << " module.";
                const bool loaded = module->Load(false);
                ASSERT_TRUE(loaded) << "EMotionFX Editor unit tests failed to load " << moduleName.c_str() << " module.";
                auto init = module->GetFunction<AZ::InitializeDynamicModuleFunction>(AZ::InitializeDynamicModuleFunctionName);
                ASSERT_TRUE(init) << "EMotionFX Editor unit tests failed to find the initialization function the " << moduleName.c_str() << " module.";
                (*init)(AZ::Environment::GetInstance());

                m_modules.emplace_back(AZStd::move(module));
            }

            ComponentFixture<Components...>::Activate();
        }

        void Deactivate() override
        {
            // Deactivate the system entity first, releasing references to SceneAPI
            ComponentFixture<Components...>::Deactivate();

            // Now tear down SceneAPI
            for (DynamicModuleHandlePtr& module : m_modules)
            {
                auto uninit = module->template GetFunction<AZ::UninitializeDynamicModuleFunction>(AZ::UninitializeDynamicModuleFunctionName);
                ASSERT_TRUE(uninit) << "EMotionFX Editor unit tests failed to find the uninitialization function in the " << module->GetFilename().c_str() << " module.";

                (*uninit)();
                module.reset();
            }
            m_modules.clear();
            m_modules.shrink_to_fit();
        }

        private:
            AZStd::vector<DynamicModuleHandlePtr> m_modules;
    };
}
