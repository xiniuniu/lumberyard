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
#include "stdafx.h"
#include "EditorCommon.h"

#include "PropertyHandlerPivot.h"

#include <QtCore>
#include <QtGui>
#include <QtWidgets>
#include <QtWidgets/QWidget>

#include "PivotPresets.h"
#include "PivotPresetsWidget.h"

PropertyPivotCtrl::PropertyPivotCtrl(QWidget* parent)
    : QWidget(parent)
    , m_common(2, 1)
    , m_propertyVectorCtrl(m_common.ConstructGUI(this))
    , m_pivotPresetsWidget(nullptr)
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Add Preset buttons.
    {
        AzToolsFramework::VectorElement** elements = m_propertyVectorCtrl->getElements();
        AZ::Vector2 controlValue(elements[0]->GetValue(), elements[1]->GetValue());

        m_pivotPresetsWidget = new PivotPresetsWidget(PivotPresets::PivotToPresetIndex(controlValue),
                [this](int presetIndex)
                {
                    AZ::Vector2 presetValues = PivotPresets::PresetIndexToPivot(presetIndex);
                    m_propertyVectorCtrl->setValuebyIndex(presetValues.GetX(), 0);
                    m_propertyVectorCtrl->setValuebyIndex(presetValues.GetY(), 1);

                    EBUS_EVENT(AzToolsFramework::PropertyEditorGUIMessages::Bus, RequestWrite, this);
                },
                this);

        layout->addWidget(m_pivotPresetsWidget);
    }

    // Vector ctrl.
    {
        m_propertyVectorCtrl->setLabel(0, "X");
        m_propertyVectorCtrl->setLabel(1, "Y");

        QObject::connect(m_propertyVectorCtrl, &AzToolsFramework::PropertyVectorCtrl::valueChanged, this, [this]()
            {
                EBUS_EVENT(AzToolsFramework::PropertyEditorGUIMessages::Bus, RequestWrite, this);
            });

        m_propertyVectorCtrl->setMinimum(-std::numeric_limits<float>::max());
        m_propertyVectorCtrl->setMaximum(std::numeric_limits<float>::max());

        layout->addWidget(m_propertyVectorCtrl);
    }
}

void PropertyPivotCtrl::ConsumeAttribute(AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName)
{
    m_common.ConsumeAttributes(GetPropertyVectorCtrl(), attrib, attrValue, debugName);
}

PivotPresetsWidget* PropertyPivotCtrl::GetPivotPresetsWidget()
{
    return m_pivotPresetsWidget;
}

AzToolsFramework::PropertyVectorCtrl* PropertyPivotCtrl::GetPropertyVectorCtrl()
{
    return m_propertyVectorCtrl;
}

//-------------------------------------------------------------------------------

QWidget* PropertyHandlerPivot::CreateGUI(QWidget* pParent)
{
    return aznew PropertyPivotCtrl(pParent);
}

void PropertyHandlerPivot::ConsumeAttribute(PropertyPivotCtrl* GUI, AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName)
{
    GUI->ConsumeAttribute(attrib, attrValue, debugName);
}

void PropertyHandlerPivot::WriteGUIValuesIntoProperty(size_t index, PropertyPivotCtrl* GUI, property_t& instance, AzToolsFramework::InstanceDataNode* node)
{
    AzToolsFramework::VectorElement** elements = GUI->GetPropertyVectorCtrl()->getElements();

    // Check if a pivot preset has been selected
    bool presetSelected = true;
    for (int idx = 0; idx < GUI->GetPropertyVectorCtrl()->getSize(); ++idx)
    {
        if (elements[idx]->WasValueEditedByUser())
        {
            presetSelected = false;
            break;
        }
    }

    AZ::Vector2 newPivot;
    if (presetSelected)
    {
        newPivot.SetX(elements[0]->GetValue());
        newPivot.SetY(elements[1]->GetValue());
    }
    else
    {
        newPivot = instance;

        if (elements[0]->WasValueEditedByUser())
        {
            newPivot.SetX(elements[0]->GetValue());
        }
        if (elements[1]->WasValueEditedByUser())
        {
            newPivot.SetY(elements[1]->GetValue());
        }
    }

    // Check if this element is being controlled by its parent
    AZ::EntityId entityId = GetParentEntityId(node, index);
    bool isControlledByParent = false;
    AZ::Entity* parentElement = nullptr;
    EBUS_EVENT_ID_RESULT(parentElement, entityId, UiElementBus, GetParent);
    if (parentElement)
    {
        EBUS_EVENT_ID_RESULT(isControlledByParent, parentElement->GetId(), UiLayoutBus, IsControllingChild, entityId);
    }

    // IMPORTANT: This will indirectly update "instance".
    if (isControlledByParent)
    {
        EBUS_EVENT_ID(entityId, UiTransformBus, SetPivot, newPivot);
    }
    else
    {
        EBUS_EVENT_ID(entityId, UiTransform2dBus, SetPivotAndAdjustOffsets, newPivot);
    }
}

bool PropertyHandlerPivot::ReadValuesIntoGUI(size_t index, PropertyPivotCtrl* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* node)
{
    (int)index;

    AzToolsFramework::PropertyVectorCtrl* ctrl = GUI->GetPropertyVectorCtrl();

    ctrl->blockSignals(true);
    {
        ctrl->setValuebyIndex(instance.GetX(), 0);
        ctrl->setValuebyIndex(instance.GetY(), 1);
    }
    ctrl->blockSignals(false);

    GUI->GetPivotPresetsWidget()->SetPresetSelection(PivotPresets::PivotToPresetIndex(instance));

    return false;
}

AZ::EntityId PropertyHandlerPivot::GetParentEntityId(AzToolsFramework::InstanceDataNode* node, size_t index)
{
    while (node)
    {
        if ((node->GetClassMetadata()) && (node->GetClassMetadata()->m_azRtti))
        {
            if (node->GetClassMetadata()->m_azRtti->IsTypeOf(AZ::Component::RTTI_Type()))
            {
                return static_cast<AZ::Component*>(node->GetInstance(index))->GetEntityId();
            }
        }

        node = node->GetParent();
    }

    return AZ::EntityId();
}

void PropertyHandlerPivot::Register()
{
    EBUS_EVENT(AzToolsFramework::PropertyTypeRegistrationMessages::Bus, RegisterPropertyType, aznew PropertyHandlerPivot());
}

#include <PropertyHandlerPivot.moc>
