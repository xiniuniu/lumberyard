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

#include <AzCore/IO/Streamer/Statistics.h>
#include <AzCore/IO/Streamer/StreamerConfiguration.h>
#include <AzCore/IO/Streamer/StreamStackEntry.h>
#include <AzCore/std/chrono/clocks.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/deque.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace AZ
{
    namespace IO
    {
        class BlockCache
            : public StreamStackEntry
        {
        public:
            BlockCache(u64 cacheSize, u32 blockSize, bool onlyEpilogWrites);
            BlockCache(BlockCache&& rhs) = delete;
            BlockCache(const BlockCache& rhs) = delete;
            ~BlockCache() override = default;

            BlockCache& operator=(BlockCache&& rhs) = delete;
            BlockCache& operator=(const BlockCache& rhs) = delete;

            void PrepareRequest(FileRequest* request) override;
            bool ExecuteRequests() override;
            void FinalizeRequest(FileRequest* request) override;
            s32 GetAvailableRequestSlots() const override;
            void UpdateCompletionEstimates(AZStd::chrono::system_clock::time_point now, AZStd::vector<FileRequest*>& internalPending,
                AZStd::deque<AZStd::shared_ptr<Request>>& externalPending) override;

            void FlushCache(const RequestPath& filePath) override;
            void FlushEntireCache() override;

            void CollectStatistics(AZStd::vector<Statistic>& statistics) const override;
            
            double CalculateHitRatePercentage() const;
            double CalculateCacheableRatePercentage() const;
            s32 CalculateAvailableRequestSlots() const;

        protected:
            static constexpr u32 s_fileNotCached = static_cast<u32>(-1);

            enum class CacheResult
            {
                ReadFromCache, //!< Data was found in the cache and reused.
                CacheMiss, //!< Data wasn't found in the cache and no sub request was queued.
                Queued, //!< A sub request was created or appended and queued for processing on the next entry in the streamer stack.
                Delayed //!< There's no more room to queue a new request, so delay the request until a slot becomes available.
            };

            struct Section
            {
                u8* m_output{ nullptr }; //!< The buffer to write the data to.
                FileRequest* m_parent{ nullptr }; //!< If set, the file request that is split up by this section.
                FileRequest* m_wait{ nullptr }; //!< If set, this contains a "wait"-operation that blocks an operation chain from continuing until this section has been loaded.
                u64 m_readOffset{ 0 }; //!< Offset into the file to start reading from.
                u64 m_readSize{ 0 }; //!< Number of bytes to read from file.
                u64 m_blockOffset{ 0 }; //!< Offset into the cache block to start copying from.
                u64 m_copySize{ 0 }; //!< Number of bytes to copy from cache.
                u32 m_cacheBlockIndex{ s_fileNotCached }; //!< If assigned, the index of the cache block assigned to this section.
                bool m_used{ false }; //!< Whether or not this section is used in further processing.

                // Add the provided section in front of this one.
                void Prefix(const Section& section);
            };

            using TimePoint = AZStd::chrono::system_clock::time_point;

            void ReadFile(FileRequest* request);
            CacheResult ReadFromCache(FileRequest* request, Section& section, const RequestPath& filePath);
            CacheResult ReadFromCache(FileRequest* request, Section& section, u32 cacheBlock);
            CacheResult ServiceFromCache(FileRequest* request, Section& section, const RequestPath& filePath);
            bool SplitRequest(Section& prolog, Section& main, Section& epilog, const RequestPath& filePath, u64 offset, u64 size, u8* buffer) const;

            u8* GetCacheBlockData(u32 index);
            void TouchBlock(u32 index);
            AZ::u32 RecycleOldestBlock(const RequestPath& filePath, u64 offset);
            u32 FindInCache(const RequestPath& filePath, u64 offset) const;
            bool IsCacheBlockInFlight(u32 index) const;
            void ResetCacheEntry(u32 index);
            void ResetCache();

            //! Map of the file requests that are being processed and the sections of the parent requests they'll complete.
            AZStd::unordered_multimap<FileRequest*, Section> m_pendingRequests;
            //! List of file sections that were delayed because the cache was full.
            AZStd::deque<Section> m_delayedSections;
            AverageWindow<u64, double, s_statisticsWindowSize * 2> m_hitRateStatistic;
            AverageWindow<u64, double, s_statisticsWindowSize> m_cacheableStatistic;
            u64 m_cacheSize;
            u32 m_blockSize;
            u32 m_numBlocks;
            s32 m_numInFlightRequests{ 0 };
            AZStd::unique_ptr<u8[]> m_cache;
            //! The file path associated with a cache block.
            AZStd::unique_ptr<RequestPath[]> m_cachedPaths; // Array of m_numBlocks size.
            //! The offset into the file the cache blocks starts at.
            AZStd::unique_ptr<u64[]> m_cachedOffsets; // Array of m_numBlocks size.
            //! The last time the cache block was read from.
            AZStd::unique_ptr<TimePoint[]> m_blockLastTouched; // Array of m_numBlocks size.
            //! The file request that's currently read data into the cache block. If null, the block has been read.
            AZStd::unique_ptr<FileRequest*[]> m_inFlightRequests; // Array of m_numbBlocks size.
            
            //! Whether or not only the epilog ever writes to the cache.
            bool m_onlyEpilogWrites;
        };
    } // namespace IO
} // namespace AZ