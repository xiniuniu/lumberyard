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

#include <AzCore/base.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <QtWidgets/QWidget>
#include <AzToolsFramework/UI/PropertyEditor/PropertyEditorAPI.h>

#include <AzToolsFramework/UI/PropertyEditor/PropertyAssetCtrl.hxx>
#include <AzToolsFramework/AssetBrowser/AssetSelectionModel.h>

class PropertyDirectoryCtrl
    : public QWidget
{
    Q_OBJECT

public:

    AZ_CLASS_ALLOCATOR(PropertyDirectoryCtrl, AZ::SystemAllocator, 0);

    PropertyDirectoryCtrl(QWidget* parent = nullptr);

    void dragEnterEvent(QDragEnterEvent* ev) override;
    void dragLeaveEvent(QDragLeaveEvent* ev) override;
    void dropEvent(QDropEvent* ev) override;

    AzToolsFramework::PropertyAssetCtrl* GetPropertyAssetCtrl();

private:

    AzToolsFramework::PropertyAssetCtrl* m_propertyAssetCtrl;
};

//-------------------------------------------------------------------------------

class PropertyAssetDirectorySelectionCtrl
    : public AzToolsFramework::PropertyAssetCtrl
{
public:
    PropertyAssetDirectorySelectionCtrl(QWidget *pParent = NULL)
        : PropertyAssetCtrl(pParent) {}
    AzToolsFramework::AssetBrowser::AssetSelectionModel GetAssetSelectionModel() override;
    void SetFolderSelection(const AZStd::string& folderPath) override;
    const AZStd::string GetFolderSelection() const override { return m_folderPath; }
    void ClearAssetInternal() override;

private:
    AZStd::string m_folderPath;
};

//-------------------------------------------------------------------------------

class PropertyHandlerDirectory
    : public AzToolsFramework::PropertyHandler<AZStd::string, PropertyDirectoryCtrl>
{
public:
    AZ_CLASS_ALLOCATOR(PropertyHandlerDirectory, AZ::SystemAllocator, 0);
    
    AZ::u32 GetHandlerName(void) const override  { return AZ_CRC("Directory", 0x467844da); }

    QWidget* CreateGUI(QWidget* pParent) override;
    void ConsumeAttribute(PropertyDirectoryCtrl* GUI, AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName) override;
    void WriteGUIValuesIntoProperty(size_t index, PropertyDirectoryCtrl* GUI, property_t& instance, AzToolsFramework::InstanceDataNode* node) override;
    bool ReadValuesIntoGUI(size_t index, PropertyDirectoryCtrl* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* node)  override;

    static void Register();
};
