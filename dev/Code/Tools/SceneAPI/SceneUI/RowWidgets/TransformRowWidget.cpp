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

#include <QLabel>
#include <QStyle>
#include <QGridLayout>
#include <SceneAPI/SceneUI/RowWidgets/TransformRowWidget.h>
#include <AzToolsFramework/UI/PropertyEditor/PropertyVectorCtrl.hxx>

namespace AZ
{
    namespace SceneAPI
    {
        namespace SceneUI
        {
            AZ_CLASS_ALLOCATOR_IMPL(ExpandedTransform, SystemAllocator, 0);

            void PopulateVector3(AzToolsFramework::PropertyVectorCtrl* vectorProperty, AZ::Vector3& vector)
            {
                AZ_Assert(vectorProperty->getSize() == 3, "Trying to populate a Vector3 from an invalidly sized Vector PropertyCtrl");

                if (vectorProperty->getSize() < 3)
                {
                    return;
                }

                AzToolsFramework::VectorElement** elements = vectorProperty->getElements();

                for (int i = 0; i < vectorProperty->getSize(); ++i)
                {
                    AzToolsFramework::VectorElement* currentElement = elements[i];
                    vector.SetElement(i, currentElement->GetValue());
                }
            }

            ExpandedTransform::ExpandedTransform()
                : m_translation(0, 0, 0)
                , m_rotation(0, 0, 0)
                , m_scale(1, 1, 1)
            {
            }

            ExpandedTransform::ExpandedTransform(const Transform& transform)
            {
                SetTransform(transform);
            }

            void ExpandedTransform::SetTransform(const AZ::Transform& transform)
            {
                m_translation = transform.GetTranslation();
                m_rotation = transform.GetEulerDegrees();
                m_scale = transform.RetrieveScaleExact();
            }

            void ExpandedTransform::GetTransform(AZ::Transform& transform) const
            {
                transform = Transform::CreateTranslation(m_translation);
                transform *= AZ::ConvertEulerDegreesToTransform(m_rotation);
                transform.MultiplyByScale(m_scale);
            }

            const AZ::Vector3& ExpandedTransform::GetTranslation() const
            {
                return m_translation;
            }

            void ExpandedTransform::SetTranslation(const AZ::Vector3& translation)
            {
                m_translation = translation;
            }

            const AZ::Vector3& ExpandedTransform::GetRotation() const
            {
                return m_rotation;
            }

            void ExpandedTransform::SetRotation(const AZ::Vector3& rotation)
            {
                m_rotation = rotation;
            }

            const AZ::Vector3& ExpandedTransform::GetScale() const
            {
                return m_scale;
            }

            void ExpandedTransform::SetScale(const AZ::Vector3& scale)
            {
                m_scale = scale;
            }
            
            
            AZ_CLASS_ALLOCATOR_IMPL(TransformRowWidget, SystemAllocator, 0);

            TransformRowWidget::TransformRowWidget(QWidget* parent)
                : QWidget(parent)
            {
                QGridLayout* layout = new QGridLayout();
                setLayout(layout);

                m_translationWidget = aznew AzToolsFramework::PropertyVectorCtrl(this, 3);
                m_translationWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
                m_translationWidget->setMinimum(-9999999);
                m_translationWidget->setMaximum(9999999);

                m_rotationWidget = aznew AzToolsFramework::PropertyVectorCtrl(this, 3);
                m_rotationWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
                m_rotationWidget->setLabel(0, "P");
                m_rotationWidget->setLabel(1, "R");
                m_rotationWidget->setLabel(2, "Y");
                m_rotationWidget->setLabelStyle(0, "font: bold; color: rgb(184,51,51);");
                m_rotationWidget->setLabelStyle(1, "font: bold; color: rgb(48,208,120);");
                m_rotationWidget->setLabelStyle(2, "font: bold; color: rgb(66,133,244);");
                m_rotationWidget->setMinimum(0);
                m_rotationWidget->setMaximum(360);
                m_rotationWidget->setSuffix(" degrees");

                m_scaleWidget = aznew AzToolsFramework::PropertyVectorCtrl(this, 3);
                m_scaleWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
                m_scaleWidget->setMinimum(0);
                m_scaleWidget->setMaximum(10000);
                
                layout->addWidget(new QLabel("Position"), 0, 0);
                layout->addWidget(m_translationWidget, 0, 1);
                layout->addWidget(new QLabel("Rotation"), 1, 0);
                layout->addWidget(m_rotationWidget, 1, 1);
                layout->addWidget(new QLabel("Scale"), 2, 0);
                layout->addWidget(m_scaleWidget, 2, 1);

                QObject::connect(m_translationWidget, &AzToolsFramework::PropertyVectorCtrl::valueChanged, this, [this]
                {
                    AzToolsFramework::PropertyVectorCtrl* widget = this->GetTranslationWidget();
                    AZ::Vector3 translation;

                    PopulateVector3(widget, translation);

                    m_transform.SetTranslation(translation);
                    AzToolsFramework::PropertyEditorGUIMessages::Bus::Broadcast(&AzToolsFramework::PropertyEditorGUIMessages::RequestWrite, this);
                });

                QObject::connect(m_rotationWidget, &AzToolsFramework::PropertyVectorCtrl::valueChanged, this, [this]
                {
                    AzToolsFramework::PropertyVectorCtrl* widget = this->GetRotationWidget();
                    AZ::Vector3 rotation;

                    PopulateVector3(widget, rotation);

                    m_transform.SetRotation(rotation);
                    AzToolsFramework::PropertyEditorGUIMessages::Bus::Broadcast(&AzToolsFramework::PropertyEditorGUIMessages::RequestWrite, this);
                });

                QObject::connect(m_scaleWidget, &AzToolsFramework::PropertyVectorCtrl::valueChanged, this, [this]
                {
                    AzToolsFramework::PropertyVectorCtrl* widget = this->GetScaleWidget();
                    AZ::Vector3 scale;

                    PopulateVector3(widget, scale);

                    m_transform.SetScale(scale);
                    AzToolsFramework::PropertyEditorGUIMessages::Bus::Broadcast(&AzToolsFramework::PropertyEditorGUIMessages::RequestWrite, this);
                });
            }

            void TransformRowWidget::SetEnableEdit(bool enableEdit)
            {
                m_translationWidget->setEnabled(enableEdit);
                m_rotationWidget->setEnabled(enableEdit);
                m_scaleWidget->setEnabled(enableEdit);
            }

            void TransformRowWidget::SetTransform(const AZ::Transform& transform)
            {
                blockSignals(true);
                
                m_transform.SetTransform(transform);
                
                m_translationWidget->setValuebyIndex(m_transform.GetTranslation().GetX(), 0);
                m_translationWidget->setValuebyIndex(m_transform.GetTranslation().GetY(), 1);
                m_translationWidget->setValuebyIndex(m_transform.GetTranslation().GetZ(), 2);

                m_rotationWidget->setValuebyIndex(m_transform.GetRotation().GetX(), 0);
                m_rotationWidget->setValuebyIndex(m_transform.GetRotation().GetY(), 1);
                m_rotationWidget->setValuebyIndex(m_transform.GetRotation().GetZ(), 2);

                m_scaleWidget->setValuebyIndex(m_transform.GetScale().GetX(), 0);
                m_scaleWidget->setValuebyIndex(m_transform.GetScale().GetY(), 1);
                m_scaleWidget->setValuebyIndex(m_transform.GetScale().GetZ(), 2);

                blockSignals(false);
            }

            void TransformRowWidget::GetTransform(AZ::Transform& transform) const
            {
                m_transform.GetTransform(transform);
            }

            const ExpandedTransform& TransformRowWidget::GetExpandedTransform() const
            {
                return m_transform;
            }

            AzToolsFramework::PropertyVectorCtrl* TransformRowWidget::GetTranslationWidget()
            {
                return m_translationWidget;
            }

            AzToolsFramework::PropertyVectorCtrl* TransformRowWidget::GetRotationWidget()
            {
                return m_rotationWidget;
            }

            AzToolsFramework::PropertyVectorCtrl* TransformRowWidget::GetScaleWidget()
            {
                return m_scaleWidget;
            }
        } // namespace SceneUI
    } // namespace SceneAPI
} // namespace AZ

#include <RowWidgets/TransformRowWidget.moc>
