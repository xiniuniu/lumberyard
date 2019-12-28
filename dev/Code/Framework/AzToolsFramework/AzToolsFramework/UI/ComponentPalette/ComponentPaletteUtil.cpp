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

#include "StdAfx.h"
#include "ComponentPaletteUtil.hxx"

#include <AzCore/Debug/Profiler.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
AZ_PUSH_DISABLE_WARNING(4251, "-Wunknown-warning-option") // 4251: 'QLayoutItem::align': class 'QFlags<Qt::AlignmentFlag>' needs to have dll-interface to be used by clients of class 'QLayoutItem'
#include <AzToolsFramework/UI/SearchWidget/SearchCriteriaWidget.hxx>
AZ_POP_DISABLE_WARNING

namespace AzToolsFramework
{
    namespace ComponentPaletteUtil
    {
        bool OffersRequiredServices(const AZ::SerializeContext::ClassData* componentClass, const AZStd::vector<AZ::ComponentServiceType>& serviceFilter)
        {
            AZ_Assert(componentClass, "Component class must not be null");

            if (!componentClass)
            {
                return false;
            }

            AZ::ComponentDescriptor* componentDescriptor = nullptr;
            EBUS_EVENT_ID_RESULT(componentDescriptor, componentClass->m_typeId, AZ::ComponentDescriptorBus, GetDescriptor);
            if (!componentDescriptor)
            {
                return false;
            }

            // If no services are provided, this function returns true
            if (serviceFilter.empty())
            {
                return true;
            }

            AZ::ComponentDescriptor::DependencyArrayType providedServices;
            componentDescriptor->GetProvidedServices(providedServices, nullptr);

            //reject this component if it does not offer any of the required services
            if (AZStd::find_first_of(
                providedServices.begin(),
                providedServices.end(),
                serviceFilter.begin(),
                serviceFilter.end()) == providedServices.end())
            {
                return false;
            }

            return true;
        }

        bool IsAddableByUser(const AZ::SerializeContext::ClassData* componentClass)
        {
            AZ_Assert(componentClass, "component class must not be null");

            if (!componentClass)
            {
                return false;
            }

            auto editorDataElement = componentClass->m_editData->FindElementData(AZ::Edit::ClassElements::EditorData);

            if (!editorDataElement)
            {
                return false;
            }

            auto attribute = editorDataElement->FindAttribute(AZ::Edit::Attributes::AddableByUser);
            if (attribute)
            {
                auto data = azdynamic_cast<AZ::Edit::AttributeData<bool>*>(attribute);
                if (data)
                {
                    if (!data->Get(nullptr))
                    {
                        return false;
                    }
                }
            }

            return true;
        }

        void BuildComponentTables(
            AZ::SerializeContext* serializeContext,
            const AzToolsFramework::ComponentFilter& componentFilter,
            const AZStd::vector<AZ::ComponentServiceType>& serviceFilter,
            ComponentDataTable &componentDataTable,
            ComponentIconTable &componentIconTable)
        {
            AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);
            serializeContext->EnumerateDerived<AZ::Component>(
                [&](const AZ::SerializeContext::ClassData* componentClass, const AZ::Uuid& knownType) -> bool
                {
                    AZ_UNUSED(knownType);

                    if (componentFilter(*componentClass) && componentClass->m_editData)
                    {
                        QString categoryName = QString::fromUtf8("Miscellaneous");
                        QString componentName = QString::fromUtf8(componentClass->m_editData->m_name);

                        // If none of the required services are offered by this component, or the component
                        // can not be added by the user, skip to the next component
                        if (!OffersRequiredServices(componentClass, serviceFilter) || !IsAddableByUser(componentClass))
                        {
                            return true;
                        }

                        if (auto editorDataElement = componentClass->m_editData->FindElementData(AZ::Edit::ClassElements::EditorData))
                        {
                            if (auto attribute = editorDataElement->FindAttribute(AZ::Edit::Attributes::Category))
                            {
                                if (auto data = azdynamic_cast<AZ::Edit::AttributeData<const char*>*>(attribute))
                                {
                                    categoryName = QString::fromUtf8(data->Get(nullptr));
                                }
                            }

                            AZStd::string componentIconPath;
                            EBUS_EVENT_RESULT(componentIconPath, AzToolsFramework::EditorRequests::Bus, GetComponentEditorIcon, componentClass->m_typeId, nullptr);
                            componentIconTable[componentClass] = QString::fromUtf8(componentIconPath.c_str());
                        }

                        componentDataTable[categoryName][componentName] = componentClass;
                    }

                    return true;
                });
        }

        bool ContainsEditableComponents(
            AZ::SerializeContext* serializeContext,
            const AzToolsFramework::ComponentFilter& componentFilter,
            const AZStd::vector<AZ::ComponentServiceType>& serviceFilter)
        {
            AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);
            
            bool containsEditable = false;

            serializeContext->EnumerateDerived<AZ::Component>(
                [&](const AZ::SerializeContext::ClassData* componentClass, const AZ::Uuid& knownType) -> bool
                {
                    AZ_UNUSED(knownType);

                    if (componentFilter(*componentClass) && componentClass->m_editData)
                    {
                        // If none of the required services are offered by this component, or the component
                        // can not be added by the user, skip to the next component
                        if (!OffersRequiredServices(componentClass, serviceFilter) || !IsAddableByUser(componentClass))
                        {
                            return true;
                        }

                        containsEditable = true;
                    }

                    // We can stop enumerating if we've found an editable component
                    return !containsEditable;
                });

            return containsEditable;
        }


        QRegExp BuildFilterRegExp(QStringList& criteriaList, AzToolsFramework::FilterOperatorType filterOperator)
        {
            // Go through the list of items and show/hide as needed due to filter.
            QString filter;
            for (const auto& criteria : criteriaList)
            {
                QString tag, text;
                AzToolsFramework::SearchCriteriaButton::SplitTagAndText(criteria, tag, text);

                if (filterOperator == AzToolsFramework::FilterOperatorType::Or)
                {
                    if (filter.isEmpty())
                    {
                        filter = text;
                    }
                    else
                    {
                        filter += "|" + text;
                    }
                }
                else if (filterOperator == AzToolsFramework::FilterOperatorType::And)
                {
                    filter += "(?=.*" + text + ")";
                }
            }

            return QRegExp(filter, Qt::CaseInsensitive, QRegExp::RegExp);
        }
    }
}

#include <UI/ComponentPalette/ComponentPaletteUtil.moc>
