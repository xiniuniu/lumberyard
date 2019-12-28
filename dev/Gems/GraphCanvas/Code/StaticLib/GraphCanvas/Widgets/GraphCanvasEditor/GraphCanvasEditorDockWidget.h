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

#include <qdockwidget.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/EBus/Ebus.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

#include <GraphCanvas/Editor/AssetEditorBus.h>
#include <GraphCanvas/Editor/EditorDockWidgetBus.h>

namespace Ui
{
    class GraphCanvasEditorDockWidget;
}

namespace GraphCanvas
{
    class GraphCanvasGraphicsView;

    class EditorDockWidget
        : public QDockWidget
        , public ActiveEditorDockWidgetRequestBus::Handler
        , public AssetEditorNotificationBus::Handler
        , public EditorDockWidgetRequestBus::Handler
    {
        Q_OBJECT

        friend class AssetEditorCentralDockWindow;
    public:    
        AZ_CLASS_ALLOCATOR(EditorDockWidget,AZ::SystemAllocator,0);
        EditorDockWidget(const EditorId& editorId, QWidget* parent);
        ~EditorDockWidget();
        
        // ActiveEditorDockWidgetRequestBus
        DockWidgetId GetDockWidgetId() const override;

        void ReleaseBus() override;
        ////

        const EditorId& GetEditorId() const;
        
        // GraphCanvasEditorDockWidgetBus
        AZ::EntityId GetViewId() const override;
        GraphCanvas::GraphId GetGraphId() const override;

        EditorDockWidget* AsEditorDockWidget() override;
        ////        
        
    signals:
        void OnEditorClosed(EditorDockWidget*);

    protected:        

        GraphCanvasGraphicsView* GetGraphicsView() const;
        
    private:

        void closeEvent(QCloseEvent* closeEvent) override;
        void SignalActiveEditor();

        DockWidgetId m_dockWidgetId;;
        EditorId     m_editorId;

        AZ::EntityId m_assetId;

        AZStd::unique_ptr<Ui::GraphCanvasEditorDockWidget> m_ui;

        int windowId = 0;
    };
}