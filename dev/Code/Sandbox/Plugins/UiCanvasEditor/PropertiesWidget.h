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

#include "EditorCommon.h"

#include <QWidget>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/UI/PropertyEditor/ComponentEditor.hxx>

class PropertiesWidget
    : public QWidget
    , public AzToolsFramework::IPropertyEditorNotify
    , public AzToolsFramework::ToolsApplicationEvents::Bus::Handler
{
    Q_OBJECT

public:

    PropertiesWidget(EditorWindow* editorWindow,
        QWidget* parent = nullptr);
    virtual ~PropertiesWidget();

    QSize sizeHint() const override { return QSize(400, -1); }

    //IPropertyEditorNotify Interface
    void BeforePropertyModified(AzToolsFramework::InstanceDataNode* pNode) override;
    void AfterPropertyModified(AzToolsFramework::InstanceDataNode* pNode) override;
    void SetPropertyEditingActive(AzToolsFramework::InstanceDataNode* pNode) override;
    void SetPropertyEditingComplete(AzToolsFramework::InstanceDataNode* pNode) override;
    void SealUndoStack() override;
    void RequestPropertyContextMenu(AzToolsFramework::InstanceDataNode* node, const QPoint& globalPos) override;

    //ToolsApplicationEvents Interface
    void InvalidatePropertyDisplay(AzToolsFramework::PropertyModificationRefreshLevel level) override;

    void TriggerRefresh(AzToolsFramework::PropertyModificationRefreshLevel refreshLevel = AzToolsFramework::PropertyModificationRefreshLevel::Refresh_EntireTree, const AZ::Uuid* componentType = nullptr);
    void TriggerImmediateRefresh(AzToolsFramework::PropertyModificationRefreshLevel refreshLevel = AzToolsFramework::PropertyModificationRefreshLevel::Refresh_EntireTree, const AZ::Uuid* componentType = nullptr);

    // Notify the properties pane when a selected entity has been recreated
    void SelectedEntityPointersChanged();

    void SetSelectedEntityDisplayNameWidget(QLineEdit* selectedEntityDisplayNameWidget);
    void SetEditorOnlyCheckbox(QCheckBox* editorOnlyCheckbox);

    float GetScrollValue();
    void SetScrollValue(float scrollValue);

    AZ::Entity::ComponentArrayType GetSelectedComponents();

public slots:

    void UserSelectionChanged(HierarchyItemRawPtrList* items);

private slots:

void Refresh(AzToolsFramework::PropertyModificationRefreshLevel refreshLevel = AzToolsFramework::PropertyModificationRefreshLevel::Refresh_EntireTree, const AZ::Uuid* componentType = nullptr);

private:

    EditorWindow* m_editorWindow;
    QTimer m_refreshTimer;
    SerializeHelpers::SerializedEntryList m_preValueChanges;

    PropertiesContainer* m_propertiesContainer;

    AZStd::string m_canvasUndoXml;

    AZ::Uuid m_componentTypeToRefresh;
    AzToolsFramework::PropertyModificationRefreshLevel m_refreshLevel;
};
