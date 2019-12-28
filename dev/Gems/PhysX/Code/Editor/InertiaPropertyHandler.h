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

#include <AzToolsFramework/UI/PropertyEditor/PropertyEditorAPI.h>
#include <AzToolsFramework/UI/PropertyEditor/PropertyVectorCtrl.hxx>
#include <AzCore/Math/Matrix3x3.h>

namespace PhysX
{
    namespace Editor
    {
        static const AZ::Crc32 InertiaHandler = AZ_CRC("RigidBodyInertia", 0x3091b106);

        class InertiaPropertyHandler
            : public QObject
            , public AzToolsFramework::PropertyHandler<AZ::Matrix3x3, AzToolsFramework::PropertyVectorCtrl>
        {
            Q_OBJECT //AUTOMOC
        public:
            AZ_CLASS_ALLOCATOR(InertiaPropertyHandler, AZ::SystemAllocator, 0);

            AZ::u32 GetHandlerName(void) const override;
            QWidget* CreateGUI(QWidget* parent) override;
            void ConsumeAttribute(AzToolsFramework::PropertyVectorCtrl* GUI, AZ::u32 attrib,
                AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName) override;
            void WriteGUIValuesIntoProperty(size_t index, AzToolsFramework::PropertyVectorCtrl* GUI,
                AZ::Matrix3x3& instance, AzToolsFramework::InstanceDataNode* node) override;
            bool ReadValuesIntoGUI(size_t index, AzToolsFramework::PropertyVectorCtrl* GUI,
                const AZ::Matrix3x3& instance, AzToolsFramework::InstanceDataNode* node) override;
        };
    } // namespace Editor
} // namespace PhysX
