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

#include "Exporter.h"


namespace ExporterLib
{
    void SaveString(const AZStd::string& textToSave, MCore::Stream* file, MCore::Endian::EEndianType targetEndianType)
    {
        MCORE_ASSERT(file);

        // convert endian and write the number of characters that follow
        uint32 numCharacters = static_cast<uint32>(textToSave.size());
        ConvertUnsignedInt(&numCharacters, targetEndianType);
        file->Write(&numCharacters, sizeof(uint32));
        if (textToSave.size() == 0)
        {
            return;
        }

        // write the string in UTF8 format
        file->Write(textToSave.data(), textToSave.size() * sizeof(char));
    }


    uint32 GetStringChunkSize(const AZStd::string& text)
    {
        return static_cast<uint32>(sizeof(uint32) + text.size() * sizeof(char));
    }


    void SaveAzString(const AZStd::string& textToSave, MCore::Stream* file, MCore::Endian::EEndianType targetEndianType)
    {
        MCORE_ASSERT(file);

        // Convert endian and write the number of characters that follow.
        uint32 numCharacters = static_cast<uint32>(textToSave.size());
        ConvertUnsignedInt(&numCharacters, targetEndianType);
        file->Write(&numCharacters, sizeof(uint32));
        if (textToSave.empty())
        {
            return;
        }

        // Write the string in UTF8 format.
        file->Write(textToSave.c_str(), textToSave.size());
    }


    size_t GetAzStringChunkSize(const AZStd::string& text)
    {
        return sizeof(uint32) + text.size();
    }


    uint32 GetFileHighVersion()
    {
        return 1;
    }


    uint32 GetFileLowVersion()
    {
        return 0;
    }


    const char* GetActorExtension(bool includingDot)
    {
        if (includingDot)
        {
            return ".actor";
        }

        return "actor";
    }


    const char* GetSkeletalMotionExtension(bool includingDot)
    {
        if (includingDot)
        {
            return ".motion";
        }

        return "motion";
    }
    

    const char* GetCompilationDate()
    {
        return MCORE_DATE;
    }

} // namespace ExporterLib
