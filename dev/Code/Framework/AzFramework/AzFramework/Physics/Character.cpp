﻿/*
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

#include <AzFramework/Physics/Character.h>
#include <AzCore/Serialization/EditContext.h>


namespace Physics
{
    AZ_CLASS_ALLOCATOR_IMPL(CharacterColliderNodeConfiguration, AZ::SystemAllocator, 0)
    AZ_CLASS_ALLOCATOR_IMPL(CharacterColliderConfiguration, AZ::SystemAllocator, 0)

    void CharacterColliderNodeConfiguration::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<CharacterColliderNodeConfiguration>()
                ->Version(1)
                ->Field("name", &CharacterColliderNodeConfiguration::m_name)
                ->Field("shapes", &CharacterColliderNodeConfiguration::m_shapes)
            ;
        }
    }

    Physics::CharacterColliderNodeConfiguration* CharacterColliderConfiguration::FindNodeConfigByName(const AZStd::string& nodeName) const
    {
        auto nodeIterator = AZStd::find_if(m_nodes.begin(), m_nodes.end(), [&nodeName](const Physics::CharacterColliderNodeConfiguration& node)
            {
                return node.m_name == nodeName;
            });

        if (nodeIterator != m_nodes.end())
        {
            return const_cast<Physics::CharacterColliderNodeConfiguration*>(nodeIterator);
        }

        return nullptr;
    }

    AZ::Outcome<size_t> CharacterColliderConfiguration::FindNodeConfigIndexByName(const AZStd::string& nodeName) const
    {
        auto nodeIterator = AZStd::find_if(m_nodes.begin(), m_nodes.end(), [&nodeName](const Physics::CharacterColliderNodeConfiguration& node)
            {
                return node.m_name == nodeName;
            });

        if (nodeIterator != m_nodes.end())
        {
            return AZ::Success(static_cast<size_t>(nodeIterator - m_nodes.begin()));
        }

        return AZ::Failure();
    }

    void CharacterColliderConfiguration::RemoveNodeConfigByName(const AZStd::string& nodeName)
    {
        const AZ::Outcome<size_t> configIndex = FindNodeConfigIndexByName(nodeName);
        if (configIndex.IsSuccess())
        {
            m_nodes.erase(m_nodes.begin() + configIndex.GetValue());
        }
    }

    void CharacterColliderConfiguration::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<CharacterColliderConfiguration>()
                ->Version(1)
                ->Field("nodes", &CharacterColliderConfiguration::m_nodes)
            ;
        }
    }

    void CharacterConfiguration::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<CharacterConfiguration>()
                ->Version(1)
                ->Field("CollisionLayer", &CharacterConfiguration::m_collisionLayer)
                ->Field("CollisionGroupId", &CharacterConfiguration::m_collisionGroupId)
                ->Field("Material", &CharacterConfiguration::m_materialSelection)
                ->Field("UpDirection", &CharacterConfiguration::m_upDirection)
                ->Field("MaximumSlopeAngle", &CharacterConfiguration::m_maximumSlopeAngle)
                ->Field("StepHeight", &CharacterConfiguration::m_stepHeight)
                ->Field("MinDistance", &CharacterConfiguration::m_minimumMovementDistance)
                ->Field("DirectControl", &CharacterConfiguration::m_directControl)
            ;

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<CharacterConfiguration>(
                    "Character Configuration", "Character Configuration")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CharacterConfiguration::m_collisionLayer,
                    "Collision Layer", "The collision layer assigned to the controller")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CharacterConfiguration::m_collisionGroupId,
                    "Collides With", "The collision layers this character controller collides with")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CharacterConfiguration::m_materialSelection,
                    "Physics Material", "Assign physics material library and select materials to use for the character")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CharacterConfiguration::m_maximumSlopeAngle,
                    "Maximum Slope Angle", "Maximum angle of slopes on which the controller can walk")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                        ->Attribute(AZ::Edit::Attributes::Step, 1.0f)
                        ->Attribute(AZ::Edit::Attributes::Max, 89.0f)
                        ->Attribute(AZ::Edit::Attributes::Suffix, " degrees")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CharacterConfiguration::m_stepHeight,
                    "Step Height", "Affects the height of steps the character controller will be able to traverse")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                        ->Attribute(AZ::Edit::Attributes::Step, 0.1f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &CharacterConfiguration::m_minimumMovementDistance,
                    "Minimum Movement Distance", "To avoid jittering, the controller will not attempt to move distances below this")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                        ->Attribute(AZ::Edit::Attributes::Step, 0.001f)
                ;
            }
        }
    }
} // Physics