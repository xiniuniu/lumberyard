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

#include "LyShine_precompiled.h"
#include <platform_impl.h>

#include "LyShineModule.h"
#include "LyShineSystemComponent.h"

#include "UiCanvasComponent.h"
#include "UiElementComponent.h"
#include "UiTransform2dComponent.h"
#include "UiImageComponent.h"
#include "UiImageSequenceComponent.h"
#include "UiTextComponent.h"
#include "UiButtonComponent.h"
#include "UiMarkupButtonComponent.h"
#include "UiCheckboxComponent.h"
#include "UiDraggableComponent.h"
#include "UiDropTargetComponent.h"
#include "UiDropdownComponent.h"
#include "UiDropdownOptionComponent.h"
#include "UiSliderComponent.h"
#include "UiTextInputComponent.h"
#include "UiScrollBarComponent.h"
#include "UiScrollBoxComponent.h"
#include "UiFaderComponent.h"
#include "UiFlipbookAnimationComponent.h"
#include "UiLayoutFitterComponent.h"
#include "UiMaskComponent.h"
#include "UiLayoutCellComponent.h"
#include "UiLayoutColumnComponent.h"
#include "UiLayoutRowComponent.h"
#include "UiLayoutGridComponent.h"
#include "UiParticleEmitterComponent.h"
#include "UiRadioButtonComponent.h"
#include "UiRadioButtonGroupComponent.h"
#include "UiTooltipComponent.h"
#include "UiTooltipDisplayComponent.h"
#include "UiDynamicLayoutComponent.h"
#include "UiDynamicScrollBoxComponent.h"
#include "UiSpawnerComponent.h"

#include "World/UiCanvasAssetRefComponent.h"
#include "World/UiCanvasProxyRefComponent.h"
#include "World/UiCanvasOnMeshComponent.h"

#if defined (LYSHINE_EDITOR)
#   include "Pipeline/LyShineBuilder/LyShineBuilderComponent.h"
#endif // LYSHINE_EDITOR

namespace LyShine
{
    LyShineModule::LyShineModule()
        : CryHooksModule()
    {
        // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
        m_descriptors.insert(m_descriptors.end(), {
                LyShineSystemComponent::CreateDescriptor(),
                UiCanvasAssetRefComponent::CreateDescriptor(),
                UiCanvasProxyRefComponent::CreateDescriptor(),
                UiCanvasOnMeshComponent::CreateDescriptor(),
                UiCanvasComponent::CreateDescriptor(),
                UiElementComponent::CreateDescriptor(),
                UiTransform2dComponent::CreateDescriptor(),
                UiImageComponent::CreateDescriptor(),
                UiImageSequenceComponent::CreateDescriptor(),
                UiTextComponent::CreateDescriptor(),
                UiButtonComponent::CreateDescriptor(),
                UiMarkupButtonComponent::CreateDescriptor(),
                UiCheckboxComponent::CreateDescriptor(),
                UiDraggableComponent::CreateDescriptor(),
                UiDropTargetComponent::CreateDescriptor(),
                UiDropdownComponent::CreateDescriptor(),
                UiDropdownOptionComponent::CreateDescriptor(),
                UiSliderComponent::CreateDescriptor(),
                UiTextInputComponent::CreateDescriptor(),
                UiScrollBoxComponent::CreateDescriptor(),
                UiScrollBarComponent::CreateDescriptor(),
                UiFaderComponent::CreateDescriptor(),
                UiFlipbookAnimationComponent::CreateDescriptor(),
                UiLayoutFitterComponent::CreateDescriptor(),
                UiMaskComponent::CreateDescriptor(),
                UiLayoutCellComponent::CreateDescriptor(),
                UiLayoutColumnComponent::CreateDescriptor(),
                UiLayoutRowComponent::CreateDescriptor(),
                UiLayoutGridComponent::CreateDescriptor(),
                UiTooltipComponent::CreateDescriptor(),
                UiTooltipDisplayComponent::CreateDescriptor(),
                UiDynamicLayoutComponent::CreateDescriptor(),
                UiDynamicScrollBoxComponent::CreateDescriptor(),
                UiSpawnerComponent::CreateDescriptor(),
                UiRadioButtonComponent::CreateDescriptor(),
                UiRadioButtonGroupComponent::CreateDescriptor(),
                UiParticleEmitterComponent::CreateDescriptor(),
    #if defined(LYSHINE_EDITOR)
                // Builder
                LyShineBuilder::LyShineBuilderComponent::CreateDescriptor(),
    #endif
            });

        // This is so the metrics system knows which component LyShine is registering
        LyShineSystemComponent::SetLyShineComponentDescriptors(&m_descriptors);
    }

    /**
     * Add required SystemComponents to the SystemEntity.
     */
    AZ::ComponentTypeList LyShineModule::GetRequiredSystemComponents() const
    {
        return AZ::ComponentTypeList{
                   azrtti_typeid<LyShineSystemComponent>(),
        };
    }
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(LyShine_0fefab3f13364722b2eab3b96ce2bf20, LyShine::LyShineModule)
