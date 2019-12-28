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

// Description : Implements a seat action for sounds (ie: honk on trucks)

#ifndef CRYINCLUDE_CRYACTION_VEHICLESYSTEM_VEHICLESEATACTIONSOUND_H
#define CRYINCLUDE_CRYACTION_VEHICLESYSTEM_VEHICLESEATACTIONSOUND_H
#pragma once

class CVehicleSeatActionSound
    : public IVehicleSeatAction
{
    IMPLEMENT_VEHICLEOBJECT
public:

    CVehicleSeatActionSound();

    virtual bool Init(IVehicle* pVehicle, IVehicleSeat* pSeat, const CVehicleParams& table);
    virtual void Reset() {}
    virtual void Release() { delete this; }

    virtual void StartUsing(EntityId passengerId) {}
    virtual void ForceUsage() {};
    virtual void StopUsing();
    virtual void OnAction(const TVehicleActionId actionId, int activationMode, float value);

    virtual void Serialize(TSerialize ser, EEntityAspects aspects);
    virtual void PostSerialize(){}
    virtual void Update(const float deltaTime) {};

    virtual void ExecuteTrigger(const Audio::TAudioControlID& controlID);
    virtual void StopTrigger();

    virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params){}

    virtual void GetMemoryUsage(ICrySizer* s) const;

protected:

    IVehicle* m_pVehicle;
    IVehicleHelper* m_pHelper;
    CVehicleSeat* m_pSeat;

    bool m_enabled;

    Audio::TAudioControlID m_nAudioControlIDStart;
    Audio::TAudioControlID m_nAudioControlIDStop;
};

#endif // CRYINCLUDE_CRYACTION_VEHICLESYSTEM_VEHICLESEATACTIONSOUND_H
