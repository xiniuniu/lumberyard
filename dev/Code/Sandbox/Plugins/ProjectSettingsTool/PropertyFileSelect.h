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

#include "PropertyFuncValLineEdit.h"

// Forward declare
class QPushButton;

namespace ProjectSettingsTool
{
    // Forward declare
    class ValidationHandler;

    class PropertyFileSelectCtrl
        : public PropertyFuncValLineEditCtrl
    {
        Q_OBJECT

    public:
        typedef QString(* FileSelectFuncType)(const QString&);

        PropertyFileSelectCtrl(QWidget* pParent = nullptr);

        virtual void ConsumeAttribute(AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName) override;

    protected:
        void SelectFile();

        QPushButton* m_selectButton;
        FileSelectFuncType m_selectFunctor;
    };

    class PropertyFileSelectHandler
        : public AzToolsFramework::PropertyHandler<AZStd::string, PropertyFileSelectCtrl>
    {
        AZ_CLASS_ALLOCATOR(PropertyFileSelectHandler, AZ::SystemAllocator, 0);

    public:
        PropertyFileSelectHandler(ValidationHandler* valHdlr);

        AZ::u32 GetHandlerName(void) const override;
        // Need to unregister ourselves
        bool AutoDelete() const override { return false; }

        QWidget* CreateGUI(QWidget* pParent) override;
        void ConsumeAttribute(PropertyFileSelectCtrl* GUI, AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName) override;
        void WriteGUIValuesIntoProperty(size_t index, PropertyFileSelectCtrl* GUI, property_t& instance, AzToolsFramework::InstanceDataNode* node) override;
        bool ReadValuesIntoGUI(size_t index, PropertyFileSelectCtrl* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* node)  override;
        static PropertyFileSelectHandler* Register(ValidationHandler* valHdlr);

    private:
        ValidationHandler* m_validationHandler;
    };
} // namespace ProjectSettingsTool
