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

#include "PropertyHandlerUiParticleColorKeyframe.h"

#include <QtCore>
#include <QtGui>
#include <QtWidgets>
#include <QtWidgets/QWidget>

#include <AzToolsFramework/UI/PropertyEditor/PropertyQTConstants.h>

PropertyUiParticleColorKeyframeCtrl::PropertyUiParticleColorKeyframeCtrl(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* vLayout = new QVBoxLayout(this);
    vLayout->setContentsMargins(0, 5, 0, 5);
    vLayout->setSpacing(2);
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    QHBoxLayout* layoutRow2 = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    QLabel* timeLabel = new QLabel(parent);
    timeLabel->setText("Time");
    timeLabel->setObjectName("Time");
    layout->addWidget(timeLabel);

    m_timeCtrl = aznew AzToolsFramework::PropertyDoubleSpinCtrl(parent);
    m_timeCtrl->setMinimum(0.0f);
    m_timeCtrl->setMaximum(1.0f);
    m_timeCtrl->setStep(0.0f);
    m_timeCtrl->setMinimumWidth(AzToolsFramework::PropertyQTConstant_MinimumWidth);
    m_timeCtrl->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    m_timeCtrl->setToolTip(tr("Time in the range [0,1]."));

    QObject::connect(m_timeCtrl, &AzToolsFramework::PropertyDoubleSpinCtrl::valueChanged, this, [this]()
        {
            EBUS_EVENT(AzToolsFramework::PropertyEditorGUIMessages::Bus, RequestWrite, this);
        });

    layout->addWidget(m_timeCtrl);

    QLabel* colorLabel = new QLabel(parent);
    colorLabel->setText("Color");
    colorLabel->setObjectName("Color");
    layout->addWidget(colorLabel);

    m_colorCtrl = aznew AzToolsFramework::PropertyColorCtrl(parent);

    QObject::connect(m_colorCtrl, &AzToolsFramework::PropertyColorCtrl::valueChanged, this, [this]()
        {
            EBUS_EVENT(AzToolsFramework::PropertyEditorGUIMessages::Bus, RequestWrite, this);
        });

    layout->addWidget(m_colorCtrl);

    AZStd::pair<AZ::s64, AZStd::string> easeInTangent;
    easeInTangent.first = static_cast<AZ::s64>(UiParticleEmitterInterface::ParticleKeyframeTangentType::EaseIn);
    easeInTangent.second = "Ease In";
    AZStd::pair<AZ::s64, AZStd::string> easeOutTangent;
    easeOutTangent.first = static_cast<AZ::s64>(UiParticleEmitterInterface::ParticleKeyframeTangentType::EaseOut);
    easeOutTangent.second = "Ease Out";
    AZStd::pair<AZ::s64, AZStd::string> linearTangent;
    linearTangent.first = static_cast<AZ::s64>(UiParticleEmitterInterface::ParticleKeyframeTangentType::Linear);
    linearTangent.second = "Linear";
    AZStd::pair<AZ::s64, AZStd::string> stepTangent;
    stepTangent.first = static_cast<AZ::s64>(UiParticleEmitterInterface::ParticleKeyframeTangentType::Step);
    stepTangent.second = "Step";

    QLabel* inTangentLabel = new QLabel(parent);
    inTangentLabel->setText("In tangent");
    inTangentLabel->setObjectName("In tangent");
    layoutRow2->addWidget(inTangentLabel);

    m_inTangentCtrl = aznew AzToolsFramework::PropertyEnumComboBoxCtrl(parent);
    m_inTangentCtrl->addEnumValue(easeInTangent);
    m_inTangentCtrl->addEnumValue(linearTangent);
    m_inTangentCtrl->addEnumValue(stepTangent);

    QObject::connect(m_inTangentCtrl, &AzToolsFramework::PropertyEnumComboBoxCtrl::valueChanged, this, [this]()
        {
            EBUS_EVENT(AzToolsFramework::PropertyEditorGUIMessages::Bus, RequestWrite, this);
        });

    layoutRow2->addWidget(m_inTangentCtrl);

    QLabel* outTangentLabel = new QLabel(parent);
    outTangentLabel->setText("Out tangent");
    outTangentLabel->setObjectName("Out tangent");
    layoutRow2->addWidget(outTangentLabel);

    m_outTangentCtrl = aznew AzToolsFramework::PropertyEnumComboBoxCtrl(parent);
    m_outTangentCtrl->addEnumValue(easeOutTangent);
    m_outTangentCtrl->addEnumValue(linearTangent);
    m_outTangentCtrl->addEnumValue(stepTangent);

    QObject::connect(m_outTangentCtrl, &AzToolsFramework::PropertyEnumComboBoxCtrl::valueChanged, this, [this]()
        {
            EBUS_EVENT(AzToolsFramework::PropertyEditorGUIMessages::Bus, RequestWrite, this);
        });

    layoutRow2->addWidget(m_outTangentCtrl);

    vLayout->addLayout(layout);
    vLayout->addLayout(layoutRow2);
}

void PropertyUiParticleColorKeyframeCtrl::ConsumeAttribute(AZ::u32 /*attrib*/, AzToolsFramework::PropertyAttributeReader* /*attrValue*/, const char* /*debugName*/)
{
}

AzToolsFramework::PropertyColorCtrl* PropertyUiParticleColorKeyframeCtrl::GetColorCtrl()
{
    return m_colorCtrl;
}

AzToolsFramework::PropertyDoubleSpinCtrl* PropertyUiParticleColorKeyframeCtrl::GetTimeCtrl()
{
    return m_timeCtrl;
}

AzToolsFramework::PropertyEnumComboBoxCtrl* PropertyUiParticleColorKeyframeCtrl::GetInTangentCtrl()
{
    return m_inTangentCtrl;
}

AzToolsFramework::PropertyEnumComboBoxCtrl* PropertyUiParticleColorKeyframeCtrl::GetOutTangentCtrl()
{
    return m_outTangentCtrl;
}

QWidget* PropertyHandlerUiParticleColorKeyframe::CreateGUI(QWidget* pParent)
{
    return aznew PropertyUiParticleColorKeyframeCtrl(pParent);
}

void PropertyHandlerUiParticleColorKeyframe::ConsumeAttribute(PropertyUiParticleColorKeyframeCtrl* GUI, AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName)
{
    GUI->ConsumeAttribute(attrib, attrValue, debugName);
}

void PropertyHandlerUiParticleColorKeyframe::WriteGUIValuesIntoProperty(size_t /*index*/, PropertyUiParticleColorKeyframeCtrl* GUI, property_t& instance, AzToolsFramework::InstanceDataNode* /*node*/)
{
    QColor val = GUI->GetColorCtrl()->value();
    AZ::Color asAZColor((float)val.redF(), (float)val.greenF(), (float)val.blueF(), (float)val.alphaF());
    instance.color = asAZColor;
    instance.time = GUI->GetTimeCtrl()->value();
    instance.inTangent = static_cast<UiParticleEmitterInterface::ParticleKeyframeTangentType>(GUI->GetInTangentCtrl()->value());
    instance.outTangent = static_cast<UiParticleEmitterInterface::ParticleKeyframeTangentType>(GUI->GetOutTangentCtrl()->value());
}

bool PropertyHandlerUiParticleColorKeyframe::ReadValuesIntoGUI(size_t /*index*/, PropertyUiParticleColorKeyframeCtrl* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* /*node*/)
{
    GUI->blockSignals(true);
    {
        GUI->GetTimeCtrl()->setValue(instance.time);
        AZ::Vector4 asVector4 = static_cast<AZ::Vector4>(instance.color);
        QColor asQColor;
        asQColor.setRedF((qreal)asVector4.GetX());
        asQColor.setGreenF((qreal)asVector4.GetY());
        asQColor.setBlueF((qreal)asVector4.GetZ());
        asQColor.setAlphaF((qreal)asVector4.GetW());
        GUI->GetColorCtrl()->setValue(asQColor);
        GUI->GetInTangentCtrl()->setValue(static_cast<AZ::s64>(instance.inTangent));
        GUI->GetOutTangentCtrl()->setValue(static_cast<AZ::s64>(instance.outTangent));
    }
    GUI->blockSignals(false);

    return false;
}

AZ::EntityId PropertyHandlerUiParticleColorKeyframe::GetParentEntityId(AzToolsFramework::InstanceDataNode* node, size_t index)
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

void PropertyHandlerUiParticleColorKeyframe::Register()
{
    EBUS_EVENT(AzToolsFramework::PropertyTypeRegistrationMessages::Bus, RegisterPropertyType, aznew PropertyHandlerUiParticleColorKeyframe());
}

#include <PropertyHandlerUiParticleColorKeyframe.moc>