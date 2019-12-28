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

#include <AzCore/Module/Module.h>
#include <AzCore/Module/DynamicModuleHandle.h>
#include <AzFramework/Metrics/MetricsPlainTextNameRegistration.h>
#include <SceneBuilder/SceneBuilderComponent.h>
#include <SceneBuilder/SceneSerializationHandler.h>
#include <Config/Components/SceneProcessingConfigSystemComponent.h>
#include <Config/Components/SoftNameBehavior.h>
#include <Exporting/Components/TangentGenerateComponent.h>
#include <Exporting/Components/TangentPreExportComponent.h>

namespace AZ
{
    namespace SceneProcessing
    {
        AZStd::unique_ptr<DynamicModuleHandle> s_sceneCoreModule;
        AZStd::unique_ptr<DynamicModuleHandle> s_sceneDataModule;
        AZStd::unique_ptr<DynamicModuleHandle> s_fbxSceneBuilderModule;

        class SceneProcessingModule
            : public Module
        {
        public:
            AZ_RTTI(SceneProcessingModule, "{13DCFEF2-BB25-4DBB-A69B-22958CAD6885}", Module);

            SceneProcessingModule()
                : Module()
            {
                LoadSceneModule(s_sceneCoreModule, "SceneCore");
                LoadSceneModule(s_sceneDataModule, "SceneData");
                LoadSceneModule(s_fbxSceneBuilderModule, "FbxSceneBuilder");

                m_descriptors.insert(m_descriptors.end(),
                {
                    SceneProcessingConfig::SceneProcessingConfigSystemComponent::CreateDescriptor(),
                    SceneProcessingConfig::SoftNameBehavior::CreateDescriptor(),
                    SceneBuilder::BuilderPluginComponent::CreateDescriptor(),
                    SceneBuilder::SceneSerializationHandler::CreateDescriptor(),
                    AZ::SceneExportingComponents::TangentPreExportComponent::CreateDescriptor(),
                    AZ::SceneExportingComponents::TangentGenerateComponent::CreateDescriptor()
                });

                // This is an internal Amazon gem, so register it's components for metrics tracking, otherwise the name of the component won't get sent back.
                // IF YOU ARE A THIRDPARTY WRITING A GEM, DO NOT REGISTER YOUR COMPONENTS WITH EditorMetricsComponentRegistrationBus
                AZStd::vector<AZ::Uuid> typeIds;
                typeIds.reserve(m_descriptors.size());
                for (AZ::ComponentDescriptor* descriptor : m_descriptors)
                {
                    typeIds.emplace_back(descriptor->GetUuid());
                }
                AzFramework::MetricsPlainTextNameRegistrationBus::Broadcast(&AzFramework::MetricsPlainTextNameRegistrationBus::Events::RegisterForNameSending, typeIds);
            }

            ~SceneProcessingModule()
            {
                UnloadModule(s_fbxSceneBuilderModule);
                UnloadModule(s_sceneDataModule);
                UnloadModule(s_sceneCoreModule);
            }

            AZ::ComponentTypeList GetRequiredSystemComponents() const override
            {
                return AZ::ComponentTypeList
                {
                    azrtti_typeid<SceneProcessingConfig::SceneProcessingConfigSystemComponent>(),
                };
            }

            void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
            {
                provided.push_back(AZ_CRC("SceneConfiguration", 0x2a3785fb));
            }

            void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
            {
                incompatible.push_back(AZ_CRC("SceneConfiguration", 0x2a3785fb));
            }

        protected:
            void LoadSceneModule(AZStd::unique_ptr<DynamicModuleHandle>& module, const char* name)
            {
                if (!module)
                {
                    module = DynamicModuleHandle::Create(name);
                    if (module)
                    {
                        module->Load(false);
                        auto init = module->GetFunction<InitializeDynamicModuleFunction>(InitializeDynamicModuleFunctionName);
                        if (init)
                        {
                            (*init)(AZ::Environment::GetInstance());
                        }
                    }
                }
            }

            void UnloadModule(AZStd::unique_ptr<DynamicModuleHandle>& module)
            {
                if (module)
                {
                    auto uninit = module->GetFunction<UninitializeDynamicModuleFunction>(UninitializeDynamicModuleFunctionName);
                    if (uninit)
                    {
                        (*uninit)();
                    }
                    module.reset();
                }
            }
        };
    } // namespace SceneProcessing
} // namespace AZ

AZ_DECLARE_MODULE_CLASS(SceneProcessing, AZ::SceneProcessing::SceneProcessingModule)
