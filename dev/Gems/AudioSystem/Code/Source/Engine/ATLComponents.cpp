﻿/*
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

#include <ATLComponents.h>

#include <AzCore/IO/FileIO.h>
#include <AzCore/std/functional.h>
#include <AzCore/std/string/string_view.h>

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
    #include <AzCore/std/string/conversions.h>
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

#include <AzFramework/FileFunc/FileFunc.h>
#include <AzFramework/StringFunc/StringFunc.h>

#include <AudioFileUtils.h>
#include <ATLCommon.h>
#include <SoundCVars.h>
#include <IAudioSystemImplementation.h>

#include <MathConversion.h>
#include <IPhysics.h>
#include <ISurfaceType.h>
#include <I3DEngine.h>
#include <IRenderAuxGeom.h>

namespace Audio
{
    extern CAudioLogger g_audioLogger;

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    // AudioObjectIDFactory
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    const TAudioObjectID AudioObjectIDFactory::s_invalidAudioObjectID = INVALID_AUDIO_OBJECT_ID;
    const TAudioObjectID AudioObjectIDFactory::s_globalAudioObjectID = GLOBAL_AUDIO_OBJECT_ID;
    const TAudioObjectID AudioObjectIDFactory::s_minValidAudioObjectID = (GLOBAL_AUDIO_OBJECT_ID + 1);
    const TAudioObjectID AudioObjectIDFactory::s_maxValidAudioObjectID = static_cast<TAudioObjectID>(-256);
    // Beyond the max ID value, allow for a range of 255 ID values which will be reserved for the audio middleware.

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    // static
    TAudioObjectID AudioObjectIDFactory::GetNextID()
    {
        static TAudioObjectID s_nextId = s_minValidAudioObjectID;

        return (s_nextId <= s_maxValidAudioObjectID ? s_nextId++ : s_invalidAudioObjectID);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //  CAudioEventManager
    ///////////////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    CAudioEventManager::CAudioEventManager()
        : m_oAudioEventPool(g_audioCVars.m_nAudioEventPoolSize, 1)
    #if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
        , m_pDebugNameStore(nullptr)
    #endif // INCLUDE_AUDIO_PRODUCTION_CODE
    {
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    CAudioEventManager::~CAudioEventManager()
    {
        Release();
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioEventManager::Initialize()
    {
        const size_t numActiveAudioEvents = m_cActiveAudioEvents.size();

        for (size_t i = 0; i < m_oAudioEventPool.m_nReserveSize - numActiveAudioEvents; ++i)
        {
            const TAudioEventID nEventID = m_oAudioEventPool.GetNextID();
            IATLEventData* pNewEventData = nullptr;
            AudioSystemImplementationRequestBus::BroadcastResult(pNewEventData, &AudioSystemImplementationRequestBus::Events::NewAudioEventData, nEventID);
            auto pNewEvent = azcreate(CATLEvent, (nEventID, eAS_AUDIO_SYSTEM_IMPLEMENTATION, pNewEventData), Audio::AudioSystemAllocator, "ATLEvent");
            m_oAudioEventPool.m_cReserved.push_back(pNewEvent);
        }

        TActiveEventMap::const_iterator Iter(m_cActiveAudioEvents.begin());
        TActiveEventMap::const_iterator const IterEnd(m_cActiveAudioEvents.end());

        for (; Iter != IterEnd; ++Iter)
        {
            CATLEvent* const pEvent = Iter->second;
            IATLEventData* pNewEventData = nullptr;
            AudioSystemImplementationRequestBus::BroadcastResult(pNewEventData, &AudioSystemImplementationRequestBus::Events::NewAudioEventData, pEvent->GetID());
            pEvent->m_pImplData = pNewEventData;
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioEventManager::Release()
    {
        for (auto audioEvent : m_oAudioEventPool.m_cReserved)
        {
            AudioSystemImplementationRequestBus::Broadcast(&AudioSystemImplementationRequestBus::Events::DeleteAudioEventData, audioEvent->m_pImplData);
            azdestroy(audioEvent, Audio::AudioSystemAllocator);
        }
        m_oAudioEventPool.m_cReserved.clear();

        TActiveEventMap::const_iterator IterActiveAudioEvents(m_cActiveAudioEvents.begin());
        TActiveEventMap::const_iterator const IterActiveAudioEventsEnd(m_cActiveAudioEvents.end());

        for (; IterActiveAudioEvents != IterActiveAudioEventsEnd; ++IterActiveAudioEvents)
        {
            CATLEvent* const pEvent = IterActiveAudioEvents->second;
            AudioSystemImplementationRequestBus::Broadcast(&AudioSystemImplementationRequestBus::Events::ResetAudioEventData, pEvent->m_pImplData);
            AudioSystemImplementationRequestBus::Broadcast(&AudioSystemImplementationRequestBus::Events::DeleteAudioEventData, pEvent->m_pImplData);
            pEvent->m_pImplData = nullptr;
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioEventManager::Update(float fUpdateIntervalMS)
    {
        //TODO: implement
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    CATLEvent* CAudioEventManager::GetEvent(const EATLSubsystem eSender)
    {
        CATLEvent* pATLEvent = nullptr;

        switch (eSender)
        {
            case eAS_AUDIO_SYSTEM_IMPLEMENTATION:
            {
                pATLEvent = GetImplInstance();
                break;
            }
            case eAS_ATL_INTERNAL:
            {
                pATLEvent = GetInternalInstance();
                break;
            }
            default:
            {
                AZ_Assert(false, "Unknown sender specified in GetEvent (%d)", eSender);
                break;
            }
        }

        if (pATLEvent)
        {
            m_cActiveAudioEvents[pATLEvent->GetID()] = pATLEvent;
        }

        return pATLEvent;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    CATLEvent* CAudioEventManager::LookupID(const TAudioEventID nID) const
    {
        auto iPlace = m_cActiveAudioEvents.begin();
        const bool bLookupResult = FindPlaceConst(m_cActiveAudioEvents, nID, iPlace);

        return bLookupResult ? iPlace->second : nullptr;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioEventManager::ReleaseEvent(CATLEvent* const pEvent)
    {
        if (pEvent)
        {
            m_cActiveAudioEvents.erase(pEvent->GetID());

            switch (pEvent->m_eSender)
            {
                case eAS_AUDIO_SYSTEM_IMPLEMENTATION:
                {
                    ReleaseImplInstance(pEvent);
                    break;
                }
                case eAS_ATL_INTERNAL:
                {
                    ReleaseInternalInstance(pEvent);
                    break;
                }
                default:
                {
                    AZ_Assert(false, "Unknown sender specified in ReleaseEvent (%d)", pEvent->m_eSender);
                    break;
                }
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    CATLEvent* CAudioEventManager::GetImplInstance()
    {
        // must be called within a block protected by a critical section!

        CATLEvent* pEvent = nullptr;

        if (!m_oAudioEventPool.m_cReserved.empty())
        {
            //have reserved instances
            pEvent = m_oAudioEventPool.m_cReserved.back();
            m_oAudioEventPool.m_cReserved.pop_back();
        }
        else
        {
            //need to get a new instance
            const TAudioEventID nNewID = m_oAudioEventPool.GetNextID();

            IATLEventData* pNewEventData = nullptr;
            AudioSystemImplementationRequestBus::BroadcastResult(pNewEventData, &AudioSystemImplementationRequestBus::Events::NewAudioEventData, nNewID);
            pEvent = azcreate(CATLEvent, (nNewID, eAS_AUDIO_SYSTEM_IMPLEMENTATION, pNewEventData), Audio::AudioSystemAllocator, "ATLEvent");

            if (!pEvent)
            {
                --m_oAudioEventPool.m_nIDCounter;

                g_audioLogger.Log(eALT_WARNING, "Failed to get a new instance of an AudioEvent from the implementation");
                //failed to get a new instance from the implementation
            }
        }

        return pEvent;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioEventManager::ReleaseImplInstance(CATLEvent* const pOldEvent)
    {
        // must be called within a block protected by a critical section!

        if (pOldEvent)
        {
            pOldEvent->Clear();

            if (m_oAudioEventPool.m_cReserved.size() < m_oAudioEventPool.m_nReserveSize)
            {
                // can return the instance to the reserved pool
                AudioSystemImplementationRequestBus::Broadcast(&AudioSystemImplementationRequestBus::Events::ResetAudioEventData, pOldEvent->m_pImplData);
                m_oAudioEventPool.m_cReserved.push_back(pOldEvent);
            }
            else
            {
                // the reserve pool is full, can return the instance to the implementation to dispose
                AudioSystemImplementationRequestBus::Broadcast(&AudioSystemImplementationRequestBus::Events::DeleteAudioEventData, pOldEvent->m_pImplData);
                azdestroy(pOldEvent, Audio::AudioSystemAllocator);
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    CATLEvent* CAudioEventManager::GetInternalInstance()
    {
        // must be called within a block protected by a critical section!

        AZ_Assert(false, "GetInternalInstance was called yet it has no implementation!"); // implement when it is needed
        return nullptr;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioEventManager::ReleaseInternalInstance(CATLEvent* const pOldEvent)
    {
        // must be called within a block protected by a critical section!

        AZ_Assert(false, "ReleaseInternalInstance was called yet it has no implementation!"); // implement when it is needed
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    size_t CAudioEventManager::GetNumActive() const
    {
        return m_cActiveAudioEvents.size();
    }



    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //  CAudioObjectManager
    ///////////////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    CAudioObjectManager::CAudioObjectManager(CAudioEventManager& refAudioEventManager)
        : m_cObjectPool(g_audioCVars.m_nAudioObjectPoolSize, AudioObjectIDFactory::s_minValidAudioObjectID)
        , m_fTimeSinceLastVelocityUpdateMS(0.0f)
        , m_refAudioEventManager(refAudioEventManager)
    #if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
        , m_pDebugNameStore(nullptr)
    #endif // INCLUDE_AUDIO_PRODUCTION_CODE
    {
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    CAudioObjectManager::~CAudioObjectManager()
    {
    }


    float CAudioObjectManager::s_fVelocityUpdateIntervalMS = 100.0f;

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioObjectManager::Update(const float fUpdateIntervalMS, const SATLWorldPosition& rListenerPosition)
    {
    #if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
        //reset the ray counts
        CATLAudioObject::CPropagationProcessor::s_nTotalAsyncPhysRays = 0;
        CATLAudioObject::CPropagationProcessor::s_nTotalSyncPhysRays = 0;
    #endif // INCLUDE_AUDIO_PRODUCTION_CODE

        m_fTimeSinceLastVelocityUpdateMS += fUpdateIntervalMS;
        const bool bUpdateVelocity = m_fTimeSinceLastVelocityUpdateMS > s_fVelocityUpdateIntervalMS;

        for (auto& audioObjectPair : m_cAudioObjects)
        {
            CATLAudioObject* const pObject = audioObjectPair.second;

            if (HasActiveEvents(pObject))
            {
                pObject->Update(fUpdateIntervalMS, rListenerPosition);

                if (pObject->CanRunObstructionOcclusion())
                {
                    SATLSoundPropagationData oPropagationData;
                    pObject->GetPropagationData(oPropagationData);

                    AudioSystemImplementationRequestBus::Broadcast(&AudioSystemImplementationRequestBus::Events::SetObstructionOcclusion,
                        pObject->GetImplDataPtr(),
                        oPropagationData.fObstruction,
                        oPropagationData.fOcclusion);
                }

                if (bUpdateVelocity && pObject->GetVelocityTracking())
                {
                    pObject->UpdateVelocity(m_fTimeSinceLastVelocityUpdateMS);
                }

                AudioSystemImplementationRequestBus::Broadcast(&AudioSystemImplementationRequestBus::Events::UpdateAudioObject, pObject->GetImplDataPtr());
            }
        }

        if (bUpdateVelocity)
        {
            m_fTimeSinceLastVelocityUpdateMS = 0.0f;
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    bool CAudioObjectManager::ReserveID(TAudioObjectID& rAudioObjectID)
    {
        CATLAudioObject* const pNewObject = GetInstance();

        bool bSuccess = false;
        rAudioObjectID = INVALID_AUDIO_OBJECT_ID;

        if (pNewObject)
        {
            EAudioRequestStatus eImplResult = eARS_FAILURE;
            AudioSystemImplementationRequestBus::BroadcastResult(eImplResult, &AudioSystemImplementationRequestBus::Events::RegisterAudioObject, pNewObject->GetImplDataPtr(), nullptr);

            if (eImplResult == eARS_SUCCESS)
            {
                pNewObject->IncrementRefCount();
                rAudioObjectID = pNewObject->GetID();
                m_cAudioObjects.emplace(rAudioObjectID, pNewObject);
                bSuccess = true;
            }
            else
            {
                ReleaseInstance(pNewObject);
                bSuccess = false;
            }
        }

        return bSuccess;
    }


    ///////////////////////////////////////////////////////////////////////////////////////////////////
    bool CAudioObjectManager::ReleaseID(const TAudioObjectID nAudioObjectID)
    {
        bool bSuccess = false;
        CATLAudioObject* const pOldObject = LookupID(nAudioObjectID);

        if (pOldObject)
        {
            if (pOldObject->GetRefCount() < 2)
            {
                bSuccess = ReleaseInstance(pOldObject);
            }
            else
            {
                pOldObject->DecrementRefCount();
            }
        }

        return bSuccess;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    CATLAudioObject* CAudioObjectManager::LookupID(const TAudioObjectID nID) const
    {
        TActiveObjectMap::const_iterator iPlace;
        CATLAudioObject* pResult = nullptr;

        if (FindPlaceConst(m_cAudioObjects, nID, iPlace))
        {
            pResult = iPlace->second;
        }

        return pResult;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioObjectManager::ReportStartedEvent(CATLEvent const* const pEvent)
    {
        if (pEvent)
        {
            CATLAudioObject* const pObject = LookupID(pEvent->m_nObjectID);

            if (pObject)
            {
                pObject->ReportStartedEvent(pEvent);
            }
        #if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
            else
            {
                g_audioLogger.Log(
                    eALT_WARNING,
                    "Failed to report starting event %u on object %s as it does not exist!",
                    pEvent->GetID(),
                    m_pDebugNameStore->LookupAudioObjectName(pEvent->m_nObjectID));
            }
        #endif // INCLUDE_AUDIO_PRODUCTION_CODE
        }
        else
        {
            g_audioLogger.Log(eALT_WARNING, "NULL pEvent in CAudioObjectManager::ReportStartedEvent");
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioObjectManager::ReportFinishedEvent(const CATLEvent* const pEvent, const bool bSuccess)
    {
        if (pEvent)
        {
            CATLAudioObject* const pObject = LookupID(pEvent->m_nObjectID);

            if (pObject)
            {
                pObject->ReportFinishedEvent(pEvent, bSuccess);

                if (pObject->GetRefCount() == 0)
                {
                    ReleaseInstance(pObject);
                }
            }
        #if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
            else
            {
                g_audioLogger.Log(
                    eALT_WARNING,
                    "Removing Event %u from Object %s: Object no longer exists!",
                    pEvent->GetID(),
                    m_pDebugNameStore->LookupAudioObjectName(pEvent->m_nObjectID));
            }
        #endif // INCLUDE_AUDIO_PRODUCTION_CODE
        }
        else
        {
            g_audioLogger.Log(eALT_WARNING, "nullptr pEvent in CAudioObjectManager::ReportFinishedEvent");
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioObjectManager::ReportObstructionRay(const TAudioObjectID nAudioObjectID, const size_t nRayID)
    {
        CATLAudioObject* const pObject = LookupID(nAudioObjectID);

        if (pObject)
        {
            pObject->ReportPhysicsRayProcessed(nRayID);

            if (pObject->GetRefCount() == 0)
            {
                ReleaseInstance(pObject);
            }
        }
    #if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
        else
        {
            g_audioLogger.Log(
                eALT_WARNING,
                "Reporting Ray %zu from Object %s: Object no longer exists!",
                nRayID,
                m_pDebugNameStore->LookupAudioObjectName(nAudioObjectID));
        }
    #endif // INCLUDE_AUDIO_PRODUCTION_CODE
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    CATLAudioObject* CAudioObjectManager::GetInstance()
    {
        CATLAudioObject* pObject = nullptr;

        if (!m_cObjectPool.m_cReserved.empty())
        {
            //have reserved instances
            pObject = m_cObjectPool.m_cReserved.back();
            m_cObjectPool.m_cReserved.pop_back();
        }
        else
        {
            //need to get a new instance
            const TAudioObjectID nNewID = AudioObjectIDFactory::GetNextID();
            IATLAudioObjectData* pObjectData = nullptr;
            AudioSystemImplementationRequestBus::BroadcastResult(pObjectData, &AudioSystemImplementationRequestBus::Events::NewAudioObjectData, nNewID);
           
            size_t unallocatedMemorySize = AZ::AllocatorInstance<Audio::AudioSystemAllocator>::Get().GetUnAllocatedMemory();

            const size_t minimalMemorySize = 100 * 1024;

            if (unallocatedMemorySize < minimalMemorySize)
            {
                AZ::AllocatorInstance<Audio::AudioSystemAllocator>::Get().GarbageCollect();
                unallocatedMemorySize = AZ::AllocatorInstance<Audio::AudioSystemAllocator>::Get().GetUnAllocatedMemory();
            }

            if (unallocatedMemorySize >= minimalMemorySize)
            {
                pObject = azcreate(CATLAudioObject, (nNewID, pObjectData), Audio::AudioSystemAllocator, "ATLAudioObject");
            }

            if (!pObject)
            {
                --m_cObjectPool.m_nIDCounter;

                const char* msg = "Failed to get a new instance of an AudioObject from the implementation. "
                    "If this limit was reached from legitimate content creation and not a scripting error, "
                    "try increasing the Capacity of Audio::AudioSystemAllocator.";

                g_audioLogger.Log(eALT_ASSERT, msg);
                //failed to get a new instance from the implementation
            }
        }

        return pObject;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    bool CAudioObjectManager::ReleaseInstance(CATLAudioObject* const pOldObject)
    {
        bool bSuccess = false;
        if (pOldObject)
        {
            const TAudioObjectID nObjectID = pOldObject->GetID();
            m_cAudioObjects.erase(nObjectID);

        #if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
            m_pDebugNameStore->RemoveAudioObject(nObjectID);
            pOldObject->CheckBeforeRemoval(m_pDebugNameStore);
        #endif // INCLUDE_AUDIO_PRODUCTION_CODE

            pOldObject->Clear();
            EAudioRequestStatus eResult = eARS_FAILURE;
            AudioSystemImplementationRequestBus::BroadcastResult(eResult, &AudioSystemImplementationRequestBus::Events::UnregisterAudioObject, pOldObject->GetImplDataPtr());
            bSuccess = (eResult == eARS_SUCCESS);

            if (m_cObjectPool.m_cReserved.size() < m_cObjectPool.m_nReserveSize)
            {
                // can return the instance to the reserved pool
                AudioSystemImplementationRequestBus::Broadcast(&AudioSystemImplementationRequestBus::Events::ResetAudioObject, pOldObject->GetImplDataPtr());
                m_cObjectPool.m_cReserved.push_back(pOldObject);
            }
            else
            {
                // the reserve pool is full, can return the instance to the implementation to dispose
                AudioSystemImplementationRequestBus::Broadcast(&AudioSystemImplementationRequestBus::Events::DeleteAudioObjectData, pOldObject->GetImplDataPtr());
                azdestroy(pOldObject, Audio::AudioSystemAllocator);
            }
        }

        return bSuccess;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioObjectManager::Initialize()
    {
        const size_t numRegisteredObjects = m_cAudioObjects.size();

        for (size_t i = 0; i < m_cObjectPool.m_nReserveSize - numRegisteredObjects; ++i)
        {
            const auto nObjectID = AudioObjectIDFactory::GetNextID();
            IATLAudioObjectData* pObjectData = nullptr;
            AudioSystemImplementationRequestBus::BroadcastResult(pObjectData, &AudioSystemImplementationRequestBus::Events::NewAudioObjectData, nObjectID);
            auto pObject = azcreate(CATLAudioObject, (nObjectID, pObjectData), Audio::AudioSystemAllocator, "ATLAudioObject");
            m_cObjectPool.m_cReserved.push_back(pObject);
        }

        TActiveObjectMap::const_iterator IterObjects(m_cAudioObjects.begin());
        TActiveObjectMap::const_iterator const IterObjectsEnd(m_cAudioObjects.end());

        for (; IterObjects != IterObjectsEnd; ++IterObjects)
        {
            CATLAudioObject* const pAudioObject = IterObjects->second;
            IATLAudioObjectData* pObjectData = nullptr;
            AudioSystemImplementationRequestBus::BroadcastResult(pObjectData, &AudioSystemImplementationRequestBus::Events::NewAudioObjectData, pAudioObject->GetID());
            pAudioObject->SetImplDataPtr(pObjectData);

            char const* szAudioObjectName = nullptr;
        #if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
            szAudioObjectName = m_pDebugNameStore->LookupAudioObjectName(pAudioObject->GetID());
        #endif // INCLUDE_AUDIO_PRODUCTION_CODE

            EAudioRequestStatus eResult = eARS_FAILURE;
            AudioSystemImplementationRequestBus::BroadcastResult(eResult, &AudioSystemImplementationRequestBus::Events::RegisterAudioObject, pAudioObject->GetImplDataPtr(), szAudioObjectName);
            AZ_Assert(eResult == eARS_SUCCESS, "RegisterAudioObject failed to register object named '%s'", szAudioObjectName);
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioObjectManager::Release()
    {
        for (auto audioObject : m_cObjectPool.m_cReserved)
        {
            AudioSystemImplementationRequestBus::Broadcast(&AudioSystemImplementationRequestBus::Events::DeleteAudioObjectData, audioObject->GetImplDataPtr());
            azdestroy(audioObject, Audio::AudioSystemAllocator);
        }

        m_cObjectPool.m_cReserved.clear();

        TActiveObjectMap::const_iterator IterObjects(m_cAudioObjects.begin());
        TActiveObjectMap::const_iterator const IterObjectsEnd(m_cAudioObjects.end());

        for (; IterObjects != IterObjectsEnd; ++IterObjects)
        {
            CATLAudioObject* const pAudioObject = IterObjects->second;
            if (auto implObject = pAudioObject->GetImplDataPtr())
            {
                EAudioRequestStatus eResult = eARS_FAILURE;
                AudioSystemImplementationRequestBus::BroadcastResult(eResult, &AudioSystemImplementationRequestBus::Events::UnregisterAudioObject, implObject);
                AZ_Error("CAudioObjectManager", eResult == eARS_SUCCESS, "Failed to Unregister Audio Object!");
                AudioSystemImplementationRequestBus::BroadcastResult(eResult, &AudioSystemImplementationRequestBus::Events::ResetAudioObject, implObject);
                AZ_Error("CAudioObjectManager", eResult = eARS_SUCCESS, "Failed to Reset Audio Object!");
                AudioSystemImplementationRequestBus::Broadcast(&AudioSystemImplementationRequestBus::Events::DeleteAudioObjectData, implObject);
                pAudioObject->SetImplDataPtr(nullptr);
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioObjectManager::ReleasePendingRays()
    {
        if (!m_cAudioObjects.empty())
        {
            CInstanceManager<CATLAudioObject>::TPointerContainer aObjectsToRelease;

            for (auto& audioObjectPair : m_cAudioObjects)
            {
                CATLAudioObject* const pObject = audioObjectPair.second;

                if (pObject)
                {
                    pObject->ReleasePendingRays();

                    if (pObject->GetRefCount() == 0)
                    {
                        aObjectsToRelease.push_back(pObject);
                    }
                }
            }

            if (!aObjectsToRelease.empty())
            {
                for (auto audioObject : aObjectsToRelease)
                {
                    ReleaseInstance(audioObject);
                }
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    bool CAudioObjectManager::HasActiveEvents(const CATLAudioObjectBase* const pAudioObject) const
    {
        bool bFoundActiveEvent = false;

        const TObjectEventSet& rActiveEvents = pAudioObject->GetActiveEvents();
        TObjectEventSet::const_iterator IterActiveEvents(rActiveEvents.begin());
        TObjectEventSet::const_iterator const IterActiveEventsEnd(rActiveEvents.end());

        for (; IterActiveEvents != IterActiveEventsEnd; ++IterActiveEvents)
        {
            const CATLEvent* const pEvent = m_refAudioEventManager.LookupID(*IterActiveEvents);

            if (pEvent->IsPlaying())
            {
                bFoundActiveEvent = true;
                break;
            }
        }

        return bFoundActiveEvent;
    }



    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //  CAudioListenerManager
    ///////////////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    CAudioListenerManager::CAudioListenerManager()
        : m_pDefaultListenerObject(nullptr)
        , m_nDefaultListenerID(AudioObjectIDFactory::GetNextID())
        , m_listenerOverrideID(INVALID_AUDIO_OBJECT_ID)
    {
        m_cListenerPool.reserve(m_numReservedListeners);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    CAudioListenerManager::~CAudioListenerManager()
    {
        Release();
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioListenerManager::Initialize()
    {
        IATLListenerData* pNewListenerData = nullptr;

        // Default listener...
        AudioSystemImplementationRequestBus::BroadcastResult(pNewListenerData, &AudioSystemImplementationRequestBus::Events::NewDefaultAudioListenerObjectData, m_nDefaultListenerID);
        m_pDefaultListenerObject = azcreate(CATLListenerObject, (m_nDefaultListenerID, pNewListenerData), Audio::AudioSystemAllocator, "ATLListenerObject-Default");
        if (m_pDefaultListenerObject)
        {
            m_cActiveListeners[m_nDefaultListenerID] = m_pDefaultListenerObject;
        }

        // Additional listeners...
        for (size_t listener = 0; listener < m_numReservedListeners; ++listener)
        {
            const TAudioObjectID listenerId = AudioObjectIDFactory::GetNextID();
            AudioSystemImplementationRequestBus::BroadcastResult(pNewListenerData, &AudioSystemImplementationRequestBus::Events::NewAudioListenerObjectData, listenerId);
            auto listenerObject = azcreate(CATLListenerObject, (listenerId, pNewListenerData), Audio::AudioSystemAllocator, "ATLListenerObject");
            m_cListenerPool.push_back(listenerObject);
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioListenerManager::Release()
    {
        if (m_pDefaultListenerObject) // guard against double deletions
        {
            m_cActiveListeners.erase(m_nDefaultListenerID);

            AudioSystemImplementationRequestBus::Broadcast(&AudioSystemImplementationRequestBus::Events::DeleteAudioListenerObjectData, m_pDefaultListenerObject->m_pImplData);
            azdestroy(m_pDefaultListenerObject, Audio::AudioSystemAllocator);
            m_pDefaultListenerObject = nullptr;
        }

        // Release any remaining active audio listeners back to the listener pool
        for (auto listener : m_cActiveListeners)
        {
            ReleaseID(listener.first);
        }

        // Delete all from the audio listener pool
        for (auto listener : m_cListenerPool)
        {
            AudioSystemImplementationRequestBus::Broadcast(&AudioSystemImplementationRequestBus::Events::DeleteAudioListenerObjectData, listener->m_pImplData);
            azdestroy(listener, Audio::AudioSystemAllocator);
        }

        m_cListenerPool.clear();
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioListenerManager::Update(const float fUpdateIntervalMS)
    {
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    bool CAudioListenerManager::ReserveID(TAudioObjectID& rAudioObjectID)
    {
        bool bSuccess = false;

        if (!m_cListenerPool.empty())
        {
            CATLListenerObject* pListener = m_cListenerPool.back();
            m_cListenerPool.pop_back();

            const TAudioObjectID nID = pListener->GetID();
            m_cActiveListeners.emplace(nID, pListener);

            rAudioObjectID = nID;
            bSuccess = true;
        }
        else
        {
            g_audioLogger.Log(eALT_WARNING, "CAudioListenerManager::ReserveID - Reserved pool of pre-allocated Audio Listeners has been exhausted!");
        }

        return bSuccess;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    bool CAudioListenerManager::ReleaseID(const TAudioObjectID nAudioObjectID)
    {
        bool bSuccess = false;

        CATLListenerObject* pListener = LookupID(nAudioObjectID);
        if (pListener)
        {
            m_cActiveListeners.erase(nAudioObjectID);
            m_cListenerPool.push_back(pListener);
            bSuccess = true;
        }

        return bSuccess;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    CATLListenerObject* CAudioListenerManager::LookupID(const TAudioObjectID nID) const
    {
        CATLListenerObject* pListenerObject = nullptr;

        TActiveListenerMap::const_iterator iPlace = m_cActiveListeners.begin();

        if (FindPlaceConst(m_cActiveListeners, nID, iPlace))
        {
            pListenerObject = iPlace->second;
        }

        return pListenerObject;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    size_t CAudioListenerManager::GetNumActive() const
    {
        return m_cActiveListeners.size();
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioListenerManager::GetDefaultListenerPosition(SATLWorldPosition& oPosition)
    {
        if (m_pDefaultListenerObject)
        {
            oPosition = m_pDefaultListenerObject->oPosition;
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    bool CAudioListenerManager::SetOverrideListenerID(const TAudioObjectID nAudioObjectID)
    {
        // If passed ID is INVALID_AUDIO_OBJECT_ID, override is being turned off.
        TActiveListenerMap::const_iterator itBegin = m_cActiveListeners.begin();

        if (nAudioObjectID == INVALID_AUDIO_OBJECT_ID
            || FindPlaceConst(m_cActiveListeners, nAudioObjectID, itBegin))
        {
            m_listenerOverrideID = nAudioObjectID;
            return true;
        }

        return false;
    }



    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //  CAudioEventListenerManager
    ///////////////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    CAudioEventListenerManager::CAudioEventListenerManager()
    {
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    CAudioEventListenerManager::~CAudioEventListenerManager()
    {
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioEventListenerManager::AddRequestListener(const SAudioEventListener& listener)
    {
        for (const auto& currentListener : m_cListeners)
        {
            if (currentListener == listener)
            {
            #if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
                g_audioLogger.Log(eALT_WARNING, "AudioEventListenerManager::AddRequestListener - Request listener being added already exists!");
            #endif // INCLUDE_AUDIO_PRODUCTION_CODE
                return;
            }
        }

        m_cListeners.push_back(listener);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioEventListenerManager::RemoveRequestListener(const SAudioEventListener& listener)
    {
        for (auto iter = m_cListeners.begin(); iter != m_cListeners.end(); ++iter)
        {
            if ((iter->m_fnOnEvent == listener.m_fnOnEvent || listener.m_fnOnEvent == nullptr) && iter->m_callbackOwner == listener.m_callbackOwner)
            {
                // Copy the back element into this iter position and pop the back element...
                (*iter) = m_cListeners.back();
                m_cListeners.pop_back();
                return;
            }
        }

    #if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
        g_audioLogger.Log(eALT_WARNING, "AudioEventListenerManager::RemoveRequestListener - Failed to remove a request listener (not found)!");
    #endif // INCLUDE_AUDIO_PRODUCTION_CODE
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioEventListenerManager::NotifyListener(const SAudioRequestInfo* const pResultInfo)
    {
        // This should always be on the main thread!
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::Audio);

        auto found = AZStd::find_if(m_cListeners.begin(), m_cListeners.end(),
            [pResultInfo](const SAudioEventListener& currentListener)
            {
                // 1) Is the listener interested in this request type?
                // 2) Is the listener interested in this request sub-type?
                // 3) Is the listener interested in this owner (or any owner)?
                return ((currentListener.m_requestType == eART_AUDIO_ALL_REQUESTS || currentListener.m_requestType == pResultInfo->eAudioRequestType)
                    && ((currentListener.m_specificRequestMask & pResultInfo->nSpecificAudioRequest) != 0)
                    && (currentListener.m_callbackOwner == nullptr || currentListener.m_callbackOwner == pResultInfo->pOwner));
            }
        );

        if (found != m_cListeners.end())
        {
            found->m_fnOnEvent(pResultInfo);
        }
    }



    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //  CATLXMLProcessor
    ///////////////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    CATLXmlProcessor::CATLXmlProcessor(
        TATLTriggerLookup& rTriggers,
        TATLRtpcLookup& rRtpcs,
        TATLSwitchLookup& rSwitches,
        TATLEnvironmentLookup& rEnvironments,
        TATLPreloadRequestLookup& rPreloadRequests,
        CFileCacheManager& rFileCacheMgr)
        : m_rTriggers(rTriggers)
        , m_rRtpcs(rRtpcs)
        , m_rSwitches(rSwitches)
        , m_rEnvironments(rEnvironments)
        , m_rPreloadRequests(rPreloadRequests)
        , m_nTriggerImplIDCounter(AUDIO_TRIGGER_IMPL_ID_NUM_RESERVED)
        , m_rFileCacheMgr(rFileCacheMgr)
        , m_rootPath("@assets@")
    #if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
        , m_pDebugNameStore(nullptr)
    #endif // INCLUDE_AUDIO_PRODUCTION_CODE
    {
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    CATLXmlProcessor::~CATLXmlProcessor()
    {
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CATLXmlProcessor::Initialize()
    {
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CATLXmlProcessor::Release()
    {
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CATLXmlProcessor::ParseControlsData(const char* const folderPath, const EATLDataScope dataScope)
    {
        AZStd::string searchPath;
        AzFramework::StringFunc::Path::Join(m_rootPath.c_str(), folderPath, searchPath);

        AZStd::vector<AZStd::string> foundFiles = Audio::FindFilesInPath(searchPath, "*.xml");

        for (const auto& file : foundFiles)
        {
            AZ_Assert(AZ::IO::FileIOBase::GetInstance()->Exists(file.c_str()), "FindFiles found file '%s' but FileIO says it doesn't exist!", file.c_str());
            g_audioLogger.Log(eALT_ALWAYS, "Loading Audio Controls Library: '%s'", file.c_str());

            Audio::ScopedXmlLoader xmlFileLoader(file);
            if (xmlFileLoader.HasError())
            {
                continue;
            }

            const AZ::rapidxml::xml_node<char>* xmlRootNode = xmlFileLoader.GetRootNode();
            if (xmlRootNode)
            {
                auto childNode = xmlRootNode->first_node(nullptr, 0, false);
                while (childNode)
                {
                    if (azstricmp(childNode->name(), ATLXmlTags::TriggersNodeTag) == 0)
                    {
                        ParseAudioTriggers(childNode, dataScope);
                    }
                    else if (azstricmp(childNode->name(), ATLXmlTags::RtpcsNodeTag) == 0)
                    {
                        ParseAudioRtpcs(childNode, dataScope);
                    }
                    else if (azstricmp(childNode->name(), ATLXmlTags::SwitchesNodeTag) == 0)
                    {
                        ParseAudioSwitches(childNode, dataScope);
                    }
                    else if (azstricmp(childNode->name(), ATLXmlTags::EnvironmentsNodeTag) == 0)
                    {
                        ParseAudioEnvironments(childNode, dataScope);
                    }

                    childNode = childNode->next_sibling(nullptr, 0, false);
                }
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CATLXmlProcessor::ParsePreloadsData(const char* const folderPath, const EATLDataScope dataScope)
    {
        AZStd::string searchPath;
        AzFramework::StringFunc::Path::Join(m_rootPath.c_str(), folderPath, searchPath);

        AZStd::vector<AZStd::string> foundFiles = Audio::FindFilesInPath(searchPath, "*.xml");

        for (const auto& file : foundFiles)
        {
            AZ_Assert(AZ::IO::FileIOBase::GetInstance()->Exists(file.c_str()), "FindFiles found file '%s' but FileIO says it doesn't exist!", file.c_str());
            g_audioLogger.Log(eALT_ALWAYS, "Loading Audio Preloads Library: '%s'", file.c_str());

            Audio::ScopedXmlLoader xmlFileLoader(file);
            if (xmlFileLoader.HasError())
            {
                continue;
            }

            const AZ::rapidxml::xml_node<char>* xmlRootNode = xmlFileLoader.GetRootNode();
            if (xmlRootNode)
            {
                auto childNode = xmlRootNode->first_node(ATLXmlTags::PreloadsNodeTag, 0, false);
                while (childNode)
                {
                    if (dataScope == eADS_LEVEL_SPECIFIC)
                    {
                        AZStd::string_view relativePath(folderPath);
                        AZStd::string levelName;
                        AzFramework::StringFunc::Path::GetFileName(relativePath.data(), levelName);
                        ParseAudioPreloads(childNode, dataScope, levelName.data());
                    }
                    else
                    {
                        ParseAudioPreloads(childNode, dataScope, nullptr);
                    }

                    childNode = childNode->next_sibling(ATLXmlTags::PreloadsNodeTag, 0, false);
                }
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CATLXmlProcessor::ClearControlsData(const EATLDataScope eDataScope)
    {
        // Remove Triggers...
        for (auto itRemove = m_rTriggers.begin(); itRemove != m_rTriggers.end(); )
        {
            auto const pTrigger = itRemove->second;
            if (eDataScope == eADS_ALL || (pTrigger->GetDataScope() == eDataScope))
            {
            #if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
                m_pDebugNameStore->RemoveAudioTrigger(pTrigger->GetID());
            #endif // INCLUDE_AUDIO_PRODUCTION_CODE

                DeleteAudioTrigger(pTrigger);
                itRemove = m_rTriggers.erase(itRemove);
            }
            else
            {
                ++itRemove;
            }
        }

        // Remove Rtpcs...
        for (auto itRemove = m_rRtpcs.begin(); itRemove != m_rRtpcs.end(); )
        {
            auto const pRtpc = itRemove->second;
            if (eDataScope == eADS_ALL || (pRtpc->GetDataScope() == eDataScope))
            {
            #if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
                m_pDebugNameStore->RemoveAudioRtpc(pRtpc->GetID());
            #endif // INCLUDE_AUDIO_PRODUCTION_CODE

                DeleteAudioRtpc(pRtpc);
                itRemove = m_rRtpcs.erase(itRemove);
            }
            else
            {
                ++itRemove;
            }
        }

        // Remove Switches...
        for (auto itRemove = m_rSwitches.begin(); itRemove != m_rSwitches.end(); )
        {
            auto const pSwitch = itRemove->second;
            if (eDataScope == eADS_ALL || (pSwitch->GetDataScope() == eDataScope))
            {
            #if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
                m_pDebugNameStore->RemoveAudioSwitch(pSwitch->GetID());
            #endif // INCLUDE_AUDIO_PRODUCTION_CODE

                DeleteAudioSwitch(pSwitch);
                itRemove = m_rSwitches.erase(itRemove);
            }
            else
            {
                ++itRemove;
            }
        }

        // Remove Environments...
        for (auto itRemove = m_rEnvironments.begin(); itRemove != m_rEnvironments.end(); )
        {
            auto const pEnvironment = itRemove->second;
            if (eDataScope == eADS_ALL || (pEnvironment->GetDataScope() == eDataScope))
            {
            #if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
                m_pDebugNameStore->RemoveAudioEnvironment(pEnvironment->GetID());
            #endif // INCLUDE_AUDIO_PRODUCTION_CODE

                DeleteAudioEnvironment(pEnvironment);
                itRemove = m_rEnvironments.erase(itRemove);
            }
            else
            {
                ++itRemove;
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CATLXmlProcessor::ParseAudioPreloads(const AZ::rapidxml::xml_node<char>* preloadsXmlRoot, const EATLDataScope dataScope, const char* const folderName)
    {
        auto preloadNode = preloadsXmlRoot->first_node(ATLXmlTags::ATLPreloadRequestTag, 0, false);
        while (preloadNode)
        {
            TAudioPreloadRequestID preloadRequestId = ATLInternalControlIDs::GlobalPreloadRequestID;
            const char* preloadRequestName = ATLInternalControlNames::GlobalPreloadRequestName;

            bool autoLoad = false;
            auto loadTypeAttr = preloadNode->first_attribute(ATLXmlTags::ATLTypeAttribute, 0, false);
            auto nameAttr = preloadNode->first_attribute(ATLXmlTags::ATLNameAttribute, 0, false);

            if (loadTypeAttr && azstricmp(loadTypeAttr->value(), ATLXmlTags::ATLDataLoadType) == 0)
            {
                autoLoad = true;
                if (dataScope == eADS_LEVEL_SPECIFIC)
                {
                    preloadRequestName = folderName;
                    preloadRequestId = AudioStringToID<TAudioPreloadRequestID>(preloadRequestName);
                }
            }
            else if (nameAttr)
            {
                preloadRequestName = nameAttr->value();
                preloadRequestId = AudioStringToID<TAudioPreloadRequestID>(preloadRequestName);
            }

            if (preloadRequestId != INVALID_AUDIO_PRELOAD_REQUEST_ID)
            {
                // Needs to have at least two child nodes: <ATLPlatforms> and one or more <ATLConfigGroup>.
                auto platformsNode = preloadNode->first_node(ATLXmlTags::ATLPlatformsTag, 0, false);
                auto configGroupNode = preloadNode->first_node(ATLXmlTags::ATLConfigGroupTag, 0, false);
                if (platformsNode && configGroupNode)
                {
                    const char* configGroupName = nullptr;
                    auto platformNode = platformsNode->first_node(ATLXmlTags::PlatformNodeTag, 0, false);
                    while (platformNode)
                    {
                        auto platformAttr = platformNode->first_attribute(ATLXmlTags::ATLNameAttribute, 0, false);
                        if (platformAttr && azstricmp(platformAttr->value(), ATLXmlTags::PlatformName) == 0)
                        {
                            auto configGroupAttr = platformNode->first_attribute(ATLXmlTags::ATLConfigGroupAttribute, 0, false);
                            if (configGroupAttr)
                            {
                                configGroupName = configGroupAttr->value();
                                break;
                            }
                        }

                        platformNode = platformNode->next_sibling(ATLXmlTags::PlatformNodeTag, 0, false);
                    }

                    if (configGroupName)
                    {
                        while (configGroupNode)
                        {
                            auto configGroupAttr = configGroupNode->first_attribute(ATLXmlTags::ATLNameAttribute, 0, false);
                            if (configGroupAttr && azstricmp(configGroupAttr->value(), configGroupName) == 0)
                            {
                                // Found a config group associated with this platform...
                                CATLPreloadRequest::TFileEntryIDs fileEntryIds;

                                auto fileNode = configGroupNode->first_node(nullptr, 0, false);
                                while (fileNode)
                                {
                                    TAudioFileEntryID fileEntryId = m_rFileCacheMgr.TryAddFileCacheEntry(fileNode, dataScope, autoLoad);
                                    if (fileEntryId != INVALID_AUDIO_FILE_ENTRY_ID)
                                    {
                                        fileEntryIds.push_back(fileEntryId);
                                    }

                                    fileNode = fileNode->next_sibling(nullptr, 0, false);
                                }

                                auto it = m_rPreloadRequests.find(preloadRequestId);
                                if (it == m_rPreloadRequests.end())
                                {
                                    auto preloadRequest = azcreate(CATLPreloadRequest, (preloadRequestId, dataScope, autoLoad, fileEntryIds), Audio::AudioSystemAllocator, "ATLPreloadRequest");
                                    m_rPreloadRequests[preloadRequestId] = preloadRequest;

                                #if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
                                    m_pDebugNameStore->AddAudioPreloadRequest(preloadRequestId, preloadRequestName);
                                #endif // INCLUDE_AUDIO_PRODUCTION_CODE
                                }
                                else
                                {
                                    it->second->m_cFileEntryIDs.insert(it->second->m_cFileEntryIDs.end(), fileEntryIds.begin(), fileEntryIds.end());
                                }

                                // No need to continue looking through the config groups...
                                break;
                            }

                            configGroupNode = configGroupNode->next_sibling(ATLXmlTags::ATLConfigGroupTag, 0, false);
                        }
                    }
                }
            }

            preloadNode = preloadNode->next_sibling(ATLXmlTags::ATLPreloadRequestTag, 0, false);
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CATLXmlProcessor::ClearPreloadsData(const EATLDataScope eDataScope)
    {
        for (auto itRemove = m_rPreloadRequests.begin(); itRemove != m_rPreloadRequests.end(); )
        {
            auto const pRequest = itRemove->second;
            if (eDataScope == eADS_ALL || (pRequest->GetDataScope() == eDataScope))
            {
            #if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
                m_pDebugNameStore->RemoveAudioPreloadRequest(pRequest->GetID());
            #endif // INCLUDE_AUDIO_PRODUCTION_CODE

                DeleteAudioPreloadRequest(pRequest);
                itRemove = m_rPreloadRequests.erase(itRemove);
            }
            else
            {
                ++itRemove;
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CATLXmlProcessor::ParseAudioEnvironments(const AZ::rapidxml::xml_node<char>* environmentsXmlRoot, const EATLDataScope dataScope)
    {
        auto environmentNode = environmentsXmlRoot->first_node(ATLXmlTags::ATLEnvironmentTag, 0, false);
        while (environmentNode)
        {
            auto environmentAttr = environmentNode->first_attribute(ATLXmlTags::ATLNameAttribute, 0, false);
            const char* atlEnvironmentName = nullptr;
            if (environmentAttr)
            {
                atlEnvironmentName = environmentAttr->value();
            }
            const auto atlEnvironmentId = AudioStringToID<TAudioEnvironmentID>(atlEnvironmentName);

            if ((atlEnvironmentId != INVALID_AUDIO_ENVIRONMENT_ID) && (m_rEnvironments.find(atlEnvironmentId) == m_rEnvironments.end()))
            {
                CATLAudioEnvironment::TImplPtrVec envImpls;

                auto environmentImplNode = environmentNode->first_node(nullptr, 0, false);
                while (environmentImplNode)
                {
                    IATLEnvironmentImplData* environmentImplData = nullptr;
                    EATLSubsystem receiver = eAS_NONE;

                    if (azstricmp(environmentImplNode->name(), ATLXmlTags::ATLEnvironmentRequestTag) == 0)
                    {
                        environmentImplData = NewAudioEnvironmentImplDataInternal(environmentImplNode);
                        receiver = eAS_ATL_INTERNAL;
                    }
                    else
                    {
                        AudioSystemImplementationRequestBus::BroadcastResult(environmentImplData, &AudioSystemImplementationRequestBus::Events::NewAudioEnvironmentImplData, environmentImplNode);
                        receiver = eAS_AUDIO_SYSTEM_IMPLEMENTATION;
                    }

                    if (environmentImplData)
                    {
                        auto environmentImpl = azcreate(CATLEnvironmentImpl, (receiver, environmentImplData), Audio::AudioSystemAllocator, "ATLEnvironmentImpl");
                        envImpls.push_back(environmentImpl);
                    }

                    environmentImplNode = environmentImplNode->next_sibling(nullptr, 0, false);
                }

                if (!envImpls.empty())
                {
                    auto newEnvironment = azcreate(CATLAudioEnvironment, (atlEnvironmentId, dataScope, envImpls), Audio::AudioSystemAllocator, "ATLAudioEnvironment");
                    m_rEnvironments[atlEnvironmentId] = newEnvironment;

                #if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
                    m_pDebugNameStore->AddAudioEnvironment(atlEnvironmentId, atlEnvironmentName);
                #endif // INCLUDE_AUDIO_PRODUCTION_CODE
                }
            }

            environmentNode = environmentNode->next_sibling(ATLXmlTags::ATLEnvironmentTag, 0, false);
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CATLXmlProcessor::ParseAudioTriggers(const AZ::rapidxml::xml_node<char>* triggersXmlRoot, const EATLDataScope dataScope)
    {
        auto triggerNode = triggersXmlRoot->first_node(ATLXmlTags::ATLTriggerTag, 0, false);
        while (triggerNode)
        {
            auto triggerAttr = triggerNode->first_attribute(ATLXmlTags::ATLNameAttribute, 0, false);
            const char* atlTriggerName = nullptr;
            if (triggerAttr)
            {
                atlTriggerName = triggerAttr->value();
            }
            const auto atlTriggerId = AudioStringToID<TAudioControlID>(atlTriggerName);

            if ((atlTriggerId != INVALID_AUDIO_CONTROL_ID) && (m_rTriggers.find(atlTriggerId) == m_rTriggers.end()))
            {
                CATLTrigger::TImplPtrVec triggerImpls;

                auto triggerImplNode = triggerNode->first_node(nullptr, 0, false);
                while (triggerImplNode)
                {
                    IATLTriggerImplData* triggerImplData = nullptr;
                    EATLSubsystem receiver = eAS_NONE;

                    if (azstricmp(triggerImplNode->name(), ATLXmlTags::ATLTriggerRequestTag) == 0)
                    {
                        triggerImplData = NewAudioTriggerImplDataInternal(triggerImplNode);
                        receiver = eAS_ATL_INTERNAL;
                    }
                    else
                    {
                        AudioSystemImplementationRequestBus::BroadcastResult(triggerImplData, &AudioSystemImplementationRequestBus::Events::NewAudioTriggerImplData, triggerImplNode);
                        receiver = eAS_AUDIO_SYSTEM_IMPLEMENTATION;
                    }

                    if (triggerImplData)
                    {
                        auto triggerImpl = azcreate(CATLTriggerImpl, (++m_nTriggerImplIDCounter, atlTriggerId, receiver, triggerImplData), Audio::AudioSystemAllocator, "ATLTriggerImpl");
                        triggerImpls.push_back(triggerImpl);
                    }

                    triggerImplNode = triggerImplNode->next_sibling(nullptr, 0, false);
                }

                if (!triggerImpls.empty())
                {
                    auto newTrigger = azcreate(CATLTrigger, (atlTriggerId, dataScope, triggerImpls), Audio::AudioSystemAllocator, "ATLTrigger");
                    m_rTriggers[atlTriggerId] = newTrigger;

                #if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
                    m_pDebugNameStore->AddAudioTrigger(atlTriggerId, atlTriggerName);
                #endif // INCLUDE_AUDIO_PRODUCTION_CODE
                }
            }

            triggerNode = triggerNode->next_sibling(ATLXmlTags::ATLTriggerTag, 0, false);
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CATLXmlProcessor::ParseAudioSwitches(const AZ::rapidxml::xml_node<char>* switchesXmlRoot, const EATLDataScope dataScope)
    {
        auto switchNode = switchesXmlRoot->first_node(ATLXmlTags::ATLSwitchTag, 0, false);
        while (switchNode)
        {
            auto switchAttr = switchNode->first_attribute(ATLXmlTags::ATLNameAttribute, 0, false);
            const char* atlSwitchName = nullptr;
            if (switchAttr)
            {
                atlSwitchName = switchAttr->value();
            }
            const auto atlSwitchId = AudioStringToID<TAudioControlID>(atlSwitchName);

            if ((atlSwitchId != INVALID_AUDIO_CONTROL_ID) && (m_rSwitches.find(atlSwitchId) == m_rSwitches.end()))
            {
                auto newSwitch = azcreate(CATLSwitch, (atlSwitchId, dataScope), Audio::AudioSystemAllocator, "ATLSwitch");

            #if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
                m_pDebugNameStore->AddAudioSwitch(atlSwitchId, atlSwitchName);
            #endif // INCLUDE_AUDIO_PRODUCTION_CODE

                auto stateNode = switchNode->first_node(ATLXmlTags::ATLSwitchStateTag, 0, false);
                while (stateNode)
                {
                    auto stateAttr = stateNode->first_attribute(ATLXmlTags::ATLNameAttribute, 0, false);
                    const char* atlStateName = nullptr;
                    if (stateAttr)
                    {
                        atlStateName = stateAttr->value();
                    }
                    const auto atlStateId = AudioStringToID<TAudioSwitchStateID>(atlStateName);

                    if (atlStateId != INVALID_AUDIO_SWITCH_STATE_ID)
                    {
                        CATLSwitchState::TImplPtrVec switchStateImplVec;
                        auto stateImplNode = stateNode->first_node(nullptr, 0, false);
                        while (stateImplNode)
                        {
                            IATLSwitchStateImplData* newSwitchStateImplData = nullptr;
                            EATLSubsystem receiver = eAS_NONE;
                            const char* stateImplTag = stateImplNode->name();
                            if (azstricmp(stateImplTag, ATLXmlTags::ATLSwitchRequestTag) == 0)
                            {
                                newSwitchStateImplData = NewAudioSwitchStateImplDataInternal(stateImplNode);
                                receiver = eAS_ATL_INTERNAL;
                            }
                            else
                            {
                                AudioSystemImplementationRequestBus::BroadcastResult(newSwitchStateImplData, &AudioSystemImplementationRequestBus::Events::NewAudioSwitchStateImplData, stateImplNode);
                                receiver = eAS_AUDIO_SYSTEM_IMPLEMENTATION;
                            }

                            if (newSwitchStateImplData)
                            {
                                auto switchStateImpl = azcreate(CATLSwitchStateImpl, (receiver, newSwitchStateImplData), Audio::AudioSystemAllocator, "ATLSwitchStateImpl");
                                switchStateImplVec.push_back(switchStateImpl);
                            }

                            stateImplNode = stateImplNode->next_sibling(nullptr, 0, false);
                        }

                        auto newState = azcreate(CATLSwitchState, (atlSwitchId, atlStateId, switchStateImplVec), Audio::AudioSystemAllocator, "ATLSwitchState");
                        newSwitch->cStates[atlStateId] = newState;

                    #if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
                        m_pDebugNameStore->AddAudioSwitchState(atlSwitchId, atlStateId, atlStateName);
                    #endif // INCLUDE_AUDIO_PRODUCTION_CODE
                    }

                    stateNode = stateNode->next_sibling(ATLXmlTags::ATLSwitchStateTag, 0, false);
                }

                m_rSwitches[atlSwitchId] = newSwitch;
            }

            switchNode = switchNode->next_sibling(ATLXmlTags::ATLSwitchTag, 0, false);
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CATLXmlProcessor::ParseAudioRtpcs(const AZ::rapidxml::xml_node<char>* rtpcsXmlRoot, const EATLDataScope dataScope)
    {
        auto rtpcNode = rtpcsXmlRoot->first_node(ATLXmlTags::ATLRtpcTag, 0, false);
        while (rtpcNode)
        {
            auto rtpcAttr = rtpcNode->first_attribute(ATLXmlTags::ATLNameAttribute, 0, false);
            const char* atlRtpcName = rtpcAttr->value();
            const auto atlRtpcId = AudioStringToID<TAudioControlID>(atlRtpcName);

            if ((atlRtpcId != INVALID_AUDIO_CONTROL_ID) && (m_rRtpcs.find(atlRtpcId) == m_rRtpcs.end()))
            {
                CATLRtpc::TImplPtrVec rtpcImpls;

                auto rtpcImplNode = rtpcNode->first_node(nullptr, 0, false);
                while (rtpcImplNode)
                {
                    IATLRtpcImplData* rtpcImplData = nullptr;
                    EATLSubsystem receiver = eAS_NONE;

                    if (azstricmp(rtpcImplNode->name(), ATLXmlTags::ATLRtpcRequestTag) == 0)
                    {
                        rtpcImplData = NewAudioRtpcImplDataInternal(rtpcImplNode);
                        receiver = eAS_ATL_INTERNAL;
                    }
                    else
                    {
                        AudioSystemImplementationRequestBus::BroadcastResult(rtpcImplData, &AudioSystemImplementationRequestBus::Events::NewAudioRtpcImplData, rtpcImplNode);
                        receiver = eAS_AUDIO_SYSTEM_IMPLEMENTATION;
                    }

                    if (rtpcImplData)
                    {
                        auto rtpcImpl = azcreate(CATLRtpcImpl, (receiver, rtpcImplData), Audio::AudioSystemAllocator, "ATLRtpcImpl");
                        rtpcImpls.push_back(rtpcImpl);
                    }

                    rtpcImplNode = rtpcImplNode->next_sibling(nullptr, 0, false);
                }

                if (!rtpcImpls.empty())
                {
                    auto newRtpc = azcreate(CATLRtpc, (atlRtpcId, dataScope, rtpcImpls), Audio::AudioSystemAllocator, "ATLRtpc");
                    m_rRtpcs[atlRtpcId] = newRtpc;

                #if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
                    m_pDebugNameStore->AddAudioRtpc(atlRtpcId, atlRtpcName);
                #endif // INCLUDE_AUDIO_PRODUCTION_CODE
                }
            }

            rtpcNode = rtpcNode->next_sibling(ATLXmlTags::ATLRtpcTag, 0, false);
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    IATLTriggerImplData* CATLXmlProcessor::NewAudioTriggerImplDataInternal(const AZ::rapidxml::xml_node<char>* triggerXmlRoot)
    {
        return nullptr;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    IATLRtpcImplData* CATLXmlProcessor::NewAudioRtpcImplDataInternal(const AZ::rapidxml::xml_node<char>* rtpcXmlRoot)
    {
        return nullptr;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    IATLSwitchStateImplData* CATLXmlProcessor::NewAudioSwitchStateImplDataInternal(const AZ::rapidxml::xml_node<char>* switchXmlRoot)
    {
        SATLSwitchStateImplData_internal* switchStateImpl = nullptr;
        auto switchNameAttr = switchXmlRoot->first_attribute(ATLXmlTags::ATLNameAttribute, 0, false);
        if (switchNameAttr)
        {
            const char* internalSwitchName = switchNameAttr->value();

            auto valueNode = switchXmlRoot->first_node(ATLXmlTags::ATLValueTag, 0, false);
            if (valueNode)
            {
                auto stateNameAttr = valueNode->first_attribute(ATLXmlTags::ATLNameAttribute, 0, false);
                if (stateNameAttr)
                {
                    const char* internalStateName = stateNameAttr->value();

                    const auto internalSwitchId = AudioStringToID<TAudioControlID>(internalSwitchName);
                    const auto internalStateId = AudioStringToID<TAudioSwitchStateID>(internalStateName);

                    if (internalSwitchId != INVALID_AUDIO_CONTROL_ID && internalStateId != INVALID_AUDIO_SWITCH_STATE_ID)
                    {
                        switchStateImpl = azcreate(SATLSwitchStateImplData_internal, (internalSwitchId, internalStateId), Audio::AudioSystemAllocator, "ATLSwitchStateImplData_internal");
                    }
                }
            }
        }

        return switchStateImpl;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    IATLEnvironmentImplData* CATLXmlProcessor::NewAudioEnvironmentImplDataInternal(const AZ::rapidxml::xml_node<char>* environmentXmlRoot)
    {
        return nullptr;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CATLXmlProcessor::DeleteAudioTrigger(CATLTrigger* const pOldTrigger)
    {
        if (pOldTrigger)
        {
            for (auto const triggerImpl : pOldTrigger->m_cImplPtrs)
            {
                if (triggerImpl->GetReceiver() == eAS_ATL_INTERNAL)
                {
                    azdestroy(triggerImpl->m_pImplData, Audio::AudioSystemAllocator);
                }
                else
                {
                    AudioSystemImplementationRequestBus::Broadcast(&AudioSystemImplementationRequestBus::Events::DeleteAudioTriggerImplData, triggerImpl->m_pImplData);
                }

                azdestroy(triggerImpl, Audio::AudioSystemAllocator);
            }

            azdestroy(pOldTrigger, Audio::AudioSystemAllocator);
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CATLXmlProcessor::DeleteAudioRtpc(CATLRtpc* const pOldRtpc)
    {
        if (pOldRtpc)
        {
            for (auto const rtpcImpl : pOldRtpc->m_cImplPtrs)
            {
                if (rtpcImpl->GetReceiver() == eAS_ATL_INTERNAL)
                {
                    azdestroy(rtpcImpl->m_pImplData, Audio::AudioSystemAllocator);
                }
                else
                {
                    AudioSystemImplementationRequestBus::Broadcast(&AudioSystemImplementationRequestBus::Events::DeleteAudioRtpcImplData, rtpcImpl->m_pImplData);
                }

                azdestroy(rtpcImpl, Audio::AudioSystemAllocator);
            }

            azdestroy(pOldRtpc, Audio::AudioSystemAllocator);
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CATLXmlProcessor::DeleteAudioSwitch(CATLSwitch* const pOldSwitch)
    {
        if (pOldSwitch)
        {
            for (auto& statePair : pOldSwitch->cStates)
            {
                auto switchState = statePair.second;
                if (switchState)
                {
                    for (auto const stateImpl : switchState->m_cImplPtrs)
                    {
                        if (stateImpl->GetReceiver() == eAS_ATL_INTERNAL)
                        {
                            azdestroy(stateImpl->m_pImplData, Audio::AudioSystemAllocator);
                        }
                        else
                        {
                            AudioSystemImplementationRequestBus::Broadcast(&AudioSystemImplementationRequestBus::Events::DeleteAudioSwitchStateImplData, stateImpl->m_pImplData);
                        }

                        azdestroy(stateImpl, Audio::AudioSystemAllocator);
                    }

                    azdestroy(switchState, Audio::AudioSystemAllocator);
                }
            }

            azdestroy(pOldSwitch, Audio::AudioSystemAllocator);
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CATLXmlProcessor::DeleteAudioPreloadRequest(CATLPreloadRequest* const pOldPreloadRequest)
    {
        if (pOldPreloadRequest)
        {
            const EATLDataScope eScope = pOldPreloadRequest->GetDataScope();
            for (auto preloadFileId : pOldPreloadRequest->m_cFileEntryIDs)
            {
                m_rFileCacheMgr.TryRemoveFileCacheEntry(preloadFileId, eScope);
            }

            azdestroy(pOldPreloadRequest, Audio::AudioSystemAllocator);
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CATLXmlProcessor::DeleteAudioEnvironment(CATLAudioEnvironment* const pOldEnvironment)
    {
        if (pOldEnvironment)
        {
            for (auto const environmentImpl : pOldEnvironment->m_cImplPtrs)
            {
                if (environmentImpl->GetReceiver() == eAS_ATL_INTERNAL)
                {
                    azdestroy(environmentImpl->m_pImplData, Audio::AudioSystemAllocator);
                }
                else
                {
                    AudioSystemImplementationRequestBus::Broadcast(&AudioSystemImplementationRequestBus::Events::DeleteAudioEnvironmentImplData, environmentImpl->m_pImplData);
                }

                azdestroy(environmentImpl, Audio::AudioSystemAllocator);
            }

            azdestroy(pOldEnvironment, Audio::AudioSystemAllocator);
        }
    }



    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //  SATLSharedData
    ///////////////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    SATLSharedData::SATLSharedData()
    {
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    SATLSharedData::~SATLSharedData()
    {
    }



#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioEventManager::DrawDebugInfo(IRenderAuxGeom& rAuxGeom, float fPosX, float fPosY) const
    {
        static float const fHeaderColor[4] = { 1.0f, 1.0f, 1.0f, 0.9f };
        static float const fItemPlayingColor[4] = { 0.3f, 0.6f, 0.3f, 0.9f };
        static float const fItemLoadingColor[4] = { 0.9f, 0.2f, 0.2f, 0.9f };
        static float const fItemOtherColor[4] = { 0.8f, 0.8f, 0.8f, 0.9f };
        static float const fNoImplColor[4] = { 1.0f, 0.6f, 0.6f, 0.9f };

        rAuxGeom.Draw2dLabel(fPosX, fPosY, 1.6f, fHeaderColor, false, "Audio Events [%zu]", m_cActiveAudioEvents.size());
        fPosX += 20.0f;
        fPosY += 17.0f;

        AZStd::string triggerFilter(g_audioCVars.m_pAudioTriggersDebugFilter->GetString());
        AZStd::to_lower(triggerFilter.begin(), triggerFilter.end());

        for (auto& audioEventPair : m_cActiveAudioEvents)
        {
            auto const atlEvent = audioEventPair.second;

            AZStd::string triggerName(m_pDebugNameStore->LookupAudioTriggerName(atlEvent->m_nTriggerID));
            AZStd::to_lower(triggerName.begin(), triggerName.end());

            if (AudioDebugDrawFilter(triggerName, triggerFilter))
            {
                const float* pColor = fItemOtherColor;

                if (atlEvent->IsPlaying())
                {
                    pColor = fItemPlayingColor;
                }
                else if (atlEvent->m_audioEventState == eAES_LOADING)
                {
                    pColor = fItemLoadingColor;
                }

                rAuxGeom.Draw2dLabel(fPosX, fPosY, 1.6f,
                    pColor,
                    false,
                    "%s (%llu): %s (%llu)",
                    m_pDebugNameStore->LookupAudioObjectName(atlEvent->m_nObjectID),
                    atlEvent->m_nObjectID,
                    triggerName.c_str(),
                    atlEvent->GetID());

                fPosY += 16.0f;
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioEventManager::SetDebugNameStore(const CATLDebugNameStore* const pDebugNameStore)
    {
        m_pDebugNameStore = pDebugNameStore;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    bool CAudioObjectManager::ReserveID(TAudioObjectID& rAudioObjectID, const char* const sAudioObjectName)
    {
        CATLAudioObject* const pNewObject  = GetInstance();

        bool bSuccess = false;
        rAudioObjectID = INVALID_AUDIO_OBJECT_ID;

        if (pNewObject)
        {
            EAudioRequestStatus eImplResult = eARS_FAILURE;
            AudioSystemImplementationRequestBus::BroadcastResult(eImplResult, &AudioSystemImplementationRequestBus::Events::RegisterAudioObject, pNewObject->GetImplDataPtr(), sAudioObjectName);

            if (eImplResult == eARS_SUCCESS)
            {
                pNewObject->IncrementRefCount();
                rAudioObjectID = pNewObject->GetID();
                m_cAudioObjects.emplace(rAudioObjectID, pNewObject);
                bSuccess = true;
            }
            else
            {
                ReleaseInstance(pNewObject);
                bSuccess = false;
            }
        }

        return bSuccess;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    size_t CAudioObjectManager::GetNumAudioObjects() const
    {
        return m_cAudioObjects.size();
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    size_t CAudioObjectManager::GetNumActiveAudioObjects() const
    {
        size_t nNumActiveAudioObjects = 0;

        for (auto& audioObjectPair : m_cAudioObjects)
        {
            auto const audioObject = audioObjectPair.second;

            if (HasActiveEvents(audioObject))
            {
                ++nNumActiveAudioObjects;
            }
        }

        return nNumActiveAudioObjects;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioObjectManager::DrawPerObjectDebugInfo(IRenderAuxGeom& rAuxGeom, const AZ::Vector3& rListenerPos) const
    {
        AZStd::string audioObjectFilter(g_audioCVars.m_pAudioObjectsDebugFilter->GetString());
        AZStd::to_lower(audioObjectFilter.begin(), audioObjectFilter.end());

        for (auto& audioObjectPair : m_cAudioObjects)
        {
            auto const audioObject = audioObjectPair.second;

            AZStd::string audioObjectName(m_pDebugNameStore->LookupAudioObjectName(audioObject->GetID()));
            AZStd::to_lower(audioObjectName.begin(), audioObjectName.end());

            bool bDraw = AudioDebugDrawFilter(audioObjectName, audioObjectFilter);

            bDraw = bDraw && (g_audioCVars.m_nShowActiveAudioObjectsOnly == 0 || HasActiveEvents(audioObject));

            if (bDraw)
            {
                audioObject->DrawDebugInfo(rAuxGeom, rListenerPos, m_pDebugNameStore);
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioObjectManager::DrawDebugInfo(IRenderAuxGeom& rAuxGeom, float fPosX, float fPosY) const
    {
        static const float fHeaderColor[4] = { 1.0f, 1.0f, 1.0f, 0.9f };
        static const float fItemActiveColor[4] = { 0.3f, 0.6f, 0.3f, 0.9f };
        static const float fItemInactiveColor[4] = { 0.8f, 0.8f, 0.8f, 0.9f };
        static const float fOverloadColor[4] = { 1.0f, 0.3f, 0.3f, 0.9f };

        size_t activeObjects = 0;
        size_t aliveObjects = m_cAudioObjects.size();
        size_t remainingObjects = (m_cObjectPool.m_nReserveSize > aliveObjects ? m_cObjectPool.m_nReserveSize - aliveObjects : 0);
        const float fHeaderPosY = fPosY;

        fPosX += 20.0f;
        fPosY += 17.0f;

        AZStd::string audioObjectFilter(g_audioCVars.m_pAudioObjectsDebugFilter->GetString());
        AZStd::to_lower(audioObjectFilter.begin(), audioObjectFilter.end());

        for (auto& audioObjectPair : m_cAudioObjects)
        {
            auto const audioObject = audioObjectPair.second;

            AZStd::string audioObjectName(m_pDebugNameStore->LookupAudioObjectName(audioObject->GetID()));
            AZStd::to_lower(audioObjectName.begin(), audioObjectName.end());

            bool bDraw = AudioDebugDrawFilter(audioObjectName, audioObjectFilter);
            bool hasActiveEvents = HasActiveEvents(audioObject);
            bDraw = bDraw && (g_audioCVars.m_nShowActiveAudioObjectsOnly == 0 || hasActiveEvents);

            if (bDraw)
            {
                const AZ::Vector3 position(audioObject->GetPosition().GetPositionVec());

                rAuxGeom.Draw2dLabel(fPosX, fPosY, 1.6f,
                    hasActiveEvents ? fItemActiveColor : fItemInactiveColor,
                    false,
                    "[%.2f  %.2f  %.2f] (%llu): %s",
                    static_cast<float>(position.GetX()),
                    static_cast<float>(position.GetY()),
                    static_cast<float>(position.GetZ()),
                    audioObject->GetID(),
                    audioObjectName.c_str());

                fPosY += 16.0f;
            }

            if (hasActiveEvents)
            {
                ++activeObjects;
            }
        }

        static const char* headerFormat = "Audio Objects [Active : %3zu | Alive: %3zu | Pool: %3zu | Remaining: %3zu]";
        const bool overloaded = (m_cAudioObjects.size() > m_cObjectPool.m_nReserveSize);

        rAuxGeom.Draw2dLabel(
            fPosX,
            fHeaderPosY,
            1.6f,
            overloaded ? fOverloadColor : fHeaderColor,
            false,
            headerFormat,
            activeObjects,
            aliveObjects,
            m_cObjectPool.m_nReserveSize,
            remainingObjects);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioObjectManager::SetDebugNameStore(CATLDebugNameStore* const pDebugNameStore)
    {
        m_pDebugNameStore = pDebugNameStore;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CATLXmlProcessor::SetDebugNameStore(CATLDebugNameStore* const pDebugNameStore)
    {
        m_pDebugNameStore = pDebugNameStore;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CATLXmlProcessor::SetRootPath(const char* path)
    {
        if (path && path[0] != '\0')
        {
            m_rootPath = path;
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CAudioListenerManager::DrawDebugInfo(IRenderAuxGeom& rAuxGeom) const
    {
        static const AZ::Color audioListenerColor(0.2f, 0.6f, 0.9f, 0.9f);
        static const AZ::Color xAxisColor(1.f, 0.f, 0.f, 0.9f);
        static const AZ::Color yAxisColor(0.f, 1.f, 0.f, 0.9f);
        static const AZ::Color zAxisColor(0.f, 0.f, 1.f, 0.9f);

        if (m_pDefaultListenerObject)
        {
            AZ::Vector3 vListenerPos = m_pDefaultListenerObject->oPosition.GetPositionVec();

            const SAuxGeomRenderFlags previousAuxGeomRenderFlags = rAuxGeom.GetRenderFlags();
            SAuxGeomRenderFlags newAuxGeomRenderFlags(e_Def3DPublicRenderflags | e_AlphaBlended);
            newAuxGeomRenderFlags.SetCullMode(e_CullModeNone);
            rAuxGeom.SetRenderFlags(newAuxGeomRenderFlags);

            // Draw Axes...
            rAuxGeom.DrawLine(AZVec3ToLYVec3(vListenerPos), AZColorToLYColorB(xAxisColor),
                AZVec3ToLYVec3(vListenerPos + m_pDefaultListenerObject->oPosition.m_transform.GetColumn(0)), AZColorToLYColorB(xAxisColor));
            rAuxGeom.DrawLine(AZVec3ToLYVec3(vListenerPos), AZColorToLYColorB(yAxisColor),
                AZVec3ToLYVec3(vListenerPos + m_pDefaultListenerObject->oPosition.m_transform.GetColumn(1)), AZColorToLYColorB(yAxisColor));
            rAuxGeom.DrawLine(AZVec3ToLYVec3(vListenerPos), AZColorToLYColorB(zAxisColor),
                AZVec3ToLYVec3(vListenerPos + m_pDefaultListenerObject->oPosition.m_transform.GetColumn(2)), AZColorToLYColorB(zAxisColor));

            // Draw Sphere...
            const float radius = 0.15f; // 0.15 meters
            rAuxGeom.DrawSphere(AZVec3ToLYVec3(vListenerPos), radius, AZColorToLYColorB(audioListenerColor));

            rAuxGeom.SetRenderFlags(previousAuxGeomRenderFlags);
        }
    }

#endif // INCLUDE_AUDIO_PRODUCTION_CODE

} // namespace Audio
