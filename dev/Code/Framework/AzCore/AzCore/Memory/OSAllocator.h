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
#ifndef AZCORE_OS_ALLOCATOR_H
#define AZCORE_OS_ALLOCATOR_H

#include <AzCore/Memory/Memory.h>
#include <AzCore/std/allocator.h>

// OS Allocations macros AZ_OS_MALLOC/AZ_OS_FREE
#include <AzCore/Memory/OSAllocator_Platform.h>

namespace AZ
{
    /**
     * OS allocator should be used for direct OS allocations (C heap)
     * It's memory usage is NOT tracked. If you don't create this allocator, it will be implicitly
     * created by the SystemAllocator when it is needed. In addition this allocator is used for
     * debug data (like drillers, memory trackng, etc.)
     */
    class OSAllocator
        : public AllocatorBase
        , public IAllocatorAllocate
    {
    public:
        AZ_TYPE_INFO(OSAllocator, "{9F835EE3-F23C-454E-B4E3-011E2F3C8118}")

        OSAllocator();
        ~OSAllocator();

        /**
         * You can override the default allocation policy.
         */
        struct Descriptor
        {
            Descriptor()
                : m_custom(0) {}
            IAllocatorAllocate*         m_custom;   ///< You can provide our own allocation scheme. If NULL a HeapScheme will be used with the provided Descriptor.
        };

        bool Create(const Descriptor& desc);

        void Destroy();

        //////////////////////////////////////////////////////////////////////////
        // IAllocator
        AllocatorDebugConfig GetDebugConfig() override;

        //////////////////////////////////////////////////////////////////////////
        // IAllocatorAllocate
        virtual pointer_type    Allocate(size_type byteSize, size_type alignment, int flags = 0, const char* name = 0, const char* fileName = 0, int lineNum = 0, unsigned int suppressStackRecord = 0);
        virtual void            DeAllocate(pointer_type ptr, size_type byteSize = 0, size_type alignment = 0);
        virtual size_type       Resize(pointer_type ptr, size_type newSize) { return m_custom ? m_custom->Resize(ptr, newSize) : 0; }
        virtual pointer_type    ReAllocate(pointer_type ptr, size_type newSize, size_type newAlignment)     { return m_custom ? m_custom->ReAllocate(ptr, newSize, newAlignment) : NULL; }
        virtual size_type       AllocationSize(pointer_type ptr) { return m_custom ? m_custom->AllocationSize(ptr) : 0; }

        virtual size_type       NumAllocatedBytes() const       { return m_custom ? m_custom->NumAllocatedBytes() : m_numAllocatedBytes; }
        virtual size_type       Capacity() const                { return m_custom ? m_custom->Capacity() : AZ_CORE_MAX_ALLOCATOR_SIZE; } // custom size or unlimited
        virtual size_type       GetMaxAllocationSize() const    { return m_custom ? m_custom->GetMaxAllocationSize() : AZ_CORE_MAX_ALLOCATOR_SIZE; } // custom size or unlimited
        virtual IAllocatorAllocate*  GetSubAllocator()          { return m_custom ? m_custom : NULL; }
         
    protected:
        OSAllocator(const OSAllocator&);
        OSAllocator& operator=(const OSAllocator&);

        IAllocatorAllocate*     m_custom;
        size_type               m_numAllocatedBytes;
    };

    typedef AZStdAlloc<OSAllocator> OSStdAllocator;
}

#endif // AZCORE_OS_ALLOCATOR_H
#pragma once


