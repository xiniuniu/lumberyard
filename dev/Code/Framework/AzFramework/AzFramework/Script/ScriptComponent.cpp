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

#if !defined(AZCORE_EXCLUDE_LUA)

#include <AzCore/Script/ScriptContext.h>
#include <AzCore/Script/ScriptSystemBus.h>
#include <AzCore/Script/ScriptProperty.h>

#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>

#include <AzCore/std/string/conversions.h>

#include <AzFramework/Script/ScriptComponent.h>
#include <AzFramework/Script/ScriptNetBindings.h>

#include <AzFramework/Network/NetworkContext.h>
#include <AzFramework/StringFunc/StringFunc.h>

#include <GridMate/Replica/ReplicaChunk.h>

extern "C" {
#   include <Lua/lualib.h>
#   include <Lua/lauxlib.h>
}

namespace AzFramework
{
    //  We generally have the following data types which we can use for editor properties
    //  simple: numbers, strings, booleans, arrays, entity
    //  complex: colors, vectors, normals, matrix,
    //
    // MyScriptComponent = {
    //      Properties = {
    //          m_speed = {
    //              default = 0,   // Support for numerical, string or boolean
    //              min = 0,
    //              max = 100,
    //              step = 1,
    //              description = "Speed in m/s for the ...",
    //          },
    //          Colors = {
    //              front = {
    //                  default = Vector4(255,255,255),
    //              },
    //              back = {
    //                  default = Vector4(128,0,0,128)
    //              }
    //          },
    //          Jump = {
    //              enabled = false,
    //              m_jumpDirection = {
    //                  default = Vector3(0,1,0),
    //              },
    //              m_jumpPosition = {
    //                  default = Vector3(0,0,0),
    //              },
    //          },
    //          m_target = {
    //              default = EntityId(),
    //          },
    //          m_elementSeed = {1,2,3,4,5}, // default array type no attibutes
    //
    //          m_networkedProperty =
    //          {
    //                default = 10,                // Configure the property like you would any other element
    //
    //
    //                netSynched = {                // Add in this special table called netSynched, and the value will be now be synchornized across the network
    //                    Enabled = true,           // Control flag for turning the network synching on and off without removing the entire table. Defaults to true if missing
    //                    OnNewValue = function,    // OnNewValue will be called whenever the the property has a new value
    //                    ForceIndex = [1..32]      // Profiling helper tool to force a property to use a particular DataSet to help avoid ambiguity within the profiler's Replica Usage.
    //                }
    //          }
    //      }
    //
    //      // NetRPCs are a table of RPC calls to be made across the network.
    //      NetRPCs =
    //      {
    //          // Construct a table inside of here, the name of which will be the RPC
    //          SampleRPC =
    //          {
    //              // Two callbacks can be registered to the NetRPC
    //              // A function to be invoked on the Master - OnMaster
    //              // and a function to be invoked on the Proxy - OnProxy
    //              //
    //              // Every NetRPC needs to  have a valid OnMaster function, while OnProxy is optional.
    //              OnMaster = function()
    //                  Debug.Log("Function to be invoked on the Master.");
    //              end
    //
    //              OnProxy = function()
    //                  Debug.Log("Function to be invoked on the Proxy.");
    //              end
    //          }
    //
    //      }
    // }
    //
    // function MyScriptComponent:Activate()
    //      self.NetRPCs.SampleRPC("Invoking an RPC");
    // end
    //
    // function MyScriptComponent:Deactivate()
    //
    // end

    //  simple: numbers, strings, booleans, arrays, entity
    //  complex: vectors, reflected classes

    namespace Internal
    {
        //=========================================================================
        // Properties__IndexFindSubtable
        //=========================================================================
        static bool Properties__IndexFindSubtable(lua_State* lua, int lookupTable, int entityProperties, int scriptProperties)
        {
            LSV_BEGIN(lua, 0);

            lua_pushnil(lua);
            while (lua_next(lua, entityProperties))
            {
                if (lua_istable(lua, -1))
                {
                    lua_pushvalue(lua, -2); // copy the key
                    //const char* key = lua_tostring(lua, -1);
                    //(void)key;
                    lua_rawget(lua, scriptProperties); // load the table from the script properties
                    if (lua_istable(lua, -1))
                    {
                        if (lua_topointer(lua, -2) == lua_topointer(lua, lookupTable))
                        {
                            return true;
                        }
                        else
                        {
                            if (Properties__IndexFindSubtable(lua, lookupTable, lua_gettop(lua) - 1, lua_gettop(lua)))
                            {
                                return true;
                            }
                        }
                    }
                    lua_pop(lua, 1); // pop the table result
                }
                lua_pop(lua, 1); // pop the value
                // leave the key for next iteration
            }
            return false;
        }
        //=========================================================================
        // Properties__Index
        //=========================================================================
        static int Properties__Index(lua_State* lua)
        {
            LSV_BEGIN(lua, 1);

            // calling format __index(table,key)
            ScriptNetBindingTable* netBindingTable = reinterpret_cast<ScriptNetBindingTable*>(lua_touserdata(lua, lua_upvalueindex(1)));

            bool readValue = false;

            if (netBindingTable != nullptr)
            {
                AZ_Error("ScriptComponent",netBindingTable->GetScriptContext() != nullptr,"ScriptNetBindingTable is missing ScriptContext.");
                AZ_Error("ScriptComponent",netBindingTable->GetScriptContext() == nullptr || netBindingTable->GetScriptContext()->NativeContext() == lua,"Trying to use a NetBindingTable in wrong lua context");

                AZ::ScriptContext* scriptContext = netBindingTable->GetScriptContext();

                if (scriptContext)
                {
                    AZ::ScriptDataContext stackContext;
                    scriptContext->ReadStack(stackContext);

                    readValue = netBindingTable->InspectTableValue(stackContext);
                }
            }

            if (!readValue)
            {
                AZ::ScriptContext::FromNativeContext(lua)->Error(AZ::ScriptContext::ErrorType::Warning, true,
                    "Property %s not found in entity table. Please push this property to your slice to avoid decrease in performance.", lua_tostring(lua, -1));
                int lookupKey = lua_gettop(lua);

                int lookupTable = lookupKey - 1;
                // This is a slow function and it's made slow so we don't cache any extra data.
                // This is done because this function will be called only the exported components
                // and script are not in sync and we added new properties.
                lua_getmetatable(lua, -2); // get the metatable which will be the top property table
                int entityProperties = lua_gettop(lua);
                if (lua_getmetatable(lua, -1) == 0) // get the matateble of the property which will be the original table
                {
                    // we are looking at top level properties
                    lua_pushvalue(lua, -2); // copy the key
                    lua_rawget(lua, -2); // read the value
                }
                else
                {
                    // we are looking into the sub table, so do a slow traversal
                    int scriptProperties = lua_gettop(lua);
                    if (!Properties__IndexFindSubtable(lua, lookupTable, entityProperties, scriptProperties))
                    {
                        lua_pushnil(lua);
                        return 1; // we did not find the table
                    }
                    else
                    {
                        lua_pushvalue(lua, lookupKey);
                        lua_rawget(lua, -2);
                    }
                }

                if (lua_istable(lua, -1))
                {
                    // if we are here the target table is on the top if the stack
                    lua_pushstring(lua, ScriptComponent::DefaultFieldName);
                    lua_rawget(lua, -2);
                    if (lua_isnil(lua, -1))
                    {
                        // parent table is a group, pop the value and return the table
                        lua_pop(lua, 1);
                    }
                }

                // Duplicate the value, so once the storage is done its on top of the stack, and returned
                lua_pushvalue(lua, -1);

                // Push key, and then move it below the value
                lua_pushvalue(lua, lookupKey);
                lua_insert(lua, -2);

                // Cache the value so that subsequent accesses to this property don't result in warnings
                lua_rawset(lua, lookupTable);
            }
            return 1;
        }
        //=========================================================================
        // Properties__NewIndex
        //=========================================================================
        static int Properties__NewIndex(lua_State* lua)
        {
            LSV_BEGIN_VARIABLE(lua);

            // calling format __newindex(table,key,value)
            ScriptNetBindingTable* netBindingTable = reinterpret_cast<ScriptNetBindingTable*>(lua_touserdata(lua, lua_upvalueindex(1)));
            if (netBindingTable != nullptr)
            {
                AZ_Error("ScriptContext",netBindingTable->GetScriptContext() != nullptr,"ScriptNetBindingTable is missing ScriptContext.");
                AZ_Error("ScriptContext",netBindingTable->GetScriptContext() == nullptr || netBindingTable->GetScriptContext()->NativeContext() == lua,"Trying to use a NetBindingTable in wrong lua context");

                AZ::ScriptContext* scriptContext = netBindingTable->GetScriptContext();
                if (scriptContext)
                {
                    AZ::ScriptDataContext stackContext;
                    scriptContext->ReadStack(stackContext);

                    const bool assignedValue = netBindingTable->AssignTableValue(stackContext);
                    if (assignedValue)
                    {
                        LSV_END_VARIABLE(0);
                        return 0;
                    }
                }
            }

            // If we didn't assign the value above, we want
            // to raw set the value to avoid coming back in here.
            lua_rawset(lua, 1);
            LSV_END_VARIABLE(-2);
            return 0;
        }
    } // namespace Internal

    // The code will create a table with uniqueEntityName which has
    // MyScriptComponent as a metatable. A full new copy of the property table will be declared will the
    // appropriate variables from editor. Every other table variable should be replicated.
    //=========================================================================
    // ScriptComponent
    // [8/9/2013]
    //=========================================================================

    const char* ScriptComponent::NetRPCFieldName = "NetRPCs";
    const char* ScriptComponent::DefaultFieldName = "default";

    ScriptComponent::ScriptComponent()
        : m_context(nullptr)
        , m_contextId(AZ::ScriptContextIds::DefaultScriptContextId)
        , m_script(AZ::Data::AssetLoadBehavior::PreLoad)
        , m_table(LUA_NOREF)
        , m_netBindingTable(nullptr)
    {
        m_properties.m_name = "Properties";
    }

    //=========================================================================
    // ~PlacementComponent
    // [8/9/2013]
    //=========================================================================
    ScriptComponent::~ScriptComponent()
    {
        m_properties.Clear();

        delete m_netBindingTable;
    }

    //=========================================================================
    // SetScriptContext
    // [8/9/2013]
    //=========================================================================
    void ScriptComponent::SetScriptContext(AZ::ScriptContext* context)
    {
        AZ_Assert(m_entity == nullptr || m_entity->GetState() != AZ::Entity::ES_ACTIVE, "You can't change the context while the entity is active");
        m_context = context;
        m_contextId = context->GetId();
    }

    //=========================================================================
    // SetScriptName
    // [8/9/2013]
    //=========================================================================
    void ScriptComponent::SetScript(const AZ::Data::Asset<AZ::ScriptAsset>& script)
    {
        AZ_Assert(m_entity == nullptr || m_entity->GetState() != AZ::Entity::ES_ACTIVE, "You can't change the script while the entity is active");

        m_script = script;
    }

    void ScriptComponent::Init()
    {
        // Grab the script context
        EBUS_EVENT_RESULT(m_context, AZ::ScriptSystemRequestBus, GetContext, m_contextId);
        AZ_Assert(m_context, "We must have a valid script context!");
    }

    //=========================================================================
    // Activate
    // [8/12/2013]
    //=========================================================================
    void ScriptComponent::Activate()
    {
        if (m_isSyncEnabled && m_netBindingTable == nullptr)
        {
            m_netBindingTable = aznew ScriptNetBindingTable();
        }

        // if we have valid asset listen for script asset events, like reload
        if (m_script.GetId().IsValid())
        {
            AZ::Data::AssetBus::Handler::BusConnect(m_script.GetId());
            m_script.QueueLoad();
        }
    }

    //=========================================================================
    // Deactivate
    // [8/12/2013]
    //=========================================================================
    void ScriptComponent::Deactivate()
    {
        AZ::Data::AssetBus::Handler::BusDisconnect(m_script.GetId());

        UnloadScript();
    }

    //=========================================================================
    // ScriptComponent::OnAssetReady
    //=========================================================================
    void ScriptComponent::OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        if (asset)
        {
            m_script = asset;
            LoadScript();
        }
    }

    //=========================================================================
    // ScriptComponent::OnAssetReloaded
    //=========================================================================
    void ScriptComponent::OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        // When we reload a script it's tricky to maintain state for that entity instance,
        // even though theoretical mapping is possible. This feature is used for
        // faster in game iteration and based on feedbacks game designers do expect on reload to
        // reset the state.
        // If we don't need to fix the state, all we need to unload and reload the script.
        // To make sure the script behaves properly (external connections and other resources)
        // we call deactivate and activate after reload. This can cause performance issues if the
        // script registers "expensive" to register assets. If this becomes a problem we should
        // provide mapping.
        // Other things to consider are properties. Reloading will just reload the code.
        // Properties are exported from ScriptEditorComponent, so they will not be updated by
        UnloadScript();

        m_script = asset;

        LoadScript();
    }

    //=========================================================================
    // ScriptComponent::GetNetworkBinding
    //=========================================================================
    GridMate::ReplicaChunkPtr ScriptComponent::GetNetworkBinding()
    {
        if (m_netBindingTable == nullptr)
        {
            m_netBindingTable = aznew ScriptNetBindingTable();
        }

        return m_netBindingTable->GetNetworkBinding();
    }

    //=========================================================================
    // ScriptComponent::SetNetworkBinding
    //=========================================================================
    void ScriptComponent::SetNetworkBinding(GridMate::ReplicaChunkPtr chunk)
    {
        if (m_netBindingTable == nullptr)
        {
            m_netBindingTable = aznew ScriptNetBindingTable();
        }

        m_netBindingTable->SetNetworkBinding(chunk);
    }

    //=========================================================================
    // ScriptComponent::UnbindFromNetwork
    //=========================================================================
    void ScriptComponent::UnbindFromNetwork()
    {
        if (m_netBindingTable)
        {
            m_netBindingTable->UnbindFromNetwork();
        }
    }

    //=========================================================================
    // LoadScript
    //=========================================================================
    void ScriptComponent::LoadScript()
    {
        AZ_PROFILE_SCOPE_DYNAMIC(AZ::Debug::ProfileCategory::Script, "Load: %s", m_script.GetHint().c_str());

        // Load the script, find the base table, create the entity table
        // find the Activate/Deactivate functions in the script and call them
        if (LoadInContext())
        {
            CreateEntityTable();
        }
    }

    //=========================================================================
    // UnloadScript
    //=========================================================================
    void ScriptComponent::UnloadScript()
    {
        AZ_PROFILE_SCOPE_DYNAMIC(AZ::Debug::ProfileCategory::Script, "Unload: %s", m_script.GetHint().c_str());

        DestroyEntityTable();

        if (m_netBindingTable)
        {
            m_netBindingTable->Unload();
        }
    }

    //=========================================================================
    // LoadInContext
    // [3/3/2014]
    //=========================================================================
    bool ScriptComponent::LoadInContext()
    {
        LSV_BEGIN(m_context->NativeContext(), 1);

        // Set the metamethods as we will use the script table as a metatable for entity tables
        bool success = false;
        EBUS_EVENT_RESULT(success, AZ::ScriptSystemRequestBus, Load, m_script, m_contextId);
        if (!success)
        {
            return false;
        }

        lua_State* lua = m_context->NativeContext();
        if (!lua_istable(lua, -1))
        {
            AZ::Data::AssetInfo assetInfo;
            AZ::Data::AssetCatalogRequestBus::BroadcastResult(assetInfo, &AZ::Data::AssetCatalogRequestBus::Events::GetAssetInfoById, m_script.GetId());

            AZStd::string tableName = assetInfo.m_relativePath;
            AzFramework::StringFunc::Path::GetFileName(tableName.c_str(), tableName);
            AZStd::to_lower(tableName.begin(), tableName.end());
            m_context->Error(AZ::ScriptContext::ErrorType::Error, false, "Script %s is setup incorrectly. Scripts must now return a table (for example, you may need to add \"return %s\" to the end of the file).", assetInfo.m_relativePath.c_str(), tableName.c_str());
            lua_pop(lua, 1);
            return false;
        }

        // Point the __index of the Script table to itself
        // because it will be used as a metatable
        lua_pushliteral(lua, "__index");
        lua_pushvalue(lua, -2);
        lua_rawset(lua, -3);

        lua_pushlstring(lua, m_properties.m_name.c_str(), m_properties.m_name.length()); // load Property table name
        lua_rawget(lua, -2);
        if (lua_istable(lua, -1))
        {
            // This property table will be used a metatable from all instances
            // set the __index so we can read values in case we change the script
            // after we export the component (so the properties will not the default value)
            lua_pushliteral(lua, "__index");
            lua_pushlightuserdata(lua, m_netBindingTable);
            lua_pushcclosure(lua, &Internal::Properties__Index, 1);
            lua_rawset(lua, -3);

            lua_pushliteral(lua, "__newindex");
            lua_pushlightuserdata(lua, m_netBindingTable);
            lua_pushcclosure(lua, &Internal::Properties__NewIndex, 1);
            lua_rawset(lua, -3);
        }
        lua_pop(lua, 1); // pop the properties table (or the nil value)
        // Leave the script table on the stack for CreateEntityTable().

        return true;
    }

    //=========================================================================
    // CopyEntityTables
    // Mirror source entity's subtables (excluding property table, which is handled \ref CreatePropertyGroup)
    // The goal is to ensure that if we modify any table, modifications are isolated to the instance (not the global/shared entity table)
    // In addition set to entity tables as a metatable so cloned tables for read-only operations.
    //=========================================================================
    void CopyAndHookEntityTables(lua_State* lua, int sourceTable, int targetTable, const char* propertyTableName)
    {
        LSV_BEGIN(lua, 0);

        lua_pushnil(lua); // first key
        while (lua_next(lua, sourceTable))
        {
            if (lua_istable(lua, -1))
            {
                int keyType = lua_type(lua, -2);
                switch (keyType)
                {
                    case LUA_TSTRING:
                    {
                        const char* tableName = lua_tolstring(lua, -2, nullptr);
                        if (strncmp(tableName, "__", 2) == 0 || // skip metatables
                            strcmp(tableName, propertyTableName) == 0 || // check if this is NOT the property table
                            strcmp(tableName, ScriptComponent::NetRPCFieldName) == 0) // Want to skip the RPC table as well
                        {
                            break;
                        }
                        // else fall through
                    }

                    case LUA_TNUMBER:
                    {
                        // stack: source subtable, source key

                        // Create the target subtable.
                        lua_createtable(lua, 0, 0);
                        lua_pushvalue(lua, -3);             // push source subtable key
                        lua_pushvalue(lua, -2);             // push new subtable instance
                        lua_rawset(lua, targetTable);       // targetTable.subtableKey = newSubTable

                        // The source subtable will be used as a metatable, so set its __index to itself.
                        lua_pushliteral(lua, "__index");    // push __index key
                        lua_pushvalue(lua, -3);             // push the source subtable
                        lua_rawset(lua, -1);                // sourceSubtable.__index = sourceSubtable;

                        // Set the target subtable's metatable to the source subtable.
                        lua_pushvalue(lua, -2);             // push the source subtable
                        lua_setmetatable(lua, -2);          // setmetatable(targetSubtable, sourceSubtable)

                        int newTarget = lua_gettop(lua);
                        int newSource = newTarget - 1;
                        CopyAndHookEntityTables(lua, newSource, newTarget, propertyTableName);

                        lua_pop(lua, 1); // pop the new table
                    } break;

                    default:
                    {
                        AZ_Warning("Script", false, "Tables associated with non string/number keys are not copied, thus will be lost in new instances.");
                    } break;
                }
            }

            lua_pop(lua, 1); // pop the value
            // leave the key for next iteration
        }
    }

    //=========================================================================
    // CreateEntityTable
    // [3/3/2014]
    //=========================================================================
    void ScriptComponent::CreateEntityTable()
    {
        lua_State* lua = m_context->NativeContext();
        LSV_BEGIN(lua, -1);

        AZ_Error("Script", lua_istable(lua, -1), "%s", "Script did not return a table!");
        int baseTableIndex = lua_gettop(lua);

        // Stack: base

        lua_pushlstring(lua, m_properties.m_name.c_str(), m_properties.m_name.length());
        lua_rawget(lua, baseTableIndex);
        AZ_Error("Script", lua_istable(lua, -1) || lua_isnil(lua, -1), "We should have the %s table for properties!", m_properties.m_name.c_str());
        int basePropertyTable = -1;
        if (lua_istable(lua, -1))
        {
            basePropertyTable = lua_gettop(lua);
        }

        // Stack: base, properties

        lua_createtable(lua, 0, 1); // Create entity table;
        int entityTableIndex = lua_gettop(lua);

        // Stack: base, properties, entity

        // Create our network binding.
        CreateNetworkBindingTable(baseTableIndex, entityTableIndex);

        if (basePropertyTable > -1) // if property table exists
        {
            CreatePropertyGroup(m_properties, basePropertyTable, lua_gettop(lua), basePropertyTable, true);
        }

        // replicate other tables to make sure we have table per instance.
        CopyAndHookEntityTables(lua, baseTableIndex, lua_gettop(lua), m_properties.m_name.c_str());

        // set my entity id
        lua_pushliteral(lua, "entityId");
        AZ::ScriptValue<AZ::EntityId>::StackPush(lua, GetEntityId());
        lua_rawset(lua, -3);

        // set the base metatable
        lua_pushvalue(lua, baseTableIndex); // copy the "Base table"
        lua_setmetatable(lua, -2); // set the scriptTable as a metatable for the entity table

        // Keep the entity table in the registry
        m_table = luaL_ref(lua, LUA_REGISTRYINDEX);

        if (m_netBindingTable)
        {
            m_netBindingTable->FinalizeNetworkTable(m_context, m_table);
        }

        // call OnActivate
        lua_pushliteral(lua, "OnActivate");
        lua_rawget(lua, baseTableIndex); // ScriptTable[OnActivate]
        if (lua_isfunction(lua, -1))
        {
            AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::Script, "OnActivate");
            lua_rawgeti(lua, LUA_REGISTRYINDEX, m_table); // push the entity table as the only argument
            AZ::Internal::LuaSafeCall(lua, 1, 0); // Call OnActivate
        }
        else
        {
            lua_pop(lua, 1); // remove the OnActivate result
        }

        lua_pop(lua, 2); // remove the base property table and base script table
    }

    //=========================================================================
    // DestroyEntityTable
    //=========================================================================
    void ScriptComponent::DestroyEntityTable()
    {
        if (m_table != LUA_NOREF)
        {
            lua_State* lua = m_context->NativeContext();
            LSV_BEGIN(lua, 0);

            // call OnDeactivate
            // load table
            lua_rawgeti(lua, LUA_REGISTRYINDEX, m_table);
            if (lua_getmetatable(lua, -1) == 0) // load the base table
            {
                AZ_Assert(false, "This should not happen, all entity table should have the base table as a metatable!");
            }
            // cache
            lua_pushliteral(lua, "OnDeactivate");
            lua_rawget(lua, -2); // ScriptTable[OnDeactivte]
            if (lua_isfunction(lua, -1))
            {
                AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::Script, "OnDeactivate");

                lua_pushvalue(lua, -3); // push the entity table as the only argument
                AZ::Internal::LuaSafeCall(lua, 1, 0); // Call OnDeactivate
            }
            else
            {
                lua_pop(lua, 1); // remove the OnDeactivate result
            }
            lua_pop(lua, 2); // remove the base table and the entity table

            // release table reference
            luaL_unref(lua, LUA_REGISTRYINDEX, m_table);
            m_table = LUA_NOREF;
        }
    }

    //=========================================================================
    // CreateNetworkBindingTable
    // [6/27/2016]
    //=========================================================================
    void ScriptComponent::CreateNetworkBindingTable(int baseTableStack, int entityTableStack)
    {
        if (m_netBindingTable)
        {
            m_netBindingTable->CreateNetworkBindingTable(m_context, baseTableStack, entityTableStack);
        }
    }

    //=========================================================================
    // CreatePropertyGroup
    // [3/3/2014]
    //=========================================================================
    void ScriptComponent::CreatePropertyGroup(const ScriptPropertyGroup& group, int propertyGroupTableIndex, int parentIndex, int metatableIndex, bool isRoot)
    {
        lua_State* lua = m_context->NativeContext();
        LSV_BEGIN(lua, 0);

        lua_pushlstring(lua, group.m_name.c_str(), group.m_name.length()); // push table key
        lua_createtable(lua, 0, static_cast<int>(group.m_properties.size() + group.m_groups.size())); // create new table

        if (isRoot)
        {
            // this is the root table (properties) it will be used as properties for all sub tables
            lua_pushliteral(lua, "__index");
            lua_pushlightuserdata(lua, m_netBindingTable);
            lua_pushcclosure(lua, &Internal::Properties__Index, 1);
            lua_rawset(lua, -3);

            lua_pushliteral(lua, "__newindex");
            lua_pushlightuserdata(lua, m_netBindingTable);
            lua_pushcclosure(lua, &Internal::Properties__NewIndex, 1);
            lua_rawset(lua, -3);

            lua_pushvalue(lua, metatableIndex);
            lua_setmetatable(lua, -2);

            metatableIndex = lua_gettop(lua); // this should the metatable for all subtables
        }
        else
        {
            lua_pushvalue(lua, metatableIndex);
            lua_setmetatable(lua, -2);
        }

        for (size_t i = 0; i < group.m_properties.size(); ++i)
        {
            AZ::ScriptProperty* prop = group.m_properties[i];

            if (m_netBindingTable != nullptr)
            {
                lua_pushlstring(lua, prop->m_name.c_str(), prop->m_name.length());
                lua_rawget(lua, propertyGroupTableIndex);

                if (lua_istable(lua, -1))
                {
                    bool isPropertyHandled = false;

                    AZ::ScriptDataContext stackContext;

                    // If we find a table value. We want to inspect it for information.
                    if (m_context->ReadStack(stackContext))
                    {
                        lua_pushliteral(lua, "netSynched");
                        lua_rawget(lua, -2);
                        if (stackContext.IsTable(-1))
                        {
                            AZ::ScriptDataContext networkTableContext;
                            if (stackContext.InspectTable(-1, networkTableContext))
                            {
                                isPropertyHandled = m_netBindingTable->RegisterDataSet(networkTableContext, prop);
                            }
                        }

                        // Network binding table
                        lua_pop(lua, 1);
                    }

                    // Property name table
                    lua_pop(lua, 1);

                    // If the property is networked, we don't want to copy it over into the table.
                    if (isPropertyHandled)
                    {
                        continue;
                    }
                }
                else
                {
                    // Remove the value we just pushed onto the stack
                    lua_pop(lua, 1);
                }
            }

            lua_pushlstring(lua, prop->m_name.c_str(), prop->m_name.length());
            if (prop->Write(*m_context))
            {
                lua_rawset(lua, -3);
            }
            else
            {
                AZ_Warning("Script", false, "No data written for property %s!", prop->m_name.c_str());
                lua_pop(lua, 1); // pop the property name and skip it
            }
        }

        for (size_t i = 0; i < group.m_groups.size(); ++i)
        {
            int parentTableIndex = lua_gettop(lua);

            const ScriptPropertyGroup& subGroup = group.m_groups[i];

            lua_pushstring(lua, subGroup.m_name.c_str());
            lua_rawget(lua, propertyGroupTableIndex);

            int childPropertyGroupIndex = lua_gettop(lua);
            AZ_Assert(lua_istable(lua, childPropertyGroupIndex), "Invalid Child Property Table");

            CreatePropertyGroup(subGroup, childPropertyGroupIndex, parentTableIndex, metatableIndex, false);

            lua_remove(lua, childPropertyGroupIndex);
        }

        lua_rawset(lua, parentIndex); // set the table into the parent table
    }

    //=========================================================================
    // Reflect
    //=========================================================================
    void ScriptComponent::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            // we may have been reflected by ScriptEditorComponent already, so check first
            if (serializeContext->FindClassData("{8D1BC97E-C55D-4D34-A460-E63C57CD0D4B}") == nullptr)
            {
                auto converter = [](AZ::SerializeContext&, AZ::SerializeContext::DataElementNode& node)
                {
                    // If version 2, remove SourceScriptName
                    if (node.GetVersion() == 2)
                    {
                        int index = node.FindElement(AZ_CRC("SourceScriptName", 0x3654ac50));
                        if (index != -1)
                        {
                            node.RemoveElement(index);
                        }
                    }

                    return true;
                };

                serializeContext->Class<ScriptComponent, AZ::Component,  NetBindable>()
                    ->Version(3, converter)
                    ->Field("ContextID", &ScriptComponent::m_contextId)
                    ->Field("Properties", &ScriptComponent::m_properties)
                    ->Field("Script", &ScriptComponent::m_script)
                    ;

                serializeContext->Class<ScriptPropertyGroup>()
                    ->Field("Name", &ScriptPropertyGroup::m_name)
                    ->Field("Properties", &ScriptPropertyGroup::m_properties)
                    ->Field("Groups", &ScriptPropertyGroup::m_groups)
                    ;

                // reflect all script properties
                AZ::ScriptProperties::Reflect(reflection);
            }
        }

        ScriptNetBindingTable::Reflect(reflection);
    }

    //=========================================================================
    // GetGroup
    //=========================================================================
    ScriptPropertyGroup* ScriptPropertyGroup::GetGroup(const char* groupName)
    {
        for (ScriptPropertyGroup& subGroup : m_groups)
        {
            if (subGroup.m_name == groupName)
            {
                return &subGroup;
            }
        }
        return nullptr;
    }

    //=========================================================================
    // GetProperty
    //=========================================================================
    AZ::ScriptProperty* ScriptPropertyGroup::GetProperty(const char* propertyName)
    {
        for (AZ::ScriptProperty* property : m_properties)
        {
            if (property->m_name == propertyName)
            {
                return property;
            }
        }
        return nullptr;
    }

    //=========================================================================
    // Clear
    //=========================================================================
    void ScriptPropertyGroup::Clear()
    {
        for (AZ::ScriptProperty* property : m_properties)
        {
            delete property;
        }
        m_properties.clear();
        m_groups.clear();
    }

    //=========================================================================
    // ~ScriptPropertyGroup
    //=========================================================================
    ScriptPropertyGroup::~ScriptPropertyGroup()
    {
        Clear();
    }

    //=========================================================================
    // operator=
    //=========================================================================
    ScriptPropertyGroup& ScriptPropertyGroup::operator=(ScriptPropertyGroup&& rhs)
    {
        m_name.swap(rhs.m_name);
        m_properties.swap(rhs.m_properties);
        m_groups.swap(rhs.m_groups);

        return *this;
    }
}   // namespace AzFramework

#endif // #if !defined(AZCORE_EXCLUDE_LUA)
