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
#include <AzCore/std/chrono/types.h>
#include <AzCore/std/containers/array.h>
#include <GridMate/Carrier/SocketDriver.h>
#include <GridMate/Carrier/SocketDriver_Platform.h>

namespace GridMate
{
    namespace Platform
    {
        int Bind(GridMate::SocketDriverCommon::SocketType socket, const sockaddr* addr, size_t addrlen)
        {
            return ::bind(socket, addr, static_cast<socklen_t>(addrlen));
        }

        int GetAddressInfo(const char* node, const char* service, const struct addrinfo* hints, struct addrinfo** res)
        {
            return ::getaddrinfo(node, service, hints, res);
        }

        void FreeAddressInfo(struct addrinfo* res)
        {
            ::freeaddrinfo(res);
        }

        bool IsValidSocket(GridMate::SocketDriverCommon::SocketType s)
        {
            return s >= 0;
        }

        GridMate::SocketDriverCommon::SocketType GetInvalidSocket()
        {
            return -1;
        }

        bool IsSocketError(AZ::s64 result)
        {
            return result < 0;
        }

        int GetSocketError()
        {
            return errno;
        }

        timeval GetTimeValue(AZStd::chrono::microseconds timeValue)
        {
            timeval timeOut;
            timeOut.tv_sec = static_cast<time_t>(timeValue.count() / 1000000);
            timeOut.tv_usec = static_cast<suseconds_t>(timeValue.count() % 1000000);
            return timeOut;
        }

        const char* GetSocketErrorString(int error, SockerErrorBuffer& array)
        {
            return (error == EAI_SYSTEM) ? strerror(GetSocketError()) : gai_strerror(error);
        }

        GridMate::Driver::ResultCode SetSocketBlockingMode(GridMate::SocketDriverCommon::SocketType sock, bool blocking)
        {
            AZ::s64 result = -1;
            AZ::s32 flags = ::fcntl(sock, F_GETFL);
            flags &= ~O_NONBLOCK;
            flags |= (blocking ? 0 : O_NONBLOCK);
            result = ::fcntl(sock, F_SETFL, flags);
            if (IsSocketError(result))
            {
                return GridMate::Driver::EC_SOCKET_MAKE_NONBLOCK;
            }
            return GridMate::Driver::EC_OK;
        }

        GridMate::Driver::ResultCode SetFastSocketClose(GridMate::SocketDriverCommon::SocketType socket, bool isDatagram)
        {
            AZ_UNUSED(socket);
            AZ_UNUSED(isDatagram);
            return GridMate::Driver::EC_OK;
        }

        void PrepareFamilyType(int ft, bool& isIpv6)
        {
            isIpv6 = (ft == GridMate::Driver::BSD_AF_INET6);
        }
    }
}
