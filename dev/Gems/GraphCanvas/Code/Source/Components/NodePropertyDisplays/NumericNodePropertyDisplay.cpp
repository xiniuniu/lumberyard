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
#include "precompiled.h"

#include <QGraphicsProxyWidget>

#include <Components/NodePropertyDisplays/NumericNodePropertyDisplay.h>

#include <GraphCanvas/Components/SceneBus.h>
#include <GraphCanvas/Components/Slots/Data/DataSlotBus.h>
#include <GraphCanvas/Components/Slots/SlotBus.h>
#include <GraphCanvas/Widgets/NodePropertyBus.h>
#include <Widgets/GraphCanvasLabel.h>

namespace GraphCanvas
{
    ///////////////////////////////
    // NumericNodePropertyDisplay
    ///////////////////////////////
    NumericNodePropertyDisplay::NumericNodePropertyDisplay(NumericDataInterface* dataInterface)
        : m_dataInterface(dataInterface)
        , m_displayLabel(nullptr)
        , m_spinBox(nullptr)
        , m_proxyWidget(nullptr)
    {
        m_dataInterface->RegisterDisplay(this);
        
        m_disabledLabel = aznew GraphCanvasLabel();
        m_displayLabel = aznew GraphCanvasLabel();
    }
    
    NumericNodePropertyDisplay::~NumericNodePropertyDisplay()
    {
        CleanupProxyWidget();
        delete m_dataInterface;
        delete m_displayLabel;
        delete m_disabledLabel;
    }

    void NumericNodePropertyDisplay::RefreshStyle()
    {
        m_disabledLabel->SetSceneStyle(GetSceneId(), NodePropertyDisplay::CreateDisabledLabelStyle("double").c_str());
        m_displayLabel->SetSceneStyle(GetSceneId(), NodePropertyDisplay::CreateDisplayLabelStyle("double").c_str());

        if (m_spinBox)
        {
            QSizeF minimumSize = m_displayLabel->minimumSize();
            QSizeF maximumSize = m_displayLabel->maximumSize();

            m_spinBox->setMinimumSize(minimumSize.width(), minimumSize.height());
            m_spinBox->setMaximumSize(maximumSize.width(), maximumSize.height());
        }
    }
    
    void NumericNodePropertyDisplay::UpdateDisplay()
    {
        double value = m_dataInterface->GetNumber();
        
        AZStd::string displayValue = AZStd::string::format("%.*g%s", m_dataInterface->GetDisplayDecimalPlaces(), value, m_dataInterface->GetSuffix());
        m_displayLabel->SetLabel(displayValue);

        if (m_spinBox)
        {
            QSignalBlocker signalBlocker(m_spinBox);
            m_spinBox->setValue(value);
            m_spinBox->deselectAll();
        }
        
        if (m_proxyWidget)
        {
            m_proxyWidget->update();
        }
    }

    QGraphicsLayoutItem* NumericNodePropertyDisplay::GetDisabledGraphicsLayoutItem()
    {
        CleanupProxyWidget();
        return m_disabledLabel;
    }

    QGraphicsLayoutItem* NumericNodePropertyDisplay::GetDisplayGraphicsLayoutItem()
    {
        CleanupProxyWidget();
        return m_displayLabel;
    }

    QGraphicsLayoutItem* NumericNodePropertyDisplay::GetEditableGraphicsLayoutItem()
    {
        SetupProxyWidget();
        return m_proxyWidget;
    }

    void NumericNodePropertyDisplay::EditStart()
    {
        NodePropertiesRequestBus::Event(GetNodeId(), &NodePropertiesRequests::LockEditState, this);

        TryAndSelectNode();
    }
    
    void NumericNodePropertyDisplay::SubmitValue()
    {
        if (m_spinBox)
        {
            m_dataInterface->SetNumber(m_spinBox->value());
            m_spinBox->selectAll();
        }
        else
        {
            AZ_Error("GraphCanvas", false, "spin box doesn't exist!");
        }
        UpdateDisplay();
    }

    void NumericNodePropertyDisplay::EditFinished()
    {
        SubmitValue();
        if (m_spinBox)
        {
            m_spinBox->deselectAll();
        }

        NodePropertiesRequestBus::Event(GetNodeId(), &NodePropertiesRequests::UnlockEditState, this);
    }

    void NumericNodePropertyDisplay::SetupProxyWidget()
    {
        if (!m_spinBox)
        {
            m_proxyWidget = new QGraphicsProxyWidget();
            m_proxyWidget->setFlag(QGraphicsItem::ItemIsFocusable, true);
            m_proxyWidget->setFocusPolicy(Qt::StrongFocus);

            m_spinBox = aznew Internal::FocusableDoubleSpinBox();
            m_spinBox->setProperty("HasNoWindowDecorations", true);
            m_spinBox->setProperty("DisableFocusWindowFix", true);

            QObject::connect(m_spinBox, &Internal::FocusableDoubleSpinBox::OnFocusIn, [this]() { this->EditStart(); });
            QObject::connect(m_spinBox, &Internal::FocusableDoubleSpinBox::OnFocusOut, [this]() { this->EditFinished(); });
            QObject::connect(m_spinBox, &QDoubleSpinBox::editingFinished, [this]() { this->SubmitValue(); });

            m_spinBox->setMinimum(m_dataInterface->GetMin());
            m_spinBox->setMaximum(m_dataInterface->GetMax());
            m_spinBox->setSuffix(QString(m_dataInterface->GetSuffix()));
            m_spinBox->setDecimals(m_dataInterface->GetDecimalPlaces());
            m_spinBox->SetDisplayDecimals(m_dataInterface->GetDisplayDecimalPlaces());

            m_proxyWidget->setWidget(m_spinBox);
            UpdateDisplay();
            RefreshStyle();
            RegisterShortcutDispatcher(m_spinBox);
        }
    }

    void NumericNodePropertyDisplay::CleanupProxyWidget()
    {
        if (m_spinBox)
        {
            UnregisterShortcutDispatcher(m_spinBox);
            delete m_spinBox; // NB: this implicitly deletes m_proxy widget
            m_spinBox = nullptr;
            m_proxyWidget = nullptr;
        }
    }

#include <Source/Components/NodePropertyDisplays/NumericNodePropertyDisplay.moc>
}