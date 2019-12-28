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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#pragma once

#include <AudioAllocators.h>
#include <AudioLogger.h>

#include <AzCore/std/string/string_view.h>
#include <AzCore/std/typetraits/is_integral.h>
#include <AzCore/std/typetraits/is_unsigned.h>

#define ATL_FLOAT_EPSILON (1.0e-6)


namespace Audio
{
    extern CAudioLogger g_audioLogger;

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<typename TMap, typename TKey>
    bool FindPlace(TMap& map, const TKey& key, typename TMap::iterator& iPlace)
    {
        iPlace = map.find(key);
        return (iPlace != map.end());
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template<typename TMap, typename TKey>
    bool FindPlaceConst(const TMap& map, const TKey& key, typename TMap::const_iterator& iPlace)
    {
        iPlace = map.find(key);
        return (iPlace != map.end());
    }

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
    bool AudioDebugDrawFilter(const AZStd::string_view objectName, const AZStd::string_view filter);
#endif //INCLUDE_AUDIO_PRODUCTION_CODE

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    template <typename ObjType, typename IDType = size_t>
    class CInstanceManager
    {
    public:
        ~CInstanceManager() {}

        using TPointerContainer = AZStd::vector<ObjType*, Audio::AudioSystemStdAllocator>;

        TPointerContainer m_cReserved;
        IDType m_nIDCounter;
        const size_t m_nReserveSize;
        const IDType m_nMinCounterValue;

        CInstanceManager(const size_t nReserveSize, const IDType nMinCounterValue)
            : m_nIDCounter(nMinCounterValue)
            , m_nReserveSize(nReserveSize)
            , m_nMinCounterValue(nMinCounterValue)
        {
            m_cReserved.reserve(m_nReserveSize);
        }

        IDType GetNextID()
        {
            if (m_nIDCounter >= m_nMinCounterValue)
            {
                return m_nIDCounter++;
            }
            else
            {
                g_audioLogger.Log(eALT_ERROR, "An AudioSystem InstanceManager ID counter wrapped around.");
                m_nIDCounter = m_nMinCounterValue;
                return m_nIDCounter;
            }
        }
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    class CSmoothFloat
    {
    public:
        explicit CSmoothFloat(const float fAlpha, const float fPrecision, const float fInitValue = 0.0f)
            : m_fValue(fInitValue)
            , m_fTarget(fInitValue)
            , m_bIsActive(false)
            , m_fAlpha(std::fabsf(fAlpha))
            , m_fPrecision(std::fabsf(fPrecision))
        {}

        ~CSmoothFloat() {}

        void Update(const float fUpdateIntervalMS)
        {
            if (m_bIsActive)
            {
                if (std::fabsf(m_fTarget - m_fValue) > m_fPrecision)
                {
                    // still need not reached the target within the specified precision
                    m_fValue += (m_fTarget - m_fValue) * m_fAlpha;
                }
                else
                {
                    //reached the target within the last update frame
                    m_fValue = m_fTarget;
                    m_bIsActive = false;
                }
            }
        }

        float GetCurrent() const
        {
            return m_fValue;
        }

        void SetNewTarget(const float fNewTarget, const bool bReset = false)
        {
            if (bReset)
            {
                m_fTarget = fNewTarget;
                m_fValue = fNewTarget;
            }
            else if (std::fabsf(fNewTarget - m_fTarget) > m_fPrecision)
            {
                m_fTarget = fNewTarget;
                m_bIsActive = true;
            }
        }

        void Reset(const float fInitValue = 0.0f)
        {
            m_fValue = m_fTarget = fInitValue;
            m_bIsActive = false;
        }

    private:
        float m_fValue;
        float m_fTarget;
        bool m_bIsActive;
        const float m_fAlpha;
        const float m_fPrecision;
    };

    /*!
     * Flags
     * Used for storing, checking, setting, and clearing related bits together.
     */
    template<typename StoredType,
        typename = AZStd::enable_if_t<AZStd::is_integral<StoredType>::value
                                      && AZStd::is_unsigned<StoredType>::value>>
    class Flags
    {
    public:
        Flags(const StoredType flags = 0)
            : m_storedFlags(flags)
        {}

        void AddFlags(const StoredType flags)
        {
            m_storedFlags |= flags;
        }

        void ClearFlags(const StoredType flags)
        {
            m_storedFlags &= ~flags;
        }

        bool AreAllFlagsActive(const StoredType flags) const
        {
            return (m_storedFlags & flags) == flags;
        }

        bool AreAnyFlagsActive(const StoredType flags) const
        {
            return (m_storedFlags & flags) != 0;
        }

        bool AreMultipleFlagsActive() const
        {
            return (m_storedFlags & (m_storedFlags - 1)) != 0;
        }

        bool IsOneFlagActive() const
        {
            return m_storedFlags != 0 && !AreMultipleFlagsActive();
        }

        void ClearAllFlags()
        {
            m_storedFlags = 0;
        }

        void SetFlags(StoredType flags, const bool enable)
        {
            enable ? AddFlags(flags) : ClearFlags(flags);
        }

        StoredType GetRawFlags() const
        {
            return m_storedFlags;
        }

        bool operator==(const Flags& other) const
        {
            return m_storedFlags == other.m_storedFlags;
        }

        bool operator!=(const Flags& other) const
        {
            return m_storedFlags != other.m_storedFlags;
        }

    private:
        StoredType m_storedFlags = 0;
    };

} // namespace Audio
