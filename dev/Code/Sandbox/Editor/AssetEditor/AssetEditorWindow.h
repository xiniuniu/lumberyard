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

#include <AzCore/Asset/AssetCommon.h>
#include <AzToolsFramework/AssetEditor/AssetEditorBus.h>
#include <QWidget>

namespace Ui
{
    class AssetEditorWindowClass;
}

/**
* Window pane wrapper for the Asset Editor widget.
*/
class AssetEditorWindow
    : public QWidget
    , AzToolsFramework::AssetEditor::AssetEditorWidgetRequestsBus::Handler
{
    Q_OBJECT

public:
    AZ_CLASS_ALLOCATOR(AssetEditorWindow, AZ::SystemAllocator, 0);

    explicit AssetEditorWindow(QWidget* parent = nullptr);
    ~AssetEditorWindow() override;

    //////////////////////////////////////////////////////////////////////////
    // AssetEditorWindow
    //////////////////////////////////////////////////////////////////////////
    void CreateAsset(const AZ::Data::AssetType& assetType) override;
    void OpenAsset(const AZ::Data::Asset<AZ::Data::AssetData>& asset) override;

    static void RegisterViewClass();

protected Q_SLOTS:
    void OnAssetSaveFailed(const AZStd::string& error) const;
    void OnAssetOpened(const AZ::Data::Asset<AZ::Data::AssetData>& asset);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    QScopedPointer<Ui::AssetEditorWindowClass> m_ui;
};
