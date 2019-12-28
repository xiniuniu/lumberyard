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
#include <AzCore/Slice/SliceAsset.h>
#include <AzCore/Slice/SliceComponent.h>
#include <AzCore/Component/Entity.h>

namespace AZ
{
    //=========================================================================
    // SliceAsset
    //=========================================================================
    SliceAsset::SliceAsset(const Data::AssetId& assetId /*= Data::AssetId()*/)
        : AssetData(assetId)
        , m_entity(nullptr)
        , m_component(nullptr)
    {
    }

    //=========================================================================
    // ~SliceAsset
    //=========================================================================
    SliceAsset::~SliceAsset()
    {
        delete m_entity;
    }

    //=========================================================================
    // SetData
    //=========================================================================
    bool SliceAsset::SetData(Entity* entity, SliceComponent* component, bool deleteExisting)
    {
        AZ_Assert((entity == nullptr && component == nullptr) ||
            (entity->FindComponent<SliceComponent>() == component), "Slice component doesn't belong to the entity!");

        if (IsLoading())
        {
            // we can't set the data while loading
            return false;
        }

        if (deleteExisting)
        {
            delete m_entity; // delete the entity if we have one, it should delete the component too
        }

        m_entity = entity;
        m_component = component;
        if (m_entity && m_component)
        {
            m_status = static_cast<int>(AssetStatus::Ready);
        }
        else
        {
            m_status = static_cast<int>(AssetStatus::NotLoaded);
        }
        return true;
    }

    //=========================================================================
    // Clone
    //=========================================================================
    SliceAsset* SliceAsset::Clone()
    {
        return aznew SliceAsset(GetId());
    }

    namespace Data
    {
        //=========================================================================
        // AssetFilterSourceSlicesOnly
        //=========================================================================
        bool AssetFilterSourceSlicesOnly(const AZ::Data::Asset<AZ::Data::AssetData>& asset)
        {
            // Expand regular slice references (but not dynamic slice references).
            return (asset.GetType() == AZ::AzTypeInfo<AZ::SliceAsset>::Uuid());
        }
    }

} // namespace AZ
