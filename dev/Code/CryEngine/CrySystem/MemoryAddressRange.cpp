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
#include "MemoryAddressRange.h"
#include "System.h"

#if defined(APPLE) || defined(LINUX)
#include <sys/mman.h>
#endif

CMemoryAddressRange::CMemoryAddressRange(char* pBaseAddress, size_t nPageSize, size_t nPageCount, const char* sName)
    : m_pBaseAddress(pBaseAddress)
    , m_nPageSize(nPageSize)
    , m_nPageCount(nPageCount)
{
}

void CMemoryAddressRange::Release()
{
    delete this;
}

char* CMemoryAddressRange::GetBaseAddress() const
{
    return m_pBaseAddress;
}

size_t CMemoryAddressRange::GetPageCount() const
{
    return m_nPageCount;
}

size_t CMemoryAddressRange::GetPageSize() const
{
    return m_nPageSize;
}

#if AZ_LEGACY_CRYSYSTEM_TRAIT_MEMADDRESSRANGE_WINDOWS_STYLE

void* CMemoryAddressRange::ReserveSpace(size_t capacity)
{
    return VirtualAlloc(NULL, capacity, MEM_RESERVE, PAGE_READWRITE);
}

size_t CMemoryAddressRange::GetSystemPageSize()
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwPageSize;
}

CMemoryAddressRange::CMemoryAddressRange(size_t capacity, const char* name)
{
    m_nPageSize = GetSystemPageSize();

    size_t algnCap = Align(capacity, m_nPageSize);
    m_pBaseAddress = (char*)ReserveSpace(algnCap);
    m_nPageCount = algnCap / m_nPageSize;
}

CMemoryAddressRange::~CMemoryAddressRange()
{
    VirtualFree(m_pBaseAddress, 0, MEM_RELEASE);
}

void* CMemoryAddressRange::MapPage(size_t pageIdx)
{
    void* pRet = VirtualAlloc(m_pBaseAddress + pageIdx * m_nPageSize, m_nPageSize, MEM_COMMIT, PAGE_READWRITE);
    return pRet;
}

void CMemoryAddressRange::UnmapPage(size_t pageIdx)
{
    char* pBase = m_pBaseAddress + pageIdx * m_nPageSize;

    // Disable warning about only decommitting pages, and not releasing them
#pragma warning( push )
#pragma warning( disable : 6250 )
    VirtualFree(pBase, m_nPageSize, MEM_DECOMMIT);
#pragma warning( pop )
}

#elif defined(AZ_RESTRICTED_PLATFORM)
    #if defined(AZ_PLATFORM_XENIA)
        #include "Xenia/MemoryAddressRange_cpp_xenia.inl"
    #elif defined(AZ_PLATFORM_PROVO)
        #include "Provo/MemoryAddressRange_cpp_provo.inl"
    #elif defined(AZ_PLATFORM_SALEM)
        #include "Salem/MemoryAddressRange_cpp_salem.inl"
    #endif
#elif defined(APPLE) || defined(LINUX)

void* CMemoryAddressRange::ReserveSpace(size_t capacity)
{
    return mmap(0, capacity, PROT_NONE, MAP_ANON | MAP_NORESERVE | MAP_PRIVATE, -1, 0);
}

size_t CMemoryAddressRange::GetSystemPageSize()
{
    return sysconf(_SC_PAGESIZE);
}

CMemoryAddressRange::CMemoryAddressRange(size_t capacity, const char* name)
{
    m_nPageSize = GetSystemPageSize();

    m_allocatedSpace = Align(capacity, m_nPageSize);
    m_pBaseAddress = (char*)ReserveSpace(m_allocatedSpace);
    assert(m_pBaseAddress != MAP_FAILED);
    m_nPageCount = m_allocatedSpace / m_nPageSize;
}

CMemoryAddressRange::~CMemoryAddressRange()
{
    int ret = munmap(m_pBaseAddress, m_allocatedSpace);
    (void) ret;
    assert(ret == 0);
}

void* CMemoryAddressRange::MapPage(size_t pageIdx)
{
    // There is no equivalent to this function with mmap, this
    // happens automatically in the OS. We just return the
    // correct address.
    void* pRet = NULL;
    if (0 == mprotect(m_pBaseAddress + (pageIdx * m_nPageSize), m_nPageSize, PROT_READ | PROT_WRITE))
    {
        pRet = m_pBaseAddress + (pageIdx * m_nPageSize);
    }

    return pRet;
}

void CMemoryAddressRange::UnmapPage(size_t pageIdx)
{
    char* pBase = m_pBaseAddress + pageIdx * m_nPageSize;
    int ret = mprotect(pBase, m_nPageSize, PROT_NONE);
    (void) ret;
    assert(ret == 0);
}


#endif
