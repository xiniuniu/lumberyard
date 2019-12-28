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
#include <windows.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzCore/IO/SystemFile.h>

namespace AZ
{
    namespace IO
    {

        Result LocalFileIO::Copy(const char* sourceFilePath, const char* destinationFilePath)
        {
            char resolvedSourcePath[AZ_MAX_PATH_LEN];
            ResolvePath(sourceFilePath, resolvedSourcePath, AZ_MAX_PATH_LEN);
            char resolvedDestPath[AZ_MAX_PATH_LEN];
            ResolvePath(destinationFilePath, resolvedDestPath, AZ_MAX_PATH_LEN);

            if (::CopyFileA(resolvedSourcePath, resolvedDestPath, true) == 0)
            {
                return ResultCode::Error;
            }

            return ResultCode::Success;
        }
    } // namespace IO
}//namespace AZ
