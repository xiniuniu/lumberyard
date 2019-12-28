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

#include "EditorEntityHelpers.h"

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Debug/Profiler.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/sort.h>
#include <AzCore/RTTI/AttributeReader.h>
#include <AzToolsFramework/Commands/EntityStateCommand.h>
#include <AzToolsFramework/Entity/EditorEntityInfoBus.h>
#include <AzToolsFramework/Entity/EditorEntityContextBus.h>
#include <AzToolsFramework/Metrics/LyEditorMetricsBus.h>
#include <AzToolsFramework/Slice/SliceMetadataEntityContextBus.h>
#include <AzToolsFramework/Slice/SliceUtilities.h>
#include <AzToolsFramework/ToolsComponents/EditorLockComponentBus.h>
#include <AzToolsFramework/ToolsComponents/EditorVisibilityBus.h>
#include <AzToolsFramework/ToolsComponents/GenericComponentWrapper.h>
#include <AzToolsFramework/ToolsComponents/EditorInspectorComponentBus.h>

namespace AzToolsFramework
{
    namespace Internal
    {
        /// Internal helper function for CloneInstantiatedEntities. Performs the initial cloning of the given set of entities if they
        /// are slice or sub-slice entities, recursively. Populates a list of loose entities to clone as it traverses the duplication set.
        /// @param duplicationSet The set of entities to clone.
        /// @param out_allEntityClones Output parameter populated with all cloned entities.
        /// @param out_sourceToCloneEntityIdMap Output parameter populated with a map from all source entities to cloned entities.
        /// @param out_looseEntitiesToClone Output parameter populated with all not yet cloned entities that are not associated with a slice.
        ///         This will be used after slices are cloned to clone these entities.
        void CloneSliceEntitiesAndChildren(
            const EntityIdSet& duplicationSet,
            EntityList& out_allEntityClones,
            AZ::SliceComponent::EntityIdToEntityIdMap& out_sourceToCloneEntityIdMap,
            EntityIdList& out_looseEntitiesToClone);

        /// Internal helper function for CloneInstantiatedEntities. Updates entity ID references in all cloned entities based on
        /// the given entity ID map. This handles cases like entity transform parents, entity attachments, and entity ID references
        /// in scripting components. This will update all references in the pool of cloned entities to reference other cloned
        /// entities, if they were previously referencing any of the source entities.
        /// @param inout_allEntityClones The collection of entities that have been cloned and should have entity references updated
        ///         based on the given map.
        /// @param sourceToCloneEntityIdMap A map of entity IDs to update in the given clone list, any references to key will be
        ///         changed to a reference to value instead.
        void UpdateClonedEntityReferences(
            AZ::SliceComponent::InstantiatedContainer& inout_allEntityClones,
            const AZ::SliceComponent::EntityIdToEntityIdMap& sourceToCloneEntityIdMap);

        /// Internal helper function for CloneInstantiatedEntities. Selects all cloned entities, and updates the undo stack with
        /// information on all cloned entities.
        /// @param allEntityClones The collection of all entities that were cloned.
        /// @param undoBatch The undo batch used for tracking the cloning operation.
        void UpdateUndoStackAndSelectClonedEntities(
            const EntityList& allEntityClones,
            ScopedUndoBatch& undoBatch);

        /// Internal helper function for CloneInstantiatedEntities. If the given entity identified by the ancestor list is a slice root, clone it.
        /// @param ancestors The entity to clone if it is a slice root entity, tracked through its ancestry.
        /// @param out_allEntityClones Output parameter populated with all cloned entities.
        /// @param out_sourceToCloneEntityIdMap Output parameter populated with a map from all source entities to cloned entities.
        /// @return True if the passed in entity was cloned, false if not.
        bool CloneIfSliceRoot(
            const AZ::SliceComponent::EntityAncestorList& ancestors,
            EntityList& out_allEntityClones,
            AZ::SliceComponent::EntityIdToEntityIdMap& out_sourceToCloneEntityIdMap);

        /// Internal helper function for CloneInstantiatedEntities. If the given entity identified by the slice address and
        /// ancestor list is a subslice root, clone it.
        /// @param owningSliceAddress The slice address that owns this entity. This is necessary to clone the subslice.
        /// @param ancestors The entity to clone if it is a subslice root entity, tracked through its ancestry.
        /// @param out_allEntityClones Output parameter populated with all cloned entities.
        /// @param out_sourceToCloneEntityIdMap Output parameter populated with a map from all source entities to cloned entities.
        /// @return True if the passed in entity was cloned, false if not.
        bool CloneIfSubsliceRoot(
            const AZ::SliceComponent::SliceInstanceAddress& owningSliceAddress,
            const AZ::SliceComponent::EntityAncestorList& ancestors,
            EntityList& out_allEntityClones,
            AZ::SliceComponent::EntityIdToEntityIdMap& out_sourceToCloneEntityIdMap);

        /// Internal helper function for CloneInstantiatedEntities. Clones the given entity collection as loose entities.
        /// @param duplicationList The collection of entities to clone.
        /// @param out_allEntityClones Output parameter populated with all cloned entities.
        /// @param out_sourceToCloneEntityIdMap Output parameter populated with a map from all source entities to cloned entities.
        /// @param out_clonedLooseEntities Output parameter populated with all cloned loose entities.
        void CloneLooseEntities(
            const EntityIdList& duplicationList,
            EntityList& out_allEntityClones,
            AZ::SliceComponent::EntityIdToEntityIdMap& out_sourceToCloneEntityIdMap,
            EntityList& out_clonedLooseEntities);

    } // namespace Internal


    AZ::Entity* GetEntityById(const AZ::EntityId& entityId)
    {
        AZ_Assert(entityId.IsValid(), "Invalid EntityId provided.");
        if (!entityId.IsValid())
        {
            return nullptr;
        }

        AZ::Entity* entity = nullptr;
        AZ::ComponentApplicationBus::BroadcastResult(entity, &AZ::ComponentApplicationRequests::FindEntity, entityId);
        return entity;
    }

    void GetAllComponentsForEntity(const AZ::Entity* entity, AZ::Entity::ComponentArrayType& componentsOnEntity)
    {
        if (entity)
        {
            //build a set of all active and pending components associated with the entity
            componentsOnEntity.insert(componentsOnEntity.end(), entity->GetComponents().begin(), entity->GetComponents().end());
            EditorPendingCompositionRequestBus::Event(entity->GetId(), &EditorPendingCompositionRequests::GetPendingComponents, componentsOnEntity);
            EditorDisabledCompositionRequestBus::Event(entity->GetId(), &EditorDisabledCompositionRequests::GetDisabledComponents, componentsOnEntity);
        }
    }

    void GetAllComponentsForEntity(const AZ::EntityId& entityId, AZ::Entity::ComponentArrayType& componentsOnEntity)
    {
        GetAllComponentsForEntity(GetEntity(entityId), componentsOnEntity);
    }

    AZ::Uuid GetComponentTypeId(const AZ::Component* component)
    {
        return GetUnderlyingComponentType(*component);
    }

    const AZ::SerializeContext::ClassData* GetComponentClassData(const AZ::Component* component)
    {
        return GetComponentClassDataForType(GetComponentTypeId(component));
    }

    const AZ::SerializeContext::ClassData* GetComponentClassDataForType(const AZ::Uuid& componentTypeId)
    {
        AZ::SerializeContext* serializeContext = nullptr;
        AZ::ComponentApplicationBus::BroadcastResult(serializeContext, &AZ::ComponentApplicationRequests::GetSerializeContext);

        const AZ::SerializeContext::ClassData* componentClassData = serializeContext->FindClassData(componentTypeId);
        return componentClassData;
    }

    AZStd::string GetFriendlyComponentName(const AZ::Component* component)
    {
        auto className = component->RTTI_GetTypeName();
        auto classData = GetComponentClassData(component);
        if (!classData)
        {
            return className;
        }

        if (!classData->m_editData)
        {
            return classData->m_name;
        }

        if (auto editorData = classData->m_editData->FindElementData(AZ::Edit::ClassElements::EditorData))
        {
            if (auto nameAttribute = editorData->FindAttribute(AZ::Edit::Attributes::NameLabelOverride))
            {
                AZStd::string name;
                AZ::AttributeReader nameReader(const_cast<AZ::Component*>(component), nameAttribute);
                nameReader.Read<AZStd::string>(name);
                return name;
            }
        }

        return classData->m_editData->m_name;
    }

    const char* GetFriendlyComponentDescription(const AZ::Component* component)
    {
        auto classData = GetComponentClassData(component);
        if (!classData || !classData->m_editData)
        {
            return "";
        }
        return classData->m_editData->m_description;
    }

    AZ::ComponentDescriptor* GetComponentDescriptor(const AZ::Component* component)
    {
        AZ::ComponentDescriptor* componentDescriptor = nullptr;
        AZ::ComponentDescriptorBus::EventResult(componentDescriptor, GetComponentTypeId(component), &AZ::ComponentDescriptor::GetDescriptor);
        return componentDescriptor;
    }

    Components::EditorComponentDescriptor* GetEditorComponentDescriptor(const AZ::Component* component)
    {
        Components::EditorComponentDescriptor* editorComponentDescriptor = nullptr;
        Components::EditorComponentDescriptorBus::EventResult(editorComponentDescriptor, component->RTTI_GetType(), &Components::EditorComponentDescriptor::GetEditorDescriptor);
        return editorComponentDescriptor;
    }

    Components::EditorComponentBase* GetEditorComponent(AZ::Component* component)
    {
        auto editorComponentBaseComponent = azrtti_cast<Components::EditorComponentBase*>(component);
        AZ_Assert(editorComponentBaseComponent, "Editor component does not derive from EditorComponentBase");
        return editorComponentBaseComponent;
    }

    bool ShouldInspectorShowComponent(const AZ::Component* component)
    {
        if (!component)
        {
            return false;
        }

        const AZ::SerializeContext::ClassData* classData = GetComponentClassData(component);

        // Don't show components without edit data
        if (!classData || !classData->m_editData)
        {
            return false;
        }

        // Don't show components that are set to invisible.
        if (const AZ::Edit::ElementData* editorDataElement = classData->m_editData->FindElementData(AZ::Edit::ClassElements::EditorData))
        {
            if (AZ::Edit::Attribute* visibilityAttribute = editorDataElement->FindAttribute(AZ::Edit::Attributes::Visibility))
            {
                PropertyAttributeReader reader(const_cast<AZ::Component*>(component), visibilityAttribute);
                AZ::u32 visibilityValue;
                if (reader.Read<AZ::u32>(visibilityValue))
                {
                    if (visibilityValue == AZ::Edit::PropertyVisibility::Hide)
                    {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    AZ::EntityId GetEntityIdForSortInfo(const AZ::EntityId parentId)
    {
        AZ::EntityId sortEntityId = parentId;
        if (!sortEntityId.IsValid())
        {
            AzFramework::EntityContextId editorEntityContextId = AzFramework::EntityContextId::CreateNull();
            EditorEntityContextRequestBus::BroadcastResult(editorEntityContextId, &EditorEntityContextRequestBus::Events::GetEditorEntityContextId);
            AZ::SliceComponent* rootSliceComponent = nullptr;
            AzFramework::EntityContextRequestBus::EventResult(rootSliceComponent, editorEntityContextId, &AzFramework::EntityContextRequestBus::Events::GetRootSlice);
            if (rootSliceComponent)
            {
                return rootSliceComponent->GetMetadataEntity()->GetId();
            }
        }
        return sortEntityId;
    }

    void AddEntityIdToSortInfo(const AZ::EntityId parentId, const AZ::EntityId childId, bool forceAddToBack)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);
        AZ::EntityId sortEntityId = GetEntityIdForSortInfo(parentId);

        bool success = false;
        EditorEntitySortRequestBus::EventResult(success, sortEntityId, &EditorEntitySortRequestBus::Events::AddChildEntity, childId, forceAddToBack);
        if (success && parentId != sortEntityId)
        {
            EditorEntitySortNotificationBus::Event(parentId, &EditorEntitySortNotificationBus::Events::ChildEntityOrderArrayUpdated);
        }
    }

    void AddEntityIdToSortInfo(const AZ::EntityId parentId, const AZ::EntityId childId, const AZ::EntityId beforeEntity)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);
        AZ::EntityId sortEntityId = GetEntityIdForSortInfo(parentId);

        bool success = false;
        EditorEntitySortRequestBus::EventResult(success, sortEntityId, &EditorEntitySortRequestBus::Events::AddChildEntityAtPosition, childId, beforeEntity);
        if (success && parentId != sortEntityId)
        {
            EditorEntitySortNotificationBus::Event(parentId, &EditorEntitySortNotificationBus::Events::ChildEntityOrderArrayUpdated);
        }
    }

    bool RecoverEntitySortInfo(const AZ::EntityId parentId, const AZ::EntityId childId, AZ::u64 sortIndex)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        EntityOrderArray entityOrderArray;
        EditorEntitySortRequestBus::EventResult(entityOrderArray, GetEntityIdForSortInfo(parentId), &EditorEntitySortRequestBus::Events::GetChildEntityOrderArray);

        // Make sure the child entity isn't already in order array.
        auto sortIter = AZStd::find(entityOrderArray.begin(), entityOrderArray.end(), childId);
        if (sortIter != entityOrderArray.end())
        {
            entityOrderArray.erase(sortIter);
        }
        // Make sure we don't overwrite the bounds of our vector.
        if (sortIndex > entityOrderArray.size())
        {
            sortIndex = entityOrderArray.size();
        }
        entityOrderArray.insert(entityOrderArray.begin() + sortIndex, childId);
        // Push the final array back to the sort component
        return SetEntityChildOrder(parentId, entityOrderArray);
    }

    void RemoveEntityIdFromSortInfo(const AZ::EntityId parentId, const AZ::EntityId childId)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);
        AZ::EntityId sortEntityId = GetEntityIdForSortInfo(parentId);

        bool success = false;
        EditorEntitySortRequestBus::EventResult(success, sortEntityId, &EditorEntitySortRequestBus::Events::RemoveChildEntity, childId);
        if (success && parentId != sortEntityId)
        {
            EditorEntitySortNotificationBus::Event(parentId, &EditorEntitySortNotificationBus::Events::ChildEntityOrderArrayUpdated);
        }
    }

    bool SetEntityChildOrder(const AZ::EntityId parentId, const EntityIdList& children)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);
        auto sortEntityId = GetEntityIdForSortInfo(parentId);

        bool success = false;
        EditorEntitySortRequestBus::EventResult(success, sortEntityId, &EditorEntitySortRequestBus::Events::SetChildEntityOrderArray, children);
        if (success && parentId != sortEntityId)
        {
            EditorEntitySortNotificationBus::Event(parentId, &EditorEntitySortNotificationBus::Events::ChildEntityOrderArrayUpdated);
        }
        return success;
    }

    EntityIdList GetEntityChildOrder(const AZ::EntityId parentId)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);
        EntityIdList children;
        EditorEntityInfoRequestBus::EventResult(children, parentId, &EditorEntityInfoRequestBus::Events::GetChildren);

        EntityIdList entityChildOrder;
        AZ::EntityId sortEntityId = GetEntityIdForSortInfo(parentId);
        EditorEntitySortRequestBus::EventResult(entityChildOrder, sortEntityId, &EditorEntitySortRequestBus::Events::GetChildEntityOrderArray);

        // Prune out any entries in the child order array that aren't currently known to be children
        entityChildOrder.erase(
            AZStd::remove_if(
                entityChildOrder.begin(),
                entityChildOrder.end(),
                [&children](const AZ::EntityId& entityId)
                {
                    // Return true to remove if entity id was not in the child array
                    return AZStd::find(children.begin(), children.end(), entityId) == children.end();
                }
            ),
            entityChildOrder.end()
        );

        return entityChildOrder;
    }

    //build an address based on depth and order of entities
    void GetEntityLocationInHierarchy(const AZ::EntityId& entityId, AZStd::list<AZ::u64>& location)
    {
        if(entityId.IsValid())
        {
            AZ::EntityId parentId;
            EditorEntityInfoRequestBus::EventResult(parentId, entityId, &EditorEntityInfoRequestBus::Events::GetParent);
            AZ::u64 entityOrder = 0;
            EditorEntitySortRequestBus::EventResult(entityOrder, GetEntityIdForSortInfo(parentId), &EditorEntitySortRequestBus::Events::GetChildEntityIndex, entityId);
            location.push_front(entityOrder);
            GetEntityLocationInHierarchy(parentId, location);
        }
    }

    //sort vector of entities by how they're arranged
    void SortEntitiesByLocationInHierarchy(EntityIdList& entityIds)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);
        //cache locations for faster sort
        AZStd::unordered_map<AZ::EntityId, AZStd::list<AZ::u64>> locations;
        for (auto entityId : entityIds)
        {
            GetEntityLocationInHierarchy(entityId, locations[entityId]);
        }
        AZStd::sort(entityIds.begin(), entityIds.end(), [&locations](const AZ::EntityId& e1, const AZ::EntityId& e2) {
            //sort by container contents
            const auto& locationsE1 = locations[e1];
            const auto& locationsE2 = locations[e2];
            return AZStd::lexicographical_compare(locationsE1.begin(), locationsE1.end(), locationsE2.begin(), locationsE2.end());
        });
    }

    AZ::SliceComponent* GetEntityRootSlice(AZ::EntityId entityId)
    {
        if (entityId.IsValid())
        {
            AzFramework::EntityContextId contextId = AzFramework::EntityContextId::CreateNull();
            AzFramework::EntityIdContextQueryBus::EventResult(contextId, entityId, &AzFramework::EntityIdContextQueryBus::Events::GetOwningContextId);
            if (!contextId.IsNull())
            {
                AZ::SliceComponent* rootSlice = nullptr;
                AzFramework::EntityContextRequestBus::EventResult(rootSlice, contextId, &AzFramework::EntityContextRequestBus::Events::GetRootSlice);
                return rootSlice;
            }
        }
        return nullptr;
    }

    bool ComponentArrayHasComponentOfType(const AZ::Entity::ComponentArrayType& components, AZ::Uuid componentType)
    {
        for (const AZ::Component* component : components)
        {
            if (component)
            {
                if (GetUnderlyingComponentType(*component) == componentType)
                {
                    return true;
                }
            }
        }

        return false;
    }

    bool EntityHasComponentOfType(const AZ::EntityId& entityId, AZ::Uuid componentType, bool checkPendingComponents, bool checkDisabledComponents)
    {
        AZ::Entity* entity = nullptr;
        AZ::ComponentApplicationBus::BroadcastResult(entity, &AZ::ComponentApplicationRequests::FindEntity, entityId);

        if (entity)
        {
            const AZ::Entity::ComponentArrayType components = entity->GetComponents();
            if (ComponentArrayHasComponentOfType(components, componentType))
            {
                return true;
            }

            if (checkPendingComponents)
            {
                AZ::Entity::ComponentArrayType pendingComponents;
                AzToolsFramework::EditorPendingCompositionRequestBus::Event(entity->GetId(), &AzToolsFramework::EditorPendingCompositionRequests::GetPendingComponents, pendingComponents);
                if (ComponentArrayHasComponentOfType(pendingComponents, componentType))
                {
                    return true;
                }
            }

            if (checkDisabledComponents)
            {
                // Check for disabled component
                AZ::Entity::ComponentArrayType disabledComponents;
                AzToolsFramework::EditorDisabledCompositionRequestBus::Event(entity->GetId(), &AzToolsFramework::EditorDisabledCompositionRequests::GetDisabledComponents, disabledComponents);
                if (ComponentArrayHasComponentOfType(disabledComponents, componentType))
                {
                    return true;
                }
            }
        }

        return false;
    }

    bool IsComponentWithServiceRegistered(const AZ::Crc32& serviceId)
    {
        bool result = false;

        AZ::SerializeContext* serializeContext = nullptr;
        AZ::ComponentApplicationBus::BroadcastResult(serializeContext, &AZ::ComponentApplicationRequests::GetSerializeContext);
        if (serializeContext)
        {
            serializeContext->EnumerateDerived<AZ::Component>(
                [&](const AZ::SerializeContext::ClassData* componentClass, const AZ::Uuid& knownType) -> bool
            {
                (void)knownType;

                AZ::ComponentDescriptor* componentDescriptor = nullptr;
                EBUS_EVENT_ID_RESULT(componentDescriptor, componentClass->m_typeId, AZ::ComponentDescriptorBus, GetDescriptor);
                if (componentDescriptor)
                {
                    AZ::ComponentDescriptor::DependencyArrayType providedServices;
                    componentDescriptor->GetProvidedServices(providedServices, nullptr);

                    if (AZStd::find(providedServices.begin(), providedServices.end(), serviceId) != providedServices.end())
                    {
                        result = true;
                        return false;
                    }
                }

                return true;
            }
            );
        }
        return result;
    }

    void RemoveHiddenComponents(AZ::Entity::ComponentArrayType& componentsOnEntity)
    {
        componentsOnEntity.erase(
            AZStd::remove_if(
                componentsOnEntity.begin(), componentsOnEntity.end(),
                [](const AZ::Component* component)
                {
                    return !ShouldInspectorShowComponent(component);
                }),
            componentsOnEntity.end());
    }

    bool IsSelected(const AZ::EntityId entityId)
    {
        bool selected = false;
        EditorEntityInfoRequestBus::EventResult(
            selected, entityId, &EditorEntityInfoRequestBus::Events::IsSelected);
        return selected;
    }

    bool IsSelectableInViewport(const AZ::EntityId entityId)
    {
        bool visibleFlag = false;
        EditorVisibilityRequestBus::EventResult(
            visibleFlag, entityId, &EditorVisibilityRequests::GetVisibilityFlag);

        bool locked = false;
        EditorLockComponentRequestBus::EventResult(
            locked, entityId, &EditorLockComponentRequests::GetLocked);

        return visibleFlag && !locked;
    }

    static void SetEntityLockStateRecursively(
        const AZ::EntityId entityId, const bool locked,
        const AZ::EntityId toggledEntityId, const bool toggledEntityWasLayer)
    {
        if (!entityId.IsValid())
        {
            return;
        }

        /// first set lock state of the entity in the outliner we clicked on to lock
        if (!toggledEntityWasLayer || toggledEntityId == entityId)
        {
            EditorLockComponentRequestBus::Event(entityId, &EditorLockComponentRequests::SetLocked, locked);
        }
        else
        {
            bool prevLockState = false;
            EditorLockComponentRequestBus::EventResult(
                prevLockState, entityId, &EditorLockComponentRequests::GetLocked);

            // for all other entities, if we're unlocking and they were individually already locked,
            // keep their lock state, otherwise if we're locking, set all entities to be locked.
            // note: this notification will update the lock state in ComponentEntityObject and EditorLockComponent
            bool newLockState = locked ? true : prevLockState;
            EditorLockComponentNotificationBus::Event(
                entityId, &EditorLockComponentNotificationBus::Events::OnEntityLockChanged,
                newLockState);
        }

        EntityIdList children;
        EditorEntityInfoRequestBus::EventResult(
            children, entityId, &EditorEntityInfoRequestBus::Events::GetChildren);

        for (auto childId : children)
        {
            SetEntityLockStateRecursively(childId, locked, toggledEntityId, toggledEntityWasLayer);
        }
    }

    void SetEntityLockState(const AZ::EntityId entityId, const bool locked)
    {
        // when an entity is unlocked, if it was in a locked layer(s), unlock those layers
        if (!locked)
        {
            AZ::EntityId currentEntityId = entityId;
            while (currentEntityId.IsValid())
            {
                AZ::EntityId parentId;
                EditorEntityInfoRequestBus::EventResult(
                    parentId, currentEntityId, &EditorEntityInfoRequestBus::Events::GetParent);

                bool parentLayer = false;
                Layers::EditorLayerComponentRequestBus::EventResult(
                    parentLayer, parentId, &Layers::EditorLayerComponentRequestBus::Events::HasLayer);

                if (parentLayer)
                {
                    bool parentLayerLocked = false;
                    EditorEntityInfoRequestBus::EventResult(
                        parentLayerLocked, parentId, &EditorEntityInfoRequestBus::Events::IsJustThisEntityLocked);

                    if (parentLayerLocked)
                    {
                        // if a child of a layer has its lock state changed to false, change that layer to no longer be locked
                        // do this for all layers in the hierarchy (this is because not all entities under these layers
                        // will be locked so the layer cannot be represented as 'fully' locked)
                        EditorLockComponentRequestBus::Event(parentId, &EditorLockComponentRequests::SetLocked, false);
                    }
                }

                currentEntityId = parentId;
            }
        }

        bool isLayer = false;
        Layers::EditorLayerComponentRequestBus::EventResult(
            isLayer, entityId, &Layers::EditorLayerComponentRequestBus::Events::HasLayer);

        SetEntityLockStateRecursively(entityId, locked, entityId, isLayer);
    }

    void ToggleEntityLockState(const AZ::EntityId entityId)
    {
        if (entityId.IsValid())
        {
            bool locked = false;
            EditorLockComponentRequestBus::EventResult(
                locked, entityId, &EditorLockComponentRequests::GetLocked);

            AzToolsFramework::ScopedUndoBatch undo("Toggle Entity Lock State");

            if (IsSelected(entityId))
            {
                // handles the case where we have multiple entities selected but must click one entity
                // specifically in the outliner, this will apply the lock state to all entities in the selection
                EntityIdList selectedEntityIds;
                ToolsApplicationRequestBus::BroadcastResult(
                    selectedEntityIds, &ToolsApplicationRequests::GetSelectedEntities);

                for (auto selectedId : selectedEntityIds)
                {
                    SetEntityLockState(selectedId, !locked);
                }
            }
            else
            {
                // just change the single clicked entity in the outliner
                // without affecting the current selection (should one exist)
                SetEntityLockState(entityId, !locked);
            }
        }
    }

    static void SetEntityVisibilityInternal(const AZ::EntityId entityId, const bool visibility)
    {
        bool layerEntity = false;
        Layers::EditorLayerComponentRequestBus::EventResult(
            layerEntity, entityId, &Layers::EditorLayerComponentRequestBus::Events::HasLayer);

        if (layerEntity)
        {
            // update the EditorLayerComponent state to stay in sync with Entity visibility
            Layers::EditorLayerComponentRequestBus::Event(
                entityId, &Layers::EditorLayerComponentRequestBus::Events::SetLayerChildrenVisibility,
                visibility);
        }
        else
        {
            EditorVisibilityRequestBus::Event(
                entityId, &EditorVisibilityRequestBus::Events::SetVisibilityFlag, visibility);
        }
    }

    static void SetEntityVisibilityStateRecursively(
        const AZ::EntityId entityId, const bool visible,
        const AZ::EntityId toggledEntityId, const bool toggledEntityWasLayer)
    {
        if (!entityId.IsValid())
        {
            return;
        }

        if (!toggledEntityWasLayer || toggledEntityId == entityId)
        {
            SetEntityVisibilityInternal(entityId, visible);
        }
        else
        {
            bool oldVisibilityState = IsEntitySetToBeVisible(entityId);

            bool newVisibilityState = visible ? oldVisibilityState : false;
            EditorVisibilityNotificationBus::Event(
                entityId, &EditorVisibilityNotificationBus::Events::OnEntityVisibilityChanged,
                newVisibilityState);
        }

        EntityIdList children;
        EditorEntityInfoRequestBus::EventResult(
            children, entityId, &EditorEntityInfoRequestBus::Events::GetChildren);

        for (auto childId : children)
        {
            SetEntityVisibilityStateRecursively(childId, visible, toggledEntityId, toggledEntityWasLayer);
        }
    }

    void SetEntityVisibility(const AZ::EntityId entityId, const bool visible)
    {
        // when an entity is set to visible, if it was in an invisible layer(s), make that layer visible
        if (visible)
        {
            AZ::EntityId currentEntityId = entityId;
            while (currentEntityId.IsValid())
            {
                AZ::EntityId parentId;
                EditorEntityInfoRequestBus::EventResult(
                    parentId, currentEntityId, &EditorEntityInfoRequestBus::Events::GetParent);

                bool parentLayer = false;
                Layers::EditorLayerComponentRequestBus::EventResult(
                    parentLayer, parentId, &Layers::EditorLayerComponentRequestBus::Events::HasLayer);

                if (parentLayer)
                {
                    if (!IsEntitySetToBeVisible(parentId))
                    {
                        // if a child of a layer has its visibility state changed to true, change
                        // that layer to be visible, do this for all layers in the hierarchy
                        SetEntityVisibilityInternal(parentId, true);
                        // even though layer visibility is saved to each layer individually, parents still
                        // need to be checked recursively so that the entity that was toggled can become visible
                    }
                }

                currentEntityId = parentId;
            }
        }

        bool isLayer = false;
        Layers::EditorLayerComponentRequestBus::EventResult(
            isLayer, entityId, &Layers::EditorLayerComponentRequestBus::Events::HasLayer);

        SetEntityVisibilityStateRecursively(entityId, visible, entityId, isLayer);
    }

    void ToggleEntityVisibility(const AZ::EntityId entityId)
    {
        if (entityId.IsValid())
        {
            bool visible = IsEntitySetToBeVisible(entityId);

            AzToolsFramework::ScopedUndoBatch undo("Toggle Entity Visibility");

            if (IsSelected(entityId))
            {
                EntityIdList selectedEntityIds;
                ToolsApplicationRequestBus::BroadcastResult(
                    selectedEntityIds, &ToolsApplicationRequests::GetSelectedEntities);

                for (AZ::EntityId selectedId : selectedEntityIds)
                {
                    SetEntityVisibility(selectedId, !visible);
                }
            }
            else
            {
                // just change the single clicked entity in the outliner
                // without affecting the current selection (should one exist)
                SetEntityVisibility(entityId, !visible);
            }
        }
    }

    bool IsEntitySetToBeVisible(const AZ::EntityId entityId)
    {
        // Visibility state is tracked in 5 places, see OutlinerListModel::dataForLock for info on 3 of these ways.
        // Visibility's fourth state over lock is the EditorVisibilityRequestBus has two sets of
        // setting and getting functions for visibility. Get/SetVisibilityFlag is what should be used in most cases.
        // The fifth state is tracked on layers. Layers are always invisible to other systems, so the visibility flag
        // is set false there. However, layers need to be able to toggle visibility to hide/show their children, so
        // layers have a unique flag.
        bool layerEntity = false;
        Layers::EditorLayerComponentRequestBus::EventResult(
            layerEntity, entityId, &Layers::EditorLayerComponentRequestBus::Events::HasLayer);

        bool visible = true;
        if (layerEntity)
        {
            Layers::EditorLayerComponentRequestBus::EventResult(
                visible, entityId, &Layers::EditorLayerComponentRequestBus::Events::AreLayerChildrenVisible);
        }
        else
        {
            EditorVisibilityRequestBus::EventResult(
                visible, entityId, &EditorVisibilityRequestBus::Events::GetVisibilityFlag);
        }

        return visible;
    }

    AZ::Vector3 GetWorldTranslation(const AZ::EntityId entityId)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        AZ::Vector3 worldTranslation = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(
            worldTranslation, entityId, &AZ::TransformBus::Events::GetWorldTranslation);

        return worldTranslation;
    }

    AZ::Vector3 GetLocalTranslation(const AZ::EntityId entityId)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        AZ::Vector3 localTranslation = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(
            localTranslation, entityId, &AZ::TransformBus::Events::GetLocalTranslation);

        return localTranslation;
    }

    bool CloneInstantiatedEntities(const EntityIdSet& entitiesToClone, EntityIdSet& clonedEntities)
    {
        EditorMetricsEventsBus::Broadcast(
            &EditorMetricsEventsBus::Events::EntitiesAboutToBeCloned);
        ScopedUndoBatch undoBatch("Clone Selection");

        // Track the mapping of source to cloned entity. This both helps make sure that an entity is not accidentally
        // cloned twice, as well as provides the information needed to remap entity references.
        AZ::SliceComponent::EntityIdToEntityIdMap sourceToCloneEntityIdMap;
        // Track every entity that has been cloned in the container type that AZ::EntityUtils::ReplaceEntityRefs uses.
        AZ::SliceComponent::InstantiatedContainer allEntityClonesContainer(false);
        // Loose entities can all be cloned at once, so track each one found while looking for slice instances to clone.
        EntityIdList looseEntitiesToClone;

        // Clone the entities.
        Internal::CloneSliceEntitiesAndChildren(
            entitiesToClone,
            allEntityClonesContainer.m_entities,
            sourceToCloneEntityIdMap,
            looseEntitiesToClone);
        // All entities cloned so far are slice entities, so store those in a container to use for adding to the editor.
        EntityList clonedSliceEntities(allEntityClonesContainer.m_entities.begin(), allEntityClonesContainer.m_entities.end());
        // Capture all cloned loose entities, so they can be added to the editor.
        EntityList clonedLooseEntities;

        Internal::CloneLooseEntities(
            looseEntitiesToClone,
            allEntityClonesContainer.m_entities,
            sourceToCloneEntityIdMap,
            clonedLooseEntities);

        // Update any references cloned entities have to each other.
        Internal::UpdateClonedEntityReferences(allEntityClonesContainer, sourceToCloneEntityIdMap);

        // Add the cloned entities to the editor, which will also activate them.
        EditorEntityContextRequestBus::Broadcast(
            &EditorEntityContextRequests::AddEditorEntities,
            clonedLooseEntities);
        EditorEntityContextRequestBus::Broadcast(
            &EditorEntityContextRequests::AddEditorSliceEntities,
            clonedSliceEntities);

        // Make sure an undo operation will delete all of these cloned entities.
        // Also replace the selection with the entities that have been cloned.
        Internal::UpdateUndoStackAndSelectClonedEntities(allEntityClonesContainer.m_entities, undoBatch);

        EditorMetricsEventsBus::Broadcast(
            &EditorMetricsEventsBus::Events::EntitiesCloned);

        for (const AZ::Entity* entity : allEntityClonesContainer.m_entities)
        {
            clonedEntities.insert(entity->GetId());
        }

        return !allEntityClonesContainer.m_entities.empty();
    }

    namespace Internal
    {
        void CloneSliceEntitiesAndChildren(
            const EntityIdSet& duplicationSet,
            EntityList& out_allEntityClones,
            AZ::SliceComponent::EntityIdToEntityIdMap& out_sourceToCloneEntityIdMap,
            EntityIdList& out_looseEntitiesToClone)
        {
            for (const AZ::EntityId& entityId : duplicationSet)
            {
                AZ::SliceComponent::SliceInstanceAddress owningSliceAddress;
                AzFramework::EntityIdContextQueryBus::EventResult(
                    owningSliceAddress,
                    entityId,
                    &AzFramework::EntityIdContextQueries::GetOwningSlice);
                AZ::SliceComponent::EntityAncestorList ancestors;
                bool hasInstanceEntityAncestors = false;
                if (owningSliceAddress.IsValid())
                {
                    hasInstanceEntityAncestors = owningSliceAddress.GetReference()->GetInstanceEntityAncestry(entityId, ancestors);
                }
                AZ::SliceComponent::SliceInstanceAddress sourceSliceInstance;

                // Don't clone if this entity has already been cloned.
                if (out_sourceToCloneEntityIdMap.find(entityId) != out_sourceToCloneEntityIdMap.end())
                {
                }
                // Slice roots take first priority when cloning.
                else if (hasInstanceEntityAncestors &&
                    CloneIfSliceRoot(
                        ancestors,
                        out_allEntityClones,
                        out_sourceToCloneEntityIdMap))
                {
                }
                // Subslice roots take second priority.
                else if (hasInstanceEntityAncestors &&
                    CloneIfSubsliceRoot(
                        owningSliceAddress,
                        ancestors,
                        out_allEntityClones,
                        out_sourceToCloneEntityIdMap))
                {
                }
                else
                {
                    // If this wasn't a slice root or subslice root, clone it as a loose entity.
                    out_looseEntitiesToClone.push_back(entityId);
                }

                // Search through all the children of this entity for anything that needs to be cloned.
                // Slice instance entities that are not subslice roots will have been cloned already
                // when the slice root was cloned or the subslice root was cloned. Entities may exist
                // in the hierarchy that weren't cloned if they are entities that have a slice instance entity
                // as a parent, but the entity itself is not part of that slice instance.
                EntityIdList children;
                AZ::TransformBus::EventResult(
                    /*result*/ children,
                    /*address*/ entityId,
                    &AZ::TransformBus::Events::GetChildren);

                EntityIdSet childrenSet;
                for (const AZ::EntityId& child : children)
                {
                    childrenSet.insert(child);
                }
                CloneSliceEntitiesAndChildren(
                    childrenSet,
                    out_allEntityClones,
                    out_sourceToCloneEntityIdMap,
                    out_looseEntitiesToClone);
            }
        }

        void UpdateClonedEntityReferences(
            AZ::SliceComponent::InstantiatedContainer& inout_allEntityClones,
            const AZ::SliceComponent::EntityIdToEntityIdMap& sourceToCloneEntityIdMap)
        {
            // Update all cloned entities to reference other cloned entities, if any references were to entities that were cloned.
            // This includes parenting. If Parent and Child are two cloned entities, and Child is a transform child of Parent,
            // this will update that reference, so that Child (Clone) is now a child of Parent (Clone).
            // It will also include other entity references. If Entity A has an entity reference in Script Canvas to Entity B,
            // and both are cloned, then Entity A (Clone)'s Script Canvas entity reference will now be to Unrelated Entity B (Clone).
            // If only Entity A was cloned, then Entity B will not be in the mapping, and Entity A (Clone) will continue to reference Entity B.
            AZ::EntityUtils::ReplaceEntityRefs(&inout_allEntityClones,
                [&sourceToCloneEntityIdMap](const AZ::EntityId& originalId, bool /*isEntityId*/) -> AZ::EntityId
            {
                auto findIt = sourceToCloneEntityIdMap.find(originalId);
                if (findIt == sourceToCloneEntityIdMap.end())
                {
                    return originalId; // entityId is not being remapped
                }
                else
                {
                    return findIt->second; // return the remapped id
                }
            });
        }

        void UpdateUndoStackAndSelectClonedEntities(
            const EntityList& allEntityClones,
            ScopedUndoBatch &undoBatch)
        {
            EntityIdList selectEntities;
            selectEntities.reserve(allEntityClones.size());
            for (AZ::Entity* newEntity : allEntityClones)
            {
                AZ::EntityId entityId = newEntity->GetId();
                selectEntities.push_back(entityId);

                // Make sure all cloned entities are contained within the currently active undo batch command.
                EntityCreateCommand* command = aznew EntityCreateCommand(
                    static_cast<UndoSystem::URCommandID>(entityId));
                command->Capture(newEntity);
                command->SetParent(undoBatch.GetUndoBatch());

                EditorMetricsEventsBus::Broadcast(
                    &EditorMetricsEventsBus::Events::EntityCreated,
                    entityId);
            }
            // Clear selection and select everything we cloned.
            ToolsApplicationRequestBus::Broadcast(&ToolsApplicationRequests::SetSelectedEntities, selectEntities);
        }

        bool CloneIfSliceRoot(
            const AZ::SliceComponent::EntityAncestorList& ancestors,
            EntityList& out_allEntityClones,
            AZ::SliceComponent::EntityIdToEntityIdMap& out_sourceToCloneEntityIdMap)
        {
            // This entity can't be a slice root if it has no ancestors.
            if (ancestors.size() <= 0)
            {
                return false;
            }
            // This entity is a slice root if it is the root entity of the first ancestor.
            if (!ancestors[0].m_entity || !SliceUtilities::IsRootEntity(*ancestors[0].m_entity))
            {
                return false;
            }

            AZ::SliceComponent::EntityIdToEntityIdMap sourceToCloneSliceEntityIdMap;
            AZ::SliceComponent::SliceInstanceAddress newInstance;
            EditorEntityContextRequestBus::BroadcastResult(
                newInstance,
                &EditorEntityContextRequests::CloneEditorSliceInstance,
                ancestors[0].m_sliceAddress,
                sourceToCloneSliceEntityIdMap);

            if (!newInstance.IsValid())
            {
                AZ_Warning(
                    "Cloning",
                    false,
                    "Unable to clone slice instance, check your duplicated entity selection and verify it contains the entities you expect to see.");
                return false;
            }

            for (AZ::Entity* clone : newInstance.GetInstance()->GetInstantiated()->m_entities)
            {
                out_allEntityClones.push_back(clone);
            }
            for (const AZStd::pair<AZ::EntityId, AZ::EntityId>& sourceIdToCloneId : sourceToCloneSliceEntityIdMap)
            {
                out_sourceToCloneEntityIdMap.insert(sourceIdToCloneId);
            }
            return true;
        }

        bool CloneIfSubsliceRoot(
            const AZ::SliceComponent::SliceInstanceAddress& owningSliceAddress,
            const AZ::SliceComponent::EntityAncestorList& ancestors,
            EntityList& out_allEntityClones,
            AZ::SliceComponent::EntityIdToEntityIdMap& out_sourceToCloneEntityIdMap)
        {
            bool result = false;
            // This entity can't be a subslice root if there was only one ancestor.
            if (ancestors.size() <= 1)
            {
                return result;
            }

            AZStd::vector<AZ::SliceComponent::SliceInstanceAddress> sourceSubSliceAncestry;


            AZ::SliceComponent::EntityAncestorList::const_iterator ancestorIter = ancestors.begin();
            // Skip the first, that would be a regular slice root and not a subslice root, which was already checked.
            ++ancestorIter;
            for (ancestorIter; ancestorIter != ancestors.end(); ++ancestorIter)
            {
                const AZ::SliceComponent::Ancestor& ancestor = *ancestorIter;
                if (!ancestor.m_entity || !SliceUtilities::IsRootEntity(*ancestor.m_entity))
                {
                    // This entity was not the root entity of this slice, so add the slice to the ancestor
                    // list and move on to the next ancestor.
                    sourceSubSliceAncestry.push_back(ancestor.m_sliceAddress);
                    continue;
                }

                // This entity has been verified to be a subslice root at this point, so clone the entity's subslice instance.
                AZ::SliceComponent::SliceInstanceAddress clonedAddress;
                AZ::SliceComponent::EntityIdToEntityIdMap sourceToCloneSliceEntityIdMap;
                EditorEntityContextRequestBus::BroadcastResult(
                    clonedAddress,
                    &EditorEntityContextRequests::CloneSubSliceInstance,
                    owningSliceAddress,
                    sourceSubSliceAncestry,
                    ancestor.m_sliceAddress,
                    &sourceToCloneSliceEntityIdMap);

                for (AZ::Entity* instanceEntity : clonedAddress.GetInstance()->GetInstantiated()->m_entities)
                {
                    out_allEntityClones.push_back(instanceEntity);
                }
                for (const AZStd::pair<AZ::EntityId, AZ::EntityId>& sourceIdToCloneId : sourceToCloneSliceEntityIdMap)
                {
                    out_sourceToCloneEntityIdMap.insert(sourceIdToCloneId);
                }

                // Only perform one clone, and prioritize the first found ancestor, which will track against
                // the rest of the ancestors automatically.
                result = true;
                break;
            }
            return result;
        }

        void CloneLooseEntities(
            const EntityIdList& duplicationList,
            EntityList& out_allEntityClones,
            AZ::SliceComponent::EntityIdToEntityIdMap& out_sourceToCloneEntityIdMap,
            EntityList& out_clonedLooseEntities)
        {
            AZ::SliceComponent::EntityIdToEntityIdMap looseSourceToCloneEntityIdMap;
            EntityList looseEntityClones;
            EditorEntityContextRequestBus::Broadcast(
                &EditorEntityContextRequests::CloneEditorEntities,
                duplicationList,
                looseEntityClones,
                looseSourceToCloneEntityIdMap);

            AZ_Error("Clone", looseEntityClones.size() == duplicationList.size(), "Cloned entity set is a different size from the source entity set.");

            out_allEntityClones.insert(out_allEntityClones.end(), looseEntityClones.begin(), looseEntityClones.end());
            out_clonedLooseEntities.insert(out_clonedLooseEntities.end(), looseEntityClones.begin(), looseEntityClones.end());

            for (int entityIndex = 0; entityIndex < looseEntityClones.size(); ++entityIndex)
            {
                EditorEntityContextNotificationBus::Broadcast(&EditorEntityContextNotification::OnEditorEntityDuplicated, duplicationList[entityIndex], looseEntityClones[entityIndex]->GetId());
                out_sourceToCloneEntityIdMap[duplicationList[entityIndex]] = looseEntityClones[entityIndex]->GetId();
            }
        }
    } // namespace Internal
} // namespace AzToolsFramework
