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

#include "GradientSignal_precompiled.h"
#include "GradientPreviewDataWidget.h"
#include <GradientSignal/Ebuses/GradientPreviewContextRequestBus.h>

#include <AzCore/Component/EntityId.h>

#include <QPushButton>
#include <QVBoxLayout>

namespace GradientSignal
{
    //
    // GradientPreviewDataWidgetHandler
    //

    GradientPreviewDataWidgetHandler* GradientPreviewDataWidgetHandler::s_instance = nullptr;

    AZ::u32 GradientPreviewDataWidgetHandler::GetHandlerName() const
    {
        return AZ_CRC("GradientPreviewer", 0x1dbbba45);
    }

    void GradientPreviewDataWidgetHandler::ConsumeAttribute(GradientPreviewDataWidget* GUI, AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName)
    {
        (void)debugName;

        if (attrib == AZ_CRC("GradientFilter", 0x99bf0362))
        {
            GradientPreviewWidget::SampleFilterFunc filterFunc;
            if (attrValue->Read<GradientPreviewWidget::SampleFilterFunc>(filterFunc))
            {
                GUI->SetGradientSampleFilter(filterFunc);
            }
        }
        else if (attrib == AZ_CRC("GradientSampler", 0xaec97010))
        {
            GradientSampler* sampler = nullptr;
            if (attrValue->Read<GradientSampler*>(sampler) && sampler)
            {
                GUI->SetGradientSampler(*sampler);
            }
        }
        else if (attrib == AZ_CRC("GradientEntity", 0xe8531817))
        {
            AZ::EntityId id;
            if (attrValue->Read<AZ::EntityId>(id))
            {
                GUI->SetGradientEntity(id);
            }
        }
    }

    bool GradientPreviewDataWidgetHandler::ReadValueIntoGUI(size_t index, GradientPreviewDataWidget* GUI, void* value, const AZ::Uuid& propertyType)
    {
        (void)index;
        (void)value;
        (void)propertyType;

        GUI->Refresh();

        return false;
    }

    QWidget* GradientPreviewDataWidgetHandler::CreateGUI(QWidget* pParent)
    {
        return new GradientPreviewDataWidget(pParent);
    }

    void GradientPreviewDataWidgetHandler::Register()
    {
        using namespace AzToolsFramework;

        if (!s_instance)
        {
            s_instance = aznew GradientPreviewDataWidgetHandler();
            PropertyTypeRegistrationMessages::Bus::Broadcast(&PropertyTypeRegistrationMessages::Bus::Events::RegisterPropertyType, s_instance);
        }
    }

    void GradientPreviewDataWidgetHandler::Unregister()
    {
        using namespace AzToolsFramework;

        if (s_instance)
        {
            PropertyTypeRegistrationMessages::Bus::Broadcast(&PropertyTypeRegistrationMessages::Bus::Events::UnregisterPropertyType, s_instance);
            delete s_instance;
            s_instance = nullptr;
        }
    }

    //
    // GradientPreviewDataWidget
    //

    GradientPreviewDataWidget::GradientPreviewDataWidget(QWidget * parent)
        : QWidget(parent)
    {
        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setContentsMargins(QMargins());
        layout->setAlignment(Qt::AlignHCenter);

        m_preview = new GradientPreviewWidget(this);
        m_preview->setFixedSize(256, 256);
        layout->addWidget(m_preview);

        QPushButton* popout = new QPushButton("Show Larger Preview");
        layout->addWidget(popout);
        connect(popout, &QPushButton::clicked, this, [this]()
        {
            delete m_previewWindow;
            m_previewWindow = new GradientPreviewWidget;

            // We need to call show() once before the resize to initialize the window frame width/height,
            // so that way the resize correctly takes them into account.  We then call show() a second time
            // afterwards to cause the resize to take effect.
            m_previewWindow->show();
            m_previewWindow->resize(750, 750);
            m_previewWindow->show();
            Refresh();
        });

        //dependency monitor must be connected to an owner/observer as a target for notifications.
        //generating a place holder entity
        m_observerEntityStub = AZ::Entity::MakeId();
        LmbrCentral::DependencyNotificationBus::Handler::BusConnect(m_observerEntityStub);
        GradientPreviewRequestBus::Handler::BusConnect();
    }

    GradientPreviewDataWidget::~GradientPreviewDataWidget()
    {
        GradientPreviewRequestBus::Handler::BusDisconnect();
        LmbrCentral::DependencyNotificationBus::Handler::BusDisconnect();
        m_dependencyMonitor.Reset();
        delete m_previewWindow;
    }

    void GradientPreviewDataWidget::SetGradientSampler(const GradientSampler& sampler)
    {
        m_sampler = sampler;
        Refresh();
    }

    void GradientPreviewDataWidget::SetGradientSampleFilter(GradientPreviewWidget::SampleFilterFunc sampleFunc)
    {
        m_sampleFilterFunc = sampleFunc;
        Refresh();
    }

    void GradientPreviewDataWidget::SetGradientEntity(const AZ::EntityId& id)
    {
        m_sampler = {};
        m_sampler.m_gradientId = id;
        m_sampler.m_ownerEntityId = id;
        Refresh();
    }

    void GradientPreviewDataWidget::OnCompositionChanged()
    {
        Refresh();
    }

    void GradientPreviewDataWidget::Refresh()
    {
        if (!m_refreshInProgress)
        {
            m_refreshInProgress = true;

            m_dependencyMonitor.Reset();
            m_dependencyMonitor.ConnectOwner(m_observerEntityStub);
            m_dependencyMonitor.ConnectDependency(m_sampler.m_gradientId);

            AZ::EntityId previewEntity;
            GradientPreviewContextRequestBus::BroadcastResult(previewEntity, &GradientPreviewContextRequestBus::Events::GetPreviewEntity);
            m_dependencyMonitor.ConnectDependency(previewEntity);

            for (GradientPreviewWidget* previewer : { m_preview, m_previewWindow })
            {
                if (previewer)
                {
                    previewer->SetGradientSampler(m_sampler);
                    previewer->SetGradientSampleFilter(m_sampleFilterFunc);
                    previewer->QueueUpdate();
                }
            }
            m_refreshInProgress = false;
        }
    }
} //namespace GradientSignal