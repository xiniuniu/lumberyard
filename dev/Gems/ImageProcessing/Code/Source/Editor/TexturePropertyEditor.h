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

#include <QDialog>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzQtComponents/Components/StyledDialog.h>
#include <AzToolsFramework/SourceControl/SourceControlAPI.h>

#include <Source/Editor/EditorCommon.h>
#include <Source/Editor/TexturePresetSelectionWidget.h>
#include <Source/Editor/TexturePreviewWidget.h>
#include <Source/Editor/ResolutionSettingWidget.h>
#include <Source/Editor/MipmapSettingWidget.h>

namespace Ui
{
    class TexturePropertyEditor;
}

namespace ImageProcessingEditor
{
    class TexturePropertyEditor
        : public AzQtComponents::StyledDialog
        , protected EditorInternalNotificationBus::Handler
    {
        Q_OBJECT
    public:

        AZ_CLASS_ALLOCATOR(TexturePropertyEditor, AZ::SystemAllocator, 0);
        explicit TexturePropertyEditor(const AZ::Uuid& sourceTextureId, QWidget* parent = nullptr);
        ~TexturePropertyEditor();

        bool HasValidImage();

    protected:
        void OnSave();
        void OnHelp();

        ////////////////////////////////////////////////////////////////////////
        //EditorInternalNotificationBus
        void OnEditorSettingsChanged(bool needRefresh, const AZStd::string& platform);
        ////////////////////////////////////////////////////////////////////////

        bool event(QEvent* event) override;

    private:
        QScopedPointer<Ui::TexturePropertyEditor> m_ui;
        QScopedPointer<TexturePreviewWidget> m_previewWidget;
        QScopedPointer<TexturePresetSelectionWidget> m_presetSelectionWidget;
        QScopedPointer<ResolutionSettingWidget> m_resolutionSettingWidget;
        QScopedPointer<MipmapSettingWidget> m_mipmapSettingWidget;

        EditorTextureSetting m_textureSetting;
        bool m_validImage = true;

        void SaveTextureSetting(AZStd::string outputPath);
        void DeleteLegacySetting();

    };
} //namespace ImageProcessingEditor

