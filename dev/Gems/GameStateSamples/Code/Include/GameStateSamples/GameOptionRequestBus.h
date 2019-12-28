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

#include <AzCore/EBus/EBus.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>

#include <LmbrCentral/Audio/AudioSystemComponentBus.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace GameStateSamples
{
    ////////////////////////////////////////////////////////////////////////////////////////////////
    //! Game options that can be modified via the options menu and saved to persistent storage.
    class GameOptions final
    {
    public:
        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Name of the game options save data file.
        static constexpr const char* SaveDataBufferName = "GameOptions";

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Default value for the specified game option.
        ///@{
        static constexpr float DefaultAmbientVolume = 100.0f;
        static constexpr float DefaultEffectsVolume = 100.0f;
        static constexpr float DefaultMasterVolume = 100.0f;
        static constexpr float DefaultMusicVolume = 100.0f;
        ///@}

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Allocator
        AZ_CLASS_ALLOCATOR(GameOptions, AZ::SystemAllocator, 0);

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Type Info
        AZ_RTTI(GameOptions, "{DC3C8011-7E2B-458F-8C95-FC1A06C9D8F4}");

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Reflection
        static void Reflect(AZ::SerializeContext& sc);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Called when loaded from persistent data.
        void OnLoadedFromPersistentData();

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Effects volume accessor function.
        ///@{
        float GetAmbientVolume() const;
        void SetAmbientVolume(float ambientVolume);
        void ApplyAmbientVolume();
        ///@}

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Effects volume accessor function.
        ///@{
        float GetEffectsVolume() const;
        void SetEffectsVolume(float effectsVolume);
        void ApplyEffectsVolume();
        ///@}

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Master volume accessor function.
        ///@{
        float GetMasterVolume() const;
        void SetMasterVolume(float masterVolume);
        void ApplyMasterVolume();
        ///@}

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Music volume accessor function.
        ///@{
        float GetMusicVolume() const;
        void SetMusicVolume(float musicVolume);
        void ApplyMusicVolume();
        ///@}

    private:
        ////////////////////////////////////////////////////////////////////////////////////////////
        // Variables
        float m_ambientVolume = DefaultAmbientVolume;   //!< The current ambient volume.
        float m_effectsVolume = DefaultEffectsVolume;   //!< The current effects volume.
        float m_masterVolume = DefaultMasterVolume;     //!< The current master volume.
        float m_musicVolume = DefaultMusicVolume;       //!< The current music volume.
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////
    //! EBus interface used to submit requests related to game options.
    class GameOptionRequests : public AZ::EBusTraits
    {
    public:
        ////////////////////////////////////////////////////////////////////////////////////////////
        //! EBus Trait: requests can only be sent to and addressed by a single instance (singleton)
        ///@{
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        ///@}

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Retrieve the game options.
        virtual AZStd::shared_ptr<GameOptions> GetGameOptions() = 0;
    };
    using GameOptionRequestBus = AZ::EBus<GameOptionRequests>;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    inline void GameOptions::Reflect(AZ::SerializeContext& sc)
    {
        sc.Class<GameOptions>()
            ->Version(1)
            ->Field("ambientVolume", &GameOptions::m_ambientVolume)
            ->Field("effectsVolume", &GameOptions::m_effectsVolume)
            ->Field("masterVolume", &GameOptions::m_masterVolume)
            ->Field("musicVolume", &GameOptions::m_musicVolume)
        ;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    inline void GameOptions::OnLoadedFromPersistentData()
    {
        ApplyAmbientVolume();
        ApplyEffectsVolume();
        ApplyMasterVolume();
        ApplyMusicVolume();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    inline float GameOptions::GetAmbientVolume() const
    {
        return m_ambientVolume;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    inline void GameOptions::SetAmbientVolume(float ambientVolume)
    {
        m_ambientVolume = ambientVolume;
        ApplyAmbientVolume();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    inline void GameOptions::ApplyAmbientVolume()
    {
        LmbrCentral::AudioSystemComponentRequestBus::Broadcast(&LmbrCentral::AudioSystemComponentRequests::GlobalSetAudioRtpc,
                                                               "AmbientVolume",
                                                               m_ambientVolume);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    inline float GameOptions::GetEffectsVolume() const
    {
        return m_effectsVolume;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    inline void GameOptions::SetEffectsVolume(float effectsVolume)
    {
        m_effectsVolume = effectsVolume;
        ApplyEffectsVolume();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    inline void GameOptions::ApplyEffectsVolume()
    {
        LmbrCentral::AudioSystemComponentRequestBus::Broadcast(&LmbrCentral::AudioSystemComponentRequests::GlobalSetAudioRtpc,
                                                               "EffectsVolume",
                                                               m_effectsVolume);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    inline float GameOptions::GetMasterVolume() const
    {
        return m_masterVolume;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    inline void GameOptions::SetMasterVolume(float masterVolume)
    {
        m_masterVolume = masterVolume;
        ApplyMasterVolume();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    inline void GameOptions::ApplyMasterVolume()
    {
        LmbrCentral::AudioSystemComponentRequestBus::Broadcast(&LmbrCentral::AudioSystemComponentRequests::GlobalSetAudioRtpc,
                                                               "MasterVolume",
                                                               m_masterVolume);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    inline float GameOptions::GetMusicVolume() const
    {
        return m_musicVolume;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    inline void GameOptions::SetMusicVolume(float musicVolume)
    {
        m_musicVolume = musicVolume;
        ApplyMusicVolume();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    inline void GameOptions::ApplyMusicVolume()
    {
        LmbrCentral::AudioSystemComponentRequestBus::Broadcast(&LmbrCentral::AudioSystemComponentRequests::GlobalSetAudioRtpc,
                                                               "MusicVolume",
                                                               m_musicVolume);
    }
} // namespace GameState
