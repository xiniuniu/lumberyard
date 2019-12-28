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

#ifndef CRYINCLUDE_CRYCOMMON_STATOBJBUS_H
#define CRYINCLUDE_CRYCOMMON_STATOBJBUS_H

#include <AzCore/EBus/EBus.h>
#include <AzCore/std/containers/unordered_set.h>

struct IStatObj;

//////////////////////////////////////////////////////////////////////////
//
// Ebus support for triggering necessary update of Entity/Object when
// associated static geometry is hot loaded in the level on the fly as
// Asset Pipeline finishes the job.
//
//////////////////////////////////////////////////////////////////////////
class StatObjEvents
    : public AZ::EBusTraits
{
public:
    virtual ~StatObjEvents() = default;

    static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
    typedef IStatObj* BusIdType;

    virtual void OnStatObjReloaded()
    {
    }


};

using StatObjEventBus = AZ::EBus<StatObjEvents>;

//////////////////////////////////////////////////////////////////////////
//
// Ebus support for handling unique IDs between IStatInstGroup instances.
//
//////////////////////////////////////////////////////////////////////////
using StatInstGroupId = int;

class StatInstGroupEvents
    : public AZ::EBusTraits
{
public:
    const static StatInstGroupId s_InvalidStatInstGroupId = -1;

    virtual ~StatInstGroupEvents() = default;

    // AZ::EBusTraits
    static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
    static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
    using MutexType = AZStd::recursive_mutex;

    virtual StatInstGroupId GenerateStatInstGroupId() = 0;
    virtual void ReleaseStatInstGroupId(StatInstGroupId statInstGroupId) = 0;
    virtual void ReleaseStatInstGroupIdSet(const AZStd::unordered_set<StatInstGroupId>& statInstGroupIdSet) = 0;
    virtual void ReserveStatInstGroupIdRange(StatInstGroupId from, StatInstGroupId to) = 0;
};

using StatInstGroupEventBus = AZ::EBus<StatInstGroupEvents>;

//////////////////////////////////////////////////////////////////////////
//
// EBUS support for triggering necessary updates when IStatObj instances
// caches should be updated when 3D Engine events happen during level loads, 
// shutting down the application, and so forth
//
//////////////////////////////////////////////////////////////////////////
class InstanceStatObjEvents
    : public AZ::EBusTraits
{
public:
    virtual ~InstanceStatObjEvents() = default;

    // AZ::EBusTraits
    static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
    static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
    using MutexType = AZStd::recursive_mutex;

    virtual void ReleaseData()
    {
    }
};

using InstanceStatObjEventBus = AZ::EBus<InstanceStatObjEvents>;

#endif // CRYINCLUDE_CRYCOMMON_STATOBJBUS_H
#pragma once