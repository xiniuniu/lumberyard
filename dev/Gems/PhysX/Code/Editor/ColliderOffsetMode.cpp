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

#include <PhysX_precompiled.h>

#include "ColliderOffsetMode.h"
#include <PhysX/EditorColliderComponentRequestBus.h>

#include <AzToolsFramework/Manipulators/ManipulatorManager.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Component/ComponentBus.h>

namespace PhysX
{
    AZ_CLASS_ALLOCATOR_IMPL(ColliderOffsetMode, AZ::SystemAllocator, 0);

    ColliderOffsetMode::ColliderOffsetMode()
        : m_translationManipulators(AzToolsFramework::TranslationManipulators::Dimensions::Three, AZ::Transform::Identity())
    {
    }

    void ColliderOffsetMode::Setup(const AZ::EntityComponentIdPair& idPair)
    {
        AZ::Transform worldTransform;
        AZ::TransformBus::EventResult(worldTransform, idPair.GetEntityId(), &AZ::TransformInterface::GetWorldTM);

        AZ::Vector3 colliderOffset;
        PhysX::EditorColliderComponentRequestBus::EventResult(colliderOffset, idPair, &PhysX::EditorColliderComponentRequests::GetColliderOffset);

        m_translationManipulators.SetSpace(worldTransform);
        m_translationManipulators.SetLocalPosition(colliderOffset);
        m_translationManipulators.AddEntityComponentIdPair(idPair);
        m_translationManipulators.Register(AzToolsFramework::g_mainManipulatorManagerId);
        AzToolsFramework::ConfigureTranslationManipulatorAppearance3d(&m_translationManipulators);

        m_translationManipulators.InstallLinearManipulatorMouseMoveCallback([this, idPair](
            const AzToolsFramework::LinearManipulator::Action& action)
        {
            OnManipulatorMoved(action.LocalPosition(), idPair);
        });

        m_translationManipulators.InstallPlanarManipulatorMouseMoveCallback([this, idPair](
            const AzToolsFramework::PlanarManipulator::Action& action)
        {
            OnManipulatorMoved(action.LocalPosition(), idPair);
        });

        m_translationManipulators.InstallSurfaceManipulatorMouseMoveCallback([this, idPair](
            const AzToolsFramework::SurfaceManipulator::Action& action)
        {
            OnManipulatorMoved(action.LocalPosition(), idPair);
        });
    }

    void ColliderOffsetMode::Refresh(const AZ::EntityComponentIdPair& idPair)
    {
        AZ::Vector3 colliderOffset = AZ::Vector3::CreateZero();
        PhysX::EditorColliderComponentRequestBus::EventResult(colliderOffset, idPair, &PhysX::EditorColliderComponentRequests::GetColliderOffset);
        m_translationManipulators.SetLocalPosition(colliderOffset);
    }

    void ColliderOffsetMode::Teardown(const AZ::EntityComponentIdPair& idPair)
    {
        m_translationManipulators.RemoveEntityComponentIdPair(idPair);
        m_translationManipulators.Unregister();
    }

    void ColliderOffsetMode::OnManipulatorMoved(const AZ::Vector3& position, const AZ::EntityComponentIdPair& idPair)
    {
        m_translationManipulators.SetLocalPosition(position);
        PhysX::EditorColliderComponentRequestBus::Event(idPair, &PhysX::EditorColliderComponentRequests::SetColliderOffset, position);
    }

    void ColliderOffsetMode::ResetValues(const AZ::EntityComponentIdPair& idPair)
    {
        PhysX::EditorColliderComponentRequestBus::Event(idPair, &PhysX::EditorColliderComponentRequests::SetColliderOffset, AZ::Vector3::CreateZero());
    }
}