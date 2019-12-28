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
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/Slice/SliceAsset.h>
#include <AzCore/Slice/SliceComponent.h>
#include <AzTest/AzTest.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/Application/ToolsApplication.h>
#include <AzToolsFramework/Entity/EditorEntityTransformBus.h>
#include <AzToolsFramework/UI/PropertyEditor/PropertyEditorAPI.h>
#include <AzToolsFramework/Viewport/ActionBus.h>
#include <AzCore/UnitTest/TestTypes.h>

#include <ostream>

AZ_PUSH_DISABLE_WARNING(4127, "-Wunknown-warning-option") // warning suppressed: constant used in conditional expression
#include <QtTest/QtTest>
AZ_POP_DISABLE_WARNING

namespace AZ
{
    class Entity;
    class EntityId;
    class Vector3;
    class Quaternion;

    std::ostream& operator<<(std::ostream& os, const Vector3& vec);
    std::ostream& operator<<(std::ostream& os, const Quaternion& quat);

} // namespace AZ

namespace UnitTest
{
    AZ_PUSH_DISABLE_WARNING(4100,"-Wno-unused-parameter")
    // matcher to make tests easier to read and failures more useful (more information is included in the output)
    MATCHER_P(IsClose, v, "") { return arg.IsClose(v); }
    AZ_POP_DISABLE_WARNING

    /// Base fixture for ToolsApplication editor tests.
    class ToolsApplicationFixture
        : public AllocatorsTestFixture
    {
    public:
        void SetUp() override final
        {
            m_app.Start(AzFramework::Application::Descriptor());
            SetUpEditorFixtureImpl();
        }

        void TearDown() override final
        {
            TearDownEditorFixtureImpl();
            m_app.Stop();
        }

        virtual void SetUpEditorFixtureImpl() {}
        virtual void TearDownEditorFixtureImpl() {}

        AzToolsFramework::ToolsApplication m_app;
    };

    /// Test widget to store QActions generated by EditorTransformComponentSelection.
    class TestWidget
        : public QWidget
    {
    public:
        TestWidget()
            : QWidget()
        {
            // ensure TestWidget can intercept and filter any incoming events itself
            installEventFilter(this);
        }

        bool eventFilter(QObject* watched, QEvent* event) override;
    };

    class TestEditorActions
        : private AzToolsFramework::EditorActionRequestBus::Handler
    {
    public:
        void Connect()
        {
            AzToolsFramework::EditorActionRequestBus::Handler::BusConnect();
            m_testWidget.setFocus();
        }

        void Disconnect()
        {
            AzToolsFramework::EditorActionRequestBus::Handler::BusDisconnect();
        }

    private:
        // EditorActionRequestBus ...
        void AddActionViaBus(int id, QAction* action) override;
        void RemoveActionViaBus(QAction* action) override;
        void EnableDefaultActions() override {}
        void DisableDefaultActions() override {}
        void AttachOverride(QWidget* object) override { AZ_UNUSED(object); }
        void DetachOverride() override {}

    public:
        TestWidget m_testWidget;
    };

    class EditorEntityComponentChangeDetector
        : private AzToolsFramework::PropertyEditorEntityChangeNotificationBus::Handler
        , private AzToolsFramework::EditorTransformChangeNotificationBus::Handler
        , private AzToolsFramework::ToolsApplicationNotificationBus::Handler
    {
    public:
        explicit EditorEntityComponentChangeDetector(const AZ::EntityId entityId);
        ~EditorEntityComponentChangeDetector();

        bool ChangeDetected() const { return !m_componentIds.empty(); }
        bool PropertyDisplayInvalidated() const { return m_propertyDisplayInvalidated; }

        AZStd::vector<AZ::ComponentId> m_componentIds;

    private:
        // PropertyEditorEntityChangeNotificationBus ...
        void OnEntityComponentPropertyChanged(AZ::ComponentId componentId) override;

        // EditorTransformChangeNotificationBus ...
        void OnEntityTransformChanged(const AzToolsFramework::EntityIdList& entityIds) override;

        // ToolsApplicationNotificationBus ...
        void InvalidatePropertyDisplay(AzToolsFramework::PropertyModificationRefreshLevel level) override;

        bool m_propertyDisplayInvalidated = false;
    };

    /// Create an Entity as it would appear in the Editor.
    /// Optional second parameter of Entity pointer if required.
    AZ::EntityId CreateDefaultEditorEntity(const char* name, AZ::Entity** outEntity = nullptr);

    using SliceAssets = AZStd::unordered_map<AZ::Data::AssetId, AZ::Data::Asset<AZ::SliceAsset>>;

    /// This function transfers the ownership of all the entity pointers - do not delete or use them afterwards.
    AZ::Data::AssetId SaveAsSlice(
        AZStd::vector<AZ::Entity*> entities, AzToolsFramework::ToolsApplication* toolsApplication,
        SliceAssets& inoutSliceAssets);

    /// Instantiate the entities from the saved slice asset.
    AZ::SliceComponent::EntityList InstantiateSlice(
        AZ::Data::AssetId sliceAssetId, const SliceAssets& sliceAssets);

    /// Destroy all the created slice assets.
    void DestroySlices(SliceAssets& sliceAssets);

} // namespace UnitTest
