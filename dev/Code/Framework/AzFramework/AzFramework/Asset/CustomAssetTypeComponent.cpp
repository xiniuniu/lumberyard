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

#include "CustomAssetTypeComponent.h"

#include <AzCore/Serialization/SerializeContext.h>

namespace AzFramework
{
    //=========================================================================
    // Reflect
    //=========================================================================
    void CustomAssetTypeComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<CustomAssetTypeComponent, AZ::Component>()
                ->Version(2);

            VersionSearchRule::Reflect(context);
            MatchingRule::Reflect(context);
            XmlSchemaAttribute::Reflect(context);
            XmlSchemaElement::Reflect(context);
            SearchRuleDefinition::Reflect(context);
            DependencySearchRule::Reflect(context);
            XmlSchemaAsset::Reflect(context);
            FileTag::FileTagAsset::Reflect(context);
        }
    }

    //=========================================================================
    // Activate
    //=========================================================================
    void CustomAssetTypeComponent::Activate()
    {
        using namespace FileTag;
        m_schemaAssetHandler.reset(aznew AzFramework::GenericAssetHandler<XmlSchemaAsset>(XmlSchemaAsset::GetDisplayName(), XmlSchemaAsset::GetGroup(), XmlSchemaAsset::GetFileFilter()));
        m_schemaAssetHandler->Register();

        m_fileTagAssetHandler.reset(aznew AzFramework::GenericAssetHandler<FileTagAsset>(FileTagAsset::GetDisplayName(), FileTagAsset::GetGroup(), FileTagAsset::Extension()));
        m_fileTagAssetHandler->Register();
    }

    //=========================================================================
    // Deactivate
    //=========================================================================
    void CustomAssetTypeComponent::Deactivate()
    {
        m_schemaAssetHandler.reset();
        m_fileTagAssetHandler.reset();
    }
} // namespace AzFramework
