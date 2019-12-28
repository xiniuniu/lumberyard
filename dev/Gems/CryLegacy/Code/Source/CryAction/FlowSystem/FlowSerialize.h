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

#ifndef CRYINCLUDE_CRYACTION_FLOWSYSTEM_FLOWSERIALIZE_H
#define CRYINCLUDE_CRYACTION_FLOWSYSTEM_FLOWSERIALIZE_H
#pragma once

#include "IFlowSystem.h"

bool SetFromString(TFlowInputData& value, const char* str);
string ConvertToString(const TFlowInputData& value);
bool SetAttr(XmlNodeRef node, const char* attr, const TFlowInputData& value);

#endif // CRYINCLUDE_CRYACTION_FLOWSYSTEM_FLOWSERIALIZE_H
