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

#include <ScriptCanvas/CodeGen/CodeGen.h>

#include <Include/ScriptCanvas/Libraries/Core/ScriptEventBase.generated.h>

#include <AzCore/std/containers/map.h>

#include <ScriptCanvas/Core/Node.h>
#include <ScriptCanvas/Core/Graph.h>
#include <ScriptCanvas/Variable/VariableBus.h>

#include <ScriptEvents/ScriptEventsBus.h>
#include <ScriptEvents/ScriptEventsAsset.h>
#include <ScriptEvents/ScriptEventsAssetRef.h>

namespace AZ
{
    class BehaviorEBus;
    class BehaviorEBusHandler;
}

namespace ScriptCanvas
{
    namespace Nodes
    {
        namespace Core
        {
            namespace Internal
            {
                struct ScriptEventEntry
                {
                    AZ_TYPE_INFO(ScriptEventEntry, "{28231E8C-6F56-4A28-A19A-2931D99FB1C9}");

                    bool IsExpectingResult() const
                    {
                        return m_resultSlotId.IsValid();
                    }

                    AZ::Data::AssetId m_scriptEventAssetId;
                    AZStd::string m_eventName;
                    SlotId m_eventSlotId;
                    SlotId m_resultSlotId;
                    AZStd::vector<SlotId> m_parameterSlotIds;
                    int m_numExpectedArguments = {};
                    bool m_resultEvaluated = {};

                    static void Reflect(AZ::ReflectContext* context);
                };

                class ScriptEventBase
                    : public Node
                    , public AZ::Data::AssetBus::Handler
                    , private ScriptCanvas::ScriptEventNodeRequestBus::Handler
                {
                public:

                    using Events = AZStd::vector<ScriptEventEntry>;
                    using EventMap = AZStd::map<AZ::Crc32, ScriptEventEntry>;
                    using SlotIdMapping = AZStd::unordered_map<AZ::Uuid, ScriptCanvas::SlotId>;

                    ScriptCanvas_Node(ScriptEventBase,
                        ScriptCanvas_Node::Name("Script Event", "Base class for Script Events.")
                        ScriptCanvas_Node::Category("Internal")
                        ScriptCanvas_Node::Uuid("{B6614CEC-4788-476C-A19A-BA0A8B490C73}")
                        ScriptCanvas_Node::Version(4)
                        ScriptCanvas_Node::EditAttributes(AZ::Script::Attributes::ExcludeFrom(AZ::Script::Attributes::ExcludeFlags::All))
                    );

                    ScriptCanvas_SerializeProperty(AZ::u32, m_version);
                    ScriptCanvas_SerializeProperty(EventMap, m_eventMap);
                    ScriptCanvas_SerializeProperty(SlotIdMapping, m_eventSlotMapping)
                    ScriptCanvas_SerializeProperty(AZ::Data::AssetId, m_scriptEventAssetId);

                    ScriptEventBase();
                    virtual ~ScriptEventBase() = default;

                    void OnInit() override;
                    void OnActivate() override;
                    void OnDeactivate() override;

                    AZ::u32 GetVersion() const { return m_version; }
                    const EventMap& GetEvents() const { return m_eventMap; }
                    const AZ::Data::AssetId GetAssetId() const { return m_scriptEventAssetId; }
                    ScriptEvents::ScriptEventsAssetRef& GetAssetRef() { return m_scriptEventAsset; }
                    const ScriptEvents::ScriptEvent& GetScriptEvent() const { return m_definition; }

                    AZ::Data::Asset<ScriptEvents::ScriptEventsAsset> GetAsset() const;

                    virtual void UpdateScriptEventAsset() { AZ_Error("ScriptCanvas", false, "Unimplemented update function for ScriptEvent Node"); };

                protected:

                    virtual void Initialize(const AZ::Data::AssetId assetId);

                    // AZ::Data::AssetBus::Handler
                    void OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset) override;
                    void OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset) override;
                    //

                    virtual void OnScriptEventReady(const AZ::Data::Asset<ScriptEvents::ScriptEventsAsset>&) {}
                    static void OnAssetChanged(const AZ::Data::Asset<ScriptEvents::ScriptEventsAsset>&, void* userData);

                    AZStd::intrusive_ptr<ScriptEvents::Internal::ScriptEvent> m_scriptEvent;
                    ScriptEvents::ScriptEventsAssetRef m_scriptEventAsset;

                    ScriptEvents::ScriptEvent m_definition; // Don't serialize.

                    AZ::BehaviorEBus* m_ebus = nullptr;

                };

            } // namespace Internal
        } // namespace Core
    } // namespace Nodes
} // namespace ScriptCanvas
