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
#include <AzCore/Component/ComponentBus.h>
#include <AzFramework/Entity/BehaviorEntity.h>
#include <AzToolsFramework/PropertyTreeEditor/PropertyTreeEditor.h>

namespace AzToolsFramework
{
    //! Exposes the Editor Component CRUD API; it is exposed to Behavior Context for Editor Scripting.
    class EditorComponentAPIRequests
        : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        //! Finds the component ids from their type names
        virtual AZStd::vector<AZ::Uuid> FindComponentTypeIds(const AZStd::vector<AZStd::string>& componentTypeNames) = 0;

        //! Finds the component names from their type ids
        virtual AZStd::vector<AZStd::string> FindComponentTypeNames(const AZ::ComponentTypeList& componentTypeIds) = 0;

        //! Returns the full list of names for all components that can be created with the EditorComponent API.
        virtual AZStd::vector<AZStd::string> BuildComponentTypeNameList() = 0;

        using AddComponentsOutcome = AZ::Outcome<AZStd::vector<AZ::EntityComponentIdPair>, AZStd::string>;

        //! Add Components of the given types to an Entity.
        // Returns an Outcome object - it contains the AZ::EntityComponentIdPairs in case of Success, or an error message in case or Failure.
        virtual AddComponentsOutcome AddComponentsOfType(AZ::EntityId entityId, const AZ::ComponentTypeList& componentTypeIds) = 0;
        
        //! Returns true if a Component of type provided can be found on Entity, false otherwise.
        virtual bool HasComponentOfType(AZ::EntityId entityId, AZ::Uuid componentTypeId) = 0;

        //! Count Components of type provided on the Entity.
        virtual size_t CountComponentsOfType(AZ::EntityId entityId, AZ::Uuid componentTypeId) = 0;

        using GetComponentOutcome = AZ::Outcome<AZ::EntityComponentIdPair, AZStd::string>;

        //! Get Component of type from Entity.
        // Only returns first component of type if found (early out).
        // Returns an Outcome object - it contains the AZ::EntityComponentIdPair in case of Success, or an error message in case or Failure.
        virtual GetComponentOutcome GetComponentOfType(AZ::EntityId entityId, AZ::Uuid componentTypeId) = 0;

        using GetComponentsOutcome = AZ::Outcome<AZStd::vector<AZ::EntityComponentIdPair>, AZStd::string>;

        //! Get all Components of type from Entity.
        // Returns vector of ComponentIds, or an empty vector if components could not be found.
        virtual GetComponentsOutcome GetComponentsOfType(AZ::EntityId entityId, AZ::Uuid componentTypeId) = 0;

        //! Verify if component instance referenced by AZ::EntityComponentIdPair is valid.
        virtual bool IsValid(AZ::EntityComponentIdPair componentInstance) = 0;

        //! Enable Components on Entity by AZ::EntityComponentIdPair. Returns true if the operation was successful, false otherwise.
        virtual bool EnableComponents(const AZStd::vector<AZ::EntityComponentIdPair>& componentInstances) = 0;

        //! Returns true if the Component is active.
        virtual bool IsComponentEnabled(const AZ::EntityComponentIdPair& componentInstance) = 0;

        //! Disable Components on Entity by AZ::EntityComponentIdPair. Returns true if the operation was successful, false otherwise.
        virtual bool DisableComponents(const AZStd::vector<AZ::EntityComponentIdPair>& componentInstances) = 0;

        //! Remove Components from Entity by AZ::EntityComponentIdPair. Returns true if the operation was successful, false otherwise.
        virtual bool RemoveComponents(const AZStd::vector<AZ::EntityComponentIdPair>& componentInstances) = 0;

        using PropertyTreeOutcome = AZ::Outcome<PropertyTreeEditor, AZStd::string>;

        //! Get the PropertyTreeEditor for the Component Instance provided.
        virtual PropertyTreeOutcome BuildComponentPropertyTreeEditor(const AZ::EntityComponentIdPair& componentInstance) = 0;

        using PropertyOutcome = AZ::Outcome<AZStd::any, AZStd::string>;

        //! Get Value of Property on Component
        virtual PropertyOutcome GetComponentProperty(const AZ::EntityComponentIdPair& componentInstance, const AZStd::string_view propertyPath) = 0;

        //! Set Value of Property on Component
        virtual PropertyOutcome SetComponentProperty(const AZ::EntityComponentIdPair& componentInstance, const AZStd::string_view propertyPath, const AZStd::any& value) = 0;

        //! Get a full list of Component Properties for the Component Entity provided
        virtual const AZStd::vector<AZStd::string> BuildComponentPropertyList(const AZ::EntityComponentIdPair& componentInstance) = 0;

    };
    using EditorComponentAPIBus = AZ::EBus<EditorComponentAPIRequests>;

}
