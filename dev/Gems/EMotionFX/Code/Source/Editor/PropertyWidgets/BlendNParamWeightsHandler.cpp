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

#include "BlendNParamWeightsHandler.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLocale>
#include <AzCore/std/containers/unordered_map.h>
#include <EMotionFX/Source/AnimGraphNode.h>

namespace EMotionFX
{
    AZ_CLASS_ALLOCATOR_IMPL(BlendNParamWeightContainerWidget, AZ::SystemAllocator, 0)
    AZ_CLASS_ALLOCATOR_IMPL(BlendNParamWeightsHandler, AZ::SystemAllocator, 0)
    AZ_CLASS_ALLOCATOR_IMPL(BlendNParamWeightElementWidget, AZ::SystemAllocator, 0)
    AZ_CLASS_ALLOCATOR_IMPL(BlendNParamWeightElementHandler, AZ::SystemAllocator, 0)
    AZ_CLASS_ALLOCATOR_IMPL(BlendNParamWeightGuiEntry, AZ::SystemAllocator, 0)


    const int BlendNParamWeightElementWidget::s_decimalPlaces = 2;

    BlendNParamWeightGuiEntry::BlendNParamWeightGuiEntry(AZ::u32 portId, float weightRange, const char* sourceNodeName) 
        : m_portId(portId),
          m_weightRange(weightRange),
          m_sourceNodeName(sourceNodeName)
    { }

    const char* BlendNParamWeightGuiEntry::GetSourceNodeName() const
    {
        return m_sourceNodeName;
    }

    const AZStd::string& BlendNParamWeightGuiEntry::GetTooltipText() const
    {
        return m_tooltipText;
    }

    void BlendNParamWeightGuiEntry::SetTooltipText(const AZStd::string& text)
    {
        m_tooltipText = text;
    }

    const char* BlendNParamWeightGuiEntry::GetPortLabel() const
    {
        return BlendTreeBlendNNode::GetPoseInputPortName(m_portId);
    }

    AZ::u32 BlendNParamWeightGuiEntry::GetPortId() const
    {
        return m_portId;
    }

    float BlendNParamWeightGuiEntry::GetWeightRange() const
    {
        return m_weightRange;
    }

    void BlendNParamWeightGuiEntry::SetWeightRange(float value)
    {
        m_weightRange = value;
    }

    void BlendNParamWeightGuiEntry::SetValid(bool valid)
    {
        m_isValid = valid;
    }

    bool BlendNParamWeightGuiEntry::IsValid()const
    {
        return m_isValid;
    }


    BlendNParamWeightElementWidget::BlendNParamWeightElementWidget(QWidget* parent) 
        : QWidget(parent)
    {
        QHBoxLayout* hLayout = new QHBoxLayout(this);

        hLayout->setMargin(0);
        setLayout(hLayout);
        m_sourceNodeNameLabel = new QLabel("element A", this);
        layout()->addWidget(m_sourceNodeNameLabel);
        m_weightField = new MysticQt::DoubleSpinBox(this);
        m_weightField->setRange(-FLT_MAX, FLT_MAX);
        m_weightField->setDecimals(s_decimalPlaces);
        layout()->addWidget(m_weightField);

        connect(m_weightField
            , static_cast<void (MysticQt::DoubleSpinBox::*)(double)>(&MysticQt::DoubleSpinBox::valueChanged)
            , this, &BlendNParamWeightElementWidget::OnWeightRangeEdited);
    }

    BlendNParamWeightElementWidget::~BlendNParamWeightElementWidget()
    {
        if (m_parentContainerWidget)
        {
            m_parentContainerWidget->RemoveElementWidget(this);
        }
    }

    void BlendNParamWeightElementWidget::OnWeightRangeEdited(double value)
    {
        emit DataChanged(this);
    }


    void BlendNParamWeightElementWidget::SetDataSource(const BlendNParamWeightGuiEntry& paramWeight)
    {
        m_paramWeight = &paramWeight;
    }

    float BlendNParamWeightElementWidget::GetWeightRange() const
    {
        return m_weightField->value();
    }

    void BlendNParamWeightElementWidget::UpdateGui()
    {
        m_sourceNodeNameLabel->setText(m_paramWeight->GetSourceNodeName());
        m_weightField->setValue(m_paramWeight->GetWeightRange());
        if (m_paramWeight->IsValid())
        {
            m_weightField->GetLineEdit()->setStyleSheet("");
            m_weightField->setToolTip("");
        }
        else
        {
            m_weightField->GetLineEdit()->setStyleSheet("color: red;");
            m_weightField->setToolTip(m_paramWeight->GetTooltipText().c_str());
        }
    }

    void BlendNParamWeightElementWidget::SetId(size_t index)
    {
        m_dataElementIndex = index;
    }

    size_t BlendNParamWeightElementWidget::GetId() const
    {
        return m_dataElementIndex;
    }


    BlendNParamWeightContainerWidget::BlendNParamWeightContainerWidget(QWidget* parent)
        : QWidget(parent)
    {
        QVBoxLayout* vLayout = new QVBoxLayout(parent);

        vLayout->setMargin(0);
        setLayout(vLayout);

        QHBoxLayout* hHeaderLayout = new QHBoxLayout(this);
        QLabel* inputNodeLabel = new QLabel("Input node", this);
        QLabel* weightRanges = new QLabel("Max weight trigger", this);

        hHeaderLayout->addWidget(inputNodeLabel);
        hHeaderLayout->addWidget(weightRanges);

        QHBoxLayout* hButtonLayout = new QHBoxLayout(this);

        QPushButton* buttonEqualize = new QPushButton("Evenly distribute", this);
        hButtonLayout->addWidget(buttonEqualize);
        
        vLayout->addLayout(hButtonLayout);
        vLayout->addLayout(hHeaderLayout);

        connect(buttonEqualize, &QPushButton::pressed, this, [this]()
        {
            EqualizeWeightRanges();
            SetAllValid();
            Update();
            emit DataChanged();
        });

        EMotionFX::AnimGraphNotificationBus::Handler::BusConnect();
    }

    BlendNParamWeightContainerWidget::~BlendNParamWeightContainerWidget()
    {
        EMotionFX::AnimGraphNotificationBus::Handler::BusDisconnect();
    }

    const AZStd::vector<BlendNParamWeightGuiEntry>& BlendNParamWeightContainerWidget::GetParamWeights() const
    {
        return m_paramWeights;
    }

    void BlendNParamWeightContainerWidget::EqualizeWeightRanges()
    {
        if (!m_paramWeights.empty())
        {
            float first = m_paramWeights.front().GetWeightRange();
            float last = m_paramWeights.back().GetWeightRange();
            float min = first < last ? first : last;
            float max = first < last ? last : first;
            EqualizeWeightRanges(min, max);
        }
    }

    void BlendNParamWeightContainerWidget::EqualizeWeightRanges(float min, float max)
    {
        if (m_paramWeights.empty())
        {
            return;
        }

        float weightRange = min;
        const size_t paramWeightsSize = m_paramWeights.size();
        float weightStep = (max - min) / (paramWeightsSize - 1);
        m_paramWeights.back().SetWeightRange(max);
        for (size_t i = 0; i < paramWeightsSize - 1; ++i)
        {
            m_paramWeights[i].SetWeightRange(weightRange);
            weightRange += weightStep;
        }
    }

    void BlendNParamWeightContainerWidget::SetParamWeights(const AZStd::vector<BlendNParamWeight>& paramWeights, const AnimGraphNode* node)
    {
        m_paramWeights.clear();
        const size_t paramWeightsCount = paramWeights.size();
        m_paramWeights.reserve(paramWeightsCount);
        for (size_t i = 0; i < paramWeightsCount; ++i)
        {
            const auto& inputPorts = node->GetInputPorts();
            const char* sourceNodeName = "";
            for (const AnimGraphNode::Port& port : inputPorts)
            {
                if (port.mConnection)
                {
                    if (port.mPortID == paramWeights[i].GetPortId())
                    {
                        sourceNodeName = port.mConnection->GetSourceNode()->GetName();
                    }
                }
            }

            m_paramWeights.emplace_back(paramWeights[i].GetPortId(), paramWeights[i].GetWeightRange(), sourceNodeName);
        }

        UpdateDataValidation();
    }

    void BlendNParamWeightContainerWidget::HandleOnChildWidgetDataChanged(BlendNParamWeightElementWidget* elementWidget)
    {
        size_t widgetId = elementWidget->GetId();
        if (widgetId >= m_paramWeights.size())
        {
            AZ_Error("EMotionFX", false, "Weight parameter widget incorrectly initialized");
            return;
        }

        m_paramWeights[widgetId].SetWeightRange(elementWidget->GetWeightRange());
        if (CheckElementValidation(widgetId))
        {
            if (CheckAllElementsValidation())
            {
                SetAllValid();
                Update();
                emit DataChanged();
            }
        }
        else
        {
            m_paramWeights[widgetId].SetValid(false);
            AZStd::string decPlacesStr = AZStd::string::format("%u", BlendNParamWeightElementWidget::s_decimalPlaces);
            if (widgetId == 0)
            {
                m_paramWeights[widgetId].SetTooltipText(AZStd::string::format("The value has to be less than or equal %.2f", m_paramWeights[widgetId + 1].GetWeightRange()));
            }
            else if (widgetId == m_paramWeights.size() - 1)
            {
                m_paramWeights[widgetId].SetTooltipText(AZStd::string::format("The value has to be more than or equal %.2f", m_paramWeights[widgetId - 1].GetWeightRange()));
            }
            else
            {
                m_paramWeights[widgetId].SetTooltipText(AZStd::string::format("The value has to be between %.2f and %.2f", m_paramWeights[widgetId - 1].GetWeightRange(), m_paramWeights[widgetId + 1].GetWeightRange()));
            }
            elementWidget->UpdateGui();
        }
    }

    void BlendNParamWeightContainerWidget::AddElementWidget(BlendNParamWeightElementWidget* widget)
    {
        if (AZStd::find(m_elementWidgets.begin(), m_elementWidgets.end(), widget) == m_elementWidgets.end())
        {
            m_elementWidgets.push_back(widget);
        }
    }

    void BlendNParamWeightContainerWidget::RemoveElementWidget(BlendNParamWeightElementWidget* widget)
    {
        m_elementWidgets.erase(AZStd::remove(m_elementWidgets.begin(), m_elementWidgets.end(), widget), m_elementWidgets.end());
    }

    void BlendNParamWeightContainerWidget::Update()
    {
        for (BlendNParamWeightElementWidget* elementWidget : m_elementWidgets)
        {
            elementWidget->UpdateGui();
        }
    }

    void BlendNParamWeightContainerWidget::ConnectWidgetToDataSource(BlendNParamWeightElementWidget* elementWidget)
    {
        elementWidget->SetId(m_widgetBoundToDataCount++);
        size_t index = elementWidget->GetId();

        if (index >= m_paramWeights.size())
        {
            AZ_Error("EMotionFX", false, "Property widget incorrectly initialized");
            return;
        }
        elementWidget->SetDataSource(m_paramWeights[index]);
        elementWidget->UpdateGui();
        elementWidget->SetParentContainerWidget(this);
        AddElementWidget(elementWidget); // Adds it only in case it hasn't been added yet.


        connect(elementWidget, &BlendNParamWeightElementWidget::DataChanged, this, &BlendNParamWeightContainerWidget::HandleOnChildWidgetDataChanged, Qt::UniqueConnection);
    }

    void BlendNParamWeightContainerWidget::SetAllValid()
    {
        for (BlendNParamWeightGuiEntry& paramWeight : m_paramWeights)
        {
            paramWeight.SetValid(true);
        }
    }

    bool BlendNParamWeightContainerWidget::CheckAllElementsValidation()
    {
        const size_t paramWeightsSize = m_paramWeights.size();
        for (size_t i = 0; i < paramWeightsSize; ++i)
        {
            if (!CheckElementValidation(i))
            {
                return false;
            }
        }
        return true;
    }

    void BlendNParamWeightContainerWidget::UpdateDataValidation()
    {
        const size_t paramWeightsSize = m_paramWeights.size();
        for(size_t i = 0; i < paramWeightsSize; ++i)
        {
            m_paramWeights[i].SetValid(CheckElementValidation(i));
        }
    }

    bool BlendNParamWeightContainerWidget::CheckElementValidation(size_t index)
    {
        if (index > 0)
        {
            if (m_paramWeights[index].GetWeightRange() < m_paramWeights[index - 1].GetWeightRange())
            {
                return false;
            }
        }
        if (index < m_paramWeights.size() - 1)
        {
            if (m_paramWeights[index + 1].GetWeightRange() < m_paramWeights[index].GetWeightRange())
            {
                return false;
            }
        }
        return true;
    }

    bool BlendNParamWeightContainerWidget::CheckValidation()
    {
        for (BlendNParamWeightGuiEntry paramWeight : m_paramWeights)
        {
            if (!paramWeight.IsValid())
            {
                return false;
            }
        }
        return true;
    }

    void BlendNParamWeightContainerWidget::OnSyncVisualObject(AnimGraphObject* object)
    {
        if (azrtti_istypeof<EMotionFX::BlendTreeBlendNNode>(object))
        {
            AzToolsFramework::PropertyEditorGUIMessages::Bus::Broadcast(&AzToolsFramework::PropertyEditorGUIMessages::RequestRefresh, AzToolsFramework::PropertyModificationRefreshLevel::Refresh_EntireTree);
        }
    }

    AZ::u32 BlendNParamWeightElementHandler::GetHandlerName() const
    {
        return AZ_CRC("BlendNParamWeightsElementHandler", 0xec71620d);
    }

    QWidget* BlendNParamWeightElementHandler::CreateGUI(QWidget* parent)
    {
        BlendNParamWeightElementWidget* paramWeightElementWidget = aznew BlendNParamWeightElementWidget(parent);
        return paramWeightElementWidget;
    }

    bool BlendNParamWeightElementHandler::ReadValuesIntoGUI(size_t index, BlendNParamWeightElementWidget* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* node)
    {
        QSignalBlocker signalBlocker(GUI);
        BlendNParemWeightWidgetNotificationBus::Broadcast(&BlendNParemWeightWidgetNotificationBus::Events::OnRequestDataBind, GUI);
        return true;
    }


    BlendNParamWeightsHandler::BlendNParamWeightsHandler()
    {
        BlendNParemWeightWidgetNotificationBus::Handler::BusConnect();
    }
    
    BlendNParamWeightsHandler::~BlendNParamWeightsHandler()
    {
        BlendNParemWeightWidgetNotificationBus::Handler::BusDisconnect();
    }

    AZ::u32 BlendNParamWeightsHandler::GetHandlerName() const
    {
        return AZ_CRC("BlendNParamWeightsContainerHandler", 0x311f6bb3);
    }

    QWidget* BlendNParamWeightsHandler::CreateGUI(QWidget* parent)
    {
        BlendNParamWeightContainerWidget* paramWeightsWidget = aznew BlendNParamWeightContainerWidget(parent);

        connect(paramWeightsWidget, &BlendNParamWeightContainerWidget::DataChanged, this, [paramWeightsWidget]()
        {
            AzToolsFramework::PropertyEditorGUIMessages::Bus::Broadcast(&AzToolsFramework::PropertyEditorGUIMessages::RequestWrite, paramWeightsWidget);
        });
        m_containerWidget = paramWeightsWidget;
        return paramWeightsWidget;
    }

    void BlendNParamWeightsHandler::ConsumeAttribute(BlendNParamWeightContainerWidget* widget, AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName)
    {
        if (attrib == AZ_CRC("BlendTreeBlendNNodeParamWeightsElement", 0x7eae1990) && attrValue)
        {
            m_node = static_cast<AnimGraphNode*>(attrValue->GetInstancePointer());
        }
    }

    void BlendNParamWeightsHandler::WriteGUIValuesIntoProperty(size_t index, BlendNParamWeightContainerWidget* GUI, property_t& instance, AzToolsFramework::InstanceDataNode* node)
    {
        const AZStd::vector<BlendNParamWeightGuiEntry>& paramWeights = GUI->GetParamWeights();
        instance.clear();
        for (size_t i = 0; i < paramWeights.size(); ++i)
        {
            instance.emplace_back(paramWeights[i].GetPortId(), paramWeights[i].GetWeightRange());
        }
    }

    bool BlendNParamWeightsHandler::ReadValuesIntoGUI(size_t index, BlendNParamWeightContainerWidget* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* node)
    {
        QSignalBlocker signalBlocker(GUI);
        GUI->SetParamWeights(instance, m_node);
        return true;
    }

    void BlendNParamWeightsHandler::OnRequestDataBind(BlendNParamWeightElementWidget* elementWidget)
    {
        // Create a QT connection between elementWidget and m_containerWidget;
        m_containerWidget->ConnectWidgetToDataSource(elementWidget);
    }
}