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

#include "ProjectSettingsTool_precompiled.h"
#include "PropertyImagePreview.h"

#include "DefaultImageValidator.h"
#include "PlatformSettings_common.h"
#include "Utils.h"
#include "ValidationHandler.h"
#include "ValidatorBus.h"

#include <QBoxLayout>
#include <QDir>
#include <QLineEdit>

namespace ProjectSettingsTool
{
    // Max width/length of preview in pixels
    static const int maxPreviewDim = 96;

    PropertyImagePreviewCtrl::PropertyImagePreviewCtrl(QWidget* parent)
        : PropertyFileSelectCtrl(parent)
        , m_defaultImagePreview(nullptr)
        , m_defaultPath("")
    {
        QLayout* myLayout = layout();
        QBoxLayout* boxLayout = qobject_cast<QBoxLayout*>(myLayout);

        m_preview = new QLabel(this);
        m_preview->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        m_preview->setFixedSize(QSize(maxPreviewDim, maxPreviewDim));

        if (boxLayout != nullptr)
        {
            boxLayout->insertWidget(0, m_preview);
        }
        else
        {
            AZ_Assert(false, "Expected QBoxLayout type not found in lineedit control.");
            myLayout->addWidget(m_preview);
        }

        m_selectFunctor = &SelectImageFromFileDialog;

        connect(m_pLineEdit, &QLineEdit::textChanged, this, &PropertyImagePreviewCtrl::LoadPreview);
    }

    void PropertyImagePreviewCtrl::SetValue(const QString& path)
    {
        PropertyFuncValLineEditCtrl::SetValue(path);
        // Preview will not be shown if path is set to empty because it has not changed so call it here
        LoadPreview();
    }

    const QString& PropertyImagePreviewCtrl::DefaultImagePath() const
    {
        return m_defaultPath;
    }

    void PropertyImagePreviewCtrl::SetDefaultImagePath(const QString& newPath)
    {
        m_defaultPath = newPath;
    }

    void PropertyImagePreviewCtrl::SetDefaultImagePreview(PropertyImagePreviewCtrl* imageSelect)
    {
        if (m_defaultImagePreview == nullptr)
        {
            m_defaultImagePreview = imageSelect;
            connect(imageSelect, &PropertyStringLineEditCtrl::valueChanged, this, &PropertyImagePreviewCtrl::LoadPreview);
        }
        else
        {
            AZ_Assert(false, "Default image preview already set.");
        }
    }

    PropertyImagePreviewCtrl* PropertyImagePreviewCtrl::DefaultImagePreview() const
    {
        return m_defaultImagePreview;
    }

    void PropertyImagePreviewCtrl::AddOverrideToValidator(PropertyImagePreviewCtrl* preview)
    {
        qobject_cast<DefaultImageValidator*>(m_validator)->AddOverride(preview);
        connect(preview, &PropertyFuncValLineEditCtrl::ValueChangedByUser, this, &PropertyFuncValLineEditCtrl::ForceValidate);
    }

    void PropertyImagePreviewCtrl::LoadPreview()
    {
        QString currentPath = GetValue();
        const QString* imagePath = nullptr;

        if (!currentPath.isEmpty())
        {
            imagePath = &currentPath;
        }
        else
        {
            if (m_defaultImagePreview != nullptr)
            {
                currentPath = m_defaultImagePreview->GetValue();
            }
            if (!currentPath.isEmpty())
            {
                imagePath = &currentPath;
            }
            else
            {
                imagePath = &m_defaultPath;
            }
        }

        QDir dirPath(*imagePath);

        // Keeps image from showing when no extension is given
        if (!imagePath->isEmpty() && dirPath.isReadable())
        {
            // Image loaded from file
            QImage originalImage;
            // Scaled image
            QImage processedImage;
            // The image to be displayed
            QImage* finalImage;
            bool validImage = originalImage.load(*imagePath);

            if (validImage)
            {
                //Scale down any images larger than 128 on either dimension
                if (originalImage.height() > maxPreviewDim || originalImage.width() > maxPreviewDim)
                {
                    processedImage = originalImage.scaled(maxPreviewDim, maxPreviewDim,
                        Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    finalImage = &processedImage;
                }
                else
                {
                    finalImage = &originalImage;
                }

                m_preview->setPixmap(QPixmap::fromImage(*finalImage));
            }
            // Failed to load image so set the preview to nothing
            else
            {
                m_preview->setPixmap(QPixmap());
            }
        }
        // Nothing valid set preview to nothing
        else
        {
            m_preview->setPixmap(QPixmap());
        }
    }

    void PropertyImagePreviewCtrl::UpgradeToDefaultValidator()
    {
        DefaultImageValidator* newValidator = new DefaultImageValidator(*m_validator);
        SetValidator(newValidator);
        // Track the memory allocated for this validator so its deleted
        ValidatorBus::Broadcast(
            &ValidatorBus::Handler::TrackValidator,
            newValidator);
    }

    void PropertyImagePreviewCtrl::ConsumeAttribute(AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName)
    {
        if (attrib == Attributes::DefaultPath)
        {
            AZStd::string path;
            if (attrValue->Read<AZStd::string>(path))
            {
                m_defaultPath = path.data();
            }
        }
        else
        {
            PropertyFileSelectCtrl::ConsumeAttribute(attrib, attrValue, debugName);
        }
    }

    //  Handler  ///////////////////////////////////////////////////////////////////

    PropertyImagePreviewHandler::PropertyImagePreviewHandler(ValidationHandler* valHdlr)
        : AzToolsFramework::PropertyHandler<AZStd::string, PropertyImagePreviewCtrl>()
        , m_validationHandler(valHdlr)
    {}

    AZ::u32 PropertyImagePreviewHandler::GetHandlerName(void) const
    {
        return Handlers::ImagePreview;
    }

    QWidget* PropertyImagePreviewHandler::CreateGUI(QWidget* pParent)
    {
        PropertyImagePreviewCtrl* ctrl = aznew PropertyImagePreviewCtrl(pParent);
        m_validationHandler->AddValidatorCtrl(ctrl);
        return ctrl;
    }

    void PropertyImagePreviewHandler::ConsumeAttribute(PropertyImagePreviewCtrl* GUI, AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName)
    {
        //This means we are using this as a default image preview
        if (attrib == Attributes::PropertyIdentfier)
        {
            AZStd::string ident;
            if (attrValue->Read<AZStd::string>(ident))
            {
                GUI->UpgradeToDefaultValidator();
                m_identToCtrl.insert(AZStd::pair<AZStd::string, PropertyImagePreviewCtrl*>(ident, GUI));
            }
        }
        else if (attrib == Attributes::DefaultImagePreview)
        {
            AZStd::string ident;
            if (attrValue->Read<AZStd::string>(ident))
            {
                auto defaultPreview = m_identToCtrl.find(ident);
                if (defaultPreview != m_identToCtrl.end())
                {
                    defaultPreview->second->AddOverrideToValidator(GUI);
                    GUI->SetDefaultImagePreview(defaultPreview->second);
                }
                else
                {
                    AZ_Assert(false, "Default image select \"%s\" not found.", ident.data());
                }
            }
        }
        else
        {
            GUI->ConsumeAttribute(attrib, attrValue, debugName);
        }
    }

    void PropertyImagePreviewHandler::WriteGUIValuesIntoProperty(size_t index, PropertyImagePreviewCtrl* GUI, property_t& instance, AzToolsFramework::InstanceDataNode* node)
    {
        instance = GUI->GetValue().toUtf8().data();
    }

    bool PropertyImagePreviewHandler::ReadValuesIntoGUI(size_t index, PropertyImagePreviewCtrl* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* node)
    {
        GUI->SetValue(instance.data());
        GUI->ForceValidate();
        return true;
    }

    PropertyImagePreviewHandler* PropertyImagePreviewHandler::Register(ValidationHandler* valHdlr)
    {
        PropertyImagePreviewHandler* handler = aznew PropertyImagePreviewHandler(valHdlr);
        AzToolsFramework::PropertyTypeRegistrationMessages::Bus::Broadcast(
            &AzToolsFramework::PropertyTypeRegistrationMessages::Bus::Handler::RegisterPropertyType,
            handler);
        return handler;
    }
} // namespace ProjectSettingsTool

#include <PropertyImagePreview.moc>
