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

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/parallel/lock.h>
#include <AzCore/std/string/conversions.h>

namespace AZ
{
    namespace Data
    {
        AssetId AssetId::CreateString(AZStd::string_view input)
        {
            size_t separatorIdx = input.find(':');
            if (separatorIdx == AZStd::string_view::npos)
            {
                return AssetId();
            }

            AssetId assetId;
            assetId.m_guid = Uuid::CreateString(input.data(), separatorIdx);
            if (assetId.m_guid.IsNull())
            {
                return AssetId();
            }

            assetId.m_subId = strtoul(&input[separatorIdx + 1], nullptr, 16);

            return assetId;
        }

        void AssetId::Reflect(AZ::ReflectContext* context)
        {
            if (SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context))
            {
                serializeContext->Class<Data::AssetId>()
                    ->Version(1)
                    ->Field("guid", &Data::AssetId::m_guid)
                    ->Field("subId", &Data::AssetId::m_subId)
                    ;
            }

            if (BehaviorContext* behaviorContext = azrtti_cast<BehaviorContext*>(context))
            {
                behaviorContext->Class<Data::AssetId>()
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                    ->Attribute(AZ::Script::Attributes::Category, "Asset")
                    ->Attribute(AZ::Script::Attributes::Module, "asset")
                    ->Method("IsValid", &Data::AssetId::IsValid)
                        ->Attribute(AZ::Script::Attributes::Alias, "is_valid")
                    ;
            }
        }

        namespace AssetInternal
        {
            //=========================================================================
            // QueueAssetLoad
            //=========================================================================
            Asset<AssetData> QueueAssetLoad(AssetData* assetData, const AZ::Data::AssetFilterCB& assetLoadFilterCB)
            {
                return AssetManager::Instance().GetAsset(assetData->GetId(), assetData->GetType(), true, assetLoadFilterCB);
            }

            //=========================================================================
            // GetAsset
            //=========================================================================
            Asset<AssetData> GetAsset(const AssetId& id, const AssetType& type, bool queueLoad, bool isCreate)
            {
                return AssetManager::Instance().GetAsset(id, type, queueLoad, nullptr, false, isCreate);
            }

            void UpdateAssetInfo(AssetId& id, AZStd::string& assetHint)
            {
                // it is possible that the assetID given is legacy / old and we have a new assetId we can use instead for it.
                // in that case, upgrade the AssetID to the new one, so that future saves are in the new format.
                // this function should only be invoked if the feature is turned on in the asset manager as it can be (slightly) expensive

                if ((!AssetManager::IsReady()) || (!AssetManager::Instance().GetAssetInfoUpgradingEnabled()))
                {
                    return;
                }

                AZ::Data::AssetInfo assetInfo;
                AZ::Data::AssetCatalogRequestBus::BroadcastResult(assetInfo, &AZ::Data::AssetCatalogRequests::GetAssetInfoById, id);
                if (assetInfo.m_assetId.IsValid())
                {
                    id = assetInfo.m_assetId;
                    if (!assetInfo.m_relativePath.empty())
                    {
                        assetHint = assetInfo.m_relativePath;
                    }
                }
            }

            //=========================================================================
            // ReloadAsset
            //=========================================================================
            bool ReloadAsset(AssetData* assetData)
            {
                AssetManager::Instance().ReloadAsset(assetData->GetId());
                return true;
            }

            //=========================================================================
            // SaveAsset
            //=========================================================================
            bool SaveAsset(AssetData* assetData)
            {
                AssetManager::Instance().SaveAsset(assetData);
                return true;
            }

            //=========================================================================
            // GetAssetData
            //=========================================================================
            Asset<AssetData> GetAssetData(const AssetId& id)
            {
                if (AssetManager::IsReady())
                {
                    AZStd::lock_guard<AZStd::recursive_mutex> assetLock(AssetManager::Instance().m_assetMutex);
                    auto it = AssetManager::Instance().m_assets.find(id);
                    if (it != AssetManager::Instance().m_assets.end())
                    {
                        return it->second;
                    }
                }
                return nullptr;
            }

            AssetId ResolveAssetId(const AssetId& id)
            {
                AZ::Data::AssetInfo assetInfo;
                AZ::Data::AssetCatalogRequestBus::BroadcastResult(assetInfo, &AZ::Data::AssetCatalogRequests::GetAssetInfoById, id);
                if (assetInfo.m_assetId.IsValid())
                {
                    return assetInfo.m_assetId;
                }
                else
                {
                    return id;
                }

            }
        }

        void AssetData::Reflect(AZ::ReflectContext* context)
        {
            if (SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(context))
            {
                serializeContext->Class<AZ::Data::AssetData>()
                    ->Version(1)
                    ;
            }
        }

        void AssetData::Acquire()
        {
            AZ_Assert(m_useCount >= 0, "AssetData has been deleted")
            ++m_useCount;
        }

        void AssetData::Release()
        {
            AZ_Assert(m_useCount > 0, "Usecount is already 0!");

            AssetId assetId = m_assetId;
            int creationToken = m_creationToken;
            AssetType assetType = GetType();
            bool removeFromHash = IsRegisterReadonlyAndShareable(); 
            // default creation token implies that the asset was not created by the asset manager and therefore it cannot be in the asset map. 
            removeFromHash = creationToken == s_defaultCreationToken ? false : removeFromHash;


            if (m_useCount.fetch_sub(1) == 1)
            {
                AssetManager::Instance().ReleaseAsset(this, assetId, assetType, removeFromHash, creationToken);
            }
        }

        //=========================================================================
        // AssetBusCallbacks::AssetBusCallbacks
        // [9/19/2012]
        //=========================================================================
        void AssetBusCallbacks::SetCallbacks(const AssetReadyCB& readyCB, const AssetMovedCB& movedCB, const AssetReloadedCB& reloadedCB, const AssetSavedCB& savedCB, const AssetUnloadedCB& unloadedCB, const AssetErrorCB& errorCB)
        {
            m_onAssetReadyCB = readyCB;
            m_onAssetMovedCB = movedCB;
            m_onAssetReloadedCB = reloadedCB;
            m_onAssetSavedCB = savedCB;
            m_onAssetUnloadedCB = unloadedCB;
            m_onAssetErrorCB = errorCB;
        }

        void AssetBusCallbacks::ClearCallbacks()
        {
            SetCallbacks(AssetBusCallbacks::AssetReadyCB(),
                AssetBusCallbacks::AssetMovedCB(),
                AssetBusCallbacks::AssetReloadedCB(),
                AssetBusCallbacks::AssetSavedCB(),
                AssetBusCallbacks::AssetUnloadedCB(),
                AssetBusCallbacks::AssetErrorCB());
        }


        void AssetBusCallbacks::SetOnAssetReadyCallback(const AssetReadyCB& readyCB)
        {
            m_onAssetReadyCB = readyCB;
        }

        void AssetBusCallbacks::SetOnAssetMovedCallback(const AssetMovedCB& movedCB)
        {
            m_onAssetMovedCB = movedCB;
        }

        void AssetBusCallbacks::SetOnAssetReloadedCallback(const AssetReloadedCB& reloadedCB)
        {
            m_onAssetReloadedCB = reloadedCB;
        }

        void AssetBusCallbacks::SetOnAssetSavedCallback(const AssetSavedCB& savedCB)
        {
            m_onAssetSavedCB = savedCB;
        }

        void AssetBusCallbacks::SetOnAssetUnloadedCallback(const AssetUnloadedCB& unloadedCB)
        {
            m_onAssetUnloadedCB = unloadedCB;
        }

        void AssetBusCallbacks::SetOnAssetErrorCallback(const AssetErrorCB& errorCB)
        {
            m_onAssetErrorCB = errorCB;
        }

        //=========================================================================
        // AssetBusCallbacks::OnAssetReady
        // [9/19/2012]
        //=========================================================================
        void AssetBusCallbacks::OnAssetReady(Asset<AssetData> asset)
        {
            if (m_onAssetReadyCB)
            {
                m_onAssetReadyCB(asset, *this);
            }
        }

        //=========================================================================
        // AssetBusCallbacks::OnAssetMoved
        // [9/19/2012]
        //=========================================================================
        void AssetBusCallbacks::OnAssetMoved(Asset<AssetData> asset, void* oldDataPointer)
        {
            if (m_onAssetMovedCB)
            {
                m_onAssetMovedCB(asset, oldDataPointer, *this);
            }
        }

        //=========================================================================
        // AssetBusCallbacks::OnAssetReloaded
        // [9/19/2012]
        //=========================================================================
        void AssetBusCallbacks::OnAssetReloaded(Asset<AssetData> asset)
        {
            if (m_onAssetReloadedCB)
            {
                m_onAssetReloadedCB(asset, *this);
            }
        }

        //=========================================================================
        // AssetBusCallbacks::OnAssetSaved
        // [9/19/2012]
        //=========================================================================
        void AssetBusCallbacks::OnAssetSaved(Asset<AssetData> asset, bool isSuccessful)
        {
            if (m_onAssetSavedCB)
            {
                m_onAssetSavedCB(asset, isSuccessful, *this);
            }
        }

        //=========================================================================
        // AssetBusCallbacks::OnAssetUnloaded
        // [9/19/2012]
        //=========================================================================
        void AssetBusCallbacks::OnAssetUnloaded(const AssetId assetId, const AssetType assetType)
        {
            if (m_onAssetUnloadedCB)
            {
                m_onAssetUnloadedCB(assetId, assetType, *this);
            }
        }

        //=========================================================================
        // AssetBusCallbacks::OnAssetError
        // [4/3/2014]
        //=========================================================================
        void AssetBusCallbacks::OnAssetError(Asset<AssetData> asset)
        {
            if (m_onAssetErrorCB)
            {
                m_onAssetErrorCB(asset, *this);
            }
        }

        //=========================================================================
        // AssetFilterNoAssetLoading
        //=========================================================================
        /*static*/ bool AssetFilterNoAssetLoading(const Asset<Data::AssetData>& /*asset*/)
        {
            return false;
        }

    }   // namespace Data
}   // namespace AZ
