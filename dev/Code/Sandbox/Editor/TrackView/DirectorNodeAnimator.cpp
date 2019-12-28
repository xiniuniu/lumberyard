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

#include "StdAfx.h"
#include "DirectorNodeAnimator.h"
#include "TrackViewAnimNode.h"
#include "TrackViewTrack.h"
#include "TrackViewSequenceManager.h"
#include "Maestro/Types/SequenceType.h"
#include "Maestro/Types/AnimParamType.h"

////////////////////////////////////////////////////////////////////////////
CDirectorNodeAnimator::CDirectorNodeAnimator(CTrackViewAnimNode* pDirectorNode)
    : m_pDirectorNode(pDirectorNode)
{
    assert(m_pDirectorNode != nullptr);
}

////////////////////////////////////////////////////////////////////////////
void CDirectorNodeAnimator::Animate(CTrackViewAnimNode* pNode, const SAnimContext& ac)
{
    if (!pNode->IsActiveDirector())
    {
        // Don't animate if it's not the sequence track of the active director
        return;
    }

    CTrackViewTrack* pSequenceTrack = pNode->GetTrackForParameter(AnimParamType::Sequence);
    if (pSequenceTrack && !pSequenceTrack->IsDisabled())
    {
        std::vector<CTrackViewSequence*> inactiveSequences;
        std::vector<CTrackViewSequence*> activeSequences;

        // Construct sets of sequences that need to be bound/unbound at this point
        const float time = ac.time;
        const unsigned int numKeys = pSequenceTrack->GetKeyCount();
        for (unsigned int i = 0; i < numKeys; ++i)
        {
            CTrackViewKeyHandle keyHandle = pSequenceTrack->GetKey(i);

            ISequenceKey sequenceKey;
            keyHandle.GetKey(&sequenceKey);

            CTrackViewSequence* pSequence = GetSequenceFromSequenceKey(sequenceKey);

            if (pSequence)
            {
                if (sequenceKey.time <= time)
                {
                    stl::push_back_unique(activeSequences, pSequence);
                    stl::find_and_erase(inactiveSequences, pSequence);
                }
                else
                {
                    if (!stl::find(activeSequences, pSequence))
                    {
                        stl::push_back_unique(inactiveSequences, pSequence);
                    }
                }
            }
        }

        // Unbind must occur before binding, because entities can be referenced in multiple sequences
        for (auto iter = inactiveSequences.begin(); iter != inactiveSequences.end(); ++iter)
        {
            CTrackViewSequence* pSequence = *iter;
            if (pSequence->IsBoundToEditorObjects())
            {
                // No notifications because unbinding would call ForceAnimation again
                CTrackViewSequenceNoNotificationContext context(pSequence);
                pSequence->UnBindFromEditorObjects();
            }
        }

        // Now bind sequences
        for (auto iter = activeSequences.begin(); iter != activeSequences.end(); ++iter)
        {
            CTrackViewSequence* pSequence = *iter;
            if (!pSequence->IsBoundToEditorObjects())
            {
                // No notifications because binding would call ForceAnimation again
                CTrackViewSequenceNoNotificationContext context(pSequence);
                pSequence->BindToEditorObjects();

                // Make sure the sequence is active, harmless to call if the sequences is already
                // active. The sequence may not be active in the Editor if this key was just created.
                pSequence->Activate();
            }
        }

        // Animate sub sequences
        ForEachActiveSequence(ac, pSequenceTrack, true,
            [&](CTrackViewSequence* pSequence, const SAnimContext& newAnimContext)
            {
                pSequence->Animate(newAnimContext);
            },
            [&](CTrackViewSequence* pSequence, const SAnimContext& newAnimContext)
            {
                pSequence->Reset(false);
            }
            );
    }
}

////////////////////////////////////////////////////////////////////////////
void CDirectorNodeAnimator::Render(CTrackViewAnimNode* pNode, const SAnimContext& ac)
{
    if (!pNode->IsActiveDirector())
    {
        // Don't animate if it's not the sequence track of the active director
        return;
    }

    CTrackViewTrack* pSequenceTrack = pNode->GetTrackForParameter(AnimParamType::Sequence);
    if (pSequenceTrack && !pSequenceTrack->IsDisabled())
    {
        // Render sub sequences
        ForEachActiveSequence(ac, pSequenceTrack, false,
            [&](CTrackViewSequence* pSequence, const SAnimContext& newAnimContext)
            {
                pSequence->Render(newAnimContext);
            },
            [&](CTrackViewSequence* pSequence, const SAnimContext& newAnimContext) {}
            );
    }
}

////////////////////////////////////////////////////////////////////////////
void CDirectorNodeAnimator::ForEachActiveSequence(const SAnimContext& ac, CTrackViewTrack* pSequenceTrack,
    const bool bHandleOtherKeys, std::function<void(CTrackViewSequence*, const SAnimContext&)> animateFunction,
    std::function<void(CTrackViewSequence*, const SAnimContext&)> resetFunction)
{
    const float time = ac.time;
    const unsigned int numKeys = pSequenceTrack->GetKeyCount();

    if (bHandleOtherKeys)
    {
        // Reset all non-active sequences first
        for (unsigned int i = 0; i < numKeys; ++i)
        {
            CTrackViewKeyHandle keyHandle = pSequenceTrack->GetKey(i);

            ISequenceKey sequenceKey;
            keyHandle.GetKey(&sequenceKey);

            CTrackViewSequence* pSequence = GetSequenceFromSequenceKey(sequenceKey);

            if (pSequence)
            {
                SAnimContext newAnimContext = ac;
                const float duration = sequenceKey.fDuration;
                const float sequenceTime = ac.time - sequenceKey.time + sequenceKey.fStartTime;
                const float sequenceDuration = duration + sequenceKey.fStartTime;

                newAnimContext.time = std::min(sequenceTime, sequenceDuration);
                const bool bInsideKeyRange = (sequenceTime >= 0.0f) && (sequenceTime <= sequenceDuration);

                if (!bInsideKeyRange)
                {
                    if (ac.forcePlay && sequenceTime >= 0.0f && newAnimContext.time != pSequence->GetTime())
                    {
                        // If forcing animation force previous keys to their last playback position
                        animateFunction(pSequence, newAnimContext);
                    }

                    resetFunction(pSequence, newAnimContext);
                }
            }
        }
    }

    for (unsigned int i = 0; i < numKeys; ++i)
    {
        CTrackViewKeyHandle keyHandle = pSequenceTrack->GetKey(i);

        ISequenceKey sequenceKey;
        keyHandle.GetKey(&sequenceKey);

        CTrackViewSequence* pSequence = GetSequenceFromSequenceKey(sequenceKey);

        if (pSequence)
        {
            SAnimContext newAnimContext = ac;
            const float duration = sequenceKey.fDuration;
            const float sequenceTime = ac.time - sequenceKey.time + sequenceKey.fStartTime;
            const float sequenceDuration = duration + sequenceKey.fStartTime;

            newAnimContext.time = std::min(sequenceTime, sequenceDuration);
            const bool bInsideKeyRange = (sequenceTime >= 0.0f) && (sequenceTime <= sequenceDuration);

            if ((bInsideKeyRange && (newAnimContext.time != pSequence->GetTime() || ac.forcePlay)))
            {
                animateFunction(pSequence, newAnimContext);
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////
void CDirectorNodeAnimator::UnBind(CTrackViewAnimNode* pNode)
{
    const CTrackViewSequenceManager* pSequenceManager = GetIEditor()->GetSequenceManager();

    const unsigned int numSequences = pSequenceManager->GetCount();
    for (unsigned int sequenceIndex = 0; sequenceIndex < numSequences; ++sequenceIndex)
    {
        CTrackViewSequence* pSequence = pSequenceManager->GetSequenceByIndex(sequenceIndex);

        if (pSequence->IsActiveSequence())
        {
            // Don't care about the active sequence
            continue;
        }

        if (pSequence->IsBoundToEditorObjects())
        {
            pSequence->UnBindFromEditorObjects();
        }
    }
}

/*static*/ CTrackViewSequence* CDirectorNodeAnimator::GetSequenceFromSequenceKey(const ISequenceKey& sequenceKey)
{
    CTrackViewSequence* retSequence = nullptr;
    const CTrackViewSequenceManager* sequenceManager = GetIEditor()->GetSequenceManager();

    if (sequenceManager)
    {
        if (sequenceKey.sequenceEntityId.IsValid())
        {            
            retSequence = sequenceManager->GetSequenceByEntityId(sequenceKey.sequenceEntityId);
            AZ_Assert(retSequence, "Null sequence returned when a Sequence Component was expected.");
        }
    }

    return retSequence;
}

