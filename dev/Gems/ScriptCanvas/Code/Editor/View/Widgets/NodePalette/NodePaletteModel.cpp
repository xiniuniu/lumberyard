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

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

#include <Editor/View/Widgets/NodePalette/NodePaletteModel.h>

#include <Editor/Include/ScriptCanvas/Bus/RequestBus.h>
#include <Editor/GraphCanvas/GraphCanvasEditorNotificationBusId.h>
#include <Editor/Nodes/NodeUtils.h>
#include <Editor/Settings.h>
#include <Editor/Translation/TranslationHelper.h>

#include <ScriptCanvas/Core/PureData.h>
#include <ScriptCanvas/Data/DataRegistry.h>
#include <ScriptCanvas/Libraries/Libraries.h>
#include <ScriptCanvas/Libraries/Core/GetVariable.h>
#include <ScriptCanvas/Libraries/Core/Method.h>
#include <ScriptCanvas/Libraries/Core/SetVariable.h>
#include <ScriptCanvas/Utils/NodeUtils.h>

#include <ScriptCanvas/Data/Traits.h>

namespace
{
    // Various Helper Methods
    bool IsDeprecated(const AZ::AttributeArray& attributes)
    {
        bool isDeprecated{};

        if (auto isDeprecatedAttributePtr = AZ::FindAttribute(AZ::Script::Attributes::Deprecated, attributes))
        {
            AZ::AttributeReader(nullptr, isDeprecatedAttributePtr).Read<bool>(isDeprecated);
        }

        return isDeprecated;
    }

    bool ShouldExcludeFromNodeList(const AZ::Edit::AttributeData<AZ::Script::Attributes::ExcludeFlags>* excludeAttributeData, const AZ::Uuid& typeId, bool showExcludedPreviewNodes)
    {
        if (excludeAttributeData)
        {
            AZ::u64 exclusionFlags = AZ::Script::Attributes::ExcludeFlags::List | AZ::Script::Attributes::ExcludeFlags::ListOnly;
            if (!showExcludedPreviewNodes)
            {
                exclusionFlags |= AZ::Script::Attributes::ExcludeFlags::Preview;
            }

            if (typeId == AzToolsFramework::Components::EditorComponentBase::TYPEINFO_Uuid())
            {
                return true;
            }

            return (static_cast<AZ::u64>(excludeAttributeData->Get(nullptr)) & exclusionFlags) != 0; // warning C4800: 'AZ::u64': forcing value to bool 'true' or 'false' (performance warning)
        }
        return false;
    }

    // Used for changing the node's style to identify as a preview node.
    bool IsPreviewNode(const AZ::Edit::AttributeData<AZ::Script::Attributes::ExcludeFlags>* excludeAttributeData)
    {
        if (excludeAttributeData)
        {
            return (static_cast<AZ::u64>(excludeAttributeData->Get(nullptr)) & AZ::Script::Attributes::ExcludeFlags::Preview) != 0; // warning C4800: 'AZ::u64': forcing value to bool 'true' or 'false' (performance warning)
        }

        return false;
    }

    bool HasExcludeFromNodeListAttribute(AZ::SerializeContext* serializeContext, const AZ::Uuid& typeId, bool showExcludedPreviewNodes)
    {
        const AZ::SerializeContext::ClassData* classData = serializeContext->FindClassData(typeId);
        if (classData && classData->m_editData)
        {
            if (auto editorElementData = classData->m_editData->FindElementData(AZ::Edit::ClassElements::EditorData))
            {
                if (auto excludeAttribute = editorElementData->FindAttribute(AZ::Script::Attributes::ExcludeFrom))
                {
                    auto excludeAttributeData = azdynamic_cast<const AZ::Edit::AttributeData<AZ::Script::Attributes::ExcludeFlags>*>(excludeAttribute);
                    return excludeAttributeData && ShouldExcludeFromNodeList(excludeAttributeData, typeId, showExcludedPreviewNodes);
                }
            }
        }

        return false;
    }

    bool MethodHasAttribute(const AZ::BehaviorMethod* method, AZ::Crc32 attribute)
    {
        return AZ::FindAttribute(attribute, method->m_attributes) != nullptr; // warning C4800: 'AZ::Attribute *': forcing value to bool 'true' or 'false' (performance warning)
    }

    bool HasAttribute(AZ::BehaviorClass* behaviorClass, AZ::Crc32 attribute)
    {
        auto foundItem = AZStd::find_if(behaviorClass->m_methods.begin(), behaviorClass->m_methods.end(),
            [attribute](const AZStd::pair<AZStd::string, AZ::BehaviorMethod*>& method)
        {
            return MethodHasAttribute(method.second, attribute);
        });

        return foundItem != behaviorClass->m_methods.end();
    }

    // Checks for and returns the Category attribute from an AZ::AttributeArray
    AZStd::string GetCategoryPath(const AZ::AttributeArray& attributes, const AZ::BehaviorContext& behaviorContext)
    {
        AZStd::string retVal;
        AZ::Attribute* categoryAttribute = AZ::FindAttribute(AZ::Script::Attributes::Category, attributes);

        if (categoryAttribute)
        {
            AZ::AttributeReader(nullptr, categoryAttribute).Read<AZStd::string>(retVal, behaviorContext);
        }

        return retVal;
    }

    // Helper function for populating the node palette model.
    // Pulled out just to make the tabbing a bit nicer, since it's a huge method.
    void PopulateNodePaletteModel(ScriptCanvasEditor::NodePaletteModel& nodePaletteModel)
    {
        auto dataRegistry = ScriptCanvas::GetDataRegistry();

        AZ::SerializeContext* serializeContext = nullptr;
        AZ::ComponentApplicationBus::BroadcastResult(serializeContext, &AZ::ComponentApplicationRequests::GetSerializeContext);

        AZ::BehaviorContext* behaviorContext = nullptr;
        AZ::ComponentApplicationBus::BroadcastResult(behaviorContext, &AZ::ComponentApplicationRequests::GetBehaviorContext);

        bool showExcludedPreviewNodes = false;
        AZStd::intrusive_ptr<ScriptCanvasEditor::EditorSettings::ScriptCanvasEditorSettings> settings = AZ::UserSettings::CreateFind<ScriptCanvasEditor::EditorSettings::ScriptCanvasEditorSettings>(AZ_CRC("ScriptCanvasPreviewSettings", 0x1c5a2965), AZ::UserSettings::CT_LOCAL);
        if (settings)
        {
            showExcludedPreviewNodes = settings->m_showExcludedNodes;
        }

        AZ_Assert(serializeContext, "Could not find SerializeContext. Aborting Palette Creation.");
        AZ_Assert(behaviorContext, "Could not find BehaviorContext. Aborting Palette Creation.");

        if (serializeContext == nullptr || behaviorContext == nullptr)
        {
            return;
        }

        {
            // Get all the types.
            serializeContext->EnumerateDerived<ScriptCanvas::Library::LibraryDefinition>(
                [&nodePaletteModel, &showExcludedPreviewNodes, serializeContext]
            (const AZ::SerializeContext::ClassData* classData, const AZ::Uuid& classUuid) -> bool
            {
                ScriptCanvasEditor::CategoryInformation categoryInfo;

                bool isDeprecated = false;
                AZStd::string categoryPath = classData->m_editData ? classData->m_editData->m_name : classData->m_name;

                if (classData->m_editData)
                {
                    auto editorElementData = classData->m_editData->FindElementData(AZ::Edit::ClassElements::EditorData);
                    if (editorElementData)
                    {
                            if (auto categoryAttribute = editorElementData->FindAttribute(AZ::Edit::Attributes::Category))
                            {
                                if (auto categoryAttributeData = azdynamic_cast<const AZ::Edit::AttributeData<const char*>*>(categoryAttribute))
                                {
                                    categoryPath = categoryAttributeData->Get(nullptr);
                                }
                            }

                            if (auto categoryStyleAttribute = editorElementData->FindAttribute(AZ::Edit::Attributes::CategoryStyle))
                            {
                                if (auto categoryAttributeData = azdynamic_cast<const AZ::Edit::AttributeData<const char*>*>(categoryStyleAttribute))
                                {
                                    categoryInfo.m_styleOverride = categoryAttributeData->Get(nullptr);
                                }
                            }

                            if (auto titlePaletteAttribute = editorElementData->FindAttribute(ScriptCanvas::Attributes::Node::TitlePaletteOverride))
                            {
                                if (auto categoryAttributeData = azdynamic_cast<const AZ::Edit::AttributeData<const char*>*>(titlePaletteAttribute))
                                {
                                    categoryInfo.m_paletteOverride = categoryAttributeData->Get(nullptr);
                                }
                            } 
                        }
                    }

                nodePaletteModel.RegisterCategoryInformation(categoryPath, categoryInfo);

                // Children
                for (auto& node : ScriptCanvas::Library::LibraryDefinition::GetNodes(classData->m_typeId))
                {
                    GraphCanvas::NodePaletteTreeItem* nodeParent = nullptr;

                    if (HasExcludeFromNodeListAttribute(serializeContext, node.first, showExcludedPreviewNodes))
                    {
                        continue;
                    }

                    // Pass in the associated class data so we can do more intensive lookups?
                    const AZ::SerializeContext::ClassData* classData = serializeContext->FindClassData(node.first);

                    if (classData == nullptr)
                    {
                        continue;
                    }

                    // Detect primitive types os we avoid making nodes out of them.
                    // Or anything that is 'pure data' and should be populated through a different mechanism.
                    if (classData->m_azRtti && classData->m_azRtti->IsTypeOf<ScriptCanvas::PureData>())
                    {
                        continue;
                    }
                    // Skip over some of our more dynamic nodes that we want to populate using different means
                    else if (classData->m_azRtti && classData->m_azRtti->IsTypeOf<ScriptCanvas::Nodes::Core::GetVariableNode>())
                    {
                        continue;
                    }
                    else if (classData->m_azRtti && classData->m_azRtti->IsTypeOf<ScriptCanvas::Nodes::Core::SetVariableNode>())
                    {
                        continue;
                    }
                    else
                    {
                        nodePaletteModel.RegisterCustomNode(categoryPath, node.first, node.second, classData);
                    }
                }

                return true;
            });
        }

        // Merged all of these into a single pass(to avoid going over the giant lists multiple times), 
        // so now this looks absolutely horrifying in what it's doing. 
        // Everything is still mostly independent, and independently scoped to try to make the division of labor clear.
        {
            // We will skip buses that are ONLY registered on classes that derive from EditorComponentBase,
            // because they don't have a runtime implementation. Buses such as the TransformComponent which
            // is implemented by both an EditorComponentBase derived class and a Component derived class
            // will still appear
            AZStd::unordered_set<AZ::Crc32> skipBuses;
            AZStd::unordered_set<AZ::Crc32> potentialSkipBuses;
            AZStd::unordered_set<AZ::Crc32> nonSkipBuses;

            const auto& typeIdTraitMap = dataRegistry->m_typeIdTraitMap;

            for (auto& type : dataRegistry->m_creatableTypes)
            {
                if (!type.second.m_isTransient)
                {
                    ScriptCanvasEditor::VariablePaletteRequestBus::Broadcast(&ScriptCanvasEditor::VariablePaletteRequests::RegisterVariableType, type.first);
                }
            }

            for (const auto& classIter : behaviorContext->m_classes)
            {
                const AZ::BehaviorClass* behaviorClass = classIter.second;

                if (IsDeprecated(behaviorClass->m_attributes))
                {
                    continue;
                }

                // Only bind Behavior Classes marked with the Scope type of Launcher
                if (!AZ::Internal::IsInScope(behaviorClass->m_attributes, AZ::Script::Attributes::ScopeFlags::Launcher))
                {
                    continue; // skip this class
                }

                // Check for "ExcludeFrom" attribute for ScriptCanvas
                auto excludeClassAttributeData = azdynamic_cast<const AZ::Edit::AttributeData<AZ::Script::Attributes::ExcludeFlags>*>(AZ::FindAttribute(AZ::Script::Attributes::ExcludeFrom, behaviorClass->m_attributes));

                // We don't want to show any components, since there isn't anything we can do with them
                // from ScriptCanvas since we use buses to communicate to everything.
                if (ShouldExcludeFromNodeList(excludeClassAttributeData, behaviorClass->m_azRtti ? behaviorClass->m_azRtti->GetTypeId() : behaviorClass->m_typeId, showExcludedPreviewNodes))
                {
                    for (const auto& requestBus : behaviorClass->m_requestBuses)
                    {
                        skipBuses.insert(AZ::Crc32(requestBus.c_str()));
                    }

                    continue;
                }

                // Objects and Object methods
                {
                    bool canCreate = serializeContext->FindClassData(behaviorClass->m_typeId) != nullptr;

                    // createable variables must have full memory support
                    canCreate = canCreate &&
                        (behaviorClass->m_allocate
                            && behaviorClass->m_cloner
                            && behaviorClass->m_mover
                            && behaviorClass->m_destructor
                            && behaviorClass->m_deallocate);

                    if (canCreate)
                    {
                        // Do not allow variable creation for data that derives from AZ::Component
                        for (auto base : behaviorClass->m_baseClasses)
                        {
                            if (AZ::Component::TYPEINFO_Uuid() == base)
                            {
                                canCreate = false;
                                break;
                            }
                        }
                    }

                    AZStd::string categoryPath;

                    AZStd::string translationContext = ScriptCanvasEditor::TranslationHelper::GetContextName(ScriptCanvasEditor::TranslationContextGroup::ClassMethod, behaviorClass->m_name);
                    AZStd::string translationKey = ScriptCanvasEditor::TranslationHelper::GetClassKey(ScriptCanvasEditor::TranslationContextGroup::ClassMethod, behaviorClass->m_name, ScriptCanvasEditor::TranslationKeyId::Category);
                    AZStd::string translatedCategory = QCoreApplication::translate(translationContext.c_str(), translationKey.c_str()).toUtf8().data();

                    if (translatedCategory != translationKey)
                    {
                        categoryPath = translatedCategory;
                    }
                    else
                    {
                        AZStd::string behaviorContextCategory = GetCategoryPath(behaviorClass->m_attributes, (*behaviorContext));
                        if (!behaviorContextCategory.empty())
                        {
                            categoryPath = behaviorContextCategory;
                        }
                    }

                    if (canCreate)
                    {
                        ScriptCanvas::Data::Type type = dataRegistry->m_typeIdTraitMap[ScriptCanvas::Data::eType::BehaviorContextObject].m_dataTraits.GetSCType(behaviorClass->m_typeId);

                        if (type.IsValid())
                        {
                            if (!AZ::FindAttribute(AZ::ScriptCanvasAttributes::AllowInternalCreation, behaviorClass->m_attributes))
                            {
                                ScriptCanvasEditor::VariablePaletteRequestBus::Broadcast(&ScriptCanvasEditor::VariablePaletteRequests::RegisterVariableType, type);
                            }
                        }
                    }

                    AZStd::string classNamePretty(classIter.first);

                    AZ::Attribute* prettyNameAttribute = AZ::FindAttribute(AZ::ScriptCanvasAttributes::PrettyName, behaviorClass->m_attributes);

                    if (prettyNameAttribute)
                    {
                        AZ::AttributeReader(nullptr, prettyNameAttribute).Read<AZStd::string>(classNamePretty, *behaviorContext);
                    }

                    if (categoryPath.empty())
                    {
                        if (classNamePretty.empty())
                        {
                            categoryPath = classNamePretty;
                        }
                        else
                        {
                            categoryPath = "Other";
                        }
                    }

                    categoryPath.append("/");

                    AZStd::string displayName = ScriptCanvasEditor::TranslationHelper::GetClassKeyTranslation(ScriptCanvasEditor::TranslationContextGroup::ClassMethod, classIter.first, ScriptCanvasEditor::TranslationKeyId::Name);

                    if (displayName.empty())
                    {
                        categoryPath.append(classNamePretty.c_str());
                    }
                    else
                    {
                        categoryPath.append(displayName.c_str());
                    }

                    for (auto method : behaviorClass->m_methods)
                    {
                        if (IsDeprecated(method.second->m_attributes))
                        {
                            continue;
                        }

                        // Check for "ExcludeFrom" attribute for ScriptCanvas
                        auto excludeMethodAttributeData = azdynamic_cast<const AZ::Edit::AttributeData<AZ::Script::Attributes::ExcludeFlags>*>(AZ::FindAttribute(AZ::Script::Attributes::ExcludeFrom, method.second->m_attributes));
                        if (ShouldExcludeFromNodeList(excludeMethodAttributeData, behaviorClass->m_azRtti ? behaviorClass->m_azRtti->GetTypeId() : behaviorClass->m_typeId, showExcludedPreviewNodes))
                        {
                            continue; // skip this method
                        }

                        const auto isExposableOutcome = ScriptCanvas::IsExposable(*method.second);
                        if (!isExposableOutcome.IsSuccess())
                        {
                            AZ_Warning("ScriptCanvas", false, "Unable to expose method: %s to ScriptCanvas because: %s", method.second->m_name.data(), isExposableOutcome.GetError().data());
                            continue;
                        }

                        nodePaletteModel.RegisterClassNode(categoryPath, classIter.first, method.first, method.second, behaviorContext);
                    }
                }

                auto baseClass = AZStd::find(behaviorClass->m_baseClasses.begin(),
                    behaviorClass->m_baseClasses.end(),
                    AzToolsFramework::Components::EditorComponentBase::TYPEINFO_Uuid());

                if (baseClass != behaviorClass->m_baseClasses.end())
                {
                    for (const auto& requestBus : behaviorClass->m_requestBuses)
                    {
                        potentialSkipBuses.insert(AZ::Crc32(requestBus.c_str()));
                    }
                }
                // If the Ebus does not inherit from EditorComponentBase then do not skip it
                else
                {
                    for (const auto& requestBus : behaviorClass->m_requestBuses)
                    {
                        nonSkipBuses.insert(AZ::Crc32(requestBus.c_str()));
                    }
                }
            }

            // Add buses which are not on the non-skip list to the skipBuses set
            for (auto potentialSkipBus : potentialSkipBuses)
            {
                if (nonSkipBuses.find(potentialSkipBus) == nonSkipBuses.end())
                {
                    skipBuses.insert(potentialSkipBus);
                }
            }

            for (const auto& ebusIter : behaviorContext->m_ebuses)
            {
                AZ::BehaviorEBus* ebus = ebusIter.second;

                if (ebus == nullptr)
                {
                    continue;
                }

                auto skipBusIterator = skipBuses.find(AZ::Crc32(ebusIter.first.c_str()));
                if (skipBusIterator != skipBuses.end())
                {
                    continue;
                }

                // Skip buses mapped by their deprecated name (usually duplicates)
                if (ebusIter.first == ebus->m_deprecatedName)
                {
                    continue;
                }
                
                // Only bind Behavior Buses marked with the Scope type of Launcher
                if (!AZ::Internal::IsInScope(ebus->m_attributes, AZ::Script::Attributes::ScopeFlags::Launcher))
                {
                    continue; // skip this bus
                }

                // EBus Handler
                {
                    if (ebus->m_createHandler)
                    {
                        AZ::BehaviorEBusHandler* handler(nullptr);
                        if (ebus->m_createHandler->InvokeResult(handler) && handler)
                        {
                            const AZ::BehaviorEBusHandler::EventArray& events(handler->GetEvents());
                            if (!events.empty())
                            {
                                if (IsDeprecated(ebus->m_attributes))
                                {
                                    continue;
                                }

                                auto excludeEventAttributeData = azdynamic_cast<const AZ::Edit::AttributeData<AZ::Script::Attributes::ExcludeFlags>*>(AZ::FindAttribute(AZ::Script::Attributes::ExcludeFrom, ebus->m_attributes));
                                if (ShouldExcludeFromNodeList(excludeEventAttributeData, handler->RTTI_GetType(), showExcludedPreviewNodes))
                                {
                                    continue;
                                }

                                if (auto runtimeEbusAttributePtr = AZ::FindAttribute(AZ::RuntimeEBusAttribute, ebus->m_attributes))
                                {
                                    bool isRuntimeEbus = false;
                                    AZ::AttributeReader(nullptr, runtimeEbusAttributePtr).Read<bool>(isRuntimeEbus);

                                    if (isRuntimeEbus)
                                    {
                                        continue;
                                    }
                                }

                                AZStd::string translationContext = ScriptCanvasEditor::TranslationHelper::GetContextName(ScriptCanvasEditor::TranslationContextGroup::EbusHandler, ebus->m_name);
                                AZStd::string categoryPath;

                                {
                                    AZStd::string translationKey = ScriptCanvasEditor::TranslationHelper::GetClassKey(ScriptCanvasEditor::TranslationContextGroup::EbusHandler, ebus->m_name, ScriptCanvasEditor::TranslationKeyId::Category);
                                    AZStd::string translatedCategory = QCoreApplication::translate(translationContext.c_str(), translationKey.c_str()).toUtf8().data();

                                    if (translatedCategory != translationKey)
                                    {
                                        categoryPath = translatedCategory;
                                    }
                                    else
                                    {
                                        AZStd::string behaviourContextCategory = GetCategoryPath(ebus->m_attributes, (*behaviorContext));
                                        if (!behaviourContextCategory.empty())
                                        {
                                            categoryPath = behaviourContextCategory;
                                        }
                                    }
                                }

                                // Treat the EBusHandler name as a Category key in order to allow multiple busses to be merged into a single Category.
                                {
                                    AZStd::string translationKey = ScriptCanvasEditor::TranslationHelper::GetClassKey(ScriptCanvasEditor::TranslationContextGroup::EbusHandler, ebus->m_name, ScriptCanvasEditor::TranslationKeyId::Name);
                                    AZStd::string translatedName = QCoreApplication::translate(translationContext.c_str(), translationKey.c_str()).toUtf8().data();

                                    if (!categoryPath.empty())
                                    {
                                        categoryPath.append("/");
                                    }
                                    else
                                    {
                                        categoryPath = "Other/";
                                    }

                                    if (translatedName != translationKey)
                                    {
                                        categoryPath.append(translatedName.c_str());
                                    }
                                    else
                                    {
                                        categoryPath.append(ebus->m_name.c_str());
                                    }
                                }

                                for (const auto& event : events)
                                {
                                    nodePaletteModel.RegisterEBusHandlerNodeModelInformation(categoryPath.c_str(), ebusIter.first, event.m_name, ScriptCanvas::EBusBusId(ebusIter.first.c_str()), event);
                                }
                            }

                            if (ebus->m_destroyHandler)
                            {
                                ebus->m_destroyHandler->Invoke(handler);
                            }
                        }
                    }
                }

                // Ebus Sender
                {
                    if (!ebus->m_events.empty())
                    {
                        if (IsDeprecated(ebus->m_attributes))
                        {
                            continue;
                        }

                        auto excludeEBusAttributeData = azdynamic_cast<const AZ::Edit::AttributeData<AZ::Script::Attributes::ExcludeFlags>*>(AZ::FindAttribute(AZ::Script::Attributes::ExcludeFrom, ebus->m_attributes));
                        if (ShouldExcludeFromNodeList(excludeEBusAttributeData, AZ::Uuid::CreateNull(), showExcludedPreviewNodes))
                        {
                            continue;
                        }

                        AZStd::string categoryPath;

                        AZStd::string translationContext = ScriptCanvasEditor::TranslationHelper::GetContextName(ScriptCanvasEditor::TranslationContextGroup::EbusSender, ebus->m_name);
                        AZStd::string translationKey = ScriptCanvasEditor::TranslationHelper::GetClassKey(ScriptCanvasEditor::TranslationContextGroup::EbusSender, ebus->m_name, ScriptCanvasEditor::TranslationKeyId::Category);
                        AZStd::string translatedCategory = QCoreApplication::translate(translationContext.c_str(), translationKey.c_str()).toUtf8().data();

                        if (translatedCategory != translationKey)
                        {
                            categoryPath = translatedCategory;
                        }
                        else
                        {
                            AZStd::string behaviourContextCategory = GetCategoryPath(ebus->m_attributes, (*behaviorContext));
                            if (!behaviourContextCategory.empty())
                            {
                                categoryPath = behaviourContextCategory;
                            }
                        }

                        // Parent
                        AZStd::string displayName = ScriptCanvasEditor::TranslationHelper::GetClassKeyTranslation(ScriptCanvasEditor::TranslationContextGroup::EbusSender, ebusIter.first, ScriptCanvasEditor::TranslationKeyId::Name);

                        // Treat the EBus name as a Category key in order to allow multiple busses to be merged into a single Category.
                        if (!categoryPath.empty())
                        {
                            categoryPath.append("/");
                        }
                        else
                        {
                            categoryPath = "Other/";
                        }

                        if (displayName.empty())
                        {
                            categoryPath.append(ebusIter.first.c_str());
                        }
                        else
                        {
                            categoryPath.append(displayName.c_str());
                        }

                        ScriptCanvasEditor::CategoryInformation ebusCategoryInformation;

                        ebusCategoryInformation.m_tooltip = ScriptCanvasEditor::TranslationHelper::GetClassKeyTranslation(ScriptCanvasEditor::TranslationContextGroup::EbusSender, ebusIter.first, ScriptCanvasEditor::TranslationKeyId::Tooltip);

                        nodePaletteModel.RegisterCategoryInformation(categoryPath, ebusCategoryInformation);

                        for (auto event : ebus->m_events)
                        {
                            if (IsDeprecated(event.second.m_attributes))
                            {
                                continue;
                            }

                            auto excludeEventAttributeData = azdynamic_cast<const AZ::Edit::AttributeData<AZ::Script::Attributes::ExcludeFlags>*>(AZ::FindAttribute(AZ::Script::Attributes::ExcludeFrom, event.second.m_attributes));
                            if (ShouldExcludeFromNodeList(excludeEventAttributeData, AZ::Uuid::CreateNull(), showExcludedPreviewNodes))
                            {
                                continue; // skip this event
                            }

                            nodePaletteModel.RegisterEBusSenderNodeModelInformation(categoryPath, ebusIter.first, event.first, ScriptCanvas::EBusBusId(ebus->m_name.c_str()), ScriptCanvas::EBusEventId(event.first.c_str()), event.second);
                        }
                    }
                }
            }
        }
    }
}

namespace ScriptCanvasEditor
{
    ////////////////////////////////
    // NodePaletteModelInformation
    ////////////////////////////////
    void NodePaletteModelInformation::PopulateTreeItem(GraphCanvas::NodePaletteTreeItem& treeItem) const
    {
        if (!m_toolTip.empty())
        {
            treeItem.SetToolTip(m_toolTip.c_str());
        }

        if (!m_styleOverride.empty())
        {
            treeItem.SetStyleOverride(m_styleOverride.c_str());
        }

        if (!m_titlePaletteOverride.empty())
        {
            const bool forceSet = true;
            treeItem.SetTitlePalette(m_titlePaletteOverride.c_str(), forceSet);
        }
    }

    /////////////////////
    // NodePaletteModel
    /////////////////////

    NodePaletteModel::NodePaletteModel()
    {

    }

    NodePaletteModel::~NodePaletteModel()
    {
        ClearRegistry();
    }

    void NodePaletteModel::RepopulateModel()
    {
        ClearRegistry();

        PopulateNodePaletteModel((*this));
    }

    void NodePaletteModel::RegisterCustomNode(AZStd::string_view categoryPath, const AZ::Uuid& uuid, AZStd::string_view name, const AZ::SerializeContext::ClassData* classData)
    {
        ScriptCanvas::NodeTypeIdentifier nodeIdentifier = ScriptCanvas::NodeUtils::ConstructCustomNodeIdentifier(uuid);

        auto mapIter = m_registeredNodes.find(nodeIdentifier);

        if (mapIter == m_registeredNodes.end())
        {
            CustomNodeModelInformation* customNodeInformation = aznew CustomNodeModelInformation();

            customNodeInformation->m_nodeIdentifier = nodeIdentifier;
            customNodeInformation->m_typeId = uuid;

            customNodeInformation->m_displayName = name;

            bool isToolTipSet(false);

            bool isMissingEntry(false);
            bool isMissingTooltip(false);
            bool isDeprecated(false);

            if (classData && classData->m_editData && classData->m_editData->m_name)
            {
                auto nodeContextName = ScriptCanvasEditor::Nodes::GetContextName(*classData);
                auto contextName = ScriptCanvasEditor::TranslationHelper::GetContextName(ScriptCanvasEditor::TranslationContextGroup::ClassMethod, nodeContextName);

                GraphCanvas::TranslationKeyedString nodeKeyedString({}, contextName);
                nodeKeyedString.m_key = ScriptCanvasEditor::TranslationHelper::GetKey(ScriptCanvasEditor::TranslationContextGroup::ClassMethod, nodeContextName, classData->m_editData->m_name, ScriptCanvasEditor::TranslationItemType::Node, ScriptCanvasEditor::TranslationKeyId::Name);
                customNodeInformation->m_displayName = nodeKeyedString.GetDisplayString();

                GraphCanvas::TranslationKeyedString tooltipKeyedString(AZStd::string(), nodeKeyedString.m_context);
                tooltipKeyedString.m_key = ScriptCanvasEditor::TranslationHelper::GetKey(ScriptCanvasEditor::TranslationContextGroup::ClassMethod, nodeContextName, classData->m_editData->m_name, ScriptCanvasEditor::TranslationItemType::Node, ScriptCanvasEditor::TranslationKeyId::Tooltip);

                customNodeInformation->m_toolTip = tooltipKeyedString.GetDisplayString();

                if (customNodeInformation->m_displayName.empty())
                {
                    customNodeInformation->m_displayName = classData->m_editData->m_name;
                }

                GraphCanvas::TranslationKeyedString categoryKeyedString(ScriptCanvasEditor::Nodes::GetCategoryName(*classData), nodeKeyedString.m_context);
                categoryKeyedString.m_key = ScriptCanvasEditor::TranslationHelper::GetKey(ScriptCanvasEditor::TranslationContextGroup::ClassMethod, nodeContextName, classData->m_editData->m_name, ScriptCanvasEditor::TranslationItemType::Node, ScriptCanvasEditor::TranslationKeyId::Category);

                customNodeInformation->m_categoryPath = categoryKeyedString.GetDisplayString();

                if (customNodeInformation->m_categoryPath.empty())
                {
                    if (contextName.empty())
                    {
                        customNodeInformation->m_categoryPath = categoryPath;
                    }
                    else
                    {
                        customNodeInformation->m_categoryPath = contextName;
                    }
                }

                auto editorDataElement = classData->m_editData->FindElementData(AZ::Edit::ClassElements::EditorData);

                if (editorDataElement)
                {
                    if (auto categoryStyleAttribute = editorDataElement->FindAttribute(AZ::Edit::Attributes::CategoryStyle))
                    {
                        if (auto categoryAttributeData = azdynamic_cast<const AZ::Edit::AttributeData<const char*>*>(categoryStyleAttribute))
                        {
                            if (categoryAttributeData->Get(nullptr))
                            {
                                customNodeInformation->m_styleOverride = categoryAttributeData->Get(nullptr);
                            }
                        }
                    }

                    if (auto titlePaletteAttribute = editorDataElement->FindAttribute(ScriptCanvas::Attributes::Node::TitlePaletteOverride))
                    {
                        if (auto titlePaletteAttributeData = azdynamic_cast<const AZ::Edit::AttributeData<const char*>*>(titlePaletteAttribute))
                        {
                            if (titlePaletteAttributeData->Get(nullptr))
                            {
                                customNodeInformation->m_titlePaletteOverride = titlePaletteAttributeData->Get(nullptr);
                            }
                        }
                    }


                    if (auto deprecatedAttribute = editorDataElement->FindAttribute(AZ::Script::Attributes::Deprecated))
                    {
                        if (auto deprecatedAttributeData = azdynamic_cast<const AZ::Edit::AttributeData<bool>*>(deprecatedAttribute))
                        {
                            isDeprecated = deprecatedAttributeData->Get(nullptr);
                        }
                    }

                    if (customNodeInformation->m_toolTip.empty() && classData->m_editData->m_description)
                    {
                        customNodeInformation->m_toolTip = classData->m_editData->m_description;
                    }
                }
            }

            if (!isDeprecated)
            {
                m_registeredNodes.emplace(AZStd::make_pair(nodeIdentifier, customNodeInformation));
            }
            else
            {
                delete customNodeInformation;
            }
        }
    }

    void NodePaletteModel::RegisterClassNode(const AZStd::string& categoryPath, const AZStd::string& methodClass, const AZStd::string& methodName, AZ::BehaviorMethod* behaviorMethod, AZ::BehaviorContext* behaviorContext)
    {
        ScriptCanvas::NodeTypeIdentifier nodeIdentifier = ScriptCanvas::NodeUtils::ConstructMethodNodeIdentifier(methodClass, methodName);

        auto registerIter = m_registeredNodes.find(nodeIdentifier);

        if (registerIter == m_registeredNodes.end())
        {
            MethodNodeModelInformation* methodModelInformation = aznew MethodNodeModelInformation();

            methodModelInformation->m_nodeIdentifier = nodeIdentifier;
            methodModelInformation->m_classMethod = methodClass;
            methodModelInformation->m_metehodName = methodName;

            methodModelInformation->m_titlePaletteOverride = "MethodNodeTitlePalette";

            methodModelInformation->m_displayName = TranslationHelper::GetKeyTranslation(TranslationContextGroup::ClassMethod, methodClass.c_str(), methodName.c_str(), TranslationItemType::Node, TranslationKeyId::Name);

            if (methodModelInformation->m_displayName.empty())
            {
                methodModelInformation->m_displayName = methodName;
            }

            methodModelInformation->m_toolTip = TranslationHelper::GetKeyTranslation(TranslationContextGroup::ClassMethod, methodClass.c_str(), methodName.c_str(), TranslationItemType::Node, TranslationKeyId::Tooltip);            

            GraphCanvas::TranslationKeyedString methodCategoryString;
            methodCategoryString.m_context = ScriptCanvasEditor::TranslationHelper::GetContextName(ScriptCanvasEditor::TranslationContextGroup::ClassMethod, methodClass.c_str());
            methodCategoryString.m_key = ScriptCanvasEditor::TranslationHelper::GetKey(ScriptCanvasEditor::TranslationContextGroup::ClassMethod, methodClass.c_str(), methodName.c_str(), ScriptCanvasEditor::TranslationItemType::Node, ScriptCanvasEditor::TranslationKeyId::Category);

            methodModelInformation->m_categoryPath = methodCategoryString.GetDisplayString();

            if (methodModelInformation->m_categoryPath.empty())
            {
                if (!MethodHasAttribute(behaviorMethod, AZ::ScriptCanvasAttributes::FloatingFunction))
                {    
                    methodModelInformation->m_categoryPath = categoryPath;
                }   
                else if(MethodHasAttribute(behaviorMethod, AZ::Script::Attributes::Category))
                {
                    methodModelInformation->m_categoryPath = GetCategoryPath(behaviorMethod->m_attributes, (*behaviorContext));                    
                }                

                if (methodModelInformation->m_categoryPath.empty())
                {
                    methodModelInformation->m_categoryPath = "Other";
                }
            }

            m_registeredNodes.emplace(AZStd::make_pair(nodeIdentifier, methodModelInformation));
        }        
    }

    void NodePaletteModel::RegisterEBusHandlerNodeModelInformation(AZStd::string_view categoryPath, AZStd::string_view busName, AZStd::string_view eventName, const ScriptCanvas::EBusBusId& busId, const AZ::BehaviorEBusHandler::BusForwarderEvent& forwardEvent)
    {
        ScriptCanvas::NodeTypeIdentifier nodeIdentifier = ScriptCanvas::NodeUtils::ConstructEBusEventReceiverIdentifier(busId, forwardEvent.m_eventId);

        auto nodeIter = m_registeredNodes.find(nodeIdentifier);

        if (nodeIter == m_registeredNodes.end())
        {
            EBusHandlerNodeModelInformation* handlerInformation = aznew EBusHandlerNodeModelInformation();
            
            handlerInformation->m_titlePaletteOverride = "HandlerNodeTitlePalette";
            handlerInformation->m_categoryPath = categoryPath;
            handlerInformation->m_nodeIdentifier = nodeIdentifier;

            handlerInformation->m_busName = busName;
            handlerInformation->m_eventName = eventName;
            handlerInformation->m_busId = busId;
            handlerInformation->m_eventId = forwardEvent.m_eventId;

            AZStd::string displayEventName = TranslationHelper::GetKeyTranslation(TranslationContextGroup::EbusHandler, busName.data(), eventName.data(), TranslationItemType::Node, TranslationKeyId::Name);

            if (displayEventName.empty())
            {
                handlerInformation->m_displayName = eventName;
            }
            else
            {
                handlerInformation->m_displayName = displayEventName;
            }

            handlerInformation->m_toolTip = TranslationHelper::GetKeyTranslation(TranslationContextGroup::EbusHandler, busName.data(), eventName.data(), TranslationItemType::Node, TranslationKeyId::Tooltip);            

            m_registeredNodes.emplace(AZStd::make_pair(nodeIdentifier, handlerInformation));
        }        
    }

    void NodePaletteModel::RegisterEBusSenderNodeModelInformation(AZStd::string_view categoryPath, AZStd::string_view busName, AZStd::string_view eventName, const ScriptCanvas::EBusBusId& busId, const ScriptCanvas::EBusEventId& eventId, const AZ::BehaviorEBusEventSender& eventDefinition)
    {
        ScriptCanvas::NodeTypeIdentifier nodeIdentifier = ScriptCanvas::NodeUtils::ConstructEBusEventSenderIdentifier(busId, eventId);

        auto nodeIter = m_registeredNodes.find(nodeIdentifier);

        if (nodeIter == m_registeredNodes.end())
        {
            EBusSenderNodeModelInformation* senderInformation = aznew EBusSenderNodeModelInformation();

            senderInformation->m_titlePaletteOverride = "MethodNodeTitlePalette";
            senderInformation->m_categoryPath = categoryPath;
            senderInformation->m_nodeIdentifier = nodeIdentifier;

            senderInformation->m_busName = busName;
            senderInformation->m_eventName = eventName;
            senderInformation->m_busId = busId;
            senderInformation->m_eventId = eventId;

            AZStd::string displayEventName = TranslationHelper::GetKeyTranslation(TranslationContextGroup::EbusSender, busName.data(), eventName.data(), TranslationItemType::Node, TranslationKeyId::Name);

            if (displayEventName.empty())
            {
                senderInformation->m_displayName = eventName;
            }
            else
            {
                senderInformation->m_displayName = displayEventName;
            }

            senderInformation->m_toolTip = TranslationHelper::GetKeyTranslation(TranslationContextGroup::EbusSender, busName.data(), eventName.data(), TranslationItemType::Node, TranslationKeyId::Tooltip);

            m_registeredNodes.emplace(AZStd::make_pair(nodeIdentifier, senderInformation));
        }
    }

    void NodePaletteModel::RegisterCategoryInformation(const AZStd::string& category, const CategoryInformation& categoryInformation)
    {
        auto categoryIter = m_categoryInformation.find(category);

        if (categoryIter == m_categoryInformation.end())
        {
            m_categoryInformation[category] = categoryInformation;
        }
    }

    const CategoryInformation* NodePaletteModel::FindCategoryInformation(const AZStd::string& categoryStyle) const
    {
        auto categoryIter = m_categoryInformation.find(categoryStyle);

        if (categoryIter != m_categoryInformation.end())
        {
            return &(categoryIter->second);
        }

        return nullptr;
    }

    const CategoryInformation* NodePaletteModel::FindBestCategoryInformation(AZStd::string_view categoryView) const
    {
        const CategoryInformation* bestCategoryFit = nullptr;

        auto categoryIter = m_categoryInformation.find(categoryView);

        size_t offset = AZStd::string_view::npos;

        AZStd::string_view categoryTrail = categoryView;

        while (categoryIter == m_categoryInformation.end() && !categoryTrail.empty())
        {
            size_t seperator = categoryTrail.find_last_of('/', offset);

            if (seperator == AZStd::string_view::npos)
            {
                categoryTrail = nullptr;
            }
            else
            {
                categoryTrail = categoryTrail.substr(0, seperator - 1);
                categoryIter = m_categoryInformation.find(categoryTrail);
            }
        }

        if (categoryIter != m_categoryInformation.end())
        {
            bestCategoryFit = &(categoryIter->second);
        }

        return bestCategoryFit;
    }

    const NodePaletteModelInformation* NodePaletteModel::FindNodePaletteInformation(const ScriptCanvas::NodeTypeIdentifier& nodeType) const
    {
        auto registryIter = m_registeredNodes.find(nodeType);

        if (registryIter != m_registeredNodes.end())
        {
            return registryIter->second;
        }

        return nullptr;
    }

    const NodePaletteModel::NodePaletteRegistry& NodePaletteModel::GetNodeRegistry() const
    {
        return m_registeredNodes;
    }

    GraphCanvas::GraphCanvasTreeItem* NodePaletteModel::CreateCategoryNode(AZStd::string_view categoryPath, AZStd::string_view categoryName, GraphCanvas::GraphCanvasTreeItem* parentItem) const
    {
        GraphCanvas::NodePaletteTreeItem* treeItem = parentItem->CreateChildNode<GraphCanvas::NodePaletteTreeItem>(categoryName, ScriptCanvasEditor::AssetEditorId);

        const CategoryInformation* categoryInformation = FindCategoryInformation(categoryPath);

        if (categoryInformation)
        {
            if (!categoryInformation->m_tooltip.empty())
            {
                treeItem->SetToolTip(categoryInformation->m_tooltip.c_str());
            }

            if (!categoryInformation->m_paletteOverride.empty())
            {
                treeItem->SetTitlePalette(categoryInformation->m_paletteOverride.c_str());
            }

            if (!categoryInformation->m_styleOverride.empty())
            {
                treeItem->SetStyleOverride(categoryInformation->m_styleOverride.c_str());
            }
        }

        return treeItem;
    }

    void NodePaletteModel::ClearRegistry()
    {
        for (auto& mapPair : m_registeredNodes)
        {
            delete mapPair.second;
        }

        m_registeredNodes.clear();

        m_categoryInformation.clear();
    }
}