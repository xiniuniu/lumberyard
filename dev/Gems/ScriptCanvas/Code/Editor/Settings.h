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

#include <AzCore/Math/Uuid.h>
#include <AzCore/UserSettings/UserSettings.h>

// qdatastream.h(173): warning C4251: 'QDataStream::d': class 'QScopedPointer<QDataStreamPrivate,QScopedPointerDeleter<T>>' needs to have dll-interface to be used by clients of class 'QDataStream'
// qwidget.h(858): warning C4800: 'uint': forcing value to bool 'true' or 'false' (performance warning)
AZ_PUSH_DISABLE_WARNING(4251 4800, "-Wunknown-warning-option")
#include <QByteArray>
#include <QMainWindow>
AZ_POP_DISABLE_WARNING

#include <GraphCanvas/Editor/AssetEditorBus.h>
#include <GraphCanvas/Types/ConstructPresets.h>

namespace AZ
{
    class ReflectContext;
}

namespace ScriptCanvasEditor
{
    namespace EditorSettings
    {
        class ScriptCanvasConstructPresets
            : public GraphCanvas::EditorConstructPresets
        {
        public:
            AZ_RTTI(ScriptCanvasConstructPresets, "{191DCCB3-670F-4243-813E-DF23BE838F45}", GraphCanvas::EditorConstructPresets);
            AZ_CLASS_ALLOCATOR(ScriptCanvasConstructPresets, AZ::SystemAllocator, 0);

            ScriptCanvasConstructPresets();
            ~ScriptCanvasConstructPresets() override = default;

            void InitializeConstructType(GraphCanvas::ConstructType constructType);            
        };

        class EditorWorkspace
            : public AZ::UserSettings
        {
        public:
            AZ_RTTI(EditorWorkspace, "{67DACC4D-B92C-4B5A-8884-6AF7C7B74246}", AZ::UserSettings);
            AZ_CLASS_ALLOCATOR(EditorWorkspace, AZ::SystemAllocator, 0);

            static void Reflect(AZ::ReflectContext* context);

            EditorWorkspace() = default;

            void ConfigureActiveAssets(AZ::Data::AssetId focusedAssetId, const AZStd::vector< AZ::Data::AssetId >& activeAssetIds);
            
            AZ::Data::AssetId GetFocusedAssetId() const;
            AZStd::vector< AZ::Data::AssetId > GetActiveAssetIds() const;

            void Init(const QByteArray& windowState, const QByteArray& windowGeometry);
            void Restore(QMainWindow* window);            

        private:

            const AZStd::vector<AZ::u8>& GetWindowState();

            AZStd::vector<AZ::u8> m_storedWindowState;
            AZStd::vector<AZ::u8> m_windowGeometry;
            AZStd::vector<AZ::u8> m_windowState;

            AZ::Data::AssetId m_focusedAssetId;
            AZStd::vector< AZ::Data::AssetId > m_activeAssetIds;

        };

        // Structure used for Toggleable Configurations
        // i.e. something that has a configuration time and the ability to turn it on/off
        class ToggleableConfiguration
        {
        public:
            AZ_RTTI(ToggleableConfiguration, "{24E8CAE7-0B5E-4B5E-94CC-08B9148B4AB5}");
            AZ_CLASS_ALLOCATOR(ToggleableConfiguration, AZ::SystemAllocator, 0);

            ToggleableConfiguration()
                : ToggleableConfiguration(false, 1000)
            {

            }

            ToggleableConfiguration(bool enabled, int timeMS)
                : m_enabled(enabled)
                , m_timeMS(timeMS)
            {

            }

            virtual ~ToggleableConfiguration() = default;

            bool m_enabled;
            int m_timeMS;
        };

        class ShakeToDespliceSettings
        {
            friend class ScriptCanvasEditorSettings;
        public:
            AZ_RTTI(ShakeToDespliceSettings, "{6401FA20-7A17-407E-81E3-D1389C9C70B7}");
            AZ_CLASS_ALLOCATOR(ShakeToDespliceSettings, AZ::SystemAllocator, 0);

            ShakeToDespliceSettings()
                : m_enabled(true)
                , m_shakeCount(3)
                , m_maximumShakeTimeMS(1000)
                , m_minimumShakeLengthPercent(3)
                , m_deadZonePercent(1)
                , m_straightnessPercent(65)
            {
            }

            virtual ~ShakeToDespliceSettings() = default;

            float GetStraightnessPercent() const
            {
                return m_straightnessPercent * 0.01f;
            }

            float GetMinimumShakeLengthPercent() const
            {
                return m_minimumShakeLengthPercent * 0.01f;
            }

            float GetDeadZonePercent() const
            {
                return m_deadZonePercent * 0.01f;
            }

            bool m_enabled;

            int m_shakeCount;
            int m_maximumShakeTimeMS;

        private:

            float m_minimumShakeLengthPercent;
            float m_deadZonePercent;
            
            float m_straightnessPercent;
        };

        class ZoomSettings
        {
            friend class ScriptCanvasEditorSettings;
        public:
            AZ_RTTI(ZoomSettings, "{276D3E97-B38C-4A3D-A484-E5A5D0A2D942}");
            AZ_CLASS_ALLOCATOR(ZoomSettings, AZ::SystemAllocator, 0);

            ZoomSettings()
                : m_zoomInSetting(2.0f)
            {

            }

            virtual ~ZoomSettings() = default;

            float GetMaxZoom() const
            {
                return 1.0f * m_zoomInSetting;
            }

        private:

            float m_zoomInSetting;
        };

        class EdgePanningSettings
        {
            friend class ScriptCanvasEditorSettings;
        public:
            AZ_RTTI(EdgePanningSettings, "{38399A9B-8D4B-4198-AAA2-D1E8761F5563}");
            AZ_CLASS_ALLOCATOR(EdgePanningSettings, AZ::SystemAllocator, 0);

            EdgePanningSettings()
                : m_edgeScrollPercent(5.0f)
                , m_edgeScrollSpeed(75.0f)
            {
            }

            virtual ~EdgePanningSettings() = default;

            float GetEdgeScrollPercent() const
            {
                return m_edgeScrollPercent * 0.01f;
            }

            float GetEdgeScrollSpeed() const
            {
                return m_edgeScrollSpeed;
            }

        private:

            float m_edgeScrollPercent;
            float m_edgeScrollSpeed;
        };

        class StylingSettings
        {
        public:
            AZ_RTTI(StylingSettings, "{2814140B-0679-492F-BE37-F89DA1414E67}");
            AZ_CLASS_ALLOCATOR(StylingSettings, AZ::SystemAllocator, 0);

            static void Reflect(AZ::ReflectContext* reflectContext);

            StylingSettings() = default;
            virtual ~StylingSettings() = default;

            GraphCanvas::Styling::ConnectionCurveType GetConnectionCurveType() const
            {
                return m_connectionCurveType;
            }

            GraphCanvas::Styling::ConnectionCurveType GetDataConnectionCurveType() const
            {
                return m_dataConnectionCurveType;
            }

        private:

            GraphCanvas::Styling::ConnectionCurveType m_connectionCurveType = GraphCanvas::Styling::ConnectionCurveType::Straight;
            GraphCanvas::Styling::ConnectionCurveType m_dataConnectionCurveType = GraphCanvas::Styling::ConnectionCurveType::Straight;
        };

        class ScriptCanvasEditorSettings
            : public AZ::UserSettings
            , public GraphCanvas::AssetEditorPresetNotificationBus::Handler
        {
        public:
            AZ_RTTI(ScriptCanvasEditorSettings, "{D8D5453C-BFB8-4C71-BBAF-0F10FDD69B3F}", AZ::UserSettings);
            AZ_CLASS_ALLOCATOR(ScriptCanvasEditorSettings, AZ::SystemAllocator, 0);

            static void Reflect(AZ::ReflectContext* context);
            static bool VersionConverter(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement);

            ScriptCanvasEditorSettings();

            // GraphCanvas::AssetEditorPResetNotifications
            void OnConstructPresetsChanged(GraphCanvas::ConstructType constructType) override;
            ////

            double m_snapDistance;

            bool m_showPreviewMessage;
            bool m_showExcludedNodes; //! During preview we're excluding some behavior context nodes that may not work perfectly in Script Canvas.

            bool m_allowBookmarkViewpointControl;
            bool m_allowNodeNudgingOnSplice;

            bool m_rememberOpenCanvases;

            ToggleableConfiguration m_dragNodeCouplingConfig;
            ToggleableConfiguration m_dragNodeSplicingConfig;

            ToggleableConfiguration m_dropNodeSplicingConfig;  

            ToggleableConfiguration m_autoSaveConfig;

            ShakeToDespliceSettings m_shakeDespliceConfig;

            ZoomSettings            m_zoomSettings;
            EdgePanningSettings     m_edgePanningSettings;

            AZStd::unordered_set<AZ::Uuid> m_pinnedDataTypes;

            ScriptCanvasConstructPresets  m_constructPresets;

            int m_variablePanelSorting;

            bool m_showValidationWarnings;
            bool m_showValidationErrors;

            AZ::u32 m_alignmentTimeMS;

            StylingSettings m_stylingSettings;
        };
    }
}