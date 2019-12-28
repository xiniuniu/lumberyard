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

#if defined (SCRIPTCANVAS_EDITOR)

#include <ScriptCanvasGem.h>

#include <platform_impl.h> // must be included once per DLL so things from CryCommon will function

#include <AzCore/Component/Entity.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/Component/ComponentApplicationBus.h>

#include <AzFramework/API/ApplicationAPI.h>
#include <AzFramework/Asset/SimpleAsset.h>
#include <AzFramework/TargetManagement/TargetManagementComponent.h>

#include <ScriptCanvas/Asset/RuntimeAssetHandler.h>
#include <ScriptCanvas/Asset/RuntimeAsset.h>
#include <ScriptCanvas/Core/Graph.h>
#include <ScriptCanvas/Data/DataRegistry.h>

#include <Debugger/Debugger.h>

#include <Editor/ReflectComponent.h>
#include <Editor/SystemComponent.h>
#include <Editor/Metrics.h>

#include <Editor/Components/IconComponent.h>
#include <Editor/Model/EntityMimeDataHandler.h>

#include <Libraries/Libraries.h>
#include <Editor/Nodes/EditorLibrary.h>

#include <ScriptCanvas/Components/EditorGraph.h>
#include <ScriptCanvas/Components/EditorGraphVariableManagerComponent.h>
#include <ScriptCanvas/Components/EditorScriptCanvasComponent.h>

#include <Asset/EditorAssetSystemComponent.h>
#include <Builder/ScriptCanvasBuilderComponent.h>
#include <Editor/GraphCanvas/Components/DynamicSlotComponent.h>
#include <Editor/GraphCanvas/Components/NodeDescriptors/ClassMethodNodeDescriptorComponent.h>
#include <Editor/GraphCanvas/Components/NodeDescriptors/EBusHandlerNodeDescriptorComponent.h>
#include <Editor/GraphCanvas/Components/NodeDescriptors/EBusHandlerEventNodeDescriptorComponent.h>
#include <Editor/GraphCanvas/Components/NodeDescriptors/ScriptEventReceiverEventNodeDescriptorComponent.h>
#include <Editor/GraphCanvas/Components/NodeDescriptors/ScriptEventReceiverNodeDescriptorComponent.h>
#include <Editor/GraphCanvas/Components/NodeDescriptors/EBusSenderNodeDescriptorComponent.h>
#include <Editor/GraphCanvas/Components/NodeDescriptors/EntityRefNodeDescriptorComponent.h>
#include <Editor/GraphCanvas/Components/NodeDescriptors/GetVariableNodeDescriptorComponent.h>
#include <Editor/GraphCanvas/Components/NodeDescriptors/NodeDescriptorComponent.h>
#include <Editor/GraphCanvas/Components/NodeDescriptors/SetVariableNodeDescriptorComponent.h>
#include <Editor/GraphCanvas/Components/NodeDescriptors/UserDefinedNodeDescriptorComponent.h>
#include <Editor/GraphCanvas/Components/NodeDescriptors/VariableNodeDescriptorComponent.h>
#include <Editor/GraphCanvas/Components/MappingComponent.h>

#include <Editor/View/Widgets/VariablePanel/VariableDockWidget.h>

namespace ScriptCanvas
{
    ////////////////////////////////////////////////////////////////////////////
    // ScriptCanvasModule
    ////////////////////////////////////////////////////////////////////////////

    //! Create ComponentDescriptors and add them to the list.
    //! The descriptors will be registered at the appropriate time.
    //! The descriptors will be destroyed (and thus unregistered) at the appropriate time.
    ScriptCanvasModule::ScriptCanvasModule()
        : ScriptCanvasModuleCommon()
    {
        m_descriptors.insert(m_descriptors.end(), {
            ScriptCanvasBuilder::PluginComponent::CreateDescriptor(),
            ScriptCanvasEditor::EditorAssetSystemComponent::CreateDescriptor(),
            ScriptCanvasEditor::EditorScriptCanvasComponent::CreateDescriptor(),
            ScriptCanvasEditor::EntityMimeDataHandler::CreateDescriptor(),
            ScriptCanvasEditor::Graph::CreateDescriptor(),
            ScriptCanvasEditor::IconComponent::CreateDescriptor(),
            ScriptCanvasEditor::Metrics::SystemComponent::CreateDescriptor(),
            ScriptCanvasEditor::ReflectComponent::CreateDescriptor(),
            ScriptCanvasEditor::SystemComponent::CreateDescriptor(),
            ScriptCanvasEditor::EditorGraphVariableManagerComponent::CreateDescriptor(),
            ScriptCanvasEditor::VariablePropertiesComponent::CreateDescriptor(),
            ScriptCanvasEditor::SlotMappingComponent::CreateDescriptor(),
            ScriptCanvasEditor::SceneMemberMappingComponent::CreateDescriptor(),

            // GraphCanvas additions
            ScriptCanvasEditor::DynamicSlotComponent::CreateDescriptor(),

            // Base Descriptor
            ScriptCanvasEditor::NodeDescriptorComponent::CreateDescriptor(),

            // Node Type Descriptor
            ScriptCanvasEditor::ClassMethodNodeDescriptorComponent::CreateDescriptor(),
            ScriptCanvasEditor::EBusHandlerNodeDescriptorComponent::CreateDescriptor(),
            ScriptCanvasEditor::EBusHandlerEventNodeDescriptorComponent::CreateDescriptor(),
            ScriptCanvasEditor::ScriptEventReceiverEventNodeDescriptorComponent::CreateDescriptor(),
            ScriptCanvasEditor::ScriptEventReceiverNodeDescriptorComponent::CreateDescriptor(),            
            ScriptCanvasEditor::EBusSenderNodeDescriptorComponent::CreateDescriptor(),
            ScriptCanvasEditor::EntityRefNodeDescriptorComponent::CreateDescriptor(),
            ScriptCanvasEditor::VariableNodeDescriptorComponent::CreateDescriptor(),
            ScriptCanvasEditor::GetVariableNodeDescriptorComponent::CreateDescriptor(),
            ScriptCanvasEditor::SetVariableNodeDescriptorComponent::CreateDescriptor(),
            ScriptCanvasEditor::UserDefinedNodeDescriptorComponent::CreateDescriptor(),
            });

        auto libraryDescriptors = ScriptCanvasEditor::GetLibraryDescriptors();
        m_descriptors.insert(m_descriptors.end(), libraryDescriptors.begin(), libraryDescriptors.end());
        ScriptCanvasEditor::Library::Editor::InitNodeRegistry(GetNodeRegistry().Get());
    }

    AZ::ComponentTypeList ScriptCanvasModule::GetRequiredSystemComponents() const
    {
        AZ::ComponentTypeList components = GetCommonSystemComponents();

        components.insert(components.end(), std::initializer_list<AZ::Uuid> {
                azrtti_typeid<ScriptCanvasEditor::EditorAssetSystemComponent>(),
                azrtti_typeid<ScriptCanvasEditor::ReflectComponent>(),
                azrtti_typeid<ScriptCanvasEditor::SystemComponent>(),
                azrtti_typeid<ScriptCanvasEditor::Metrics::SystemComponent>()
        });

        return components;
    }
}

AZ_DECLARE_MODULE_CLASS(ScriptCanvasGem_869a0d0ec11a45c299917d45c81555e6, ScriptCanvas::ScriptCanvasModule)

#endif // SCRIPTCANVAS_EDITOR
