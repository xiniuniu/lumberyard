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

#include "time_UnixLike.h"

/**
* This file is to be included from the semaphore.h only. It should NOT be included by the user.
*/


namespace AZStd
{
    inline semaphore::semaphore(unsigned int initialCount, unsigned int maximumCount)
    {
        int result = sem_init(&m_semaphore, 0, initialCount);
        (void)result;
        AZ_Assert(result == 0, "sem_init error %s\n", strerror(errno));

        result = sem_init(&m_maxCountSemaphore, 0, maximumCount);
        AZ_Assert(result == 0, "sem_init error for max count semaphore %s\n", strerror(errno));
    }

    inline semaphore::semaphore(const char* name, unsigned int initialCount, unsigned int maximumCount)
    {
        (void)name; // name is used only for debug, if we pass it to the semaphore it will become named semaphore
        // we can share the semaphore and use the name, but the intention of the name is for debugging
        // for named shared semaphores use the OS directly as this is a slot operation
        int result = sem_init(&m_semaphore, 0, initialCount);
        (void)result;
        AZ_Assert(result == 0, "sem_init error %s\n", strerror(errno));

        result = sem_init(&m_maxCountSemaphore, 0, maximumCount);
        AZ_Assert(result == 0, "sem_init error for max count semaphore %s\n", strerror(errno));
    }

    inline semaphore::~semaphore()
    {
        int result = sem_destroy(&m_semaphore);
        (void)result;
        AZ_Assert(result == 0, "sem_destroy error %s\n", strerror(errno));

        result = sem_destroy(&m_maxCountSemaphore);
        AZ_Assert(result == 0, "sem_destroy error for max count semaphore:%s\n", strerror(errno));
    }

    AZ_FORCE_INLINE void semaphore::acquire()
    {
        int result = sem_post(&m_maxCountSemaphore);
        AZ_Assert(result == 0, "post error for max count semaphore:%s\n", strerror(errno));

        result = sem_wait(&m_semaphore);
        (void)result;
        AZ_Assert(result == 0, "sem_wait error %s\n", strerror(errno));
    }

    template <class Rep, class Period>
    AZ_FORCE_INLINE bool semaphore::try_acquire_for(const chrono::duration<Rep, Period>& rel_time)
    {
        timespec ts = Internal::CurrentTimeAndOffset(rel_time);
        int result = 0;
        while ((result = sem_timedwait(&m_semaphore, &ts)) == -1 && errno == EINTR)
        {
            continue; /* Restart if interrupted by handler */
        }

        AZ_Assert(result == 0 || errno == ETIMEDOUT, "sem_timedwait error %s\n", strerror(errno));
        return result == 0;
    }

    AZ_FORCE_INLINE void semaphore::release(unsigned int releaseCount)
    {
        int result = sem_wait(&m_maxCountSemaphore);
        AZ_Assert(result == 0, "sem_wait error for max count semaphore: %s\n", strerror(errno));

        while (releaseCount)
        {
            result = sem_post(&m_semaphore);
            AZ_Assert(result == 0, "sem_post error: %s\n", strerror(errno));

            if (result != 0)
            {
                break; // exit on error
            }

            --releaseCount;
        }
    }

    AZ_FORCE_INLINE semaphore::native_handle_type semaphore::native_handle()
    {
        return &m_semaphore;
    }
}
