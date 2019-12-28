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

#include "ViewportTypes.h"

#include <AzCore/RTTI/RTTI.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace AzToolsFramework
{
    namespace ViewportInteraction
    {
        const AZ::s32 g_mainViewportEntityDebugDisplayId = AZ_CRC("MainViewportEntityDebugDisplayId", 0x58ae7fe8);

        void ViewportInteractionReflect(AZ::ReflectContext* context)
        {
            if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
            {
                serializeContext->Class<KeyboardModifiers>()->
                    Field("KeyboardModifiers", &KeyboardModifiers::m_keyModifiers);

                serializeContext->Class<MouseButtons>()->
                    Field("MouseButtons", &MouseButtons::m_mouseButtons);

                serializeContext->Class<InteractionId>()->
                    Field("CameraId", &InteractionId::m_cameraId)->
                    Field("ViewportId", &InteractionId::m_viewportId);

                serializeContext->Class<MousePick>()->
                    Field("RayOrigin", &MousePick::m_rayOrigin)->
                    Field("RayDirection", &MousePick::m_rayDirection)->
                    Field("ScreenCoordinates", &MousePick::m_screenCoordinates);

                serializeContext->Class<MouseInteraction>()->
                    Field("MousePick", &MouseInteraction::m_mousePick)->
                    Field("MouseButtons", &MouseInteraction::m_mouseButtons)->
                    Field("InteractionId", &MouseInteraction::m_interactionId)->
                    Field("KeyboardModifiers", &MouseInteraction::m_keyboardModifiers);

                serializeContext->Class<ScreenPoint>()->
                    Field("X", &ScreenPoint::m_x)->
                    Field("Y", &ScreenPoint::m_y);

                MouseInteractionEvent::Reflect(*serializeContext);
            }
        }

        void MouseInteractionEvent::Reflect(AZ::SerializeContext& serializeContext)
        {
            serializeContext.Class<MouseInteractionEvent>()->
                Field("MouseInteraction", &MouseInteractionEvent::m_mouseInteraction)->
                Field("MouseEvent", &MouseInteractionEvent::m_mouseEvent)->
                Field("WheelDelta", &MouseInteractionEvent::m_wheelDelta);
        }
    }
}