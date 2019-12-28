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

#include <AzFramework/Asset/AssetCatalog.h>
#include <AzCore/std/string/string.h>

#include <AzToolsFramework/AssetCatalog/PlatformAddressedAssetCatalogBus.h>
#include <AzFramework/Platform/PlatformDefaults.h>

namespace AzToolsFramework
{

    class AssetRegistry;
    class AssetBundleManifest;

    /*
     * Implements an asset catalog for a particular platform, listening to requests based on platform ID
     */
    class PlatformAddressedAssetCatalog final :
        public AzFramework::AssetCatalog,
        public AzToolsFramework::AssetCatalog::PlatformAddressedAssetCatalogRequestBus::Handler
    {
    public:

        AZ_CLASS_ALLOCATOR(PlatformAddressedAssetCatalog, AZ::SystemAllocator, 0)

        explicit PlatformAddressedAssetCatalog(AzFramework::PlatformId platformId, bool useRequestBus = false);

        ~PlatformAddressedAssetCatalog() override;

        //! Load the correct catalog based on m_platformId and connect to the ebus
        void InitCatalog();

        //! Returns Asset Root for a given Platform
        static AZStd::string GetAssetRootForPlatform(AzFramework::PlatformId platformId);

        //! Returns an absolute path to the AssetCatalog for a given platform
        static AZStd::string GetCatalogRegistryPathForPlatform(AzFramework::PlatformId platformId);

        //! Check if the catalog for the given platform exists
        static bool CatalogExists(AzFramework::PlatformId platformId);

        AZStd::string GetCatalogRegistryPath() const;

        //////////////////////////////////////////////////////////////////////////
        // AssetCatalogRequestBus
        void EnableCatalogForAsset(const AZ::Data::AssetType& assetType) override;
        void GetHandledAssetTypes(AZStd::vector<AZ::Data::AssetType>& assetTypes) override;
        void DisableCatalog() override;
        void StartMonitoringAssets() override;
        void StopMonitoringAssets() override;
        bool LoadCatalog(const char* catalogRegistryFile) override;
        void ClearCatalog() override;

        bool SaveCatalog(const char* catalogRegistryFile) override;
        bool AddDeltaCatalog(AZStd::shared_ptr<AzFramework::AssetRegistry> deltaCatalog) override;
        bool InsertDeltaCatalog(AZStd::shared_ptr<AzFramework::AssetRegistry> deltaCatalog, size_t slotNum) override;
        bool InsertDeltaCatalogBefore(AZStd::shared_ptr<AzFramework::AssetRegistry> deltaCatalog, AZStd::shared_ptr<AzFramework::AssetRegistry> afterDeltaCatalog) override;
        bool RemoveDeltaCatalog(AZStd::shared_ptr<AzFramework::AssetRegistry> deltaCatalog) override;

        bool CreateBundleManifest(const AZStd::string& deltaCatalogPath, const AZStd::vector<AZStd::string>& dependentBundleNames, const AZStd::string& fileDirectory, int bundleVersion) override;
        bool CreateDeltaCatalog(const AZStd::vector<AZStd::string>& files, const AZStd::string& filePath) override;
        void AddExtension(const char* extension) override;

        void RegisterAsset(const AZ::Data::AssetId& id, AZ::Data::AssetInfo& info) override;
        void UnregisterAsset(const AZ::Data::AssetId& id) override;

        AZStd::string GetAssetPathById(const AZ::Data::AssetId& id) override;
        AZ::Data::AssetInfo GetAssetInfoById(const AZ::Data::AssetId& id) override;
        AZ::Data::AssetId GetAssetIdByPath(const char* path, const AZ::Data::AssetType& typeToRegister, bool autoRegisterIfNotFound) override;
        AZ::Outcome<AZStd::vector<AZ::Data::ProductDependency>, AZStd::string> GetDirectProductDependencies(const AZ::Data::AssetId& asset) override;
        AZ::Outcome<AZStd::vector<AZ::Data::ProductDependency>, AZStd::string> GetAllProductDependencies(const AZ::Data::AssetId& asset) override;

        void EnumerateAssets(BeginAssetEnumerationCB beginCB, AssetEnumerationCB enumerateCB, EndAssetEnumerationCB endCB) override;
        //////////////////////////////////////////////////////////////////////////
    private:
        AzFramework::PlatformId m_platformId{ AzFramework::PlatformId::Invalid };
 
    };

} // namespace AzFramework
