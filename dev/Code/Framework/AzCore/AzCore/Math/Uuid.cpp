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

#include <AzCore/Math/Uuid.h>
#include <AzCore/Math/Sfmt.h>
#include <AzCore/Math/Sha1.h>

#include <AzCore/std/algorithm.h>

#include <AzCore/PlatformIncl.h>

#include <AzCore/Math/Guid.h>

namespace AZ
{
    //=========================================================================
    // CreateNull
    // [4/10/2012]
    //=========================================================================
    Uuid Uuid::CreateNull()
    {
        Uuid id;
        u64* value64 = reinterpret_cast<u64*>(id.data);
        value64[0] = 0;
        value64[1] = 0;
        return id;
    }

    static const char* const s_uuid_digits = "0123456789ABCDEFabcdef";
    static const unsigned char s_uuid_values[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 10, 11, 12, 13, 14, 15, static_cast<unsigned char>(-1) };

    //=========================================================================
    // GetValue
    // [4/10/2012]
    //=========================================================================
    unsigned char GetValue(char c)
    {
        char const* const digits_begin = s_uuid_digits;
        char const* const digits_end = digits_begin + 22;
        const char* d = AZStd::find(digits_begin, digits_end, c);
        return s_uuid_values[d - digits_begin];
    }

    //=========================================================================
    // CreateString
    // [4/10/2012]
    //=========================================================================
    Uuid Uuid::CreateString(const char* string, size_t stringLength)
    {
        if (string == NULL)
        {
            return Uuid::CreateNull();
        }

        const char* current = string;
        size_t      len = stringLength;

        if (len == 0)
        {
            len = strlen(string);
        }

        if (len < 32 || len > 38)
        {
            AZ_Warning("Math", false, "Invalid UUID format %s (must be) {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx} (or without dashes and braces)", string != NULL ? string : "null");
            return Uuid::CreateNull();
        }

        // check open brace
        char c = *current++;
        bool has_open_brace = false;
        if (c == '{')
        {
            c = *current++;
            has_open_brace = true;
        }
        bool has_dashes = false;
        Uuid id;
        for (int i = 0; i < 16; ++i)
        {
            if (i == 4)
            {
                has_dashes = (c == '-');
                if (has_dashes)
                {
                    c = *current++;
                }
            }

            if (has_dashes)
            {
                if (i == 6 || i == 8 || i == 10)
                {
                    if (c == '-')
                    {
                        c = *current++;
                    }
                    else
                    {
                        AZ_Warning("Math", false, "Invalid UUID format %s (must be) {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx} (or without dashes and braces)", string);
                        return Uuid::CreateNull();
                    }
                }
            }

            id.data[i] = GetValue(c);

            c = *current++;

            id.data[i] <<= 4;
            id.data[i] |= GetValue(c);

            c = *current++;
        }

        // check close brace
        if (has_open_brace)
        {
            AZ_Warning("Math", c == '}', "Invalid UUID format %s (must be) {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx} (or without dashes and braces)", string);
        }

        return id;
    }

    //=========================================================================
    // ToString
    // [4/11/2012]
    //=========================================================================
    int
    Uuid::ToString(char* output, int outputSize, bool isBrackets, bool isDashes) const
    {
        int minSize = 32 + 1 /*1 is terminate*/;
        if (isDashes)
        {
            minSize += 4;
        }
        if (isBrackets)
        {
            minSize += 2;
        }

        if (outputSize < minSize)
        {
            return -minSize;
        }

        if (isBrackets)
        {
            *output++ = '{';
        }

        for (int i = 0; i < 16; ++i)
        {
            if (isDashes && (i == 4 || i == 6 || i == 8 || i == 10))
            {
                *output++ = '-';
            }

            unsigned char val = data[i];
            *output++ = s_uuid_digits[(val >> 4)];
            *output++ = s_uuid_digits[(val & 15)];
        }

        if (isBrackets)
        {
            *output++ = '}';
        }

        *output = '\0';
        return minSize;
    }

    //=========================================================================
    // CreateRandom
    // [4/10/2012]
    //=========================================================================
    Uuid Uuid::CreateRandom()
    {
        Uuid id;
        Sfmt& smft = Sfmt::GetInstance();

        AZ::u32* id32 = reinterpret_cast<AZ::u32*>(&id);
        id32[0] = smft.Rand32();
        id32[1] = smft.Rand32();
        id32[2] = smft.Rand32();
        id32[3] = smft.Rand32();

        // variant VAR_RFC_4122
        id.data[8] &= 0xBF;
        id.data[8] |= 0x80;

        // version VER_RANDOM
        id.data[6] &= 0x4F;
        id.data[6] |= 0x40;

        return id;
    }

    //=========================================================================
    // CreateName
    // [4/10/2012]
    //=========================================================================
    Uuid Uuid::CreateName(const char* name)
    {
        return CreateData(name, strlen(name));
    }

    //=========================================================================
    // CreateName
    //=========================================================================
    Uuid Uuid::CreateData(const void* data, size_t dataSize)
    {
        if (data && dataSize > 0)
        {
            Sha1 sha;
            sha.ProcessBytes(data, dataSize);

            AZ::u32 digest[5];
            sha.GetDigest(digest);

            Uuid id;
            for (int i = 0; i < 4; ++i)
            {
                id.data[i * 4 + 0] = ((digest[i] >> 24) & 0xff);
                id.data[i * 4 + 1] = ((digest[i] >> 16) & 0xff);
                id.data[i * 4 + 2] = ((digest[i] >> 8) & 0xff);
                id.data[i * 4 + 3] = ((digest[i] >> 0) & 0xff);
            }

            // variant VAR_RFC_4122
            id.data[8] &= 0xBF;
            id.data[8] |= 0x80;

            // version VER_NAME_SHA1
            id.data[6] &= 0x5F;
            id.data[6] |= 0x50;

            return id;
        }
        return Uuid::CreateNull();
    }

    //=========================================================================
    // IsNull
    // [4/10/2012]
    //=========================================================================
    bool Uuid::IsNull() const
    {
        const AZ::u64* value64 = reinterpret_cast<const AZ::u64*>(data);
        if (value64[0] != 0 || value64[1] != 0)
        {
            return false;
        }
        return true;
    }

    //=========================================================================
    // GetVariant
    // [4/11/2012]
    //=========================================================================
    Uuid::Variant
    Uuid::GetVariant() const
    {
        unsigned char val = data[8];
        if ((val & 0x80) == 0x00)
        {
            return VAR_NCS;
        }
        else if ((val & 0xC0) == 0x80)
        {
            return VAR_RFC_4122;
        }
        else if ((val & 0xE0) == 0xC0)
        {
            return VAR_MICROSOFT;
        }
        else if ((val & 0xE0) == 0xE0)
        {
            return VAR_RESERVED;
        }
        else
        {
            return VAR_UNKNOWN;
        }
    }

    //=========================================================================
    // Version
    // [4/11/2012]
    //=========================================================================
    Uuid::Version
    Uuid::GetVersion() const
    {
        unsigned char val = data[6];
        if ((val & 0xF0) == 0x10)
        {
            return VER_TIME;
        }
        else if ((val & 0xF0) == 0x20)
        {
            return VER_DCE;
        }
        else if ((val & 0xF0) == 0x30)
        {
            return VER_NAME_MD5;
        }
        else if ((val & 0xF0) == 0x40)
        {
            return VER_RANDOM;
        }
        else if ((val & 0xF0) == 0x50)
        {
            return VER_NAME_SHA1;
        }
        else
        {
            return VER_UNKNOWN;
        }
    }

    //=========================================================================
    // operator<
    // [4/11/2012]
    //=========================================================================
    bool Uuid::operator<(const Uuid& rhs) const
    {
        return AZStd::lexicographical_compare(data, data + AZ_ARRAY_SIZE(data), rhs.data, rhs.data + AZ_ARRAY_SIZE(rhs.data));
    }

    //=========================================================================
    // operator>
    // [4/11/2012]
    //=========================================================================
    bool Uuid::operator>(const Uuid& rhs) const
    {
        return AZStd::lexicographical_compare(rhs.data, rhs.data + AZ_ARRAY_SIZE(rhs.data), data, data + AZ_ARRAY_SIZE(data));
    }

    //=========================================================================
    // operator+
    //=========================================================================
    Uuid Uuid::operator + (const Uuid& rhs) const
    {
        u8 mergedData[sizeof(data) * 2];
        memcpy(mergedData, data, sizeof(data));
        memcpy(mergedData + sizeof(data), rhs.data, sizeof(data));
        return CreateData(&mergedData, AZ_ARRAY_SIZE(mergedData));
    }

#if AZ_TRAIT_UUID_SUPPORTS_GUID_CONVERSION
    //=========================================================================
    // Uuid
    // [4/10/2012]
    //=========================================================================
    Uuid::Uuid(const GUID& guid)
    {
        memcpy(data, &guid, 16);
        // make big endian (internal storage type), windows is little endian
        AZStd::endian_swap(*reinterpret_cast<unsigned int*>(&data[0]));
        AZStd::endian_swap(*reinterpret_cast<unsigned short*>(&data[4]));
        AZStd::endian_swap(*reinterpret_cast<unsigned short*>(&data[6]));
    }

    //=========================================================================
    // GUID
    // [4/10/2012]
    //=========================================================================
    Uuid::operator GUID() const
    {
        GUID guid;
        memcpy(&guid, data, 16);
        // make big endian (internal storage type), windows is little endian
        AZStd::endian_swap(guid.Data1);
        AZStd::endian_swap(guid.Data2);
        AZStd::endian_swap(guid.Data3);
        return guid;
    }

#endif // AZ_PLATFORM_WINDOWS
} // namespace AZ
