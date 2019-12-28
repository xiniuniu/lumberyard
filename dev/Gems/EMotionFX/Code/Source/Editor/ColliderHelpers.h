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

#include <AzCore/RTTI/TypeInfo.h>
#include <MCore/Source/CommandGroup.h>
#include <EMotionFX/Source/PhysicsSetup.h>
#include <QModelIndexList>

QT_FORWARD_DECLARE_CLASS(QLayout)
QT_FORWARD_DECLARE_CLASS(QMenu)
QT_FORWARD_DECLARE_CLASS(QObject)

namespace EMotionFX
{
    class ColliderHelpers
    {
    public:
        static void AddCopyColliderCommandToGroup(const Actor* actor, const Node* joint, PhysicsSetup::ColliderConfigType copyFrom, PhysicsSetup::ColliderConfigType copyTo, MCore::CommandGroup& commandGroup);
        static void CopyColliders(const QModelIndexList& modelIndices, PhysicsSetup::ColliderConfigType copyFrom, PhysicsSetup::ColliderConfigType copyTo, bool removeExistingColliders = true);
        static void AddCollider(const QModelIndexList& modelIndices, PhysicsSetup::ColliderConfigType addTo, const AZ::TypeId& colliderType);
        static void ClearColliders(const QModelIndexList& modelIndices, PhysicsSetup::ColliderConfigType removeFrom);
        static bool AreCollidersReflected();

        static void AddCopyFromMenu(QObject* parent, QMenu* parentMenu, PhysicsSetup::ColliderConfigType createForType,
            const AZStd::function<void(PhysicsSetup::ColliderConfigType copyFrom, PhysicsSetup::ColliderConfigType copyTo)>& copyFunc);
        static void AddCopyFromMenu(QObject* parent, QMenu* parentMenu, PhysicsSetup::ColliderConfigType createForType, const QModelIndexList& modelIndices);

        static QLayout* CreateCopyFromButtonLayout(QObject* parent, PhysicsSetup::ColliderConfigType createForType,
            const AZStd::function<void(PhysicsSetup::ColliderConfigType copyFrom, PhysicsSetup::ColliderConfigType copyTo)>& copyFunc);
    };
} // namespace EMotionFX
