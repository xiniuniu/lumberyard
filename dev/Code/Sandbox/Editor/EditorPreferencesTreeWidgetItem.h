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

#include <QTreeWidgetItem>
#include <QPixmap>

struct IPreferencesPage;

namespace AzToolsFramework
{
    class ReflectedPropertyEditor;
}

class EditorPreferencesTreeWidgetItem
    : public QTreeWidgetItem
{
public:

    enum CustomType
    {
        EditorPreferencesPage = QTreeWidgetItem::UserType
    };

    EditorPreferencesTreeWidgetItem(IPreferencesPage* page, const QPixmap& selectedImage, QPixmap& unselectedImage);
    ~EditorPreferencesTreeWidgetItem();

    void SetActivePage(bool active);
    void Filter(const QString& filter);
    void UpdateEditorFilter(AzToolsFramework::ReflectedPropertyEditor* editor, const QString& filter);

    IPreferencesPage* GetPreferencesPage() const;

private:
    IPreferencesPage* m_preferencesPage;
    QPixmap m_selectedImage;
    QPixmap m_unselectedImage;
    QStringList m_propertyNames;
    bool m_entirePageMatchesFilter;
};

