﻿
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

#include <AzFramework/Physics/Material.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <TerrainComponent.h>
#include <PhysX/ComponentTypeIds.h>
#include <AzToolsFramework/Physics/EditorTerrainComponentBus.h>
#include <PhysX/ConfigurationBus.h>
#include <IEditor.h>

namespace PhysX
{
    /**
     * Component responsible for exporting cry terrain
     * and baking it into a physx file format.
     */
    class EditorTerrainComponent
        : public AzToolsFramework::Components::EditorComponentBase
        , private IEditorNotifyListener
        , private AZ::Data::AssetBus::Handler
        , public Physics::EditorTerrainMaterialRequestsBus::Handler
        , private Physics::EditorTerrainComponentRequestsBus::Handler
        , protected AzToolsFramework::EntitySelectionEvents::Bus::Handler
        , private PhysX::ConfigurationNotificationBus::Handler
        , private AzToolsFramework::ToolsApplicationNotificationBus::Handler
    {
    public:
        AZ_EDITOR_COMPONENT(EditorTerrainComponent, EditorTerrainComponentTypeId, AzToolsFramework::Components::EditorComponentBase);
        static void Reflect(AZ::ReflectContext* context);

        EditorTerrainComponent() = default;

        // EditorComponentBase
        void BuildGameEntity(AZ::Entity* gameEntity) override;

    protected:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("TerrainService", 0x28ee7719));
        }
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC("TerrainService", 0x28ee7719));
            incompatible.push_back(AZ_CRC("LegacyCryPhysicsService", 0xbb370351));
        }
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
        }
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
        {
        }

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // Physics::EditorTerrainComponentRequestsBus
        void GetTrianglesForHeightField(AZStd::vector<AZ::Vector3>& verts, AZStd::vector<AZ::u32>& indices, AZStd::vector<Physics::MaterialId>& materialIds) const;
        void UpdateHeightFieldAsset() override;
        void SaveHeightFieldAsset() override;
        void LoadHeightFieldAsset() override;

        // AzToolsFramework::EntitySelectionEvents
        void OnSelected() override;
        void OnDeselected() override;

        // PhysX::ConfigurationNotificationBus
        virtual void OnConfigurationRefreshed(const PhysX::Configuration& configuration) override;

    private:
        // ToolsApplicationNotificationBus
        void AfterUndoRedo() override;
        void OnEndUndo(const char* label, bool changed) override;

        void RegisterForEditorEvents();
        void UnregisterForEditorEvents();
        void OnEditorNotifyEvent(EEditorNotifyEvent event) override;
        void CreateEditorTerrain();
        bool ShouldUpdateTerrain(const char* commandName) const;
        AZStd::string GetExportPath();

        AZ::Data::Asset<Pipeline::HeightFieldAsset> CreateHeightFieldAsset();

        // Physics::TerrainMaterialRequestsBus
        void SetMaterialSelectionForSurfaceId(int surfaceId, const Physics::MaterialSelection& selection) override;
        bool GetMaterialSelectionForSurfaceId(int surfaceId, Physics::MaterialSelection& selection) override;

        // AZ::Data::AssetBus::Handler
        void OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset) override;
        void OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset) override;
        void OnAssetError(AZ::Data::Asset<AZ::Data::AssetData> asset) override;

        TerrainConfiguration m_configuration; ///< Configuration used for creating terrain.
        AZStd::string m_exportAssetPath = "terrain/terrain.pxheightfield"; ///< Relative path to current level folder.
        bool m_heightFieldDirty = false; ///< Height field asset should be serialised next time the level is saved.
        bool m_createTerrainInEditor = true; ///< Create terrain in the editor.
        AZStd::unique_ptr<Physics::RigidBodyStatic> m_editorTerrain; ///< Terrain object in the editor.

        AZ_DISABLE_COPY_MOVE(EditorTerrainComponent)
    };
} // namespace PhysX
