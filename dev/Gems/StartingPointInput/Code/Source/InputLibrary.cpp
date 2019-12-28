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
#include <StartingPointInput_precompiled.h>
#include <InputLibrary.h>

#include <AzCore/Serialization/EditContext.h>

// script canvas
#include <ScriptCanvas/Libraries/Libraries.h>

#include <InputNode.h>


namespace Input
{
    void InputLibrary::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<InputLibrary, LibraryDefinition>()
                ->Version(1)
                ;

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<InputLibrary>("Input", "")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/InputConfig.png")
                    ;
            }
        }
    }

    void InputLibrary::InitNodeRegistry(ScriptCanvas::NodeRegistry& nodeRegistry)
    {
        ScriptCanvas::Library::AddNodeToRegistry<InputLibrary, InputNodes::InputNode>(nodeRegistry);
    }

    AZStd::vector<AZ::ComponentDescriptor*> InputLibrary::GetComponentDescriptors()
    {
        return AZStd::vector<AZ::ComponentDescriptor*>({
            InputNodes::InputNode::CreateDescriptor(),
        });
    }

} // namespace Input
