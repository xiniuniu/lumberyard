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

#include <SceneAPI/SceneCore/Components/RCExportingComponent.h>

namespace AZ
{
    namespace RC
    {
        struct AnimationExportContext;

        class AnimationExporter
            : public SceneAPI::SceneCore::RCExportingComponent
        {
        public:
            AZ_COMPONENT(AnimationExporter, "{78AAD156-2D43-4DC0-B083-CFD67559EBC1}", SceneAPI::SceneCore::RCExportingComponent);

            AnimationExporter();
            ~AnimationExporter() override = default;

            static void Reflect(ReflectContext* context);

            SceneAPI::Events::ProcessingResult CreateControllerData(AnimationExportContext& context);            
        };
    } // namespace RC
} // namespace AZ