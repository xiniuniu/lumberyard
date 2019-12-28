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
#include <AzFramework/Components/BootstrapReaderComponent.h>
#include <AzFramework/AzFramework_Traits_Platform.h>

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>

#include <AzFramework/StringFunc/StringFunc.h>
#include <AzFramework/Application/Application.h>

#include <AzCore/std/string/regex.h>

namespace AzFramework
{
    namespace Platform
    {
        const char* FindAssetsDirectory();
    }

    const char* const CURRENT_PLATFORM = AZ_TRAIT_AZFRAMEWORK_BOOTSTRAP_CFG_CURRENT_PLATFORM;

    BootstrapReaderComponent::BootstrapReaderComponent()
    {
        // Set the default path, for backward compatibility.
        // SimpleAssetReference doesn't have a constructor, and expects paths to be set this way.
        m_configFile.SetAssetPath("bootstrap.cfg");
    }

    bool BootstrapReaderComponent::VersionConverter(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement)
    {
        // Version 1-> 2: Raw string for bootstrap file reference was replaced with a simple asset reference.
        if (classElement.GetVersion() <= 1)
        {
            AZ::Crc32 configFileCRC("configFileName");
            int configFileNameIndex = classElement.FindElement(configFileCRC);
            if (configFileNameIndex != -1)
            {
                AZStd::string configFileName;
                classElement.GetSubElement(configFileNameIndex).GetData(configFileName);

                classElement.RemoveElement(configFileNameIndex);

                AzFramework::SimpleAssetReference<AzFramework::CfgFileAsset> configFileReference;
                configFileReference.SetAssetPath(configFileName.c_str());
                classElement.AddElementWithData(context, "configFile", configFileReference);
            }
        }
        return true;
    }

    void BootstrapReaderComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            AzFramework::SimpleAssetReference<AzFramework::CfgFileAsset>::Register(*serializeContext);

            serializeContext->Class<BootstrapReaderComponent, AZ::Component>()
                ->Version(2, &VersionConverter)
                ->Field("configFile", &BootstrapReaderComponent::m_configFile)
                ;

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<BootstrapReaderComponent>("Bootstrap reader", "Reads, and allows access to fields in, the bootstrap config file.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Engine")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System", 0xc94d118b))
                    ->DataElement(AZ::Edit::UIHandlers::Default, &BootstrapReaderComponent::m_configFile, "Config file", "Bootstrap configuration file.")
                    ;
            }
        }
    }

    void BootstrapReaderComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("BootstrapService", 0x90c22e09));
    }

    void BootstrapReaderComponent::Activate()
    {
        AZStd::string path;

        // Attempt to get the app root to look for bootstrap.cfg, but default to the executable path
        const char* appRoot = nullptr;
        AZ::ComponentApplicationBus::BroadcastResult(appRoot, &AZ::ComponentApplicationRequests::GetAppRoot);
        if (appRoot != nullptr)
        {
            path = AZStd::string(appRoot);
        }
        else
        {
            EBUS_EVENT_RESULT(path, AZ::ComponentApplicationBus, GetExecutableFolder);
        }

        AZ::IO::FileIOBase* fileIO = AZ::IO::FileIOBase::GetInstance();
        if (!fileIO)
        {
            AZ_Error("BootstrapReaderComponent", false, "%s", "AZ::IO::FileIOBase::GetInstance() not set, please ensure application is setting FileIO instance.");
            return;
        }

        AzFramework::StringFunc::Path::Normalize(path);

        if (!path.empty())
        {
            //removing trailing separator
            if (path[path.length() - 1] == AZ_CORRECT_FILESYSTEM_SEPARATOR)
            {
                path = path.substr(0, path.length() - 1);
            }
        }
        else
        {
            path = Platform::FindAssetsDirectory();
        }

        AZStd::string fullPath;
        size_t found = 0;
        do
        {
            if (fileIO->IsDirectory(path.c_str()))
            {
                // if the path is a directory than check whether the bootstrap file exists in that folder
                if (!AzFramework::StringFunc::Path::ConstructFull(path.c_str(), m_configFile.GetAssetPath().c_str(), fullPath, true))
                {
                    return;
                }
            }
            else
            {
                fullPath = path;
            }

            if (fileIO->Exists(fullPath.c_str()))
            {
                AZ::IO::HandleType file = AZ::IO::InvalidHandle;
                if (fileIO->Open(fullPath.c_str(), AZ::IO::OpenMode::ModeRead | AZ::IO::OpenMode::ModeText, file) != AZ::IO::ResultCode::Success)
                {
                    return;
                }

                AZ::u64 actualSize = 0;
                fileIO->Size(file, actualSize);

                if (actualSize == 0)
                {
                    fileIO->Close(file);
                    return;
                }

                m_fileContents.resize_no_construct(actualSize + 1);

                // Reading the entire bootstrap file
                if (fileIO->Read(file, m_fileContents.data(), actualSize) != AZ::IO::ResultCode::Success)
                {
                    break;
                }
                m_fileContents[actualSize] = 0;

                fileIO->Close(file);
                break;
            }

            // Move one folder up if bootstrap is not found
            found = path.rfind(AZ_CORRECT_FILESYSTEM_SEPARATOR);
            if (found != AZStd::string::npos)
            {
                path.erase(found, path.length() - found);
            }
        } while (found != AZStd::string::npos);

        BootstrapReaderRequestBus::Handler::BusConnect();
    }

    void BootstrapReaderComponent::Deactivate()
    {
        BootstrapReaderRequestBus::Handler::BusDisconnect();

        m_fileContents.clear();
        m_keyValueCache.clear();
    }

    void BootstrapReaderComponent::OverrideFileContents(const AZStd::string& newContents)
    {
        m_fileContents = newContents;
        m_keyValueCache.clear();
    }

    bool BootstrapReaderComponent::SearchConfigurationForKey(const AZStd::string& key, bool checkPlatform, AZStd::string& value)
    {
        // Attempt platform override first.
        if (checkPlatform && ReadFieldImpl(AZStd::string(CURRENT_PLATFORM) + "_" + key, value))
        {
            return true;
        }

        return ReadFieldImpl(key, value);
    }
    bool BootstrapReaderComponent::ReadFieldImpl(const AZStd::string& key, AZStd::string& value)
    {
        // Check cache to short-circuit lookup for common keys
        auto cacheIt = m_keyValueCache.find(key);
        if (cacheIt != m_keyValueCache.end())
        {
            value = cacheIt->second;
            return true;
        }

        AZStd::string regexString("^\\s*key\\s*=\\s*(\\S+)\\b");

        // Replace the dummy key with the actual one
        auto found = regexString.find("key");
        if (found != AZStd::string::npos)
        {
            regexString.replace(found, 3, key);
        }

        AZStd::regex configRegex(regexString.c_str(), AZStd::regex::ECMAScript | AZStd::regex::icase);
        AZStd::smatch match;
        if (AZStd::regex_search(m_fileContents, match, configRegex))
        {
            value = match[1].str();
            m_keyValueCache.emplace(key, value);
            return true;
        }

        return false;
    }
} // namespace AzFramework
