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

#include <AzCore/std/containers/map.h>

#include <ScriptCanvas/CodeGen/CodeGen.h>
#include <ScriptCanvas/Core/Node.h>

#include <Include/ScriptCanvas/Internal/Nodes/StringFormatted.generated.h>

namespace ScriptCanvas
{
    namespace Nodes
    {
        namespace Internal
        {
            //! This node is intended as a base-class for any node that requires the string formatted capabilities
            //! of generating slots based on curly bracket formatted text to produce input slots.
            class StringFormatted
                : public Node
            {
            public:
                ScriptCanvas_Node(StringFormatted,
                    ScriptCanvas_Node::Name("StringFormatted", "Base class for any nodes that use string formatting capabilities.")
                    ScriptCanvas_Node::Uuid("{0B1577E0-339D-4573-93D1-6C311AD12A13}")
                    ScriptCanvas_Node::Category("Internal")
                    ScriptCanvas_Node::Version(1)
                );

            protected:

                // Inputs
                ScriptCanvas_In(ScriptCanvas_In::Name("In", "Input signal"));

                // Outputs
                ScriptCanvas_Out(ScriptCanvas_Out::Name("Out", ""));

                ScriptCanvas_EditPropertyWithDefaults(AZStd::string, m_format, "{Value}",
                    EditProperty::Name("String", "The format string; any word within {} will create a data pin on the node.")
                    EditProperty::EditAttributes(AZ::Edit::Attributes::ChangeNotify(&StringFormatted::OnFormatChanged)
                    )
                );

                ScriptCanvas_EditPropertyWithDefaults(int, m_numericPrecision, 4,
                    EditProperty::Name("Precision", "The precision with which to print any numeric values.")
                    EditProperty::EditAttributes(AZ::Edit::Attributes::Min(0) AZ::Edit::Attributes::Max(24))
                );

                // This is a map that binds the index into m_unresolvedString to the SlotId that needs to be checked for a valid datum.
                using ArrayBindingMap = AZStd::map<AZ::u64, SlotId>;
                ScriptCanvas_SerializeProperty(ArrayBindingMap, m_arrayBindingMap);

                // A vector of strings that holds all the parts of the string and reserves empty strings for those parts of the string whose
                // values come from slots.
                ScriptCanvas_SerializeProperty(AZStd::vector<AZStd::string>, m_unresolvedString);

                // Maps the slot name to the created SlotId for that slot
                using NamedSlotIdMap = AZStd::map<AZStd::string, SlotId>;
                ScriptCanvas_SerializeProperty(NamedSlotIdMap, m_formatSlotMap);

                void OnInit() override;

                // Parses the format field to produce the intermediate data for strings that use curly brackets to produce slots.
                void ParseFormat();

                // Called when a change to the format string is detected.
                AZ::u32 OnFormatChanged();

                //! Parses the user specified format and resolves the data from the appropriate slots.
                //! \return AZStd::string The fully formatted and resolved string ready for output.
                AZStd::string ProcessFormat();
            };
        }
    }
}