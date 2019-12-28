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
#include "StdAfx.h"
#include <AzCore/Rtti/RTTI.h>
#include <AzToolsFramework/Entity/EditorEntityFixupComponent.h>
#include <AzToolsFramework/ToolsComponents/GenericComponentWrapper.h>
#include <AzToolsFramework/ToolsComponents/TransformComponent.h>

namespace AzToolsFramework
{
    void EditorEntityFixupComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<EditorEntityFixupComponent, AZ::Component>();
        }
    }

    void EditorEntityFixupComponent::Activate()
    {
        AZ::SliceAssetSerializationNotificationBus::Handler::BusConnect();
    }

    void EditorEntityFixupComponent::Deactivate()
    {
        AZ::SliceAssetSerializationNotificationBus::Handler::BusDisconnect();
    }

    // Why do the fixup after slice entities are loaded?
    // If a component changes in a way that requires querying or modifying the
    // entity, we can't make that change during version-conversion
    // because converters don't have access to parent data.
    // We could have done fixup after writing the AZ::Entity from data,
    // but Entities are written with high frequency for many reasons (ex: undo).
    // Therefore, do the fixup after slice entities finish loading.
    // Any entity that's saved out to disk will come in via a SliceAsset,
    // so this is a safe place for the check.
    void EditorEntityFixupComponent::OnSliceEntitiesLoaded(const AZStd::vector<AZ::Entity*>& entities)
    {
        for (AZ::Entity* entity : entities)
        {
            for (AZ::Component* component : entity->GetComponents())
            {
                // It's possible that a component which hadn't been an editor-component
                // has been changed into an editor-component over the course of development.
                // If this happens to the component inside GenericComponentWrapper,
                // the wrapper should swap places with the component and delete itself.
                //
                // Can't deal with this in GenericComponentWrapper::Init() because
                // the entity is in the middle of iterating over its components
                // and won't tolerate components being added or removed.
                if (auto genericComponentWrapper = azrtti_cast<Components::GenericComponentWrapper*>(component))
                {
                    if (auto wrappedComponent = azrtti_cast<Components::EditorComponentBase*>(genericComponentWrapper->GetTemplate()))
                    {
                        entity->SwapComponents(genericComponentWrapper, wrappedComponent);

                        genericComponentWrapper->ReleaseTemplate();
                        delete genericComponentWrapper;
                    }
                }

                // Required only after an up-conversion from TransformComponent version < 6 to >= 6.
                // We used to store slice root entity Id, which could be our own Id.
                // Since we don't have an entity association during data conversion,
                // we have to fix up this case post-entity-assignment.
                // 
                // Can't deal with this in component's Init() because some slice
                // functions directly access uninitialized TransformComponents.
                else if (auto editorTransformComponent = azrtti_cast<Components::TransformComponent*>(component))
                {
                    if (editorTransformComponent->GetParentId() == entity->GetId())
                    {
                        editorTransformComponent->SetParent(AZ::EntityId());
                    }
                }
            }
        }
    }
} // namespace AzToolsFramework