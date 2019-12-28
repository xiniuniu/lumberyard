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

namespace PhysX
{
    inline ActorData* Utils::GetUserData(const physx::PxActor* actor)
    {
        if (actor == nullptr || actor->userData == nullptr)
        {
            return nullptr;
        }

        ActorData* actorData = static_cast<ActorData*>(actor->userData);
        if (!actorData->IsValid())
        {
            AZ_Warning("PhysX::Utils::GetUserData", false, "The actor data does not look valid and is not safe to use");
            return nullptr;
        }

        return actorData;
    }

    inline Physics::Material* Utils::GetUserData(const physx::PxMaterial* material)
    {
        return (material == nullptr) ? nullptr : static_cast<Physics::Material*>(material->userData);
    }

    inline Physics::Shape* Utils::GetUserData(const physx::PxShape* pxShape)
    {
        return (pxShape == nullptr) ? nullptr : static_cast<Physics::Shape*>(pxShape->userData);
    }

    inline Physics::World* Utils::GetUserData(physx::PxScene* scene)
    {
        return scene ? static_cast<Physics::World*>(scene->userData) : nullptr;
    }

    inline AZ::u64 Utils::Collision::Combine(AZ::u32 word0, AZ::u32 word1)
    {
        return (static_cast<AZ::u64>(word0) << 32) | word1;
    }

    inline void Utils::Collision::SetLayer(const Physics::CollisionLayer& layer, physx::PxFilterData& filterData)
    {
        filterData.word0 = (physx::PxU32)(layer.GetMask() >> 32);
        filterData.word1 = (physx::PxU32)(layer.GetMask());
    }

    inline void Utils::Collision::SetGroup(const Physics::CollisionGroup& group, physx::PxFilterData& filterData)
    {
        filterData.word2 = (physx::PxU32)(group.GetMask() >> 32);
        filterData.word3 = (physx::PxU32)(group.GetMask());
    }

    inline void Utils::Collision::SetCollisionLayerAndGroup(physx::PxShape* shape, const Physics::CollisionLayer& layer, const Physics::CollisionGroup&  group)
    {
        physx::PxFilterData filterData;
        SetLayer(layer, filterData);
        SetGroup(group, filterData);
        shape->setSimulationFilterData(filterData);
    }

    inline bool Utils::Collision::ShouldCollide(const physx::PxFilterData& filterData0, const physx::PxFilterData& filterData1)
    {
        AZ::u64 layer0 = Combine(filterData0.word0, filterData0.word1);
        AZ::u64 layer1 = Combine(filterData1.word0, filterData1.word1);
        AZ::u64 group0 = Combine(filterData0.word2, filterData0.word3);
        AZ::u64 group1 = Combine(filterData1.word2, filterData1.word3);
        return (layer0 & group1) && (layer1 & group0);
    }
}