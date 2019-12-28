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
#include "AcquireByTag.h"
#include <AzCore/EBus/EBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Math/Transform.h>
#include <StartingPointCamera/StartingPointCameraConstants.h>

namespace Camera
{
    namespace ClassConverters
    {
        static bool DeprecateCameraTargetComponentAcquirer(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement);
    } // namespace ClassConverters

    void AcquireByTag::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            // Deprecating the CameraTargetComponent.  This acquire behavior makes it obsolete
            serializeContext->ClassDeprecate("CameraTargetComponentAcquirer", "{CF1C04E4-1195-42DD-AF0B-C9F94E80B35D}", &ClassConverters::DeprecateCameraTargetComponentAcquirer);

            serializeContext->Class<AcquireByTag>()
                ->Version(1)
                ->Field("Target Tag", &AcquireByTag::m_targetTag)
                ->Field("Use Target Rotation", &AcquireByTag::m_shouldUseTargetRotation)
                ->Field("Use Target Position", &AcquireByTag::m_shouldUseTargetPosition);

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<AcquireByTag>("AcquireByTag", "Acquires a target by tag")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AcquireByTag::m_targetTag, "Target tag", "The tag on an entity you want to target")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AcquireByTag::m_shouldUseTargetRotation, "Use target rotation", "Set to false to not have the camera orient itself with the target")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &AcquireByTag::m_shouldUseTargetPosition, "Use target position", "Set to false to not have the camera position itself with the target");
            }
        }
    }

    bool AcquireByTag::AcquireTarget(AZ::Transform& outTransformInformation)
    {
        if (m_targets.size())
        {
            AZ::Transform targetsTransform = AZ::Transform::Identity();
            AZ::TransformBus::EventResult(targetsTransform, m_targets[0], &AZ::TransformInterface::GetWorldTM);
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

    void AcquireByTag::Activate(AZ::EntityId)
    {
        LmbrCentral::TagGlobalNotificationBus::Handler::BusConnect(LmbrCentral::Tag(m_targetTag.c_str()));
    }

    void AcquireByTag::Deactivate()
    {
        LmbrCentral::TagGlobalNotificationBus::Handler::BusDisconnect();
    }

    void AcquireByTag::OnEntityTagAdded(const AZ::EntityId& entityId)
    {
        AZ_Error("AcquireByTag", entityId.IsValid(), "A tag was added to an invalid entity, this should never happen");
        m_targets.push_back(entityId);
    }

    void AcquireByTag::OnEntityTagRemoved(const AZ::EntityId& entityId)
    {
        auto&& iterator = AZStd::find(m_targets.begin(), m_targets.end(), entityId);
        AZ_Error("AcquireByTag", iterator != m_targets.end(), "A tag was removed without being added, this should never happen");
        m_targets.erase(iterator);
    }

    namespace ClassConverters
    {
        static bool DeprecateCameraTargetComponentAcquirer(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement)
        {
            AZStd::string tag;
            classElement.GetChildData(AZ::Crc32("Tag of Specific Target"), tag);

            bool useTargetRotation = true;
            classElement.GetChildData(AZ::Crc32("Use Target Rotation"), useTargetRotation);

            bool useTargetPosition = true;
            classElement.GetChildData(AZ::Crc32("Use Target Position"), useTargetPosition);

            classElement.Convert(context, AZ::AzTypeInfo<AcquireByTag>::Uuid());
            classElement.AddElementWithData(context, "Target Tag", tag);
            classElement.AddElementWithData(context, "Use Target Rotation", useTargetRotation);
            classElement.AddElementWithData(context, "Use Target Position", useTargetPosition);

            return true;
        }
    } // namespace ClassConverters
} // namespace Camera