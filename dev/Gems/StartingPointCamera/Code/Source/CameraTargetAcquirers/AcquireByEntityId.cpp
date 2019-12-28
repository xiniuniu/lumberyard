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
#include "StartingPointCamera_precompiled.h"
#include "AcquireByEntityId.h"
#include <AzCore/EBus/EBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Math/Transform.h>
#include <StartingPointCamera/StartingPointCameraConstants.h>

namespace Camera
{
    void AcquireByEntityId::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            // Deprecating the CameraTargetComponent.  This acquire behavior makes it obsolete
            serializeContext->ClassDeprecate("CameraTargetComponent", "{0D6A6574-4B79-4907-8529-EB61F343D957}");

            serializeContext->Class<AcquireByEntityId>()
                ->Version(1)
                ->Field("Entity Target", &AcquireByEntityId::m_target)
                ->Field("Use Target Rotation", &AcquireByEntityId::m_shouldUseTargetRotation)
                ->Field("Use Target Position", &AcquireByEntityId::m_shouldUseTargetPosition);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<AcquireByEntityId>("AcquireByEntityId", "Acquires a target by entity ref")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AcquireByEntityId::m_target, "Entity target", "Specify an entity to target")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AcquireByEntityId::m_shouldUseTargetRotation, "Use target rotation", "Set to false to not have the camera orient itself with the target")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AcquireByEntityId::m_shouldUseTargetPosition, "Use target position", "Set to false to not have the camera position itself with the target");
            }
        }
    }

    bool AcquireByEntityId::AcquireTarget(AZ::Transform& outTransformInformation)
    {
        if (m_target.IsValid())
        {
            AZ::Transform targetsTransform = AZ::Transform::Identity();
            AZ::TransformBus::EventResult(targetsTransform, m_target, &AZ::TransformInterface::GetWorldTM);
            if (m_shouldUseTargetPosition)
            {
                outTransformInformation.SetPosition(targetsTransform.GetPosition());
            }
            if (m_shouldUseTargetRotation)
            {
                outTransformInformation.SetColumns(targetsTransform.GetColumn(0), targetsTransform.GetColumn(1), targetsTransform.GetColumn(2), outTransformInformation.GetColumn(3));
            }
            return true;
        }
        return false;
    }
} // namespace Camera