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

#include <QCoreApplication>

#include <Components/Slots/Execution/ExecutionSlotLayoutComponent.h>

#include <Components/Slots/Execution/ExecutionSlotConnectionPin.h>

namespace GraphCanvas
{
    ////////////////////////
    // ExecutionSlotLayout
    ////////////////////////

    ExecutionSlotLayout::ExecutionSlotLayout(ExecutionSlotLayoutComponent& owner)
        : m_connectionType(ConnectionType::CT_Invalid)
        , m_owner(owner)
    {
        setInstantInvalidatePropagation(true);
        setOrientation(Qt::Horizontal);

        m_slotConnectionPin = aznew ExecutionSlotConnectionPin(owner.GetEntityId());
        m_slotText = aznew GraphCanvasLabel();
    }

    ExecutionSlotLayout::~ExecutionSlotLayout()
    {
    }

    void ExecutionSlotLayout::Activate()
    {
        SceneMemberNotificationBus::Handler::BusConnect(m_owner.GetEntityId());
        SlotNotificationBus::Handler::BusConnect(m_owner.GetEntityId());
        StyleNotificationBus::Handler::BusConnect(m_owner.GetEntityId());
        m_slotConnectionPin->Activate();
    }

    void ExecutionSlotLayout::Deactivate()
    {
        m_slotConnectionPin->Deactivate();
        SceneMemberNotificationBus::Handler::BusDisconnect();
        SlotNotificationBus::Handler::BusDisconnect();
        StyleNotificationBus::Handler::BusDisconnect();
    }

    void ExecutionSlotLayout::OnSceneSet(const AZ::EntityId&)
    {
        SlotRequestBus::EventResult(m_connectionType, m_owner.GetEntityId(), &SlotRequests::GetConnectionType);

        TranslationKeyedString slotName;
        SlotRequestBus::EventResult(slotName, m_owner.GetEntityId(), &SlotRequests::GetTranslationKeyedName);

        m_slotText->SetLabel(slotName);

        TranslationKeyedString toolTip;
        SlotRequestBus::EventResult(toolTip, m_owner.GetEntityId(), &SlotRequests::GetTranslationKeyedTooltip);

        OnTooltipChanged(toolTip);

        UpdateLayout();
        OnStyleChanged();
    }

    void ExecutionSlotLayout::OnSceneReady()
    {
        OnStyleChanged();
    }

    void ExecutionSlotLayout::OnRegisteredToNode(const AZ::EntityId& nodeId)
    {
        OnStyleChanged();
    }

    void ExecutionSlotLayout::OnNameChanged(const TranslationKeyedString& name)
    {
        m_slotText->SetLabel(name);
    }

    void ExecutionSlotLayout::OnTooltipChanged(const TranslationKeyedString& tooltip)
    {
        AZStd::string displayText = tooltip.GetDisplayString();

        m_slotConnectionPin->setToolTip(displayText.c_str());
        m_slotText->setToolTip(displayText.c_str());
    }

    void ExecutionSlotLayout::OnStyleChanged()
    {
        m_style.SetStyle(m_owner.GetEntityId());

        switch (m_connectionType)
        {
        case ConnectionType::CT_Input:
            m_slotText->SetStyle(m_owner.GetEntityId(), ".inputSlotName");
            break;
        case ConnectionType::CT_Output:
            m_slotText->SetStyle(m_owner.GetEntityId(), ".outputSlotName");
            break;
        default:
            m_slotText->SetStyle(m_owner.GetEntityId(), ".slotName");
            break;
        };

        m_slotConnectionPin->RefreshStyle();

        qreal padding = m_style.GetAttribute(Styling::Attribute::Padding, 2.);
        setContentsMargins(padding, padding, padding, padding);
        setSpacing(m_style.GetAttribute(Styling::Attribute::Spacing, 2.));

        UpdateGeometry();
    }

    void ExecutionSlotLayout::UpdateLayout()
    {
        for (int i = count() - 1; i >= 0; --i)
        {
            removeAt(i);
        }

        switch (m_connectionType)
        {
        case ConnectionType::CT_Input:
            addItem(m_slotConnectionPin);
            setAlignment(m_slotConnectionPin, Qt::AlignLeft);

            addItem(m_slotText);
            setAlignment(m_slotText, Qt::AlignLeft);
            break;
        case ConnectionType::CT_Output:
            addItem(m_slotText);
            setAlignment(m_slotText, Qt::AlignRight);

            addItem(m_slotConnectionPin);
            setAlignment(m_slotConnectionPin, Qt::AlignRight);
            break;
        default:
            addItem(m_slotConnectionPin);
            addItem(m_slotText);
            break;
        }
    }

    void ExecutionSlotLayout::UpdateGeometry()
    {
        m_slotConnectionPin->updateGeometry();
        m_slotText->update();

        invalidate();
        updateGeometry();
    }

    /////////////////////////////////
    // ExecutionSlotLayoutComponent
    /////////////////////////////////

    void ExecutionSlotLayoutComponent::Reflect(AZ::ReflectContext* reflectContext)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflectContext);

        if (serializeContext)
        {
            serializeContext->Class<ExecutionSlotLayoutComponent, AZ::Component>()
                ->Version(1)
                ;
        }
    }

    ExecutionSlotLayoutComponent::ExecutionSlotLayoutComponent()
        : m_layout(nullptr)
    {
    }

    void ExecutionSlotLayoutComponent::Init()
    {
        SlotLayoutComponent::Init();

        m_layout = aznew ExecutionSlotLayout((*this));
        SetLayout(m_layout);
    }

    void ExecutionSlotLayoutComponent::Activate()
    {
        SlotLayoutComponent::Activate();
        m_layout->Activate();
    }

    void ExecutionSlotLayoutComponent::Deactivate()
    {
        SlotLayoutComponent::Deactivate();
        m_layout->Deactivate();
    }
}