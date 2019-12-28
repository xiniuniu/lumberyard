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

#include "CryLegacy_precompiled.h"
#include "XmlSaveGame.h"
#include "XmlSerializeHelper.h"

static const int XML_SAVEGAME_MAX_CHUNK_SIZE = 32767 / 2;

struct CXmlSaveGame::Impl
{
    void GetMemoryUsage(ICrySizer* pSizer) const
    {
        pSizer->Add(*this);
        pSizer->AddObject(sections);
        pSizer->AddObject(root);
        pSizer->AddObject(metadata);
        pSizer->AddObject(outputFile);
    }

    XmlNodeRef root;
    XmlNodeRef metadata;
    string outputFile;

    typedef std::vector<_smart_ptr<CXmlSerializeHelper> > TContexts;
    TContexts sections;
};

CXmlSaveGame::CXmlSaveGame()
    : m_pImpl(new Impl)
    , m_eReason(eSGR_QuickSave)
{
}

CXmlSaveGame::~CXmlSaveGame()
{
}

bool CXmlSaveGame::Init(const char* name)
{
    m_pImpl->outputFile = name;
    m_pImpl->root = GetISystem()->CreateXmlNode("SaveGame", true);
    m_pImpl->metadata = m_pImpl->root->createNode("Metadata");
    m_pImpl->root->addChild(m_pImpl->metadata);
    return true;
}


XmlNodeRef CXmlSaveGame::GetMetadataXmlNode() const
{
    return m_pImpl->metadata;
}

void CXmlSaveGame::AddMetadata(const char* tag, const char* value)
{
    m_pImpl->metadata->setAttr(tag, value);
}

void CXmlSaveGame::AddMetadata(const char* tag, int value)
{
    m_pImpl->metadata->setAttr(tag, value);
}

TSerialize CXmlSaveGame::AddSection(const char* section)
{
    XmlNodeRef node = m_pImpl->root->createNode(section);
    m_pImpl->root->addChild(node);

    _smart_ptr<CXmlSerializeHelper> pSerializer = new CXmlSerializeHelper;
    m_pImpl->sections.push_back(pSerializer);

    return TSerialize(pSerializer->GetWriter(node));
}

uint8* CXmlSaveGame::SetThumbnail(const uint8* imageData, int width, int height, int depth)
{
    return 0;
}

bool CXmlSaveGame::SetThumbnailFromBMP(const char* filename)
{
    return false;
}

bool CXmlSaveGame::Complete(bool successfulSoFar)
{
    if (successfulSoFar)
    {
        successfulSoFar &= Write(m_pImpl->outputFile.c_str(), m_pImpl->root);
    }
    delete this;
    return successfulSoFar;
}

const char* CXmlSaveGame::GetFileName() const
{
    if (m_pImpl.get())
    {
        return m_pImpl.get()->outputFile;
    }
    return NULL;
}

void CXmlSaveGame::GetMemoryUsage(ICrySizer* pSizer) const
{
    pSizer->AddObject(m_pImpl);
}

bool CXmlSaveGame::Write(const char* filename, XmlNodeRef data)
{
    #ifdef WIN32
    CrySetFileAttributes(filename, 0x00000080);      // FILE_ATTRIBUTE_NORMAL
    #endif //WIN32

    // Try opening file for creation first, will also create directory.
    AZ::IO::HandleType fileHandle = AZ::IO::InvalidHandle;
    fileHandle = gEnv->pCryPak->FOpen(filename, "wb");
    if (fileHandle == AZ::IO::InvalidHandle)
    {
        CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Failed to create file %s", filename);
        return false;
    }

    const bool bSavedToFile = data->saveToFile(filename, XML_SAVEGAME_MAX_CHUNK_SIZE, fileHandle);
    if (fileHandle != AZ::IO::InvalidHandle)
    {
        gEnv->pCryPak->FClose(fileHandle);
    }
    return bSavedToFile;
}
