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

#include <AzCore/base.h>
#include <AzCore/IO/StreamerRequest.h>
#include <AzCore/IO/Streamer/FileRange.h>
#include <AzCore/IO/Streamer/Statistics.h>
#include <AzCore/std/chrono/clocks.h>
#include <AzCore/std/containers/deque.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/string/string.h>

namespace AZ
{
    namespace IO
    {
        class FileRequest;
        class StreamerContext;

        //! The StreamStack is a stack of elements that serve a specific file IO function such
        //! collecting information from a file, cache read data, decompressing data, etc. The basic
        //! functionality tries perform its task and if it fails pass the request to the next entry
        //! in the stack. For instance if a request isn't cached it passes the request to the next
        //! entry which might read it from disk instead.
        class StreamStackEntry
        {
        public:
            explicit StreamStackEntry(AZStd::string&& name);
            virtual ~StreamStackEntry() = default;

            //! Returns the name that uniquely identifies this entry.
            virtual const AZStd::string& GetName() const;

            //! Set the next entry in the stack or reset it by using a nullptr.
            //! If the next entry has already been set, this will overwrite it.
            virtual void SetNext(AZStd::shared_ptr<StreamStackEntry> next);
            //! Returns the next entry in the stack that handles a request if 
            //! this request can't handle the request, or null if there are no more entries. 
            virtual AZStd::shared_ptr<StreamStackEntry> GetNext() const;

            virtual void SetContext(StreamerContext& context);

            //! Prepares a request to be executed at a later point. This can include splitting up
            //! the request in more fine-grained steps.
            virtual void PrepareRequest(FileRequest* request);
            //! Executes one or more queued requests. This is needed for synchronously executing requests,
            //! but asynchronous requests can already be running from the PrepareRequest call in which case
            //! this call is ignored.
            //! @return True if a request was processed, otherwise false.
            virtual bool ExecuteRequests();
            //! If a request needs to be finalized by a node, the node can register itself with the request
            //! and will receive this callback to complete the request.
            virtual void FinalizeRequest(FileRequest* request);
            //! Gets the number of requests that can still be submitted to the system. If this is positive
            //! more requests can be issued. If it's negative the streamer is saturated and no more requests
            //! should be issued.
            virtual s32 GetAvailableRequestSlots() const;
            //! Updates the estimate of the time the requests will complete. This generally works by bubble
            //! up the estimation and each stack entry adding it's additional overhead if any. When chaining
            //! this call, first call the next entry in the stack before adding the current entry's estimate.
            //! @param now The current time. This is captured once to avoid repeatedly querying the system clock.
            //! @param internalPending The requests that are pending in the stream stack. These are always estimated as coming after the queued requests.
            //!     Requests are assumed to be processed in the order they are queued. Collection is done in reverse order as it needs to collect information
            //!     from the top of the queue to the bottom, but processing happens bottom up.
            //! @param externalPending The requests that haven't been queued yet. These are always estimated as coming after the queued requests.
            virtual void UpdateCompletionEstimates(AZStd::chrono::system_clock::time_point now, AZStd::vector<FileRequest*>& internalPending,
                AZStd::deque<AZStd::shared_ptr<Request>>& externalPending);

            //! Gets the file size of the file at the requested location.
            //! @result The file size if retrieval was successful.
            //! @filePath The path to the file that has its size queried.
            //! @return True if the size could be retrieved, otherwise false.
            virtual bool GetFileSize(u64& result, const RequestPath& filePath);

            //! Flushes the requested file from a cache if it's cached, otherwise does nothing.
            //! @filePath The file to be flushed from the cache.
            virtual void FlushCache(const RequestPath& filePath);
            //! Clears out the entire cache.
            virtual void FlushEntireCache();
            //! Creates a dedicated cache for the provide file at the given range.
            virtual void CreateDedicatedCache(const RequestPath& filePath, const FileRange& range);
            //! Destroys a previously created dedicated cache.
            virtual void DestroyDedicatedCache(const RequestPath& filePath, const FileRange& range);

            //! Collect various statistics on this stack entry. These are for profiling and debugging
            //! purposes only.
            virtual void CollectStatistics(AZStd::vector<Statistic>& statistics) const;

        protected:
            StreamStackEntry() = default;
            
            //! The name that uniquely identifies this entry.
            AZStd::string m_name;
            //! The next entry in the stack
            AZStd::shared_ptr<StreamStackEntry> m_next;
            //! Context information for the entire streaming stack.
            StreamerContext* m_context;
        };
    } // namespace IO
} // namespace AZ