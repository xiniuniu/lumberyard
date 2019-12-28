/*
 * All or portions of this file Copyright(c) Amazon.com, Inc.or its affiliates or
 * its licensors.
 *
 * For complete copyright and license terms please see the LICENSE at the root of this
 * distribution (the "License"). All use of this software is governed by the License,
 *or, if provided, by the license below or the license accompanying this file. Do not
 * remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
 *WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 */
#pragma once

#include <AzCore/IO/GenericStreams.h>
#include <AzCore/Script/ScriptContext.h>

namespace LuaBuilder
{
    // Write a lua function to a stream
    bool LuaDumpToStream(AZ::IO::GenericStream& stream, lua_State* lua);
}
