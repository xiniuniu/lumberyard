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


//      This contains compiled code that is used by other projects in the solution.
//      Because we don't want static DLL dependencies, the CryCommon project is not compiled into a library.
//      Instead, this .cpp file is included in every project which needs it.
//      But we also include it in the CryCommon project (disabled in the build),
//      so that CryCommon can have the same editable settings as other projects.

// Set this to 1 to get an output of some pre-defined compiler symbols.

#if 0

#ifdef _WIN32
#pragma message("_WIN32")
#endif
#ifdef _WIN64
#pragma message("_WIN64")
#endif

#ifdef _M_IX86
#pragma message("_M_IX86")
#endif
#ifdef _M_PPC
#pragma message("_M_PPC")
#endif

#ifdef _DEBUG
#pragma message("_DEBUG")
#endif

#ifdef _DLL
#pragma message("_DLL")
#endif
#ifdef _USRDLL
#pragma message("_USRDLL")
#endif
#ifdef _MT
#pragma message("_MT")
#endif

#endif

#include <AzCore/PlatformIncl.h>

#include "TypeInfo_impl.h"
#include "CryTypeInfo.cpp"
#include "MTPseudoRandom.cpp"
#include "CryStructPack.cpp"
#include "IResourceCompilerHelper.cpp"

#if AZ_TRAIT_LEGACY_CRYCOMMON_USE_WINDOWS_STUBS
    #include "WinBase.cpp"
#endif

#ifdef __GNUC__
// GCC+STLPORT bug workaround, see comment in ISerialize.h.
#include <ISerialize.h>
const uint16 SNetObjectID::InvalidId = ~uint16(0);
#endif

