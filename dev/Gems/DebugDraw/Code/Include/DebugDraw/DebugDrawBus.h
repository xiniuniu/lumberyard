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

#include <AzCore/EBus/EBus.h>

#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/Obb.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Vector2.h>

#include <AzCore/Script/ScriptTimePoint.h>

namespace DebugDraw
{
    class DebugDrawLineElement
    {
    public:
        AZ_CLASS_ALLOCATOR(DebugDrawLineElement, AZ::SystemAllocator, 0);
        AZ_TYPE_INFO(DebugDrawLineElement, "{A26E844A-36C6-4832-B779-237019324FAA}");
        static void Reflect(AZ::ReflectContext* context);

        AZ::EntityId            m_startEntityId;
        AZ::EntityId            m_endEntityId;
        float                   m_duration = 0.0f;
        AZ::ScriptTimePoint     m_activateTime;
        AZ::Color               m_color = AZ::Color(1.0f, 1.0f, 1.0f, 1.0f);
        AZ::Vector3             m_startWorldLocation = AZ::Vector3::CreateZero();
        AZ::Vector3             m_endWorldLocation = AZ::Vector3::CreateZero();
        AZ::ComponentId         m_owningEditorComponent = AZ::InvalidComponentId;
    };

    class DebugDrawRequests
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////

        virtual ~DebugDrawRequests() = default;

        /**
         * Draws an axis-aligned bounding-box (Aabb) in the world centered at worldLocation
         *
         * @param worldLocation     World location for the Aabb to be centered at
         * @param aabb              Aabb to render
         * @param color             Color of Aabb
         * @param duration          How long to display the Aabb for; 0 value will draw for one frame; negative values draw forever
         */
        virtual void DrawAabb(const AZ::Aabb& aabb, const AZ::Color& color, float duration) {}

        /**
         * Draws an axis-aligned bounding-box (Aabb) in the world centered at targetEntity's location
         *
         * @param targetEntity      Entity for the world location of the Aabb to be centered at
         * @param Aabb              Aabb to render
         * @param color             Color of Aabb
         * @param duration          How long to display the Aabb for; 0 value will draw for one frame; negative values draw forever
         */
        virtual void DrawAabbOnEntity(const AZ::EntityId& targetEntity, const AZ::Aabb& aabb, const AZ::Color& color, float duration) {}

        /**
         * Draws a line in the world for a specified duration
         *
         * @param startLocation     World location for the line to start at
         * @param endLocation       World location for the line to end at
         * @param color             Color of line
         * @param duration          How long to display the line for; 0 value will draw for one frame; negative values draw forever
         */
        virtual void DrawLineLocationToLocation(const AZ::Vector3& startLocation, const AZ::Vector3& endLocation, const AZ::Color& color, float duration) {}

        /**
         * Draw a batch of lines in the world

         * @param lineBatch          A collection of lines
         */
        virtual void DrawLineBatchLocationToLocation(const AZStd::vector<DebugDraw::DebugDrawLineElement>& lineBatch) {}

        /**
         * Draws a line in the world from an entity to a location for a specified duration
         *
         * @param startEntity   Entity for the world location of the line to start at
         * @param endLocation   World location for the line to end at
         * @param color         Color of line
         * @param duration      How long to display the line for; 0 value will draw for one frame; negative values draw forever
         */
        virtual void DrawLineEntityToLocation(const AZ::EntityId& startEntity, const AZ::Vector3& endLocation, const AZ::Color& color, float duration) {}

        /**
         * Draws a line in the world from an entity to a location for a specified duration
         *
         * @param startEntity   Entity for the world location of the line to start at
         * @param endEntity     Entity for the world location of the line to end at
         * @param color         Color of line
         * @param duration      How long to display the line for; 0 value will draw for one frame; negative values draw forever
         */
        virtual void DrawLineEntityToEntity(const AZ::EntityId& startEntity, const AZ::EntityId& endEntity, const AZ::Color& color, float duration) {}

        /**
         * Draws an oriented bounding-box (Obb) in the world
         *
         * @param obb               Obb to render
         * @param color             Color of Obb
         * @param duration          How long to display the Obb for; 0 value will draw for one frame; negative values draw forever
         */
        virtual void DrawObb(const AZ::Obb& obb, const AZ::Color& color, float duration) {}

        /**
         * Draws an oriented bounding-box (Obb) in the world centered at targetEntity's location and in entity space (rotates/scales with entity)
         *
         * @param targetEntity      Entity for the Obb to be transformed by (located at entity location, rotates/scales with entity)
         * @param Obb               Obb to render
         * @param color             Color of Obb
         * @param duration          How long to display the Obb for; 0 value will draw for one frame; negative values draw forever
         */
        virtual void DrawObbOnEntity(const AZ::EntityId& targetEntity, const AZ::Obb& obb, const AZ::Color& color, float duration) {}

        /**
         * Draws text in the world centered at worldLocation
         *
         * @param worldLocation     World location for the text to be centered at
         * @param text              Text to be displayed
         * @param color             Color of text
         * @param duration          How long to display the text for; 0 value will draw for one frame; negative values draw forever
         */
        virtual void DrawTextAtLocation(const AZ::Vector3& worldLocation, const AZStd::string& text, const AZ::Color& color, float duration) {}

        /**
         * Draws text in the world at targetEntity's location
         *
         * @param targetEntity      Entity for the world location of the text to be centered at
         * @param text              Text to be displayed
         * @param color             Color of text
         * @param duration          How long to display the text for; 0 value will draw for one frame; negative values draw forever
         */
        virtual void DrawTextOnEntity(const AZ::EntityId& targetEntity, const AZStd::string& text, const AZ::Color& color, float duration) {}

        /**
         * Draws text on the screen
         *
         * @param text              Text to be displayed. prefix with "-category:Name " for automatic grouping of screen text
         *                          Ex: "-category:MyRenderingInfo FPS:60" will draw "FPS:60" in a MyRenderingInfo category box
         * @param color             Color of text
         * @param duration          How long to display the text for; 0 value will draw for one frame; negative values draw forever
         */
        virtual void DrawTextOnScreen(const AZStd::string& text, const AZ::Color& color, float duration) {}

        /**
         * Draws a ray in the world for a specified duration
         *
         * @param worldLocation     World location for the ray to start at
         * @param worldDirection    World direction for the ray to draw towards
         * @param color             Color of ray
         * @param duration          How long to display the ray for; 0 value will draw for one frame; negative values draw forever
         */
        virtual void DrawRayLocationToDirection(const AZ::Vector3& worldLocation, const AZ::Vector3& worldDirection, const AZ::Color& color, float duration) {}

        /**
         * Draws a ray in the world starting at an entity's location for a specified duration
         *
         * @param startEntity       Entity for the world location of the ray to start at
         * @param worldDirection    World direction for the ray to draw towards
         * @param color             Color of ray
         * @param duration          How long to display the ray for; 0 value will draw for one frame; negative values draw forever
         */
        virtual void DrawRayEntityToDirection(const AZ::EntityId& startEntity, const AZ::Vector3& worldDirection, const AZ::Color& color, float duration) {}

        /**
         * Draws a ray in the world starting at an entity's location and ending at another's for a specified duration
         *
         * @param startEntity       Entity for the world location of the ray to start at
         * @param endEntity         Entity for the world location of the ray to end at
         * @param color             Color of ray
         * @param duration          How long to display the ray for; 0 value will draw for one frame; negative values draw forever
         */
        virtual void DrawRayEntityToEntity(const AZ::EntityId& startEntity, const AZ::EntityId& endEntity, const AZ::Color& color, float duration) {}

        /**
         * Draws a sphere in the world centered at worldLocation
         *
         * @param worldLocation     World location for the sphere to be centered at
         * @param radius            Radius of the sphere
         * @param color             Color of sphere
         * @param duration          How long to display the sphere for; 0 value will draw for one frame; negative values draw forever
         */
        virtual void DrawSphereAtLocation(const AZ::Vector3& worldLocation, float radius, const AZ::Color& color, float duration) {}

        /**
         * Draws a sphere in the world centered at targetEntity's location
         *
         * @param targetEntity      Entity for the world location of the sphere to be centered at
         * @param radius            Radius of the sphere
         * @param color             Color of sphere
         * @param duration          How long to display the sphere for; 0 value will draw for one frame; negative values draw forever
         */
        virtual void DrawSphereOnEntity(const AZ::EntityId& targetEntity, float radius, const AZ::Color& color, float duration) {}
    };
    using DebugDrawRequestBus = AZ::EBus<DebugDrawRequests>;

    class DebugDrawInternalRequests
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////

        virtual ~DebugDrawInternalRequests() = default;

        /**
         * Registers a DebugDraw component with the DebugDraw system component
         *
         * @param component         DebugDraw component that needs registered
         */
        virtual void RegisterDebugDrawComponent(AZ::Component* component) = 0;

        /**
         * Unregisters a DebugDraw component with the DebugDraw system component
         *
         * @param component         DebugDraw component that needs unregistered
         */
        virtual void UnregisterDebugDrawComponent(AZ::Component* component) = 0;
    };
    using DebugDrawInternalRequestBus = AZ::EBus<DebugDrawInternalRequests>;
} // namespace DebugDraw
