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

#include <FileCacheManager.h>

#include <AzTest/AzTest.h>
#include <IRenderAuxGeom.h>

namespace Audio
{
    class FileCacheManagerMock
        : public CFileCacheManager
    {
    public:
        MOCK_METHOD1(Init, void(IAudioSystemImplementation* const));
        MOCK_METHOD0(Release, void());
        MOCK_METHOD0(Update, void());

        MOCK_METHOD3(TryAddFileCacheEntry, TAudioFileEntryID(const AZ::rapidxml::xml_node<char>*, const EATLDataScope, const bool));
        MOCK_METHOD2(TryRemoveFileCacheEntry, bool(const TAudioFileEntryID, const EATLDataScope));

        MOCK_METHOD0(UpdateLocalizedFileCacheEntries, void());

        MOCK_METHOD3(DrawDebugInfo, void(IRenderAuxGeom&, const float, const float));

        MOCK_METHOD3(TryLoadRequest, EAudioRequestStatus(const TAudioPreloadRequestID, const bool, const bool));
        MOCK_METHOD1(TruUnloadRequest, EAudioRequestStatus(const TAudioPreloadRequestID));
        MOCK_METHOD1(UnloadDataByScope, EAudioRequestStatus(const EATLDataScope));

    private:

        MOCK_METHOD2(AllocateHeap, void(const size_t, const char* const));
        MOCK_METHOD3(UncacheFileCacheEntryInternal, bool(CATLAudioFileEntry* const, const bool, const bool));
        MOCK_METHOD1(DoesRequestFitInternal, bool(const size_t));
        MOCK_METHOD3(FinishCachingFileInternal, bool(CATLAudioFileEntry* const, AZ::IO::SizeType, AZ::IO::Request::StateType));
        MOCK_METHOD0(UpdatePreloadRequestStatus, void());

        MOCK_METHOD1(FinishAsyncStreamRequest, void(AsyncStreamCompletionState));
        MOCK_METHOD4(OnAsyncStreamFinished, void(AZ::IO::RequestHandle, AZ::IO::SizeType, void*, AZ::IO::Request::StateType));

        MOCK_METHOD1(AllocateMemoryBlockInternal, bool(CATLAudioFileEntry* const));
        MOCK_METHOD1(UncacheFile, void(CATLAudioFileEntry* const));
        MOCK_METHOD0(TryToUncacheFiles, void());

        MOCK_METHOD1(UpdateLocalizedFileEntryData, void(CATLAudioFileEntry* const));
        MOCK_METHOD5(TryCacheFileCacheEntryInternal, bool(CATLAudioFileEntry* const, const TAudioFileEntryID, const bool, const bool, const size_t));
    };

} // namespace Audio
