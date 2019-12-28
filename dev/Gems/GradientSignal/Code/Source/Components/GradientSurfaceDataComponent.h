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
#include <GradientSignal/GradientSampler.h>
#include <GradientSignal/Ebuses/GradientSurfaceDataRequestBus.h>
#include <SurfaceData/SurfaceDataModifierRequestBus.h>
#include <SurfaceData/SurfaceDataTypes.h>
#include <LmbrCentral/Dependency/DependencyNotificationBus.h>

namespace LmbrCentral
{
    template<typename, typename>
    class EditorWrappedComponentBase;
}

namespace GradientSignal
{
    class GradientSurfaceDataConfig
        : public AZ::ComponentConfig
    {
    public:
        AZ_CLASS_ALLOCATOR(GradientSurfaceDataConfig, AZ::SystemAllocator, 0);
        AZ_RTTI(GradientSurfaceDataConfig, "{34516BA4-2B13-4A84-A46B-01E1980CA778}", AZ::ComponentConfig);
        static void Reflect(AZ::ReflectContext* context);
        float m_thresholdMin = 0.1f;
        float m_thresholdMax = 1.0f;
        SurfaceData::SurfaceTagVector m_modifierTags;

        size_t GetNumTags() const;
        AZ::Crc32 GetTag(int tagIndex) const;
        void RemoveTag(int tagIndex);
        void AddTag(AZStd::string tag);
    };

    static const AZ::Uuid GradientSurfaceDataComponentTypeId = "{BE5AF9E8-C509-4A8C-8D9E-D24BCD402812}";

    class GradientSurfaceDataComponent
        : public AZ::Component
        , private SurfaceData::SurfaceDataModifierRequestBus::Handler
        , private LmbrCentral::DependencyNotificationBus::Handler
        , private GradientSurfaceDataRequestBus::Handler
    {
    public:
        template<typename, typename> friend class LmbrCentral::EditorWrappedComponentBase;
        AZ_COMPONENT(GradientSurfaceDataComponent, GradientSurfaceDataComponentTypeId);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& services);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& services);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& services);
        static void Reflect(AZ::ReflectContext* context);

        GradientSurfaceDataComponent(const GradientSurfaceDataConfig& configuration);
        GradientSurfaceDataComponent() = default;
        ~GradientSurfaceDataComponent() = default;

        //////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Activate() override;
        void Deactivate() override;
        bool ReadInConfig(const AZ::ComponentConfig* baseConfig) override;
        bool WriteOutConfig(AZ::ComponentConfig* outBaseConfig) const override;

        ////////////////////////////////////////////////////////////////////////
        // SurfaceData::SurfaceDataModifierRequestBus
        void ModifySurfacePoints(SurfaceData::SurfacePointList& surfacePointList) const override;

        //////////////////////////////////////////////////////////////////////////
        // LmbrCentral::DependencyNotificationBus
        void OnCompositionChanged() override;

    protected:
        //////////////////////////////////////////////////////////////////////////
        // GradientSurfaceDataRequestBus
        void SetThresholdMin(float thresholdMin) override;
        float GetThresholdMin() const override;
        void SetThresholdMax(float thresholdMax) override;
        float GetThresholdMax() const override;
        size_t GetNumTags() const override;
        AZ::Crc32 GetTag(int tagIndex) const override;
        void RemoveTag(int tagIndex) override;
        void AddTag(AZStd::string tag) override;

    private:
        SurfaceData::SurfaceDataRegistryHandle m_modifierHandle = SurfaceData::InvalidSurfaceDataRegistryHandle;
        GradientSurfaceDataConfig m_configuration;
        GradientSampler m_gradientSampler;
    };
}