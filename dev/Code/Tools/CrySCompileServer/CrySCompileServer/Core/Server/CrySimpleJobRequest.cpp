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

#include "CrySimpleJobRequest.hpp"
#include "CrySimpleServer.hpp"
#include "ShaderList.hpp"

#include <Core/StdTypes.hpp>
#include <Core/Error.hpp>
#include <Core/STLHelper.hpp>
#include <Core/Common.h>
#include <Core/tinyxml/tinyxml.h>
#include <Core/WindowsAPIImplementation.h>

#include <AzCore/Debug/Trace.h>
#include <AzCore/IO/SystemFile.h>

CCrySimpleJobRequest::CCrySimpleJobRequest(EProtocolVersion Version, uint32_t requestIP)
    : CCrySimpleJob(requestIP)
    , m_Version(Version)
{
}

bool CCrySimpleJobRequest::Execute(const TiXmlElement* pElement)
{
    const char* shaderRequest = pElement->Attribute("ShaderRequest");
    if (!shaderRequest)
    {
        State(ECSJS_ERROR_INVALID_SHADERREQUESTLINE);
        CrySimple_ERROR("Missing shader request line");
        return false;
    }

    AZStd::string shaderListFilename;
    if (m_Version >= EPV_V0023)
    {
        const char* project    = pElement->Attribute("Project");
        const char* shaderList = pElement->Attribute("ShaderList");
        if (!project)
        {
            State(ECSJS_ERROR_INVALID_PROJECT);
            CrySimple_ERROR("Missing Project for shader request");
            return false;
        }
        if (!shaderList)
        {
            State(ECSJS_ERROR_INVALID_SHADERLIST);
            CrySimple_ERROR("Missing Shader List for shader request");
            return false;
        }
        
        // NOTE: These attributes were alredy validated.
        AZStd::string platform = pElement->Attribute("Platform");
        AZStd::string compiler = pElement->Attribute("Compiler");
        AZStd::string language = pElement->Attribute("Language");

        shaderListFilename = AZStd::string::format("%s%s-%s-%s/%s", project, platform.c_str(), compiler.c_str(), language.c_str(), shaderList);
    }
    else
    {
        // In previous versions Platform attribute is the shader list filename directly
        shaderListFilename = pElement->Attribute("Platform");
    }
    
    if (shaderListFilename.length() >= AZ_MAX_PATH_LEN)
    {
        State(ECSJS_ERROR);
        CrySimple_ERROR("Shader list filename is too long");
        return false;
    }

    std::string shaderRequestLine(shaderRequest);
    tdEntryVec toks;
    CSTLHelper::Tokenize(toks, shaderRequestLine, ";");
    for (size_t a = 0, s = toks.size(); a < s; a++)
    {
        CShaderList::Instance().Add(shaderListFilename.c_str(), toks[a].c_str());
    }

    State(ECSJS_DONE);

    return true;
}
