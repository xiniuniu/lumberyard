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

#include <ISystem.h>

namespace PakLoadDataUtils
{
    template <class T>
    static bool LoadDataFromFile(T* data, size_t elems, AZ::IO::HandleType& fileHandle, int& nDataSize, EEndian eEndian, int* pSeek = 0)
    {
        ICryPak* ipak = GetISystem()->GetIPak();
        if (pSeek)
        {
            *pSeek = ipak->FTell(fileHandle);
        }

        if (ipak->FRead(data, elems, fileHandle, eEndian) != elems)
        {
            AZ_Assert(false, "Failed to read %zu elements", elems);
            return false;
        }
        nDataSize -= sizeof(T) * elems;
        AZ_Assert(nDataSize >= 0, "nDataSize must be equal or greater than 0");
        return true;
    }

    static bool LoadDataFromFile_Seek(size_t elems, AZ::IO::HandleType& fileHandle, int& nDataSize, EEndian eEndian)
    {
        GetISystem()->GetIPak()->FSeek(fileHandle, elems, SEEK_CUR);
        nDataSize -= elems;
        AZ_Assert(nDataSize >= 0, "nDataSize must be equal or greater than 0");
        return (nDataSize >= 0);
    }

    template <class T>
    static bool LoadDataFromFile(T* data, size_t elems, uint8*& f, int& nDataSize, EEndian eEndian, int* pSeek = 0)
    {
        StepDataCopy(data, f, elems, eEndian);
        nDataSize -= elems * sizeof(T);
        AZ_Assert(nDataSize >= 0, "nDataSize must be equal or greater than 0");
        return (nDataSize >= 0);
    }

    static bool LoadDataFromFile_Seek(size_t elems, uint8*& f, int& nDataSize, EEndian eEndian)
    {
        nDataSize -= elems;
        f += elems;
        AZ_Assert(nDataSize >= 0, "nDataSize must be equal or greater than 0");
        return true;
    }

    static void LoadDataFromFile_FixAlignment(AZ::IO::HandleType& fileHandle, int& nDataSize)
    {
        while (nDataSize & 3)
        {
            int nRes = GetISystem()->GetIPak()->FSeek(fileHandle, 1, SEEK_CUR);
            AZ_Assert(nRes == 0, "FSeek failed for 1 byte");
            AZ_Assert(nDataSize, "nDataSize reached zero" );
            nDataSize--;
        }
        AZ_Assert(nDataSize >= 0, "nDataSize must be equal or greater than 0");
    }

    static void LoadDataFromFile_FixAlignment(uint8*& f, int& nDataSize)
    {
        while (nDataSize & 3)
        {
            AZ_Assert(*f == 222, "Found invalid data in buffer.");
            f++;
            AZ_Assert(nDataSize, "nDataSize reached zero");
            nDataSize--;
        }
        AZ_Assert(nDataSize >= 0, "nDataSize must be equal or greater than 0");
    }

} //namespace PakLoadDataUtils