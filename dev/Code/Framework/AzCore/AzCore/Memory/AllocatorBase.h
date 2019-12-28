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

#include <AzCore/Memory/IAllocator.h>

namespace AZ
{
    namespace Debug
    {
        struct AllocationInfo;
    }

    /**
    * AllocatorBase - all AZ-compatible allocators should inherit from this implementation of IAllocator.
    */
    class AllocatorBase : public IAllocator
    {
    protected:
        AllocatorBase(IAllocatorAllocate* allocationSource, const char* name, const char* desc);
        ~AllocatorBase();

    public:

        //---------------------------------------------------------------------
        // IAllocator implementation
        //---------------------------------------------------------------------
        const char* GetName() const override;
        const char* GetDescription() const override;
        IAllocatorAllocate* GetSchema() override;
        Debug::AllocationRecords* GetRecords() final;
        void SetRecords(Debug::AllocationRecords* records) final;
        bool IsReady() const final;
        bool CanBeOverridden() const final;
        void PostCreate() override;
        void PreDestroy() final;
        void SetLazilyCreated(bool lazy) final;
        bool IsLazilyCreated() const final;
        void SetProfilingActive(bool active) final;
        bool IsProfilingActive() const final;
        //---------------------------------------------------------------------

    protected:
        /// Returns the size of a memory allocation after adjusting for tracking.
        AZ_FORCE_INLINE size_t MemorySizeAdjustedUp(size_t byteSize) const
        {
#ifdef AZCORE_ENABLE_MEMORY_TRACKING
            if (m_records && byteSize > 0)
            {
                byteSize += m_memoryGuardSize;
            }
#endif // AZCORE_ENABLE_MEMORY_TRACKING

            return byteSize;
        }

        /// Returns the size of a memory allocation, removing any tracking overhead
        AZ_FORCE_INLINE size_t MemorySizeAdjustedDown(size_t byteSize) const
        {
#ifdef AZCORE_ENABLE_MEMORY_TRACKING
            if (m_records && byteSize > 0)
            {
                byteSize -= m_memoryGuardSize;
            }
#endif // AZCORE_ENABLE_MEMORY_TRACKING

            return byteSize;
        }

        /// Call to disallow this allocator from being overridden.
        /// Only kernel-level allocators where it would be especially problematic for them to be overridden should do this.
        void DisableOverriding();

        /// Call to disallow this allocator from being registered with the AllocatorManager.
        /// Only kernel-level allocators where it would be especially problematic for them to be registered with the AllocatorManager should do this.
        void DisableRegistration();

        /// Records an allocation for profiling.
        void ProfileAllocation(void* ptr, size_t byteSize, size_t alignment, const char* name, const char* fileName, int lineNum, int suppressStackRecord);

        /// Records a deallocation for profiling.
        void ProfileDeallocation(void* ptr, size_t byteSize, size_t alignment, Debug::AllocationInfo* info);

        /// Records a reallocation for profiling.
        void ProfileReallocation(void* ptr, void* newPtr, size_t newSize, size_t newAlignment);

        /// Records a resize for profiling.
        void ProfileResize(void* ptr, size_t newSize);

        /// User allocator should call this function when they run out of memory!
        bool OnOutOfMemory(size_t byteSize, size_t alignment, int flags, const char* name, const char* fileName, int lineNum);

    private:
        const char* m_name = nullptr;
        const char* m_desc = nullptr;
        Debug::AllocationRecords* m_records = nullptr;  // Cached pointer to allocation records. Works together with the MemoryDriller.
        size_t m_memoryGuardSize = 0;
        bool m_isLazilyCreated = false;
        bool m_isProfilingActive = false;
        bool m_isReady = false;
        bool m_canBeOverridden = true;
        bool m_registrationEnabled = true;
    };

    namespace Internal  {
        struct AllocatorDummy{};
    }
}
