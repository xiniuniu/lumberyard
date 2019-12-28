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

#ifndef PROPERTY_DOUBLESPINBOX_CTRL
#define PROPERTY_DOUBLESPINBOX_CTRL

#include <AzCore/base.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <QtWidgets/QWidget>
#include "PropertyEditorAPI.h"

#pragma once

namespace AzToolsFramework
{
    class DHQDoubleSpinbox;

    class PropertyDoubleSpinCtrl
        : public QWidget
    {
        Q_OBJECT
    public:
        AZ_CLASS_ALLOCATOR(PropertyDoubleSpinCtrl, AZ::SystemAllocator, 0);

        PropertyDoubleSpinCtrl(QWidget* pParent = NULL);
        virtual ~PropertyDoubleSpinCtrl();

        double value() const;
        double minimum() const;
        double maximum() const;
        double step() const;
        double multiplier() const;

        QWidget* GetFirstInTabOrder();
        QWidget* GetLastInTabOrder();
        void UpdateTabOrder();

    signals:
        void valueChanged(double newValue);
        void editingFinished();

    public slots:
        void setValue(double val);
        void setMinimum(double val);
        void setMaximum(double val);
        void setStep(double val);
        void setMultiplier(double val);
        void setPrefix(QString val);
        void setSuffix(QString val);
        void setDecimals(int precision);
        void setDisplayDecimals(int displayDecimals);

    protected slots:
        void onChildSpinboxValueChange(double value);

    private:
        DHQDoubleSpinbox* m_pSpinBox;
        double m_multiplier;

    protected:
        void focusInEvent(QFocusEvent* e) override;
        void focusOutEvent(QFocusEvent* e) override;
    };

    template <class ValueType>
    class DoubleSpinBoxHandlerCommon
        : public PropertyHandler<ValueType, PropertyDoubleSpinCtrl>
    {
    public:
        AZ::u32 GetHandlerName(void) const override  { return AZ::Edit::UIHandlers::SpinBox; }
        bool IsDefaultHandler() const override { return true; }
        QWidget* GetFirstInTabOrder(PropertyDoubleSpinCtrl* widget) override { return widget->GetFirstInTabOrder(); }
        QWidget* GetLastInTabOrder(PropertyDoubleSpinCtrl* widget) override { return widget->GetLastInTabOrder(); }
        void UpdateWidgetInternalTabbing(PropertyDoubleSpinCtrl* widget) override { widget->UpdateTabOrder(); }
    };

    class doublePropertySpinboxHandler
        : QObject
        , public DoubleSpinBoxHandlerCommon<double>
    {
        // this is a Qt Object purely so it can connect to slots with context.  This is the only reason its in this header.
        Q_OBJECT
    public:
        AZ_CLASS_ALLOCATOR(doublePropertySpinboxHandler, AZ::SystemAllocator, 0);

        // common to all double spinners
        static void ConsumeAttributeCommon(PropertyDoubleSpinCtrl* GUI, AZ::u32 attrib, PropertyAttributeReader* attrValue, const char* debugName);

        QWidget* CreateGUI(QWidget* pParent) override;
        void ConsumeAttribute(PropertyDoubleSpinCtrl* GUI, AZ::u32 attrib, PropertyAttributeReader* attrValue, const char* debugName) override;
        void WriteGUIValuesIntoProperty(size_t index, PropertyDoubleSpinCtrl* GUI, property_t& instance, InstanceDataNode* node) override;
        bool ReadValuesIntoGUI(size_t index, PropertyDoubleSpinCtrl* GUI, const property_t& instance, InstanceDataNode* node)  override;
        bool ModifyTooltip(QWidget* widget, QString& toolTipString) override;
    };

    class floatPropertySpinboxHandler
        : QObject
        , public DoubleSpinBoxHandlerCommon<float>
    {
        // this is a Qt Object purely so it can connect to slots with context.  This is the only reason its in this header.
        Q_OBJECT
    public:
        AZ_CLASS_ALLOCATOR(floatPropertySpinboxHandler, AZ::SystemAllocator, 0);

        QWidget* CreateGUI(QWidget* pParent) override;
        void ConsumeAttribute(PropertyDoubleSpinCtrl* GUI, AZ::u32 attrib, PropertyAttributeReader* attrValue, const char* debugName) override;
        void WriteGUIValuesIntoProperty(size_t index, PropertyDoubleSpinCtrl* GUI, property_t& instance, InstanceDataNode* node) override;
        bool ReadValuesIntoGUI(size_t index, PropertyDoubleSpinCtrl* GUI, const property_t& instance, InstanceDataNode* node)  override;
        bool ModifyTooltip(QWidget* widget, QString& toolTipString) override;
    };


    void RegisterDoubleSpinBoxHandlers();
};

#endif
