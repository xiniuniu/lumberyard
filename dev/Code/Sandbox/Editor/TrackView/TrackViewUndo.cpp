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

#include <AzToolsFramework/API/ComponentEntityObjectBus.h>
#include <Maestro/Bus/EditorSequenceComponentBus.h>
#include <AzCore/Component/ComponentBus.h>

#include "TrackViewUndo.h"
#include "TrackViewSequenceManager.h"
#include "TrackViewSequence.h"
#include "TrackViewTrack.h"
#include "Objects/ObjectLayer.h"
#include "Objects/EntityObject.h"
#include <Maestro/Types/AnimNodeType.h>
#include <Maestro/Types/AnimParamType.h>
#include <Maestro/Types/SequenceType.h>
#include <AnimationContext.h>


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CUndoComponentEntityTrackObject::CUndoComponentEntityTrackObject(CTrackViewTrack* track)
{
    AZ_Assert(track, "Expected a valid track");
    if (track)
    {
        m_trackName = track->GetName();
        AZ_Assert(!m_trackName.empty(), "Expected a valid track name");

        CTrackViewAnimNode* animNode = track->GetAnimNode();
        AZ_Assert(animNode, "Expected a valid anim node");
        if (animNode)
        {
            m_trackComponentId = animNode->GetComponentId();
            AZ_Assert(m_trackComponentId != AZ::InvalidComponentId, "Expected a valid track component id");

            CTrackViewSequence* sequence = track->GetSequence();
            AZ_Assert(sequence, "Expected to find the sequence");
            if (sequence)
            {
                m_sequenceId = sequence->GetSequenceComponentEntityId();
                AZ_Assert(m_sequenceId.IsValid(), "Expected a valid sequence id");

                AnimNodeType nodeType = animNode->GetType();
                AZ_Assert(nodeType == AnimNodeType::Component, "Expected a this node to be a AnimNodeType::Component type");
                if (nodeType == AnimNodeType::Component)
                {
                    CTrackViewAnimNode* parentAnimNode = static_cast<CTrackViewAnimNode*>(animNode->GetParentNode());
                    AZ_Assert(parentAnimNode, "Expected a valid parent node");
                    if (parentAnimNode)
                    {
                        m_entityId = parentAnimNode->GetAzEntityId();
                        AZ_Assert(m_entityId.IsValid(), "Expected a valid sequence id");

                        // Store undo info.
                        m_undo = track->GetMemento();

                        CObjectLayer* objectLayer = sequence->GetSequenceObjectLayer();

                        if (objectLayer)
                        {
                            objectLayer->SetModified();
                        }
                    }
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
CTrackViewTrack* CUndoComponentEntityTrackObject::FindTrack(CTrackViewSequence* sequence)
{
    AZ_Assert(sequence, "Expected to find the sequence");

    CTrackViewTrack* track = nullptr;
    CTrackViewTrackBundle allTracks = sequence->GetAllTracks();
    for (int trackIndex = 0; trackIndex < allTracks.GetCount(); trackIndex++)
    {
        CTrackViewTrack* curTrack = allTracks.GetTrack(trackIndex);
        if (curTrack->GetAnimNode() && curTrack->GetAnimNode()->GetComponentId() == m_trackComponentId)
        {
            if (0 == azstricmp(curTrack->GetName(), m_trackName.c_str()))
            {
                CTrackViewAnimNode* parentAnimNode = static_cast<CTrackViewAnimNode*>(curTrack->GetAnimNode()->GetParentNode());
                if (parentAnimNode && parentAnimNode->GetAzEntityId() == m_entityId)
                {
                    track = curTrack;
                    break;
                }
            }
        }
    }
    return track;
}

//////////////////////////////////////////////////////////////////////////
void CUndoComponentEntityTrackObject::Undo(bool bUndo)
{
    CTrackViewSequence* sequence = CTrackViewSequence::LookUpSequenceByEntityId(m_sequenceId);
    AZ_Assert(sequence, "Expected to find the sequence");
    if (sequence)
    {
        CTrackViewTrack* track = FindTrack(sequence);
        AZ_Assert(track, "Expected to find track");
        {
            CTrackViewSequenceNoNotificationContext context(sequence);

            if (bUndo)
            {
                m_redo = track->GetMemento();
            }

            // Undo track state.
            track->RestoreFromMemento(m_undo);

            CObjectLayer* objectLayer = sequence->GetSequenceObjectLayer();

            AZ_Assert(objectLayer, "Expected a valid layer object");
            if (objectLayer && bUndo)
            {
                objectLayer->SetModified();
            }
        }

        if (bUndo)
        {
            sequence->OnKeysChanged();
        }
        else
        {
            sequence->ForceAnimation();
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CUndoComponentEntityTrackObject::Redo()
{
    CTrackViewSequence* sequence = CTrackViewSequence::LookUpSequenceByEntityId(m_sequenceId);
    AZ_Assert(sequence, "Expected to find the sequence");
    if (sequence)
    {
        CTrackViewTrack* track = FindTrack(sequence);
        AZ_Assert(track, "Expected to find track");

        // Redo track state.
        track->RestoreFromMemento(m_redo);

        CObjectLayer* objectLayer = sequence->GetSequenceObjectLayer();

        AZ_Assert(objectLayer, "Expected a valid layer object");
        if (objectLayer)
        {
            objectLayer->SetModified();
        }

        sequence->OnKeysChanged();
    }
}
