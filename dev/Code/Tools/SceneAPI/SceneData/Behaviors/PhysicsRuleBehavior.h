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

#include <AzCore/std/string/string.h>
#include <SceneAPI/SceneCore/Components/BehaviorComponent.h>
#include <SceneAPI/SceneCore/Events/AssetImportRequest.h>
#include <SceneAPI/SceneCore/Events/GraphMetaInfoBus.h>
#include <SceneAPI/SceneCore/Events/ManifestMetaInfoBus.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace DataTypes
        {
            class IGroup;
            class ISceneNodeSelectionList;
        }
        namespace SceneData
        {
            class PhysicsRuleBehavior 
                : public SceneCore::BehaviorComponent
                , public Events::ManifestMetaInfoBus::Handler
                , public Events::AssetImportRequestBus::Handler
                , public Events::GraphMetaInfoBus::Handler
            {
            public:
                AZ_COMPONENT(PhysicsRuleBehavior, "{B3F5D0AC-F9FC-483E-A3EE-FACC479A5B54}", SceneCore::BehaviorComponent);

                ~PhysicsRuleBehavior() override = default;

                void Activate() override;
                void Deactivate() override;
                static void Reflect(ReflectContext* context);

                void InitializeObject(const Containers::Scene& scene, DataTypes::IManifestObject& target) override;
                Events::ProcessingResult UpdateManifest(Containers::Scene& scene, ManifestAction action,
                    RequestingApplication requester) override;
                void GetAvailableModifiers(SceneAPI::Events::ManifestMetaInfo::ModifiersList& modifiers,
                    const SceneAPI::Containers::Scene& scene,
                    const SceneAPI::DataTypes::IManifestObject& target) override;

                void GetVirtualTypeName(AZStd::string& name, Crc32 type) override;
                void GetAllVirtualTypes(AZStd::set<Crc32>& types) override;

            private:
                size_t SelectPhysicsMeshes(const Containers::Scene& scene, DataTypes::ISceneNodeSelectionList& selection) const;
                void UpdatePhysicsRules(Containers::Scene& scene) const;
            };
        } // namespace SceneData
    } // namespace SceneAPI
} // namespace AZ
