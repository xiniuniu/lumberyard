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

#include <AzCore/Component/Component.h>
#include <Source/Pipeline/CgfMeshBuilder/CgfMeshAssetBuilderWorker.h>
#include <CrySystemBus.h>

namespace PhysX
{
    namespace Pipeline
    {
        class CgfMeshAssetBuilderComponent
            : public AZ::Component
        {
        public:
            AZ_COMPONENT(CgfMeshAssetBuilderComponent, "{D36072AA-D442-4E05-AB6F-B8F8EE7F8D78}");

            CgfMeshAssetBuilderComponent() = default;

            void Activate() override;
            void Deactivate() override;

            static void Reflect(AZ::ReflectContext* context);
            static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
            static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        private:
            CgfMeshAssetBuilderWorker m_meshAssetBuilder;
        };
    }
}