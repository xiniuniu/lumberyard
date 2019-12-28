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

#include <ATLEntities.h>
#include <AzTest/AzTest.h>

namespace Audio
{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
    class ATLDebugNameStoreMock
        : public CATLDebugNameStore
    {
    public:
        MOCK_METHOD1(SyncChanges, void(const CATLDebugNameStore&));

        MOCK_METHOD2(AddAudioObject, void(const TAudioObjectID, const char* const));
        MOCK_METHOD2(AddAudioTrigger, void(const TAudioControlID, const char* const));
        MOCK_METHOD2(AddAudioRtpc, void(const TAudioControlID, const char* const));
        MOCK_METHOD2(AddAudioSwitch, void(const TAudioControlID, const char* const));
        MOCK_METHOD3(AddAudioSwitchState, void(const TAudioControlID, const TAudioSwitchStateID, const char* const));
        MOCK_METHOD2(AddAudioPreloadRequest, void(const TAudioPreloadRequestID, const char* const));
        MOCK_METHOD2(AddAudioEnvironment, void(const TAudioEnvironmentID, const char* const));

        MOCK_METHOD1(RemoveAudioObject, void(const TAudioObjectID));
        MOCK_METHOD1(RemoveAudioTrigger, void(const TAudioControlID));
        MOCK_METHOD1(RemoveAudioRtpc, void(const TAudioControlID));
        MOCK_METHOD1(RemoveAudioSwitch, void(const TAudioControlID));
        MOCK_METHOD2(RemoveAudioSwitchState, void(const TAudioControlID, const TAudioSwitchStateID));
        MOCK_METHOD1(RemoveAudioPreloadRequest, void(const TAudioPreloadRequestID));
        MOCK_METHOD1(RemoveAudioEnvironment, void(const TAudioEnvironmentID));

        MOCK_CONST_METHOD0(AudioObjectsChanged, bool());
        MOCK_CONST_METHOD0(AudioTriggersChanged, bool());
        MOCK_CONST_METHOD0(AudioRtpcsChanged, bool());
        MOCK_CONST_METHOD0(AudioSwitchesChanged, bool());
        MOCK_CONST_METHOD0(AudioPreloadsChanged, bool());
        MOCK_CONST_METHOD0(AudioEnvironmentsChanged, bool());

        MOCK_CONST_METHOD1(LookupAudioObjectName, const char*(const TAudioObjectID));
        MOCK_CONST_METHOD1(LookupAudioTriggerName, const char*(const TAudioControlID));
        MOCK_CONST_METHOD1(LookupAudioRtpcName, const char*(const TAudioControlID));
        MOCK_CONST_METHOD1(LookupAudioSwitchName, const char*(const TAudioControlID));
        MOCK_CONST_METHOD2(LookupAudioSwitchStateName, const char*(const TAudioControlID, const TAudioSwitchStateID));
        MOCK_CONST_METHOD1(LookupAudioPreloadRequestName, const char*(const TAudioPreloadRequestID));
        MOCK_CONST_METHOD1(LookupAudioEnvironmentName, const char*(const TAudioEnvironmentID));
    };
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
} // namespace Audio
