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
#include "EditorPreferencesTreeWidgetItem.h"
#include "IPreferencesPage.h"
#include <AzToolsFramework/UI/PropertyEditor/InstanceDataHierarchy.h>
#include <AzToolsFramework/UI/PropertyEditor/ReflectedPropertyEditor.hxx>
#include <AzCore/Serialization/SerializeContext.h>

EditorPreferencesTreeWidgetItem::EditorPreferencesTreeWidgetItem(IPreferencesPage* page, const QPixmap& selectedImage, QPixmap& unselectedImage)
    : QTreeWidgetItem(EditorPreferencesPage)
    , m_preferencesPage(page)
    , m_selectedImage(selectedImage)
    , m_unselectedImage(unselectedImage)
    , m_entirePageMatchesFilter(true)
{
    setData(0, Qt::DisplayRole, m_preferencesPage->GetTitle());
    SetActivePage(false);

    AZ::SerializeContext* serializeContext = nullptr;
    EBUS_EVENT_RESULT(serializeContext, AZ::ComponentApplicationBus, GetSerializeContext);
    AZ_Assert(serializeContext, "Serialization context not available");

    // Grab all property names on the page so we can filter by property text by recursing through the hierarchy
    AzToolsFramework::InstanceDataHierarchy hierarchy;
    hierarchy.AddRootInstance(page);
    hierarchy.Build(serializeContext, AZ::SerializeContext::ENUM_ACCESS_FOR_READ);
    std::function<void(AzToolsFramework::InstanceDataNode&)> visitNode = [&](AzToolsFramework::InstanceDataNode& node)
    {
        const AZ::Edit::ElementData* data = node.GetElementEditMetadata();
        QString text;
        if (data)
        {
            text = data->m_name;
        }

        if (!text.isEmpty() && !m_propertyNames.contains(text))
        {
            m_propertyNames.append(text);
        }

        for (AzToolsFramework::InstanceDataNode& child : node.GetChildren())
        {
            visitNode(child);
        }
    };
    visitNode(hierarchy);
}


EditorPreferencesTreeWidgetItem::~EditorPreferencesTreeWidgetItem()
{
}


void EditorPreferencesTreeWidgetItem::SetActivePage(bool active)
{
    if (active)
    {
        setData(0, Qt::DecorationRole, m_selectedImage);
    }
    else
    {
        setData(0, Qt::DecorationRole, m_unselectedImage);
    }
}


IPreferencesPage* EditorPreferencesTreeWidgetItem::GetPreferencesPage() const
{
    return m_preferencesPage;
}

void EditorPreferencesTreeWidgetItem::Filter(const QString& filter)
{
    // Everything on a page matches the filter if its or any of its parents' text matches
    m_entirePageMatchesFilter = false;
    QTreeWidgetItem* item = this;
    while (item && !m_entirePageMatchesFilter)
    {
        m_entirePageMatchesFilter = item->text(0).contains(filter, Qt::CaseInsensitive);
        item = item->parent();
    }

    // Otherwise, just check the contents to see if there's a match
    bool filtered = !m_entirePageMatchesFilter;
    for (auto it = m_propertyNames.begin(), end = m_propertyNames.end(); it != end && filtered; ++it)
    {
        filtered = !(*it).contains(filter, Qt::CaseInsensitive);
    }

    setHidden(filtered);
}

void EditorPreferencesTreeWidgetItem::UpdateEditorFilter(AzToolsFramework::ReflectedPropertyEditor* editor, const QString& filter)
{
    QString filterText = m_entirePageMatchesFilter ? QString() : filter;
    editor->InvalidateAll(filterText.toUtf8().constData());
    editor->ExpandAll();
}
