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

#include <AzCore/Serialization/EditContextConstants.inl>

#include <ScriptCanvas/CodeGen/CodeGen.h>
#include <ScriptCanvas/Core/GraphBus.h>
#include <ScriptCanvas/Core/Node.h>
#include <ScriptCanvas/Data/PropertyTraits.h>
#include <ScriptCanvas/Variable/VariableBus.h>
#include <ScriptCanvas/Variable/VariableCore.h>

#include <Include/ScriptCanvas/Libraries/Core/GetVariable.generated.h>

namespace ScriptCanvas
{
    namespace Nodes
    {
        namespace Core
        {
            class GetVariableNode
                : public Node
                , protected VariableNotificationBus::Handler
                , protected VariableNodeRequestBus::Handler
            {
            public:
                ScriptCanvas_Node(GetVariableNode,
                    ScriptCanvas_Node::Name("Get Variable", "Node for referencing a property within the graph")
                    ScriptCanvas_Node::Uuid("{8225BE35-4C45-4A32-94D9-3DE114F6F5AF}")
                    ScriptCanvas_Node::Icon("Editor/Icons/ScriptCanvas/Placeholder.png")
                    ScriptCanvas_Node::Version(0)
                );

                //// VariableNodeRequestBus
                void SetId(const VariableId& variableId) override;
                const VariableId& GetId() const override;
                //// 
                const Datum* GetDatum() const;

                const SlotId& GetDataOutSlotId() const;

            protected:
                void OnInit() override;
                void OnInputSignal(const SlotId&) override;

                void AddOutputSlot();
                void RemoveOutputSlot();

                AZStd::vector<AZStd::pair<VariableId, AZStd::string>> GetGraphVariables() const;
                void OnIdChanged(const VariableId& oldVariableId);

                // VariableNotificationBus
                void OnVariableRemoved() override;
                ////

                // Adds/Remove Property Slots from the GetVariable node
                void AddPropertySlots(const Data::Type& type);
                void ClearPropertySlots();

                void RefreshPropertyFunctions();

                ScriptCanvas_In(ScriptCanvas_In::Name("In", "When signaled sends the property referenced by this node to a Data Output slot"));

                // Outputs
                ScriptCanvas_Out(ScriptCanvas_Out::Name("Out", "Signaled after the referenced property has been pushed to the Data Output slot"));

                ScriptCanvas_EditPropertyWithDefaults(VariableId, m_variableId, , EditProperty::NameLabelOverride("Variable Name"),
                    EditProperty::DescriptionTextOverride("Name of ScriptCanvas Variable"),
                    EditProperty::UIHandler(AZ::Edit::UIHandlers::ComboBox),
                    EditProperty::EditAttributes(AZ::Edit::Attributes::GenericValueList(&GetVariableNode::GetGraphVariables), AZ::Edit::Attributes::PostChangeNotify(&GetVariableNode::OnIdChanged)));

                ScriptCanvas_SerializeProperty(SlotId, m_variableDataOutSlotId);

                ScriptCanvas_SerializeProperty(AZStd::vector<Data::PropertyMetadata>, m_propertyAccounts);
            };
        }
    }
}