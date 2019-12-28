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

#include <AudioEngineWwiseGemSystemComponent.h>

#include <AzCore/PlatformDef.h>
#include <AzCore/Memory/OSAllocator.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>

#include <ISystem.h>
#include <AudioAllocators.h>
#include <AudioLogger.h>
#include <AudioSystemImplCVars.h>
#include <AudioSystemImpl_wwise.h>
#include <Common_wwise.h>

#if defined(AUDIO_ENGINE_WWISE_EDITOR)
    #include <AudioSystemEditor_wwise.h>
#endif // AUDIO_ENGINE_WWISE_EDITOR


namespace Audio
{
    CAudioLogger g_audioImplLogger_wwise;
    CAudioWwiseImplCVars g_audioImplCVars_wwise;

#if AZ_TRAIT_AUDIOENGINEWWISE_PROVIDE_IMPL_SECONDARY_POOL
    TMemoryPoolReferenced g_audioImplMemoryPoolSecondary_wwise;
#endif // AZ_TRAIT_AUDIOENGINEWWISE_PROVIDE_IMPL_SECONDARY_POOL

    namespace Platform
    {
        void* InitializeSecondaryMemoryPool(size_t& secondarySize, CAudioWwiseImplCVars& audioImplCVars_wwise);
    }
} // namespace Audio


namespace AudioEngineWwiseGem
{
    void AudioEngineWwiseGemSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<AudioEngineWwiseGemSystemComponent, AZ::Component>()
                ->Version(0)
                ;

            if (AZ::EditContext* ec = serialize->GetEditContext())
            {
                ec->Class<AudioEngineWwiseGemSystemComponent>("Audio Engine Wwise Gem", "Wwise implementation of the Audio Engine interfaces")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ;
            }
        }
    }

    void AudioEngineWwiseGemSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("AudioEngineService"));
    }

    void AudioEngineWwiseGemSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("AudioEngineService"));
    }

    void AudioEngineWwiseGemSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC("AudioSystemService"));
    }

    void AudioEngineWwiseGemSystemComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        dependent.push_back(AZ_CRC("AudioSystemService"));
    }

    void AudioEngineWwiseGemSystemComponent::Init()
    {
    }

    void AudioEngineWwiseGemSystemComponent::Activate()
    {
        Audio::Gem::AudioEngineGemRequestBus::Handler::BusConnect();

    #if defined(AUDIO_ENGINE_WWISE_EDITOR)
        AudioControlsEditor::EditorImplPluginEventBus::Handler::BusConnect();
    #endif // AUDIO_ENGINE_WWISE_EDITOR
    }

    void AudioEngineWwiseGemSystemComponent::Deactivate()
    {
        Audio::Gem::AudioEngineGemRequestBus::Handler::BusDisconnect();

    #if defined(AUDIO_ENGINE_WWISE_EDITOR)
        AudioControlsEditor::EditorImplPluginEventBus::Handler::BusDisconnect();
    #endif // AUDIO_ENGINE_WWISE_EDITOR
    }

    bool AudioEngineWwiseGemSystemComponent::Initialize(const SSystemInitParams* initParams)
    {
        using namespace Audio;
        bool success = false;

        g_audioImplCVars_wwise.RegisterVariables();

        // initialize audio impl memory pools
        if (!AZ::AllocatorInstance<Audio::AudioImplAllocator>::IsReady())
        {
            const size_t poolSize = g_audioImplCVars_wwise.m_nPrimaryMemoryPoolSize << 10;

            Audio::AudioImplAllocator::Descriptor allocDesc;

            // Generic Allocator:
            allocDesc.m_allocationRecords = true;
            allocDesc.m_heap.m_numFixedMemoryBlocks = 1;
            allocDesc.m_heap.m_fixedMemoryBlocksByteSize[0] = poolSize;

            allocDesc.m_heap.m_fixedMemoryBlocks[0] = AZ::AllocatorInstance<AZ::OSAllocator>::Get().Allocate(allocDesc.m_heap.m_fixedMemoryBlocksByteSize[0], allocDesc.m_heap.m_memoryBlockAlignment);

            // Note: This allocator is destroyed in CAudioSystemImpl_wwise::Release() after the impl object has been freed.
            AZ::AllocatorInstance<Audio::AudioImplAllocator>::Create(allocDesc);
        }

        m_engineWwise = AZStd::make_unique<Audio::CAudioSystemImpl_wwise>();
        if (m_engineWwise)
        {
        #if AZ_TRAIT_AUDIOENGINEWWISE_PROVIDE_IMPL_SECONDARY_POOL
            size_t secondarySize = 0;
            void* secondaryMemoryPtr = Platform::InitializeSecondaryMemoryPool(secondarySize, g_audioImplCVars_wwise);

            g_audioImplMemoryPoolSecondary_wwise.InitMem(secondarySize, static_cast<uint8*>(secondaryMemoryPtr));
        #endif // AZ_TRAIT_AUDIOENGINEWWISE_PROVIDE_IMPL_SECONDARY_POOL

            g_audioImplLogger_wwise.Log(eALT_ALWAYS, "AudioEngineWwise created!");

            SAudioRequest oAudioRequestData;
            oAudioRequestData.nFlags = (eARF_PRIORITY_HIGH | eARF_EXECUTE_BLOCKING);

            SAudioManagerRequestData<eAMRT_INIT_AUDIO_IMPL> oAMData;
            oAudioRequestData.pData = &oAMData;
            Audio::AudioSystemRequestBus::Broadcast(&Audio::AudioSystemRequestBus::Events::PushRequestBlocking, oAudioRequestData);

            success = true;
        }
        else
        {
            g_audioImplLogger_wwise.Log(eALT_ALWAYS, "Could not create AudioEngineWwise!");
        }

        return success;
    }

    void AudioEngineWwiseGemSystemComponent::Release()
    {
        m_engineWwise.reset();

        if (AZ::AllocatorInstance<Audio::AudioImplAllocator>::IsReady())
        {
            AZ::AllocatorInstance<Audio::AudioImplAllocator>::Destroy();
        }

        Audio::g_audioImplCVars_wwise.UnregisterVariables();
    }

#if defined(AUDIO_ENGINE_WWISE_EDITOR)
    void AudioEngineWwiseGemSystemComponent::InitializeEditorImplPlugin()
    {
        m_editorImplPlugin.reset(new AudioControls::CAudioSystemEditor_wwise());
    }

    void AudioEngineWwiseGemSystemComponent::ReleaseEditorImplPlugin()
    {
        m_editorImplPlugin.reset();
    }

    AudioControls::IAudioSystemEditor* AudioEngineWwiseGemSystemComponent::GetEditorImplPlugin()
    {
        return m_editorImplPlugin.get();
    }
#endif // AUDIO_ENGINE_WWISE_EDITOR

} // namespace AudioEngineWwiseGem
