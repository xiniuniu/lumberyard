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

#include <SceneAPI/SceneData/SceneDataConfiguration.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IMaterialData.h>
#include <AzCore/std/containers/unordered_map.h>

namespace AZ
{
    class ReflectContext;

    namespace SceneData
    {
        namespace GraphData
        {
            class SCENE_DATA_CLASS MaterialData
                : public AZ::SceneAPI::DataTypes::IMaterialData
            {
            public:
                AZ_RTTI(MaterialData, "{F2EE1768-183B-483E-9778-CB3D3D0DA68A}", AZ::SceneAPI::DataTypes::IMaterialData)
                
                SCENE_DATA_API MaterialData();
                SCENE_DATA_API virtual ~MaterialData() = default;

                SCENE_DATA_API virtual void SetTexture(TextureMapType mapType, const char* textureFileName);
                SCENE_DATA_API virtual void SetTexture(TextureMapType mapType, const AZStd::string& textureFileName);
                SCENE_DATA_API virtual void SetTexture(TextureMapType mapType, AZStd::string&& textureFileName);
                SCENE_DATA_API virtual void SetNoDraw(bool isNoDraw);

                SCENE_DATA_API virtual void SetDiffuseColor(const AZ::Vector3& color);
                SCENE_DATA_API virtual void SetSpecularColor(const AZ::Vector3& color);
                SCENE_DATA_API virtual void SetEmissiveColor(const AZ::Vector3& color);
                SCENE_DATA_API virtual void SetOpacity(float opacity);
                SCENE_DATA_API virtual void SetShininess(float shininess);

                SCENE_DATA_API const AZStd::string& GetTexture(TextureMapType mapType) const override;
                SCENE_DATA_API bool IsNoDraw() const override;

                SCENE_DATA_API const AZ::Vector3& GetDiffuseColor() const override;
                SCENE_DATA_API const AZ::Vector3& GetSpecularColor() const override;
                SCENE_DATA_API const AZ::Vector3& GetEmissiveColor() const override;
                SCENE_DATA_API float GetOpacity() const override;
                SCENE_DATA_API float GetShininess() const override;

                static void Reflect(ReflectContext* context);

            protected:
                AZStd::unordered_map<TextureMapType, AZStd::string> m_textureMap;
                
                AZ::Vector3 m_diffuseColor;
                AZ::Vector3 m_specularColor;
                AZ::Vector3 m_emissiveColor;
                float m_opacity;
                float m_shininess;

                bool m_isNoDraw;

                const static AZStd::string s_DiffuseMapName;
                const static AZStd::string s_SpecularMapName;
                const static AZStd::string s_BumpMapName;
                const static AZStd::string s_emptyString;
            };
        } // namespace GraphData
    } // namespace SceneData
} // namespace AZ
