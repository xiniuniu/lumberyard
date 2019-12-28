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

#include <AzCore/Component/Component.h>
#include <AzCore/Math/Uuid.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/containers/vector.h>
#include <AzToolsFramework/API/EntityCompositionRequestBus.h>
#include <AzQtComponents/Components/Widgets/Card.h>
#include <AzToolsFramework/ComponentMode/EditorComponentModeBus.h>

#include <QFrame>
#include <QIcon>

class QVBoxLayout;

namespace AzQtComponents
{
    class CardHeader;
    class CardNotification;
}

namespace AZ
{
    class Component;
    class SerializeContext;
}

namespace AzToolsFramework
{
    class ComponentEditorHeader;
    class IPropertyEditorNotify;
    class ReflectedPropertyEditor;
    enum PropertyModificationRefreshLevel : int;

    /**
     * Widget for editing an AZ::Component (or multiple components of the same type).
     */
    class ComponentEditor
        : public AzQtComponents::Card
    {
        Q_OBJECT;
    public:
        explicit ComponentEditor(
            AZ::SerializeContext* context, IPropertyEditorNotify* notifyTarget = nullptr, QWidget* parent = nullptr);
        ~ComponentEditor();

        void AddInstance(AZ::Component* componentInstance, AZ::Component* aggregateInstance, AZ::Component* compareInstance);
        void ClearInstances(bool invalidateImmediately);

        void AddNotifications();
        void ClearNotifications();

        void InvalidateAll(const char* filter = nullptr);
        void QueuePropertyEditorInvalidation(PropertyModificationRefreshLevel refreshLevel);
        void CancelQueuedRefresh();
        void PreventRefresh(bool shouldPrevent);
        void contextMenuEvent(QContextMenuEvent *event) override;

        void UpdateExpandability();
        void SetExpanded(bool expanded);
        bool IsExpanded() const;
        bool IsExpandable() const;

        void SetSelected(bool selected);
        bool IsSelected() const;

        void SetDragged(bool dragged);
        bool IsDragged() const;

        void SetDropTarget(bool dropTarget);
        bool IsDropTarget() const;

        bool HasComponentWithId(AZ::ComponentId componentId);

        ComponentEditorHeader* GetHeader() const;
        ReflectedPropertyEditor* GetPropertyEditor();
        AZStd::vector<AZ::Component*>& GetComponents();
        const AZStd::vector<AZ::Component*>& GetComponents() const;

        const AZ::Uuid& GetComponentType() const { return m_componentType; }

        void SetComponentOverridden(const bool overridden);

        // Calls match EditorComponentModeNotificationBus - called from EntityPropertyEditor
        void EnteredComponentMode(const AZStd::vector<AZ::Uuid>& componentModeTypes);
        void LeftComponentMode(const AZStd::vector<AZ::Uuid>& componentModeTypes);
        void ActiveComponentModeChanged(const AZ::Uuid& componentType);

    Q_SIGNALS:
        void OnExpansionContractionDone();
        void OnDisplayComponentEditorMenu(const QPoint& position);
        void OnRequestRemoveComponents(const AZStd::vector<AZ::Component*>& components);
        void OnRequestDisableComponents(const AZStd::vector<AZ::Component*>& components);
        void OnRequestRequiredComponents(const QPoint& position, const QSize& size, const AZStd::vector<AZ::ComponentServiceType>& services);
        void OnRequestSelectionChange(const QPoint& position);

    private:
        /// Set up header for this component type.
        void SetComponentType(const AZ::Component& componentInstance);

        /// Clear header of anything specific to component type.
        void InvalidateComponentType();

        /// Update tooltip for the component's header, describing dependencies and other metadata.
        QString BuildHeaderTooltip();

        void OnExpanderChanged(bool expanded);
        void OnContextMenuClicked(const QPoint& position);

        AzQtComponents::CardNotification* CreateNotification(const QString& message);
        AzQtComponents::CardNotification* CreateNotificationForConflictingComponents(const QString& message, const AZ::Entity::ComponentArrayType& conflictingComponents);
        AzQtComponents::CardNotification* CreateNotificationForMissingComponents(const QString& message, const AZStd::vector<AZ::ComponentServiceType>& services);

        bool AreAnyComponentsDisabled() const;
        AzToolsFramework::EntityCompositionRequests::PendingComponentInfo GetPendingComponentInfoForAllComponents() const;
        AzToolsFramework::EntityCompositionRequests::PendingComponentInfo GetPendingComponentInfoForAllComponentsInReverse() const;
        QIcon m_warningIcon;

        ReflectedPropertyEditor* m_propertyEditor = nullptr;
        QVBoxLayout* m_mainLayout = nullptr;

        AZ::SerializeContext* m_serializeContext;

        /// Type of component being shown
        AZ::Uuid m_componentType = AZ::Uuid::CreateNull();

        AZStd::vector<AZ::Component*> m_components;
        AZ::Crc32 m_savedKeySeed;
    };

} // namespace AzToolsFramework
