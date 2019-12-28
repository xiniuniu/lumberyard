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

#include <AzCore/Asset/AssetManager.h>
#include <AzCore/RTTI/BehaviorContext.h>

#include <ScriptEvents/Internal/BehaviorContextBinding/BehaviorContextFactoryMethods.h>
#include <ScriptEvents/Internal/BehaviorContextBinding/ScriptEventBinding.h>
#include <ScriptEvents/Internal/VersionedProperty.h>

namespace ScriptEvents
{
    static AZ::Outcome<bool, AZStd::string> IsAddressableType(const AZ::Uuid& uuid)
    {
        static AZStd::pair<AZ::Uuid, const char*> unsupportedTypes[] = {
            { AZ::Uuid::CreateNull(), "null" },
            { azrtti_typeid<void>(), "void" },
            { azrtti_typeid<float>(), "float" },
            { azrtti_typeid<double>(), "double" } // Due to precision issues, floating point numbers make poor address types
        };

        for (auto& unsupportedType : unsupportedTypes)
        {
            if (unsupportedType.first == uuid)
            {
                return AZ::Failure(AZStd::string::format("The type %s with id %s is not supported as an address type.", unsupportedType.second, uuid.ToString<AZStd::string>().c_str()));
            }
        }

        return AZ::Success(true);
    }

    class Parameter;

    namespace Internal
    {
        class Utils
        {
        public:
            static void BehaviorParameterFromType(AZ::Uuid typeId, bool addressable, AZ::BehaviorParameter& outParameter);
            static void BehaviorParameterFromParameter(AZ::BehaviorContext* behaviorContext, const Parameter& parameter, const char* name, AZ::BehaviorParameter& outParameter);

            static AZ::BehaviorEBus* ConstructAndRegisterScriptEventBehaviorEBus(const ScriptEvents::ScriptEvent& definition);
            static bool DestroyScriptEventBehaviorEBus(AZStd::string_view ebusName);
        };

        //! This is the internal object that represents a ScriptEvent.
        //! It provides the binding between the BehaviorContext and the messaging functionality.
        //! It is ref counted so that it remains alive as long as anything is referencing it, this can happen
        //! when multiple scripts or script canvas graphs are sending or receiving events defined in a given script event.
        class ScriptEvent
            : public AZ::Data::AssetBus::Handler
        {
        public:
            AZ_RTTI(ScriptEvent, "{B8801400-65CD-49D5-B797-58E56D705A0A}");
            AZ_CLASS_ALLOCATOR(ScriptEvent, AZ::SystemAllocator, 0);

            AZ::AttributeArray* m_currentAttributes;

            ScriptEvent() = default;
            virtual ~ScriptEvent();

            ScriptEvent(AZ::Data::AssetId scriptEventAssetId)
            {
                Init(scriptEventAssetId);
            }

            void Init(AZ::Data::AssetId scriptEventAssetId);

            bool GetMethod(AZStd::string_view eventName, AZ::BehaviorMethod*& outMethod);

            AZ::BehaviorEBus* GetBehaviorBus(AZ::u32 version = std::numeric_limits<AZ::u32>::max())
            {
                if (version == std::numeric_limits<AZ::u32>::max())
                {
                    return m_behaviorEBus[m_maxVersion];
                }

                return m_behaviorEBus[version];
            }

            void CompleteRegistration(AZ::Data::Asset<AZ::Data::AssetData> asset);

            AZStd::string GetBusName() const 
            {
                return m_busName;
            }

            bool IsReady() const { return m_isReady; }

        private:

            AZ::u32 m_maxVersion = 0;
            AZ::Data::AssetId m_assetId;

            AZStd::string m_busName;            
            AZStd::unordered_map<AZ::u32, AZ::BehaviorEBus*> m_behaviorEBus; // version, ebus

            AZStd::unordered_map<AZ::Data::AssetId, AZStd::unique_ptr<ScriptEventBinding>> m_scriptEventBindings;

            bool m_isReady = false;

            // AZ::Data::AssetBus::Handler
            void OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset) override;
            void OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset) override;
            //


            // Reference count for intrusive_ptr
            template<class T>
            friend struct AZStd::IntrusivePtrCountPolicy;
            mutable unsigned int m_refCount = 0;
            AZ_FORCE_INLINE void add_ref() { ++m_refCount; }
            AZ_FORCE_INLINE void release()
            {
                AZ_Assert(m_refCount > 0, "Reference count logic error, trying to release reference when there are none left.");
                if (--m_refCount == 0)
                {
                    delete this;
                }
            }

            AZStd::string m_previousName;
            int m_previousVersion;
        };
    }
}
