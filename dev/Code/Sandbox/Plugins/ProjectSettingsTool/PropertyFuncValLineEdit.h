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

#include "FunctorValidator.h"

#include <AzToolsFramework/UI/PropertyEditor/PropertyStringLineEditCtrl.hxx>

namespace ProjectSettingsTool
{
    // Forward Declare
    class ValidationHandler;

    class PropertyFuncValLineEditCtrl
        : public AzToolsFramework::PropertyStringLineEditCtrl
    {
        Q_OBJECT

    public:
        PropertyFuncValLineEditCtrl(QWidget* pParent = nullptr);

        virtual QString GetValue() const;
        // Sets value programmtically and triggers validation
        virtual void SetValue(const QString& value);
        // Sets value as if user set it
        void SetValueUser(const QString& value);
        // Returns pointer to the validator used
        FunctorValidator* GetValidator();
        // Sets the validator for the lineedit
        void SetValidator(FunctorValidator* validator);
        // Sets the validator for the linedit
        void SetValidator(FunctorValidator::FunctorType validator);
        // Returns false if invalid and returns shows error as tooltip
        bool ValidateAndShowErrors();
        // Forces the values to up validated and style updated
        void ForceValidate();

        virtual void ConsumeAttribute(AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName);

    signals:
        void ValueChangedByUser();

    protected:
        // Keeps track of the validator so no const_casts must be done
        FunctorValidator* m_validator;
    };

    class PropertyFuncValLineEditHandler
        : public AzToolsFramework::PropertyHandler <AZStd::string, PropertyFuncValLineEditCtrl>
    {
        AZ_CLASS_ALLOCATOR(PropertyFuncValLineEditHandler, AZ::SystemAllocator, 0);

    public:
        PropertyFuncValLineEditHandler(ValidationHandler* valHdlr);

        AZ::u32 GetHandlerName(void) const override;
        // Need to unregister ourselves
        bool AutoDelete() const override { return false; }

        QWidget* CreateGUI(QWidget* pParent) override;
        void ConsumeAttribute(PropertyFuncValLineEditCtrl* GUI, AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName) override;
        void WriteGUIValuesIntoProperty(size_t index, PropertyFuncValLineEditCtrl* GUI, property_t& instance, AzToolsFramework::InstanceDataNode* node) override;
        bool ReadValuesIntoGUI(size_t index, PropertyFuncValLineEditCtrl* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* node)  override;
        static PropertyFuncValLineEditHandler* Register(ValidationHandler* valHdlr);

    private:
        ValidationHandler* m_validationHandler;
    };
} // namespace ProjectSettingsTool
