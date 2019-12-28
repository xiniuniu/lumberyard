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
#include <AzCore/Component/Entity.h>
#include <AzCore/std/containers/vector.h>

#include <AzToolsFramework/Application/ToolsApplication.h>
#include <AzToolsFramework/Entity/EditorEntitySearchComponent.h>

namespace AzToolsFramework
{
    //! Class storing the match conditions for an editor entity search
    class EntitySearchFilter
    {
    public:
        AZ_TYPE_INFO(EntitySearchFilter, "{48A94382-72BE-457B-BB43-0E6C245824D2}")

        //! List of names (matches if any match); can contain wildcards in the name.
        AZStd::vector<AZStd::string> m_names;

        //! Determines if the name matching should be case sensitive.
        bool m_namesCaseSensitive = false;

        //! List of component type ids (matches if any match).
        AZStd::vector<AZ::Uuid> m_componentTypeIds;

        //! Determines if the filter should match all component type ids (AND).
        bool m_mustMatchAllComponents = false;

        //! Specifies the entities that act as roots of the search.
        AZStd::vector<AZ::EntityId> m_roots;

        //! Determines if the names are relative to the root or should be searched in children too.
        bool m_namesAreRootBased = false;
    };

    //! Provides an API to search editor entity that match some conditions in the currently open level.
    class EditorEntitySearchRequests : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;

        //! Iterates through all entities in the current level, and returns a list of the ones that match the conditions.
        virtual EntityIdList SearchEntities(const EntitySearchFilter& conditions) = 0;

        //! Returns a list of all editor entities at the root level in the current level.
        virtual EntityIdList GetRootEditorEntities() = 0;

    protected:
        ~EditorEntitySearchRequests() = default;
    };
    using EditorEntitySearchBus = AZ::EBus<EditorEntitySearchRequests>;

} // namespace AzToolsFramework
