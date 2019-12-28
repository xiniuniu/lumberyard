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

#include <AssetValidationSystemComponent.h>

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/std/string/conversions.h>
#include <AzFramework/API/ApplicationAPI.h>
#include <AzFramework/FileTag/FileTag.h>
#include <AzFramework/FileTag/FileTagBus.h>
#include <AzFramework/StringFunc/StringFunc.h>

#include <ISystem.h>
#include <IConsole.h>

#include <platform_impl.h>

namespace AssetValidation
{
    void AssetValidationSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<AssetValidationSystemComponent, AZ::Component>()
                ->Version(0);

            if (AZ::EditContext* ec = serialize->GetEditContext())
            {
                ec->Class<AssetValidationSystemComponent>("AssetValidation", "[Description of functionality provided by this System Component]")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ;
            }
        }
    }

    void AssetValidationSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("AssetValidationService"));
    }

    void AssetValidationSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("AssetValidationService"));
    }

    void AssetValidationSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        AZ_UNUSED(required);
    }

    void AssetValidationSystemComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        AZ_UNUSED(dependent);
    }

    void AssetValidationSystemComponent::Init()
    {
    }

    void AssetValidationSystemComponent::Activate()
    {
        CrySystemEventBus::Handler::BusConnect();
        AssetValidationRequestBus::Handler::BusConnect();
    }

    void AssetValidationSystemComponent::Deactivate()
    {
        if (m_seedMode)
        {
            CryPak::CryPakNotificationBus::Handler::BusDisconnect();
        }
        AssetValidationRequestBus::Handler::BusDisconnect();
        CrySystemEventBus::Handler::BusDisconnect();
    }

    void AssetValidationSystemComponent::ConsoleCommandSeedMode(IConsoleCmdArgs* pCmdArgs)
    {
        AssetValidationRequestBus::Broadcast(&AssetValidationRequestBus::Events::SeedMode);
    }

    void AssetValidationSystemComponent::ConsoleCommandTogglePrintBlacklisted(IConsoleCmdArgs* pCmdArgs)
    {
        AssetValidationRequestBus::Broadcast(&AssetValidationRequestBus::Events::TogglePrintBlacklisted);
    }

    void AssetValidationSystemComponent::TogglePrintBlacklisted()
    {
        m_printBlacklisted = !m_printBlacklisted;
        if (m_printBlacklisted)
        {
            AZ_TracePrintf("AssetValidation", "Asset Validation is now on");
        }
        else
        {
            AZ_TracePrintf("AssetValidation", "Asset Validation is now off");
        }
    }

    void AssetValidationSystemComponent::SeedMode()
    {
        m_seedMode = !m_seedMode;
        if (m_seedMode)
        {
            // adding blacklist tags
            m_blacklistFileTags.emplace_back(AzFramework::FileTag::FileTags[static_cast<unsigned int>(AzFramework::FileTag::FileTagsIndex::Ignore)]);
            m_blacklistFileTags.emplace_back(AzFramework::FileTag::FileTags[static_cast<unsigned int>(AzFramework::FileTag::FileTagsIndex::ProductDependency)]);
            CryPak::CryPakNotificationBus::Handler::BusConnect();
            BuildAssetList();
            AZ_TracePrintf("AssetValidation", "Asset Validation is now on");
        }
        else
        {
            CryPak::CryPakNotificationBus::Handler::BusDisconnect();
            m_blacklistFileTags.clear();
            AZ_TracePrintf("AssetValidation", "Asset Validation is now off");
        }
        AssetValidationNotificationBus::Broadcast(&AssetValidationNotificationBus::Events::SetSeedMode, m_seedMode);
    }

    void AssetValidationSystemComponent::OnCrySystemInitialized(ISystem& system, const SSystemInitParams& systemInitParams)
    {
        AZ_UNUSED(systemInitParams);

        system.GetIConsole()->AddCommand("seedmode", ConsoleCommandSeedMode);
        system.GetIConsole()->AddCommand("addseedpath", ConsoleCommandAddSeedPath);
        system.GetIConsole()->AddCommand("removeseedpath", ConsoleCommandRemoveSeedPath);
        system.GetIConsole()->AddCommand("listknownassets", ConsoleCommandKnownAssets);
        system.GetIConsole()->AddCommand("addseedlist", ConsoleCommandAddSeedList);
        system.GetIConsole()->AddCommand("removeseedlist", ConsoleCommandRemoveSeedList);
        system.GetIConsole()->AddCommand("printblacklisted", ConsoleCommandTogglePrintBlacklisted);
    }       

    bool AssetValidationSystemComponent::IsKnownAsset(const char* assetPath)
    {
        AZStd::string lowerAsset{ assetPath };
        AZStd::replace(lowerAsset.begin(), lowerAsset.end(), AZ_WRONG_DATABASE_SEPARATOR, AZ_CORRECT_DATABASE_SEPARATOR);

        const AZStd::vector<AZStd::string> prefixes = { "./", "@assets@/" };
        for (const AZStd::string& prefix : prefixes)
        {
            if (lowerAsset.starts_with(prefix))
            {
                lowerAsset = lowerAsset.substr(prefix.length());
                break;
            }
        }
        AZStd::to_lower(lowerAsset.begin(), lowerAsset.end());

        return m_knownAssetPaths.count(lowerAsset) > 0;
    }

    bool AssetValidationSystemComponent::CheckKnownAsset(const char* assetPath)
    {
        if (!IsKnownAsset(assetPath))
        {
            using namespace AzFramework::FileTag;
            bool shouldIgnore = false;
            QueryFileTagsEventBus::EventResult(shouldIgnore, FileTagType::BlackList, &QueryFileTagsEventBus::Events::Match, assetPath, m_blacklistFileTags);
            if (shouldIgnore)
            {
                if (m_printBlacklisted)
                {
                    AZ_TracePrintf("AssetValidation", "Asset ( %s ) is blacklisted. \n", assetPath);
                }
                return true;
            }

            AZ_Warning("AssetValidation", false, "Asset not found in seed graph: %s", assetPath);
            AssetValidationNotificationBus::Broadcast(&AssetValidationNotificationBus::Events::UnknownAsset, assetPath);
            return false;
        }
        return true;
    }

    void AssetValidationSystemComponent::FileAccess(const char* assetPath)
    {
        CheckKnownAsset(assetPath);
    }

    void AssetValidationSystemComponent::AddKnownAssets(AZ::Data::AssetId assetId)
    {
        AZ::Data::AssetInfo baseAssetInfo;
        AZ::Data::AssetCatalogRequestBus::BroadcastResult(baseAssetInfo, &AZ::Data::AssetCatalogRequests::GetAssetInfoById, assetId);

        m_knownAssetPaths.insert(baseAssetInfo.m_relativePath);

        AZ::Outcome<AZStd::vector<AZ::Data::ProductDependency>, AZStd::string> getDependenciesResult = AZ::Failure(AZStd::string());
        AZ::Data::AssetCatalogRequestBus::BroadcastResult(getDependenciesResult, &AZ::Data::AssetCatalogRequestBus::Events::GetAllProductDependencies, assetId);
        if (getDependenciesResult.IsSuccess())
        {
            AZStd::vector<AZ::Data::ProductDependency> entries = getDependenciesResult.TakeValue();

            for (const AZ::Data::ProductDependency& productDependency : entries)
            {
                if (m_knownAssetIds.insert(productDependency.m_assetId).second)
                {
                    AZ::Data::AssetInfo thisAssetInfo;
                    AZ::Data::AssetCatalogRequestBus::BroadcastResult(thisAssetInfo, &AZ::Data::AssetCatalogRequests::GetAssetInfoById, productDependency.m_assetId);

                    if (!thisAssetInfo.m_relativePath.empty())
                    {
                        m_knownAssetPaths.insert(thisAssetInfo.m_relativePath);
                    }
                }
            }
        }
    }

    void AssetValidationSystemComponent::ListKnownAssets()
    {
        AZ_TracePrintf("AssetValidation", "Seed Asset ids:");
        for (const auto& thisIdPair : m_seedAssetIds)
        {
            AZ::Data::AssetInfo thisAssetInfo;
            AZ::Data::AssetCatalogRequestBus::BroadcastResult(thisAssetInfo, &AZ::Data::AssetCatalogRequests::GetAssetInfoById, thisIdPair.first);
            AZ_TracePrintf("AssetValidation", "%s - (%s)", thisAssetInfo.m_relativePath.c_str(), thisIdPair.first.ToString<AZStd::string>().c_str());
        }
        AZ_TracePrintf("AssetValidation", "Known Paths:");
        for (const auto& thisPath : m_knownAssetPaths)
        {
            AZ::Data::AssetId thisAssetId;
            AZ::Data::AssetCatalogRequestBus::BroadcastResult(thisAssetId, &AZ::Data::AssetCatalogRequests::GetAssetIdByPath, thisPath.c_str(), AZ::Data::s_invalidAssetType, false);
            AZ_TracePrintf("AssetValidation", "%s - (%s)", thisPath.c_str(), thisAssetId.ToString<AZStd::string>().c_str());
        }
    }

    void AssetValidationSystemComponent::BuildAssetList()
    {
        m_knownAssetIds.clear();
        m_knownAssetPaths.clear();
        for (const auto& thisAsset : m_seedAssetIds)
        {
            AddKnownAssets(thisAsset.first);
        }
        for (const auto& thisPath : m_seedLists)
        {
            AzFramework::AssetSeedList seedList;
            if (!AZ::Utils::LoadObjectFromFileInPlace(thisPath, seedList))
            {
                AZ_Warning("AssetValidation", false, "Failed to load seed list %s", thisPath.c_str());
                continue;
            }
            AddSeedsFor(seedList, AZ_CRC(thisPath.c_str()));
        }
    }

    bool AssetValidationSystemComponent::AddSeedPath(const char* seedPath)
    {
        AZ::Data::AssetId assetId;
        const bool AutoRegister{ false };
        AZ::Data::AssetCatalogRequestBus::BroadcastResult(assetId, &AZ::Data::AssetCatalogRequestBus::Events::GetAssetIdByPath, seedPath, AZ::Data::s_invalidAssetType, AutoRegister);
        if (!assetId.IsValid())
        {
            AZ_Warning("AssetValidation", false, "Can't find an asset %s", seedPath);
            return false;
        }
        return AddSeedAssetId(assetId, 0);
    }

    bool AssetValidationSystemComponent::AddSeedAssetId(AZ::Data::AssetId assetId, AZ::u32 sourceId)
    {
        auto curEntry = m_seedAssetIds.find(assetId);

        if (curEntry != m_seedAssetIds.end() && curEntry->second.count(sourceId) > 0)
        {
            AZ_Warning("AssetValidation", false, "AssetID %s from source %d is already added", assetId.ToString< AZStd::string>().c_str(), sourceId);
            return false;
        }
        m_seedAssetIds[assetId].insert(sourceId);
        AddKnownAssets(assetId);
        AZ_TracePrintf("AssetValidation", "Added assedId %s from source %d", assetId.ToString< AZStd::string>().c_str(), sourceId);
        return true;
    }

    bool AssetValidationSystemComponent::RemoveSeedPath(const char* seedPath)
    {
        AZ::Data::AssetId assetId;
        const bool AutoRegister{ false };
        AZ::Data::AssetCatalogRequestBus::BroadcastResult(assetId, &AZ::Data::AssetCatalogRequestBus::Events::GetAssetIdByPath, seedPath, AZ::Data::s_invalidAssetType, AutoRegister);
        if (!assetId.IsValid())
        {
            AZ_Warning("AssetValidation", false, "Can't find an asset %s", seedPath);
            return false;
        }
        return RemoveSeedAssetId(assetId, 0);
    }

    bool AssetValidationSystemComponent::RemoveSeedAssetId(AZ::Data::AssetId assetId, AZ::u32 sourceId)
    {
        int removeResult = RemoveSeedAssetIdBySource(assetId, sourceId);
        if (removeResult == -1)
        {
            // invalid remove request
            return false;
        }
        else if(removeResult == 0)
        {
            // Success and it was the last entry of that assetId, we need to rebuild
            BuildAssetList();
        }
        return true;
    }

    bool AssetValidationSystemComponent::RemoveSeedAssetIdList(AssetSourceList assetList)
    {
        bool needRebuild{ false };
        for (auto& thisPair : assetList)
        {
            if (RemoveSeedAssetIdBySource(thisPair.first, thisPair.second) == 0)
            {
                needRebuild = true;
            }
        }
        if (needRebuild)
        {
            BuildAssetList();
        }
        return true;
    }

    int AssetValidationSystemComponent::RemoveSeedAssetIdBySource(const AZ::Data::AssetId& assetId, AZ::u32 sourceId)
    {
        auto curEntry = m_seedAssetIds.find(assetId);

        if (curEntry == m_seedAssetIds.end() || !curEntry->second.erase(sourceId))
        {
            AZ_Warning("AssetValidation", false, "AssetID %s from source %d is not in the seed assets list", assetId.ToString< AZStd::string>().c_str(), sourceId);
            return -1;
        }

        if (!curEntry->second.size())
        {
            m_seedAssetIds.erase(assetId);
            return 0;
        }

        return static_cast<int>(curEntry->second.size());
    }

    void AssetValidationSystemComponent::ConsoleCommandAddSeedPath(IConsoleCmdArgs* pCmdArgs)
    {
        if (pCmdArgs->GetArgCount() < 2)
        {
            AZ_TracePrintf("AssetValidation", "addseedpath assetpath");
            return;
        }

        AssetValidationRequestBus::Broadcast(&AssetValidationRequestBus::Events::AddSeedPath, pCmdArgs->GetArg(1));
    }

    void AssetValidationSystemComponent::ConsoleCommandKnownAssets(IConsoleCmdArgs* pCmdArgs)
    {

        AssetValidationRequestBus::Broadcast(&AssetValidationRequestBus::Events::ListKnownAssets);
    }

    void AssetValidationSystemComponent::ConsoleCommandRemoveSeedPath(IConsoleCmdArgs* pCmdArgs)
    {
        if (pCmdArgs->GetArgCount() < 2)
        {
            AZ_TracePrintf("AssetValidation", "removeseedpath assetpath");
            return;
        }

        AssetValidationRequestBus::Broadcast(&AssetValidationRequestBus::Events::RemoveSeedPath, pCmdArgs->GetArg(1));
    }

    void AssetValidationSystemComponent::ConsoleCommandAddSeedList(IConsoleCmdArgs* pCmdArgs)
    {
        if (pCmdArgs->GetArgCount() < 2)
        {
            AZ_TracePrintf("AssetValidation", "Command syntax is: addseedlist <path/to/seedfile> as a relative path under the /dev folder");
            return;
        }

        const char* seedfilepath = pCmdArgs->GetArg(1);
        AssetValidationRequestBus::Broadcast(&AssetValidationRequestBus::Events::AddSeedList, seedfilepath);
    }

    bool AssetValidationSystemComponent::AddSeedsFor(const AzFramework::AssetSeedList& seedList, AZ::u32 seedId) 
    {
        for (const AzFramework::SeedInfo& thisSeed : seedList)
        {
            AddSeedAssetId(thisSeed.m_assetId, seedId);
        }
        return true;
    }

    bool AssetValidationSystemComponent::RemoveSeedsFor(const AzFramework::AssetSeedList& seedList, AZ::u32 seedId) 
    {
        AssetValidation::AssetValidationRequests::AssetSourceList removeList;
        for (const AzFramework::SeedInfo& thisSeed : seedList)
        {
            removeList.push_back({ thisSeed.m_assetId, seedId });
        }
        RemoveSeedAssetIdList(removeList);
        return true;
    }

    bool AssetValidationSystemComponent::AddSeedList(const char* seedPath)
    {
        AZStd::string absoluteSeedPath;
        AZ::Outcome<AzFramework::AssetSeedList, AZStd::string> loadSeedOutcome = LoadSeedList(seedPath, absoluteSeedPath);
        if (!loadSeedOutcome.IsSuccess())
        {
            AZ_Warning("AssetValidation", false, loadSeedOutcome.GetError().c_str());
            return false;
        }
        if (m_seedLists.count(absoluteSeedPath.c_str()))
        {
            AZ_Warning("AssetValidation", false, "Seed list %s (%s) already loaded", seedPath, absoluteSeedPath.c_str());
            return false;
        }

        AzFramework::AssetSeedList seedList = loadSeedOutcome.TakeValue();
        if (m_seedMode && !AddSeedsFor(seedList, AZ_CRC(absoluteSeedPath.c_str())))
        {
            return false;
        }
        m_seedLists.insert(absoluteSeedPath.c_str());
        AZ_TracePrintf("AssetValidation", "Added seed list %s (%s) with %u elements", seedPath, absoluteSeedPath.c_str(), seedList.size());
        return true;
    }

    void AssetValidationSystemComponent::ConsoleCommandRemoveSeedList(IConsoleCmdArgs* pCmdArgs)
    {
        if (pCmdArgs->GetArgCount() < 2)
        {
            AZ_TracePrintf("AssetValidation", "removeseedlist seedlistpath");
            return;
        }

        const char* seedfilepath = pCmdArgs->GetArg(1);
        AssetValidationRequestBus::Broadcast(&AssetValidationRequestBus::Events::RemoveSeedList, seedfilepath);
    }

    AZ::Outcome<AzFramework::AssetSeedList, AZStd::string> AssetValidationSystemComponent::LoadSeedList(const char* seedPath, AZStd::string& seedListPath)
    {
        AZ::IO::FileIOBase* fileIO = AZ::IO::FileIOBase::GetInstance();
        if (!fileIO)
        {
            return AZ::Failure(AZStd::string("Couldn't get fileIO"));
        }
        const char* appRoot = nullptr;
        AzFramework::ApplicationRequests::Bus::BroadcastResult(appRoot, &AzFramework::ApplicationRequests::GetEngineRoot);
        if (!appRoot)
        {
            return AZ::Failure(AZStd::string("Couldn't get engine root"));
        }
        AZStd::string absoluteSeedPath = AZStd::string::format("%s/%s", appRoot, seedPath);
        AzFramework::StringFunc::Path::Normalize(absoluteSeedPath);
        AzFramework::AssetSeedList seedList;
        if (!AZ::Utils::LoadObjectFromFileInPlace(absoluteSeedPath, seedList))
        {
            return AZ::Failure(AZStd::string::format("Failed to load seed list %s", absoluteSeedPath.c_str()));
        }
        seedListPath = absoluteSeedPath;
        return AZ::Success(seedList);
    }
    bool AssetValidationSystemComponent::RemoveSeedList(const char* seedPath)
    {
        AZStd::string absoluteSeedPath;
        AZ::Outcome<AzFramework::AssetSeedList, AZStd::string> loadSeedOutcome = LoadSeedList(seedPath, absoluteSeedPath);
        if (!loadSeedOutcome.IsSuccess())
        {
            AZ_Warning("AssetValidation", false, loadSeedOutcome.GetError().c_str());
            return false;
        }

        AzFramework::AssetSeedList seedList = loadSeedOutcome.TakeValue();
        if (!m_seedLists.count(absoluteSeedPath))
        {
            AZ_Warning("AssetValidation", false, "Seed path %s (%s) wasn't in seed lists", seedPath, absoluteSeedPath.c_str());
            return false;
        }
        m_seedLists.erase(absoluteSeedPath.c_str());

        // Don't check if we're currently in seed mode - that will be dealt with on the other side
        if (!RemoveSeedsFor(seedList, AZ_CRC(absoluteSeedPath.c_str())))
        {
            return false;
        }

        AZ_TracePrintf("AssetValidation", "Removed seed list %s with %u elements", absoluteSeedPath.c_str(), seedList.size());

        return true;
    }
}
