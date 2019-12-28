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

#include <AzCore/EBus/EBus.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzFramework/Asset/SimpleAsset.h>

#include "TextureAtlas/TextureAtlas.h"


namespace TextureAtlasNamespace
{
    using AtlasCoordinateSets = AZStd::vector<AZStd::pair<AZStd::string, AtlasCoordinates>>;

    class TextureAtlasRequests : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////

        // Put your public methods here
        //! Saves a texture atlas to file
        virtual void SaveAtlasToFile(const AZStd::string& outputPath,
            AtlasCoordinateSets& handles, int width, int height) = 0;

        //! Tells the TextureAtlas system to load an Atlas and return a pointer for the atlas
        virtual TextureAtlas* LoadAtlas(const AZStd::string& filePath) = 0;

        //! Tells the TextureAtlas system to unload an Atlas
        virtual void UnloadAtlas(TextureAtlas* atlas) = 0;

        //! Returns a pointer to the first Atlas that contains the image, or nullptr if no atlas contains it. 
        //! Does not add a reference, use the notification bus to know when to unload
        virtual TextureAtlas* FindAtlasContainingImage(const AZStd::string& filePath) = 0;

    };
    using TextureAtlasRequestBus = AZ::EBus<TextureAtlasRequests>;

    class TextureAtlasAsset
    {
    public:
        AZ_TYPE_INFO(TextureAtlasAsset, "{BFC6C91F-66CE-4D78-B68A-7F697C9EA2E8}")
            static const char* GetFileFilter() { return "*.texatlas"; }
    };
} // namespace TextureAtlasNamespace



namespace AZ
{
    AZ_TYPE_INFO_SPECIALIZE(AzFramework::SimpleAssetReference<TextureAtlasNamespace::TextureAtlasAsset>,
        "{6F612FE6-A054-4E49-830C-0288F3C79A52}");
}