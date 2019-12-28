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
#ifndef AZCORE_SLICE_ASSET_HANDLER_H
#define AZCORE_SLICE_ASSET_HANDLER_H

#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Asset/AssetTypeInfoBus.h>

namespace AZ
{
    class SerializeContext;

    /**
     * Manages prefab assets.
     */
    class SliceAssetHandler
        : public Data::AssetHandler
        , AZ::AssetTypeInfoBus::MultiHandler
    {
    public:
        AZ_CLASS_ALLOCATOR(SliceAssetHandler, AZ::SystemAllocator, 0);
        AZ_RTTI(SliceAssetHandler, "{4DA1A81B-EEFE-4129-97A2-258233437A88}", Data::AssetHandler);

        SliceAssetHandler(SerializeContext* context = nullptr);
        ~SliceAssetHandler();

        // Called by the asset manager to create a new asset. No loading should occur during this call
        Data::AssetPtr CreateAsset(const Data::AssetId& id, const Data::AssetType& type) override;

        // Called by the asset manager to perform actual asset load.
        bool LoadAssetData(const Data::Asset<Data::AssetData>& asset, IO::GenericStream* stream, const AZ::Data::AssetFilterCB& assetLoadFilterCB) override;
        bool LoadAssetData(const Data::Asset<Data::AssetData>& asset, const char* assetPath, const AZ::Data::AssetFilterCB& assetLoadFilterCB) override;

        // Called by the asset manager to perform actual asset save. Returns true if successful otherwise false (default - as we don't require support save).
        bool SaveAssetData(const Data::Asset<Data::AssetData>& asset, IO::GenericStream* stream) override;

        // Called by the asset manager when an asset should be deleted.
        void DestroyAsset(Data::AssetPtr ptr) override;

        // Called by asset manager on registration.
        void GetHandledAssetTypes(AZStd::vector<Data::AssetType>& assetTypes) override;

        //////////////////////////////////////////////////////////////////////////////////////////////
        // AZ::AssetTypeInfoBus::Handler
        //////////////////////////////////////////////////////////////////////////////////////////////
        AZ::Data::AssetType GetAssetType() const override;
        const char* GetAssetTypeDisplayName() const override;
        const char* GetGroup() const override;
        void GetAssetTypeExtensions(AZStd::vector<AZStd::string>& extensions) override;

        SerializeContext* GetSerializeContext() const;

        void SetSerializeContext(SerializeContext* context);

        //! Sets the filter flags to use when loading the asset data
        void SetFilterFlags(u32 flags);

    protected:
        SerializeContext* m_serializeContext;
        u32 m_filterFlags;
    };
}

#endif // AZCORE_SLICE_ASSET_HANDLER_H
#pragma once
