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
#include "MemoryManager.h"
#include "platform.h"
#include "CustomMemoryHeap.h"
#include "GeneralMemoryHeap.h"
#include "PageMappingHeap.h"
#include "DefragAllocator.h"


#if defined(AZ_RESTRICTED_PLATFORM)
#undef AZ_RESTRICTED_SECTION
#define MEMORYMANAGER_CPP_SECTION_1 1
#endif

#if defined(WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <Psapi.h>
#endif

#if defined(APPLE)
#include <mach/mach.h>  // task_info
#endif

#if defined(APPLE) || defined(LINUX)
#include <sys/types.h> // required by mman.h
#include <sys/mman.h> //mmap - virtual memory manager
#endif

#ifdef MEMMAN_STATIC
CCryMemoryManager g_memoryManager;
#endif

//////////////////////////////////////////////////////////////////////////
CCryMemoryManager* CCryMemoryManager::GetInstance()
{
#ifdef MEMMAN_STATIC
    return &g_memoryManager;
#else
    static CCryMemoryManager memman;
    return &memman;
#endif
}

//////////////////////////////////////////////////////////////////////////
bool CCryMemoryManager::GetProcessMemInfo(SProcessMemInfo& minfo)
{
    ZeroStruct(minfo);
#if defined(WIN32)

    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx (&mem);

    minfo.TotalPhysicalMemory = mem.ullTotalPhys;
    minfo.FreePhysicalMemory = mem.ullAvailPhys;

    //////////////////////////////////////////////////////////////////////////
    typedef BOOL (WINAPI * GetProcessMemoryInfoProc)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);

    PROCESS_MEMORY_COUNTERS pc;
    ZeroStruct(pc);
    pc.cb = sizeof(pc);
    static HMODULE hPSAPI = LoadLibraryA("psapi.dll");
    if (hPSAPI)
    {
        static GetProcessMemoryInfoProc pGetProcessMemoryInfo = (GetProcessMemoryInfoProc)GetProcAddress(hPSAPI, "GetProcessMemoryInfo");
        if (pGetProcessMemoryInfo)
        {
            if (pGetProcessMemoryInfo(GetCurrentProcess(), &pc, sizeof(pc)))
            {
                minfo.PageFaultCount = pc.PageFaultCount;
                minfo.PeakWorkingSetSize = pc.PeakWorkingSetSize;
                minfo.WorkingSetSize = pc.WorkingSetSize;
                minfo.QuotaPeakPagedPoolUsage = pc.QuotaPeakPagedPoolUsage;
                minfo.QuotaPagedPoolUsage = pc.QuotaPagedPoolUsage;
                minfo.QuotaPeakNonPagedPoolUsage = pc.QuotaPeakNonPagedPoolUsage;
                minfo.QuotaNonPagedPoolUsage = pc.QuotaNonPagedPoolUsage;
                minfo.PagefileUsage = pc.PagefileUsage;
                minfo.PeakPagefileUsage = pc.PeakPagefileUsage;

                return true;
            }
        }
    }
    return false;


#define AZ_RESTRICTED_SECTION_IMPLEMENTED
#elif defined(AZ_RESTRICTED_PLATFORM)
#define AZ_RESTRICTED_SECTION MEMORYMANAGER_CPP_SECTION_1
    #if defined(AZ_PLATFORM_XENIA)
        #include "Xenia/MemoryManager_cpp_xenia.inl"
    #elif defined(AZ_PLATFORM_PROVO)
        #include "Provo/MemoryManager_cpp_provo.inl"
    #elif defined(AZ_PLATFORM_SALEM)
        #include "Salem/MemoryManager_cpp_salem.inl"
    #endif
#endif
#if defined(AZ_RESTRICTED_SECTION_IMPLEMENTED)
#undef AZ_RESTRICTED_SECTION_IMPLEMENTED
#elif defined(LINUX)

    MEMORYSTATUS MemoryStatus;
    GlobalMemoryStatus(&MemoryStatus);
    minfo.PagefileUsage = minfo.PeakPagefileUsage = MemoryStatus.dwTotalPhys - MemoryStatus.dwAvailPhys;

    minfo.FreePhysicalMemory = MemoryStatus.dwAvailPhys;
    minfo.TotalPhysicalMemory = MemoryStatus.dwTotalPhys;

#if defined(ANDROID)
    // On Android, mallinfo() is an EXTREMELY time consuming operation. Nearly 80% CPU time will be spent
    // on this operation once -memreplay is given. Since WorkingSetSize is only used for statistics and
    // debugging purpose, it's simply ignored.
    minfo.WorkingSetSize = 0;
#else
    struct mallinfo meminfo = mallinfo();
    minfo.WorkingSetSize = meminfo.usmblks + meminfo.uordblks;
#endif

#elif defined(APPLE)

    MEMORYSTATUS MemoryStatus;
    GlobalMemoryStatus(&MemoryStatus);
    minfo.PagefileUsage = minfo.PeakPagefileUsage = MemoryStatus.dwTotalPhys - MemoryStatus.dwAvailPhys;

    minfo.FreePhysicalMemory = MemoryStatus.dwAvailPhys;
    minfo.TotalPhysicalMemory = MemoryStatus.dwTotalPhys;

    // Retrieve WorkingSetSize from task_info
    task_basic_info kTaskInfo;
    mach_msg_type_number_t uInfoCount(sizeof(kTaskInfo) / sizeof(natural_t));
    if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&kTaskInfo, &uInfoCount) != 0)
    {
        gEnv->pLog->LogError("task_info failed\n");
        return false;
    }
    minfo.WorkingSetSize = kTaskInfo.resident_size;
#else
    return false;
#endif

    return true;
}

//////////////////////////////////////////////////////////////////////////
CCryMemoryManager::HeapHandle CCryMemoryManager::TraceDefineHeap(const char* heapName, size_t size, const void* pBase)
{
    return 0;
}

//////////////////////////////////////////////////////////////////////////
void CCryMemoryManager::TraceHeapAlloc(HeapHandle heap, void* mem, size_t size, size_t blockSize, const char* sUsage, const char* sNameHint)
{
}

//////////////////////////////////////////////////////////////////////////
void CCryMemoryManager::TraceHeapFree(HeapHandle heap, void* mem, size_t blockSize)
{
}


//////////////////////////////////////////////////////////////////////////
void CCryMemoryManager::TraceHeapSetColor(uint32 color)
{
}

//////////////////////////////////////////////////////////////////////////
void CCryMemoryManager::TraceHeapSetLabel(const char* sLabel)
{
}

//////////////////////////////////////////////////////////////////////////
uint32 CCryMemoryManager::TraceHeapGetColor()
{
    return 0;
}

//////////////////////////////////////////////////////////////////////////
ICustomMemoryHeap* const CCryMemoryManager::CreateCustomMemoryHeapInstance(IMemoryManager::EAllocPolicy const eAllocPolicy)
{
    return new CCustomMemoryHeap(eAllocPolicy);
}

IGeneralMemoryHeap* CCryMemoryManager::CreateGeneralExpandingMemoryHeap(size_t upperLimit, size_t reserveSize, const char* sUsage)
{
    return new CGeneralMemoryHeap(static_cast<UINT_PTR>(0), upperLimit, reserveSize, sUsage);
}

IGeneralMemoryHeap* CCryMemoryManager::CreateGeneralMemoryHeap(void* base, size_t sz, const char* sUsage)
{
    return new CGeneralMemoryHeap(base, sz, sUsage);
}

IMemoryAddressRange* CCryMemoryManager::ReserveAddressRange(size_t capacity, const char* sName)
{
    return new CMemoryAddressRange(capacity, sName);
}

IPageMappingHeap* CCryMemoryManager::CreatePageMappingHeap(size_t addressSpace, const char* sName)
{
    return new CPageMappingHeap(addressSpace, sName);
}

IDefragAllocator* CCryMemoryManager::CreateDefragAllocator()
{
    return new CDefragAllocator();
}

extern "C"
{
    CRYMEMORYMANAGER_API void CryGetIMemoryManagerInterface(void** pIMemoryManager)
    {
        // Static instance of the memory manager
        *pIMemoryManager = CCryMemoryManager::GetInstance();
    }
};
