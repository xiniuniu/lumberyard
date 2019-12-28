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

#include <AzCore/Utils/Utils.h>
#include <unistd.h>

namespace AZ
{
    namespace Utils
    {
        GetExecutablePathReturnType GetExecutablePath(char* exeStorageBuffer, size_t exeStorageSize)
        {
            GetExecutablePathReturnType result;
            result.m_pathIncludesFilename = true;

            // http://man7.org/linux/man-pages/man5/proc.5.html
            const ssize_t bytesWritten = readlink("/proc/self/exe", exeStorageBuffer, exeStorageSize);
            if (bytesWritten == -1)
            {
                result.m_pathStored = ExecutablePathResult::GeneralError;
            }
            else if (bytesWritten == exeStorageSize)
            {
                result.m_pathStored = ExecutablePathResult::BufferSizeNotLargeEnough;
            }
            else
            {
                // readlink doesn't null terminate
                exeStorageBuffer[bytesWritten] = '\0';
            }

            return result;
        }

    }
}
