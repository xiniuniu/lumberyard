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

#include <AzCore/std/parallel/config.h>
#include <AzCore/std/chrono/types.h>

namespace AZStd
{
    /**
     * Semaphore synchronization primitive. This is not
     * part of the \ref C++0x standard.
     */
    class semaphore
    {
    public:
        enum
        {
#ifdef AZ_PLATFORM_MAC
            // OSX does't like it when you initialize a semaphore with INT_MAX. It hangs when doing a signal and then a wait.
            MAXIMUM_COUNT = INT_MAX - 1
#else
            MAXIMUM_COUNT = 0x7fff
#endif // AZ_PLATFORM_MAC
        };

        typedef native_semaphore_handle_type native_handle_type;

        semaphore(unsigned int initialCount = 0, unsigned int maximumCount = MAXIMUM_COUNT);
        semaphore(const char* name, unsigned int initialCount = 0, unsigned int maximumCount = MAXIMUM_COUNT);
        ~semaphore();
        void acquire();
        template <class Rep, class Period>
        bool try_acquire_for(const chrono::duration<Rep, Period>& rel_time);
        //template <class Clock, class Duration>
        //bool try_acquire_until(const chrono::time_point<Clock, Duration>& abs_time);
        void release(unsigned int releaseCount = 1);

        native_handle_type native_handle();
    private:
        semaphore(const semaphore&) {}
        semaphore& operator=(const semaphore&) { return *this; }

        native_semaphore_data_type m_semaphore;

#if !AZ_TRAIT_SEMAPHORE_HAS_NATIVE_MAX_COUNT
        //Unlike Windows, these platforms do not natively support a semaphore max count. So we use a second mutex to implement
        //the producer-consumer pattern which gives us the same behaviour.
        native_semaphore_data_type m_maxCountSemaphore;
#endif
    };
}

#include <AzCore/std/parallel/internal/semaphore_Platform.h>
