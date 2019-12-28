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

#include <AzCore/Component/Entity.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzFramework/Entity/EntityContext.h>
#include <AzFramework/Components/TransformComponent.h>
#include <AzFramework/API/ApplicationAPI.h>

#include "GameEntityContextComponent.h"

namespace AzFramework
{
    //=========================================================================
    // Reflect
    //=========================================================================
    void GameEntityContextComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<GameEntityContextComponent, AZ::Component>()
                ;

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<GameEntityContextComponent>(
                    "Game Entity Context", "Owns entities in the game runtime, as well as during play-in-editor")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Engine")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System", 0xc94d118b))
                    ;
            }
        }

        if (AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->EBus<GameEntityContextRequestBus>("GameEntityContextRequestBus")
                ->Event("CreateGameEntity", &GameEntityContextRequestBus::Events::CreateGameEntityForBehaviorContext)
                    ->Attribute(AZ::Script::Attributes::ExcludeFrom, AZ::Script::Attributes::ExcludeFlags::All)
                ->Event("DestroyGameEntity", &GameEntityContextRequestBus::Events::DestroyGameEntity)
                ->Event("DestroyGameEntityAndDescendants", &GameEntityContextRequestBus::Events::DestroyGameEntityAndDescendants)
                ->Event("ActivateGameEntity", &GameEntityContextRequestBus::Events::ActivateGameEntity)
                ->Event("DeactivateGameEntity", &GameEntityContextRequestBus::Events::DeactivateGameEntity)
                ->Event("DestroyDynamicSliceByEntity", &GameEntityContextRequestBus::Events::DestroyDynamicSliceByEntity)
                ->Event("GetEntityName", &GameEntityContextRequestBus::Events::GetEntityName)

                // Deprecated-renamed APIs. These will warn if used.
                ->Event("ActivateGameEntityById", &GameEntityContextRequestBus::Events::ActivateGameEntityById)
                ->Event("DeactivateGameEntityById", &GameEntityContextRequestBus::Events::DeactivateGameEntityById)
                ->Event("DestroySliceByEntityId", &GameEntityContextRequestBus::Events::DestroySliceByEntityId)
                ->Event("DestroySliceByEntity", &GameEntityContextRequestBus::Events::DestroySliceByEntity)
                ;
        }
    }

    //=========================================================================
    // GameEntityContextComponent ctor
    //=========================================================================
    GameEntityContextComponent::GameEntityContextComponent()
        : EntityContext(EntityContextId::CreateRandom())
    {
    }

    //=========================================================================
    // GameEntityContextComponent dtor
    //=========================================================================
    GameEntityContextComponent::~GameEntityContextComponent()
    {
    }

    //=========================================================================
    // Init
    //=========================================================================
    void GameEntityContextComponent::Init()
    {
    }

    //=========================================================================
    // Activate
    //=========================================================================
    void GameEntityContextComponent::Activate()
    {
        InitContext();

        GameEntityContextRequestBus::Handler::BusConnect();
    }

    //=========================================================================
    // Deactivate
    //=========================================================================
    void GameEntityContextComponent::Deactivate()
    {
        GameEntityContextRequestBus::Handler::BusDisconnect();

        DestroyContext();
    }

    //=========================================================================
    // GameEntityContextRequestBus::ResetGameContext
    //=========================================================================
    void GameEntityContextComponent::ResetGameContext()
    {
        ResetContext();

        SliceInstantiationResultBus::MultiHandler::BusDisconnect();
        m_instantiatingDynamicSlices.clear();
    }

    //=========================================================================
    // GameEntityContextRequestBus::CreateGameEntity
    //=========================================================================
    AZ::Entity* GameEntityContextComponent::CreateGameEntity(const char* name)
    {
        return CreateEntity(name);
    }

    //=========================================================================
    // GameEntityContextRequestBus::CreateGameEntityForBehaviorContext
    //=========================================================================
    BehaviorEntity GameEntityContextComponent::CreateGameEntityForBehaviorContext(const char* name)
    {
        if (AZ::Entity* entity = CreateGameEntity(name))
        {
            return BehaviorEntity(entity->GetId());
        }
        return BehaviorEntity();
    }

    //=========================================================================
    // GameEntityContextRequestBus::AddGameEntity
    //=========================================================================
    void GameEntityContextComponent::AddGameEntity(AZ::Entity* entity)
    {
        AddEntity(entity);
    }


    //=========================================================================
    // CreateEntity
    //=========================================================================
    AZ::Entity* GameEntityContextComponent::CreateEntity(const char* name)
    {
        auto entity = aznew AZ::Entity(name);

        // Caller will want to configure entity before it's activated.
        entity->SetRuntimeActiveByDefault(false);

        AddEntity(entity);

        return entity;
    }

    //=========================================================================
    // OnRootSliceCreated
    //=========================================================================
    void GameEntityContextComponent::OnRootSliceCreated()
    {
        // We want all dynamic slices spawned in the game entity context to be
        // instantiated, which depends on the root slice itself being instantiated.
        GetRootSlice()->Instantiate();
    }

    //=========================================================================
    // OnRootSlicePreDestruction
    //=========================================================================
    void GameEntityContextComponent::OnRootSlicePreDestruction()
    {
        // Clearing dynamic slice destruction queue now since all slices in it 
        // are being deleted during the destruction phase (ResetContext()).
        // We don't want this list holding onto deleted slices!
        m_dynamicSlicesToDestroy.clear();
    }

    //=========================================================================
    // OnContextReset
    //=========================================================================
    void GameEntityContextComponent::OnContextReset()
    {
        EBUS_EVENT(GameEntityContextEventBus, OnGameEntitiesReset);
    }

    //=========================================================================
    // GameEntityContextComponent::ValidateEntitiesAreValidForContext
    //=========================================================================
    bool GameEntityContextComponent::ValidateEntitiesAreValidForContext(const EntityList& entities)
    {
        // All entities in a slice being instantiated in the level editor should
        // have the TransformComponent on them. Since it is not possible to create
        // a slice with entities from different contexts, it is OK to check
        // the first entity only
        if (entities.size() > 0)
        {
            return entities[0]->FindComponent<AzFramework::TransformComponent>() != nullptr;
        }

        return true;
    }

    //=========================================================================
    // GameEntityContextComponent::OnContextEntitiesAdded
    //=========================================================================
    void GameEntityContextComponent::OnContextEntitiesAdded(const EntityList& entities)
    {
        EntityContext::OnContextEntitiesAdded(entities);

    #if (AZ_TRAIT_PUMP_SYSTEM_EVENTS_WHILE_LOADING)
        auto timeOfLastEventPump = AZStd::chrono::high_resolution_clock::now();
        auto PumpSystemEventsIfNeeded = [&timeOfLastEventPump]()
        {
            static const AZStd::chrono::milliseconds maxMillisecondsBetweenSystemEventPumps(AZ_TRAIT_PUMP_SYSTEM_EVENTS_WHILE_LOADING_INTERVAL_MS);
            const auto now = AZStd::chrono::high_resolution_clock::now();
            if (now - timeOfLastEventPump > maxMillisecondsBetweenSystemEventPumps)
            {
                timeOfLastEventPump = now;
                ApplicationRequests::Bus::Broadcast(&ApplicationRequests::PumpSystemEventLoopUntilEmpty);
            }
        };
    #endif // (AZ_TRAIT_PUMP_SYSTEM_EVENTS_WHILE_LOADING)

        for (AZ::Entity* entity : entities)
        {
            if (entity->GetState() == AZ::Entity::ES_CONSTRUCTED)
            {
                entity->Init();
            #if (AZ_TRAIT_PUMP_SYSTEM_EVENTS_WHILE_LOADING)
                PumpSystemEventsIfNeeded();
            #endif // (AZ_TRAIT_PUMP_SYSTEM_EVENTS_WHILE_LOADING)
            }
        }

        for (AZ::Entity* entity : entities)
        {
            if (entity->GetState() == AZ::Entity::ES_INIT)
            {
                if (entity->IsRuntimeActiveByDefault())
                {
                    entity->Activate();
                #if (AZ_TRAIT_PUMP_SYSTEM_EVENTS_WHILE_LOADING)
                    PumpSystemEventsIfNeeded();
                #endif // (AZ_TRAIT_PUMP_SYSTEM_EVENTS_WHILE_LOADING)
                }
            }
        }
    }

    //=========================================================================
    // GameEntityContextComponent::DestroyGameEntityById
    //=========================================================================
    void GameEntityContextComponent::DestroyGameEntity(const AZ::EntityId& id)
    {
        DestroyGameEntityInternal(id, false);
    }

    //=========================================================================
    // GameEntityContextComponent::DestroyGameEntityAndDescendantsById
    //=========================================================================
    void GameEntityContextComponent::DestroyGameEntityAndDescendants(const AZ::EntityId& id)
    {
        DestroyGameEntityInternal(id, true);
    }

    //=========================================================================
    // GameEntityContextComponent::DestroyGameEntityInternal
    //=========================================================================
    void GameEntityContextComponent::DestroyGameEntityInternal(const AZ::EntityId& entityId, bool destroyChildren)
    {
        AZStd::vector<AZ::EntityId> entityIdsToBeDeleted;

        AZ::Entity* entity = nullptr;
        EBUS_EVENT_RESULT(entity, AZ::ComponentApplicationBus, FindEntity, entityId);
        if (entity)
        {
            if (destroyChildren)
            {
                EBUS_EVENT_ID_RESULT(entityIdsToBeDeleted, entityId, AZ::TransformBus, GetAllDescendants);
            }

            // Inserting the parent to the list before its children; it will be deleted last by the reverse iterator
            entityIdsToBeDeleted.insert(entityIdsToBeDeleted.begin(), entityId);
        }

        for (AZStd::vector<AZ::EntityId>::reverse_iterator entityIdIter = entityIdsToBeDeleted.rbegin();
            entityIdIter != entityIdsToBeDeleted.rend(); ++entityIdIter)
        {
            AZ::Entity* currentEntity = nullptr;
            EBUS_EVENT_RESULT(currentEntity, AZ::ComponentApplicationBus, FindEntity, *entityIdIter);
            if (currentEntity)
            {
                if (currentEntity->GetState() == AZ::Entity::ES_ACTIVE)
                {
                    // Deactivate the entity, we'll destroy it as soon as it is safe.
                    currentEntity->Deactivate();
                }
                else
                {
                    // Don't activate the entity, it will be destroyed.
                    currentEntity->SetRuntimeActiveByDefault(false);
                }
            }
        }

        // Queue the entity destruction on the tick bus for safety, this guarantees that we will not attempt to destroy
        // an entity during activation.
        AZStd::function<void()> destroyEntity = [this,entityIdsToBeDeleted]() mutable
        {
            for (AZStd::vector<AZ::EntityId>::reverse_iterator entityIdIter = entityIdsToBeDeleted.rbegin();
                 entityIdIter != entityIdsToBeDeleted.rend(); ++entityIdIter)
            {
                EntityContext::DestroyEntityById(*entityIdIter);
            }
        };

        EBUS_QUEUE_FUNCTION(AZ::TickBus, destroyEntity);
    }

    //=========================================================================
    // GameEntityContextComponent::DestroyDynamicSliceByEntity
    //=========================================================================
    bool GameEntityContextComponent::DestroyDynamicSliceByEntity(const AZ::EntityId& id)
    {
        AZ::SliceComponent* rootSlice = GetRootSlice();
        if (rootSlice)
        {
            const auto address = rootSlice->FindSlice(id);
            if (address.GetInstance())
            {
                auto sliceInstance = address.GetInstance();
                const auto instantiatedSliceEntities = sliceInstance->GetInstantiated();
                if (instantiatedSliceEntities)
                {
                    for (AZ::Entity* currentEntity : instantiatedSliceEntities->m_entities)
                    {
                        if (currentEntity)
                        {
                            if (currentEntity->GetState() == AZ::Entity::ES_ACTIVE)
                            {
                                currentEntity->Deactivate();
                            }
                            MarkEntityForNoActivation(currentEntity->GetId());
                        }
                    }
                }

                // Queue Slice deletion until next tick. This prevents deleting a dynamic slice from an active entity within that slice.
                m_dynamicSlicesToDestroy.insert(address);

                AZStd::function<void()> deleteDynamicSlices = [this]()
                {
                    this->FlushDynamicSliceDeletionList();
                };

                AZ::TickBus::QueueFunction(deleteDynamicSlices);
                return true;
            }
        }

        return false;
    }

    //=========================================================================
    // GameEntityContextComponent::ActivateGameEntity
    //=========================================================================
    void GameEntityContextComponent::ActivateGameEntity(const AZ::EntityId& entityId)
    {
        ActivateEntity(entityId);
    }

    //=========================================================================
    // GameEntityContextComponent::FlushDynamicSliceDeletionList
    //=========================================================================
    void GameEntityContextComponent::FlushDynamicSliceDeletionList()
    {
        for (const auto& sliceAddress : m_dynamicSlicesToDestroy)
        {
            GetRootSlice()->RemoveSliceInstance(sliceAddress);
        }

        m_dynamicSlicesToDestroy.clear();
    }

    //=========================================================================
    // GameEntityContextComponent::DeactivateGameEntity
    //=========================================================================
    void GameEntityContextComponent::DeactivateGameEntity(const AZ::EntityId& entityId)
    {
        DeactivateEntity(entityId);
    }

    //=========================================================================
    // GameEntityContextRequestBus::InstantiateDynamicSlice
    //=========================================================================
    SliceInstantiationTicket GameEntityContextComponent::InstantiateDynamicSlice(const AZ::Data::Asset<AZ::Data::AssetData>& sliceAsset, const AZ::Transform& worldTransform, const AZ::IdUtils::Remapper<AZ::EntityId>::IdMapper& customIdMapper)
    {
        if (sliceAsset.GetId().IsValid())
        {
            const SliceInstantiationTicket ticket = InstantiateSlice(sliceAsset, customIdMapper);
            if (ticket)
            {
                InstantiatingDynamicSliceInfo& info = m_instantiatingDynamicSlices[ticket];
                info.m_asset = sliceAsset;
                info.m_transform = worldTransform;

                SliceInstantiationResultBus::MultiHandler::BusConnect(ticket);

                return ticket;
            }
        }

        return SliceInstantiationTicket();
    }

    //=========================================================================
    // GameEntityContextRequestBus::CancelDynamicSliceInstantiation
    //=========================================================================
    void GameEntityContextComponent::CancelDynamicSliceInstantiation(const SliceInstantiationTicket& ticket)
    {
        // Cleanup of m_instantiatingDynamicSlices will be handled by OnSliceInstantiationFailed()

        CancelSliceInstantiation(ticket);
    }

    //=========================================================================
    // EntityContextEventBus::LoadFromStream
    //=========================================================================
    bool GameEntityContextComponent::LoadFromStream(AZ::IO::GenericStream& stream, bool remapIds)
    {
        if (AzFramework::EntityContext::LoadFromStream(stream, remapIds))
        {
            EBUS_EVENT(GameEntityContextEventBus, OnGameEntitiesStarted);
            return true;
        }

        return false;
    }

    //=========================================================================
    // GameEntityContextRequestBus::GetEntityName
    //=========================================================================
    AZStd::string GameEntityContextComponent::GetEntityName(const AZ::EntityId& id)
    {
        AZStd::string entityName;
        AZ::ComponentApplicationBus::BroadcastResult(entityName, &AZ::ComponentApplicationBus::Events::GetEntityName, id);
        return entityName;
    }

    //=========================================================================
    // GameEntityContextRequestBus::MarkEntityForNoActivation
    //=========================================================================
    void GameEntityContextComponent::MarkEntityForNoActivation(AZ::EntityId entityId)
    {
        AZ::Entity* entity = nullptr;
        AZ::ComponentApplicationBus::BroadcastResult(entity, &AZ::ComponentApplicationBus::Events::FindEntity, entityId);

        AZ_Error("GameEntityContext", entity,
            "Failed to locate entity with id %s. It is either not yet Initialized, or the Id is invalid.",
            entityId.ToString().c_str());

        if (entity)
        {
            entity->SetRuntimeActiveByDefault(false);
        }
    }

    //=========================================================================
    // EntityContextEventBus::OnSlicePreInstantiate
    //=========================================================================
    void GameEntityContextComponent::OnSlicePreInstantiate(const AZ::Data::AssetId& /*sliceAssetId*/, const AZ::SliceComponent::SliceInstanceAddress& sliceAddress)
    {
        const SliceInstantiationTicket ticket = *SliceInstantiationResultBus::GetCurrentBusId();

        auto instantiatingIter = m_instantiatingDynamicSlices.find(ticket);
        if (instantiatingIter != m_instantiatingDynamicSlices.end())
        {
            InstantiatingDynamicSliceInfo& instantiating = instantiatingIter->second;

            const AZ::SliceComponent::EntityList& entities = sliceAddress.GetInstance()->GetInstantiated()->m_entities;

            // If the context was loaded from a stream and Ids were remapped, fix up entity Ids in that slice that
            // point to entities in the stream (i.e. level entities).
            if (!m_loadedEntityIdMap.empty())
            {
                AZ::EntityUtils::SerializableEntityContainer instanceEntities;
                instanceEntities.m_entities = entities;
                AZ::IdUtils::Remapper<AZ::EntityId>::RemapIds(&instanceEntities,
                    [this](const AZ::EntityId& originalId, bool isEntityId, const AZStd::function<AZ::EntityId()>&) -> AZ::EntityId
                    {
                        if (!isEntityId)
                        {
                            auto iter = m_loadedEntityIdMap.find(originalId);
                            if (iter != m_loadedEntityIdMap.end())
                            {
                                return iter->second;
                            }
                        }
                        return originalId;

                    }, m_serializeContext, false);
            }

            // Set initial transform for slice root entity based on the requested root transform for the instance.
            for (AZ::Entity* entity : entities)
            {
                auto* transformComponent = entity->FindComponent<AzFramework::TransformComponent>();
                if (transformComponent)
                {
                    // Non-root entities will be positioned relative to their parents.
                    if (!transformComponent->GetParentId().IsValid())
                    {
                        // Note: Root slice entity always has translation at origin, so this maintains scale & rotation.
                        transformComponent->SetWorldTM(instantiating.m_transform * transformComponent->GetWorldTM());
                    }
                }
            }
        }
    }

    //=========================================================================
    // EntityContextEventBus::OnSliceInstantiated
    //=========================================================================
    void GameEntityContextComponent::OnSliceInstantiated(const AZ::Data::AssetId& sliceAssetId, const AZ::SliceComponent::SliceInstanceAddress& instance)
    {
        const SliceInstantiationTicket ticket = *SliceInstantiationResultBus::GetCurrentBusId();

        if (m_instantiatingDynamicSlices.erase(ticket) > 0)
        {
            EBUS_EVENT(GameEntityContextEventBus, OnSliceInstantiated, sliceAssetId, instance, ticket);
        }

        SliceInstantiationResultBus::MultiHandler::BusDisconnect(ticket);
    }

    //=========================================================================
    // EntityContextEventBus::OnSliceInstantiationFailed
    //=========================================================================
    void GameEntityContextComponent::OnSliceInstantiationFailed(const AZ::Data::AssetId& sliceAssetId)
    {
        const SliceInstantiationTicket ticket = *SliceInstantiationResultBus::GetCurrentBusId();

        if (m_instantiatingDynamicSlices.erase(ticket) > 0)
        {
            EBUS_EVENT(GameEntityContextEventBus, OnSliceInstantiationFailed, sliceAssetId, ticket);
        }

        SliceInstantiationResultBus::MultiHandler::BusDisconnect(ticket);
    }
} // namespace AzFramework
