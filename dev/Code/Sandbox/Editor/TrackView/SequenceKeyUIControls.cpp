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
#include "TrackViewDialog.h"
#include "TrackViewKeyPropertiesDlg.h"
#include "TrackViewSequence.h"
#include "TrackViewTrack.h"
#include "TrackViewSequenceManager.h"
#include <Maestro/Types/AnimParamType.h>
#include <Maestro/Types/SequenceType.h>

//////////////////////////////////////////////////////////////////////////
class CSequenceKeyUIControls
    : public CTrackViewKeyUIControls
{
public:
    CSmartVariableArray mv_table;
    CSmartVariableEnum<QString> mv_sequence;
    CSmartVariable<bool> mv_overrideTimes;
    CSmartVariable<float> mv_startTime;
    CSmartVariable<float> mv_endTime;

    virtual void OnCreateVars()
    {
        AddVariable(mv_table, "Key Properties");
        AddVariable(mv_table, mv_sequence, "Sequence");
        AddVariable(mv_table, mv_overrideTimes, "Override Start/End Times");
        AddVariable(mv_table, mv_startTime, "Start Time");
        AddVariable(mv_table, mv_endTime, "End Time");
    }
    bool SupportTrackType(const CAnimParamType& paramType, EAnimCurveType trackType, AnimValueType valueType) const
    {
        return paramType == AnimParamType::Sequence;
    }
    virtual bool OnKeySelectionChange(CTrackViewKeyBundle& selectedKeys);
    virtual void OnUIChange(IVariable* pVar, CTrackViewKeyBundle& selectedKeys);

    virtual unsigned int GetPriority() const { return 1; }

    static const GUID& GetClassID()
    {
        // {68030C46-1402-45d1-91B3-8EC6F29C0FED}
        static const GUID guid =
        {
            0x68030c46, 0x1402, 0x45d1, { 0x91, 0xb3, 0x8e, 0xc6, 0xf2, 0x9c, 0xf, 0xed }
        };
        return guid;
    }

private:
    bool m_skipOnUIChange = false;
};

//////////////////////////////////////////////////////////////////////////
bool CSequenceKeyUIControls::OnKeySelectionChange(CTrackViewKeyBundle& selectedKeys)
{
    if (!selectedKeys.AreAllKeysOfSameType())
    {
        return false;
    }

    bool bAssigned = false;
    if (selectedKeys.GetKeyCount() == 1)
    {
        const CTrackViewKeyHandle& keyHandle = selectedKeys.GetKey(0);

        CAnimParamType paramType = keyHandle.GetTrack()->GetParameterType();
        if (paramType == AnimParamType::Sequence)
        {
            std::vector<CBaseObject*> objects;
            CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();

            /////////////////////////////////////////////////////////////////////////////////
            // fill sequence comboBox with available sequences
            mv_sequence.SetEnumList(NULL);

            // Insert '<None>' empty enum
            mv_sequence->AddEnumItem(QObject::tr("<None>"), CTrackViewDialog::GetEntityIdAsString(AZ::EntityId(AZ::EntityId::InvalidEntityId)));
            
            const CTrackViewSequenceManager* pSequenceManager = GetIEditor()->GetSequenceManager();

            for (int i = 0; i < pSequenceManager->GetCount(); ++i)
            {
                CTrackViewSequence* pCurrentSequence = pSequenceManager->GetSequenceByIndex(i);
                bool bNotMe = pCurrentSequence != pSequence;
                bool bNotParent = !bNotMe || pCurrentSequence->IsAncestorOf(pSequence) == false;
                if (bNotMe && bNotParent)
                {
                    string seqName = pCurrentSequence->GetName();

                    QString ownerIdString = CTrackViewDialog::GetEntityIdAsString(pCurrentSequence->GetSequenceComponentEntityId());
                    mv_sequence->AddEnumItem(seqName.c_str(), ownerIdString);
                }
            }

            /////////////////////////////////////////////////////////////////////////////////
            // fill Key Properties with selected key values
            ISequenceKey sequenceKey;
            keyHandle.GetKey(&sequenceKey);
            
            QString entityIdString = CTrackViewDialog::GetEntityIdAsString((sequenceKey.sequenceEntityId));
            mv_sequence = entityIdString;            
           
            mv_overrideTimes = sequenceKey.bOverrideTimes;
            if (!sequenceKey.bOverrideTimes)
            {
                IAnimSequence* pSequence = GetIEditor()->GetSystem()->GetIMovieSystem()->FindSequence(sequenceKey.sequenceEntityId);

                if (pSequence)
                {
                    sequenceKey.fStartTime = pSequence->GetTimeRange().start;
                    sequenceKey.fEndTime = pSequence->GetTimeRange().end;
                }
                else
                {
                    sequenceKey.fStartTime = 0.0f;
                    sequenceKey.fEndTime = 0.0f;
                }
            }

            // Don't trigger an OnUIChange event, since this code is the one
            // updating the start/end ui elements, not the user setting new values.
            m_skipOnUIChange = true;
            mv_startTime = sequenceKey.fStartTime;
            mv_endTime = sequenceKey.fEndTime;
            m_skipOnUIChange = false;

            bAssigned = true;
        }
    }
    return bAssigned;
}

// Called when UI variable changes.
void CSequenceKeyUIControls::OnUIChange(IVariable* pVar, CTrackViewKeyBundle& selectedKeys)
{
    CTrackViewSequence* sequence = GetIEditor()->GetAnimation()->GetSequence();

    if (!sequence || !selectedKeys.AreAllKeysOfSameType() || m_skipOnUIChange)
    {
        return;
    }

    for (unsigned int keyIndex = 0; keyIndex < selectedKeys.GetKeyCount(); ++keyIndex)
    {
        CTrackViewKeyHandle keyHandle = selectedKeys.GetKey(keyIndex);

        CAnimParamType paramType = keyHandle.GetTrack()->GetParameterType();
        if (paramType == AnimParamType::Sequence)
        {
            ISequenceKey sequenceKey;
            keyHandle.GetKey(&sequenceKey);

            AZ::EntityId seqOwnerId;
            if (pVar == mv_sequence.GetVar())
            {
                QString entityIdString = mv_sequence;
                seqOwnerId = AZ::EntityId(static_cast<AZ::u64>(entityIdString.toULongLong()));

                sequenceKey.szSelection.clear();            // clear deprecated legacy data
                sequenceKey.sequenceEntityId = seqOwnerId;
            }

            SyncValue(mv_overrideTimes, sequenceKey.bOverrideTimes, false, pVar);

            IAnimSequence* pSequence = GetIEditor()->GetSystem()->GetIMovieSystem()->FindSequence(seqOwnerId);

            if (!sequenceKey.bOverrideTimes)
            {
                if (pSequence)
                {
                    sequenceKey.fStartTime = pSequence->GetTimeRange().start;
                    sequenceKey.fEndTime = pSequence->GetTimeRange().end;
                }
                else
                {
                    sequenceKey.fStartTime = 0.0f;
                    sequenceKey.fEndTime = 0.0f;
                }
            }
            else
            {
                SyncValue(mv_startTime, sequenceKey.fStartTime, false, pVar);
                SyncValue(mv_endTime, sequenceKey.fEndTime, false, pVar);
            }

            sequenceKey.fDuration = sequenceKey.fEndTime - sequenceKey.fStartTime > 0 ? sequenceKey.fEndTime - sequenceKey.fStartTime : 0.0f;

            IMovieSystem* pMovieSystem = GetIEditor()->GetSystem()->GetIMovieSystem();

            if (pMovieSystem != NULL)
            {
                pMovieSystem->SetStartEndTime(pSequence, sequenceKey.fStartTime, sequenceKey.fEndTime);
            }

            bool isDuringUndo = false;
            AzToolsFramework::ToolsApplicationRequests::Bus::BroadcastResult(isDuringUndo, &AzToolsFramework::ToolsApplicationRequests::Bus::Events::IsDuringUndoRedo);

            if (isDuringUndo)
            {
                keyHandle.SetKey(&sequenceKey);
            }
            else
            {
                AzToolsFramework::ScopedUndoBatch undoBatch("Set Key Value");
                keyHandle.SetKey(&sequenceKey);
                undoBatch.MarkEntityDirty(sequence->GetSequenceComponentEntityId());
            }

        }
    }
}

REGISTER_QT_CLASS_DESC(CSequenceKeyUIControls, "TrackView.KeyUI.Sequence", "TrackViewKeyUI");
