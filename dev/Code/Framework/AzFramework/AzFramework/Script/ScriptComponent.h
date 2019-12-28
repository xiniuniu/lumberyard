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
#ifndef AZ_SCRIPT_COMPONENT_H
#define AZ_SCRIPT_COMPONENT_H

#include <AzCore/Script/ScriptAsset.h>
#include <AzCore/Script/ScriptContext.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Serialization/DynamicSerializableField.h>
#include <AzCore/Math/Crc.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/smart_ptr/intrusive_ptr.h>

#include <AzFramework/Network/NetBindable.h>

namespace AZ
{
    class ScriptProperty;
}

namespace AzToolsFramework
{
    namespace Components
    {
        class ScriptEditorComponent;
    }
}

namespace AzFramework
{
    class ScriptNetBindingTable;    

    struct ScriptPropertyGroup
    {
        AZ_TYPE_INFO(ScriptPropertyGroup, "{79682522-2f81-4b36-9fc2-a091c7504f7f}");
        AZStd::string                       m_name;
        AZStd::vector<AZ::ScriptProperty*>  m_properties;
        AZStd::vector<ScriptPropertyGroup>  m_groups;

        // Get the pointer to the specified group in m_groups. Returns nullptr if not found.
        ScriptPropertyGroup* GetGroup(const char* groupName);
        // Get the pointer to the specified property in m_properties. Returns nullptr if not found.
        AZ::ScriptProperty* GetProperty(const char* propertyName);
        // Remove all properties and groups
        void Clear();

        ScriptPropertyGroup() = default;
        ~ScriptPropertyGroup();

        ScriptPropertyGroup(const ScriptPropertyGroup& rhs) = delete;
        ScriptPropertyGroup& operator=(ScriptPropertyGroup&) = delete;
    public:
        ScriptPropertyGroup(ScriptPropertyGroup&& rhs) { *this = AZStd::move(rhs); }
        ScriptPropertyGroup& operator=(ScriptPropertyGroup&& rhs);
    };

    class ScriptComponent
        : public AZ::Component
        , private AZ::Data::AssetBus::Handler
        , public AzFramework::NetBindable
    {
        friend class AzToolsFramework::Components::ScriptEditorComponent;        

    public:
        static const char* NetRPCFieldName;
        static const char* DefaultFieldName;

        AZ_COMPONENT(AzFramework::ScriptComponent, "{8D1BC97E-C55D-4D34-A460-E63C57CD0D4B}", NetBindable);        

        ScriptComponent();
        ~ScriptComponent();

        AZ::ScriptContext* GetScriptContext() const            { return m_context; }
        void                  SetScriptContext(AZ::ScriptContext* context);

        const AZ::Data::Asset<AZ::ScriptAsset>& GetScript() const       { return m_script; }
        void                                    SetScript(const AZ::Data::Asset<AZ::ScriptAsset>& script);

    protected:
        ScriptComponent(const ScriptComponent&) = delete;
        //////////////////////////////////////////////////////////////////////////
        // Component base
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // AssetBus
        void OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset) override;
        void OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset) override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // NetBindable
        GridMate::ReplicaChunkPtr GetNetworkBinding() override;
        void SetNetworkBinding(GridMate::ReplicaChunkPtr chunk) override;
        void UnbindFromNetwork() override;
        //////////////////////////////////////////////////////////////////////////

        /// Load script (unless already by other instances) and creates the script instance into the VM
        void LoadScript();
        /// Removes the script instance and unloads the script (unless needed by other instances)
        void UnloadScript();

        /// Loads the script into the context/VM, \returns true if the script is loaded
        bool LoadInContext();

        // Create script instance table.
        void CreateEntityTable();
        void DestroyEntityTable();

        void CreateNetworkBindingTable(int baseTableIndex, int entityTableIndex);

        void CreatePropertyGroup(const ScriptPropertyGroup& group, int prototypeParentIndex, int parentIndex, int metatableIndex, bool isRoot);

        /// \red ComponentDescriptor::Reflect
        static void Reflect(AZ::ReflectContext* reflection);        

        AZ::ScriptContext*               m_context;              ///< Context in which the script will be running
        AZ::ScriptContextId                 m_contextId;            ///< Id of the script context.
        AZ::Data::Asset<AZ::ScriptAsset>    m_script;               ///< Reference to the script asset used for this component.
        int                                 m_table;                ///< Cached table index
        ScriptPropertyGroup                 m_properties;           ///< List with all properties that were tweaked in the editor and should override values in the m_scourceScriptName class inside m_script.
        ScriptNetBindingTable*              m_netBindingTable;      ///< Table that will hold our networked script values, and manage callbacks
    };        
}   // namespace AZ

#endif  // AZ_SCRIPT_COMPONENTH_
#pragma once
