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
#include "stdafx.h"

#include "AssetDropHelpers.h"
#include <LyShine/UiAssetTypes.h>
#include <AzCore/Asset/AssetTypeInfoBus.h>
#include <AzCore/Slice/SliceAsset.h>
#include <AzToolsFramework/AssetBrowser/AssetBrowserEntry.h>

namespace AssetDropHelpers
{
    using ProductAssetList = AZStd::vector<const AzToolsFramework::AssetBrowser::ProductAssetBrowserEntry*>;

    ProductAssetList GetProductsFromAssetMimeData(const QMimeData* mimeData)
    {
        using namespace AzToolsFramework;

        AZStd::vector<AssetBrowser::AssetBrowserEntry*> entries;
        AssetBrowser::AssetBrowserEntry::FromMimeData(mimeData, entries);

        AZStd::vector<const AssetBrowser::ProductAssetBrowserEntry*> products;
        products.reserve(entries.size());

        for (AssetBrowser::AssetBrowserEntry* entry : entries)
        {
            const AssetBrowser::ProductAssetBrowserEntry* browserEntry = azrtti_cast<const AssetBrowser::ProductAssetBrowserEntry*>(entry);
            if (browserEntry)
            {
                products.push_back(browserEntry);
            }
            else
            {
                entry->GetChildren<AssetBrowser::ProductAssetBrowserEntry>(products);
            }
        }

        return products;
    }

    bool AcceptsMimeType(const QMimeData* mimeData)
    {
        return mimeData && mimeData->hasFormat(AzToolsFramework::AssetBrowser::AssetBrowserEntry::GetMimeType());
    }

    bool DoesMimeDataContainSliceOrComponentAssets(const QMimeData* mimeData)
    {
        if (AcceptsMimeType(mimeData))
        {
            ComponentAssetHelpers::ComponentAssetPairs componentAssetPairs;
            AssetList sliceAssets;
            DecodeSliceAndComponentAssetsFromMimeData(mimeData, componentAssetPairs, sliceAssets);

            return (!componentAssetPairs.empty() || !sliceAssets.empty());
        }

        return false;
    }

    void DecodeSliceAndComponentAssetsFromMimeData(const QMimeData* mimeData, ComponentAssetHelpers::ComponentAssetPairs& componentAssetPairs, AssetList& sliceAssets)
    {
        ProductAssetList products = GetProductsFromAssetMimeData(mimeData);

        // Look at all products and determine if they have a slice asset or an asset with an associated component
        for (const auto* product : products)
        {
            if (product->GetAssetType() == AZ::AzTypeInfo<AZ::SliceAsset>::Uuid())
            {
                sliceAssets.push_back(product->GetAssetId());
            }
            else
            {
                bool canCreateComponent = false;
                AZ::AssetTypeInfoBus::EventResult(canCreateComponent, product->GetAssetType(), &AZ::AssetTypeInfo::CanCreateComponent, product->GetAssetId());

                AZ::TypeId componentType;
                AZ::AssetTypeInfoBus::EventResult(componentType, product->GetAssetType(), &AZ::AssetTypeInfo::GetComponentTypeId);

                if (canCreateComponent && !componentType.IsNull())
                {
                    componentAssetPairs.push_back(AZStd::make_pair(componentType, product->GetAssetId()));
                }
            }
        }
    }

    void DecodeUiCanvasAssetsFromMimeData(const QMimeData* mimeData, AssetList& canvasAssets)
    {
        ProductAssetList products = GetProductsFromAssetMimeData(mimeData);

        // Look at all products and determine if they have a UiCanvas asset
        for (const auto* product : products)
        {
            if (product->GetAssetType() == AZ::AzTypeInfo<LyShine::CanvasAsset>::Uuid())
            {
                canvasAssets.push_back(product->GetAssetId());
            }
        }
    }
}   // namespace AssetDropHelpers
