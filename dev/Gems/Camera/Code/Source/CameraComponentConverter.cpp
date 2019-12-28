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
#include "Camera_precompiled.h"
#include <AzCore/Serialization/SerializeContext.h>

#if defined(CAMERA_EDITOR)
#include "EditorCameraComponent.h"
#else
#include "CameraComponent.h"
#endif

namespace Camera
{
    namespace ClassConverters
    {

        bool DeprecateCameraComponentWithoutEditor(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement)
        {
            // Capture the old values
            float fov = s_defaultFoV;
            classElement.GetChildData(AZ::Crc32("Field of View"), fov);

            float nearDistance = s_defaultNearPlaneDistance;
            classElement.GetChildData(AZ::Crc32("Near Clip Plane Distance"), nearDistance);

            float farDistance = s_defaultFarClipPlaneDistance;
            classElement.GetChildData(AZ::Crc32("Far Clip Plane Distance"), farDistance);

            bool shouldSpecifyFrustum = false;
            classElement.GetChildData(AZ::Crc32("SpecifyDimensions"), shouldSpecifyFrustum);

            float frustumWidth = s_defaultFrustumDimension;
            classElement.GetChildData(AZ::Crc32("FrustumWidth"), frustumWidth);

            float frustumHeight = s_defaultFrustumDimension;
            classElement.GetChildData(AZ::Crc32("FrustumHeight"), frustumHeight);

            // convert to the new class
#if defined(CAMERA_EDITOR)
            if (classElement.GetName() == AZ::Crc32("m_template"))
            {
                classElement.Convert<EditorCameraComponent>(context);
            }
            else
#else
            classElement.Convert<CameraComponent>(context);
#endif // CAMERA_EDITOR

            // add the new values
            classElement.AddElementWithData(context, "Field of View", fov);
            classElement.AddElementWithData(context, "Near Clip Plane Distance", nearDistance);
            classElement.AddElementWithData(context, "Far Clip Plane Distance", farDistance);
            classElement.AddElementWithData(context, "SpecifyDimensions", shouldSpecifyFrustum);
            classElement.AddElementWithData(context, "FrustumWidth", frustumWidth);
            classElement.AddElementWithData(context, "FrustumHeight", frustumHeight);

            return true;
        }
    }
} // Camera