/*
* All or portions of this file Copyright(c) Amazon.com, Inc.or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution(the "License").All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file.Do not
* remove or modify any license notices.This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#pragma once

#include <AzCore/RTTI/RTTI.h>
#include <AzCore/Component/Component.h>

#include "AudioControlBuilderWorker/AudioControlBuilderWorker.h"
#include "CfgBuilderWorker/CfgBuilderWorker.h"
#include "FontBuilderWorker/FontBuilderWorker.h"
#include "ParticlePreloadLibsBuilderWorker/ParticlePreloadLibsBuilderWorker.h"
#include "SchemaBuilderWorker/SchemaBuilderWorker.h"
#include "XmlBuilderWorker/XmlBuilderWorker.h"

namespace CopyDependencyBuilder
{
    //! The CopyDependencyBuilderComponent is responsible for setting up the CopyDependencyBuilderWorker.
    class CopyDependencyBuilderComponent
        : public AZ::Component
    {
    public:
        AZ_COMPONENT(CopyDependencyBuilderComponent, "{020E806C-E153-4E3A-8F4B-A550E3730808}");

        static void Reflect(AZ::ReflectContext* context);

        //////////////////////////////////////////////////////////////////////////
        // AZ::Component
        void Activate() override;
        void Deactivate() override;
        //////////////////////////////////////////////////////////////////////////


    private:
        FontBuilderWorker m_fontBuilderWorker;
        AudioControlBuilderWorker m_audioControlBuilderWorker;
        CfgBuilderWorker m_cfgBuilderWorker;
        ParticlePreloadLibsBuilderWorker m_particlePreloadBuilderWorker;
        XmlBuilderWorker m_xmlBuilderWorker;
        SchemaBuilderWorker m_schemaBuilderWorker;
    };
}
