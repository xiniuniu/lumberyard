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

#include "precompiled.h"

#include "ScriptEvent.h"

#include <ScriptEvents/ScriptEventsBus.h>
#include <ScriptEvents/ScriptEventFundamentalTypes.h>
#include <ScriptEvents/ScriptEventsAsset.h>
#include <ScriptEvents/Internal/BehaviorContextBinding/ScriptEventMethod.h>
#include <ScriptEvents/Internal/BehaviorContextBinding/ScriptEventBroadcast.h>

namespace ScriptEvents
{
    namespace Internal
    {
        void Utils::BehaviorParameterFromParameter(AZ::BehaviorContext* behaviorContext, const Parameter& parameter, const char* name, AZ::BehaviorParameter& outParameter)
        {
            AZ::Uuid typeId = parameter.GetType();

            outParameter.m_azRtti = nullptr;
            outParameter.m_traits = AZ::BehaviorParameter::TR_NONE;

            const FundamentalTypes* fundamentalTypes = nullptr;
            ScriptEventBus::BroadcastResult(fundamentalTypes, &ScriptEventRequests::GetFundamentalTypes);

            if (typeId == azrtti_typeid<void>())
            {
                outParameter.m_name = name;
                outParameter.m_typeId = typeId;
            }
            else if (fundamentalTypes->IsFundamentalType(typeId))
            {
                const char* typeName = fundamentalTypes->FindFundamentalTypeName(typeId);
                outParameter.m_name = name ? name : (typeName ? typeName : "UnknownType");
                outParameter.m_typeId = typeId;
            }
            else if (const auto& behaviorClass = behaviorContext->m_typeToClassMap.at(typeId))
            {
                outParameter.m_azRtti = behaviorClass->m_azRtti;
                outParameter.m_name = name ? name : behaviorClass->m_name.c_str();
                outParameter.m_typeId = typeId;
            }
            else
            {
                outParameter.m_name = "ERROR";
                outParameter.m_typeId = AZ::Uuid::CreateNull();
                AZStd::string uuid;
                typeId.ToString(uuid);

                AZ_Error("Script Events", false, "Failed to find type %s for parameter %s", uuid.c_str(), name ? name : "UnknownType");
            }
        }

        void Utils::BehaviorParameterFromType(AZ::Uuid typeId, bool addressable, AZ::BehaviorParameter& outParameter)
        {
            AZ::BehaviorContext* behaviorContext = nullptr;
            AZ::ComponentApplicationBus::BroadcastResult(behaviorContext, &AZ::ComponentApplicationBus::Events::GetBehaviorContext);
            AZ_Assert(behaviorContext, "Script Events require a valid Behavior Context");

            outParameter.m_traits = AZ::BehaviorParameter::TR_NONE;
            outParameter.m_typeId = typeId;
            outParameter.m_azRtti = nullptr;

            if (addressable)
            {
                AZ::Outcome<bool, AZStd::string> isAddressableType = IsAddressableType(typeId);
                if (!isAddressableType.IsSuccess())
                {
                    AZ_Error("Script Events", false, "%s", isAddressableType.GetError().c_str());
                    return;
                }
            }

            const FundamentalTypes* fundamentalTypes = nullptr;
            ScriptEventBus::BroadcastResult(fundamentalTypes, &ScriptEventRequests::GetFundamentalTypes);

            if (fundamentalTypes && fundamentalTypes->IsFundamentalType(typeId))
            {
                const char* typeName = fundamentalTypes->FindFundamentalTypeName(typeId);
                outParameter.m_name = typeName;
            }
            else if (behaviorContext->m_typeToClassMap.find(typeId) != behaviorContext->m_typeToClassMap.end())
            {
                if (const auto& behaviorClass = behaviorContext->m_typeToClassMap.at(typeId))
                {
                    outParameter.m_azRtti = behaviorClass->m_azRtti;
                    outParameter.m_name = behaviorClass->m_name.c_str();
                }
            }
            else if (typeId == AZ::Uuid::CreateNull() || typeId == AZ::BehaviorContext::GetVoidTypeId())
            {
                outParameter.m_name = "void";
            }
            else
            {
                AZ_Warning("Script Events", false, "Invalid type specified for BehaviorParameter %s", typeId.ToString<AZStd::string>().c_str());
            }
        }

        AZ::BehaviorEBus* Utils::ConstructAndRegisterScriptEventBehaviorEBus(const ScriptEvents::ScriptEvent& definition)
        {
            AZ::BehaviorContext* behaviorContext = nullptr;
            AZ::ComponentApplicationBus::BroadcastResult(behaviorContext, &AZ::ComponentApplicationBus::Events::GetBehaviorContext);

            if (behaviorContext == nullptr)
            {
                return nullptr;
            }

            AZ::BehaviorEBus* bus = aznew AZ::BehaviorEBus();

            bus->m_attributes.push_back(AZStd::make_pair(AZ::RuntimeEBusAttribute, aznew AZ::Edit::AttributeData<bool>(true)));
            bus->m_name = definition.GetName();

            AZ::Uuid busIdType = azrtti_typeid<void>();
            bool addressRequired = definition.IsAddressRequired();
            if (addressRequired)
            {
                busIdType = definition.GetAddressType();
            }

            BehaviorParameterFromType(busIdType, addressRequired, bus->m_idParam);

            bus->m_createHandler = aznew DefaultBehaviorHandlerCreator(bus, behaviorContext, bus->m_name + "::CreateHandler");
            bus->m_destroyHandler = aznew DefaultBehaviorHandlerDestroyer(bus, behaviorContext, bus->m_name + "::DestroyHandler");

            for (auto& method : definition.GetMethods())
            {
                const AZStd::string& methodName = method.GetName();

                // If the script event has a valid address type, then we create an event method
                if (IsAddressableType(busIdType).IsSuccess())
                {
                    bus->m_events[methodName].m_event = aznew ScriptEventMethod(behaviorContext, definition, methodName);
                }

                // For all Script Events provide a Broadcast, using Broadcast will bypass the addressing mechanism.
                bus->m_events[methodName].m_broadcast = aznew ScriptEventBroadcast(behaviorContext, definition, methodName);
            }

            behaviorContext->m_ebuses[definition.GetBehaviorContextName()] = bus;

            return bus;
        }
        
        bool Utils::DestroyScriptEventBehaviorEBus(AZStd::string_view ebusName)
        {
            AZ::BehaviorContext* behaviorContext = nullptr;
            AZ::ComponentApplicationBus::BroadcastResult(behaviorContext, &AZ::ComponentApplicationBus::Events::GetBehaviorContext);

            if (behaviorContext == nullptr)
            {
                return false;
            }

            bool erasedBus = false;

            auto behaviorEbusEntry = behaviorContext->m_ebuses.find(ebusName);
            if (behaviorEbusEntry != behaviorContext->m_ebuses.end())
            {
                AZ::BehaviorEBus* bus = behaviorEbusEntry->second;
                delete bus;

                behaviorContext->m_ebuses.erase(behaviorEbusEntry);
                erasedBus = true;
            }

            return erasedBus;
        }


        ScriptEvent::~ScriptEvent()
        {            
            for (auto ebusPair : m_behaviorEBus)
            {                
                Utils::DestroyScriptEventBehaviorEBus(ebusPair.second->m_name);
            }

            m_scriptEventBindings.clear();
        }

        void ScriptEvent::Init(AZ::Data::AssetId scriptEventAssetId)
        {
            AZ_Assert(scriptEventAssetId.IsValid(), "Script Event requires a valid Asset Id");
            
            m_assetId = scriptEventAssetId;

            AZ::Data::AssetBus::Handler::BusConnect(scriptEventAssetId);

            auto asset = AZ::Data::AssetManager::Instance().FindAsset<ScriptEvents::ScriptEventsAsset>(m_assetId);
            if (asset && asset.IsReady())
            {
                CompleteRegistration(asset);
            }
        }

        void ScriptEvent::OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset)
        {
            CompleteRegistration(asset);
        }

        void ScriptEvent::OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset)
        {
            CompleteRegistration(asset);
        }

        void ScriptEvent::CompleteRegistration(AZ::Data::Asset<AZ::Data::AssetData> asset)
        {
            m_assetId = asset.GetId();

            const ScriptEvents::ScriptEvent& definition = asset.GetAs<ScriptEvents::ScriptEventsAsset>()->m_definition;

            if (m_behaviorEBus.find(definition.GetVersion()) != m_behaviorEBus.end())
            {
                return;
            }

            AZ::BehaviorContext* behaviorContext = nullptr;
            AZ::ComponentApplicationBus::BroadcastResult(behaviorContext, &AZ::ComponentApplicationBus::Events::GetBehaviorContext);
            AZ_Assert(behaviorContext, "Script Events require a valid Behavior Context");

            m_busName = definition.GetName();

            auto behaviorEbusEntry = behaviorContext->m_ebuses.find(definition.GetBehaviorContextName());
            if (behaviorEbusEntry != behaviorContext->m_ebuses.end())
            {
                m_behaviorEBus[definition.GetVersion()] = behaviorEbusEntry->second;

                if (m_maxVersion < definition.GetVersion())
                {
                    m_maxVersion = definition.GetVersion();
                }

                m_scriptEventBindings[m_assetId] = AZStd::make_unique<ScriptEventBinding>(behaviorContext, m_busName.c_str(), definition.GetAddressType());                

                ScriptEventNotificationBus::Broadcast(&ScriptEventNotifications::OnRegistered, definition);

                return;
            }

            AZ::BehaviorEBus* bus = Utils::ConstructAndRegisterScriptEventBehaviorEBus(definition);

            if (bus == nullptr)
            {
                return;
            }

            m_behaviorEBus[definition.GetVersion()] = bus;

            if (m_maxVersion < definition.GetVersion())
            {
                m_maxVersion = definition.GetVersion();
            }

            AZ::BehaviorContextBus::Broadcast(&AZ::BehaviorContextBus::Events::OnAddEBus, m_busName.c_str(), bus);
            m_scriptEventBindings[m_assetId] = AZStd::make_unique<ScriptEventBinding>(behaviorContext, m_busName.c_str(), definition.GetAddressType());            

            ScriptEventNotificationBus::Event(m_assetId, &ScriptEventNotifications::OnRegistered, definition);

            asset.Release();

            m_isReady = true;
        }

        bool ScriptEvent::GetMethod(AZStd::string_view eventName, AZ::BehaviorMethod*& outMethod)
        {
            AZ::BehaviorEBus* ebus = GetBehaviorBus();
            AZ_Assert(ebus, "BehaviorEBus is invalid: %s", m_busName.c_str());

            const auto& method = ebus->m_events.find(eventName);
            if (method == ebus->m_events.end())
            {
                AZ_Error("Script Events", false, "No method by name of %s found in the script event: %s", eventName.data(), m_busName.c_str());
                return false;
            }

            AZ::EBusAddressPolicy addressPolicy
                = (ebus->m_idParam.m_typeId.IsNull() || ebus->m_idParam.m_typeId == AZ::AzTypeInfo<void>::Uuid())
                ? AZ::EBusAddressPolicy::Single
                : AZ::EBusAddressPolicy::ById;

            AZ::BehaviorMethod* behaviorMethod
                = ebus->m_queueFunction
                ? (addressPolicy == AZ::EBusAddressPolicy::ById ? method->second.m_queueEvent : method->second.m_queueBroadcast)
                : (addressPolicy == AZ::EBusAddressPolicy::ById ? method->second.m_event : method->second.m_broadcast);

            if (!behaviorMethod)
            {
                AZ_Error("Script Canvas", false, "Queue function mismatch in %s-%s", eventName.data(), m_busName.c_str());
                return false;
            }

            outMethod = behaviorMethod;
            return true;
        }        
    }
}