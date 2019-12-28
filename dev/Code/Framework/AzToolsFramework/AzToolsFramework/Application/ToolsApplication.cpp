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

#include "StdAfx.h"

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/Debug/Profiler.h>

#include <AzFramework/TargetManagement/TargetManagementComponent.h>
#include <AzFramework/StringFunc/StringFunc.h>

#include <AzToolsFramework/AzToolsFrameworkModule.h>
#include <AzToolsFramework/Undo/UndoSystem.h>
#include <AzToolsFramework/Application/ToolsApplication.h>
#include <AzToolsFramework/Commands/EntityStateCommand.h>
#include <AzToolsFramework/Commands/SelectionCommand.h>
#include <AzToolsFramework/UI/PropertyEditor/PropertyManagerComponent.h>
#include <AzToolsFramework/ToolsComponents/EditorAssetMimeDataContainer.h>
#include <AzToolsFramework/ToolsComponents/ComponentAssetMimeDataContainer.h>
#include <AzToolsFramework/ToolsComponents/TransformComponent.h>
#include <AzToolsFramework/Entity/EditorEntityContextComponent.h>
#include <AzToolsFramework/Entity/EditorEntityInfoBus.h>
#include <AzToolsFramework/Slice/SliceMetadataEntityContextComponent.h>
#include <AzToolsFramework/Component/EditorComponentAPIComponent.h>
#include <AzToolsFramework/Entity/EditorEntityActionComponent.h>
#include <AzToolsFramework/Entity/EditorEntityFixupComponent.h>
#include <AzToolsFramework/SourceControl/SourceControlAPI.h>
#include <AzToolsFramework/SourceControl/PerforceComponent.h>
#include <AzToolsFramework/Archive/ArchiveComponent.h>
#include <AzToolsFramework/Asset/AssetSystemComponent.h>
#include <AzToolsFramework/AssetBrowser/AssetBrowserEntry.h>
#include <AzToolsFramework/ComponentMode/ComponentModeDelegate.h>
#include <AzToolsFramework/AssetBundle/AssetBundleComponent.h>
#include <AzToolsFramework/UI/UICore/QTreeViewStateSaver.hxx>
#include <AzToolsFramework/UI/UICore/QWidgetSavedState.h>
#include <AzToolsFramework/UI/UICore/ProgressShield.hxx>
#include <AzToolsFramework/Metrics/LyEditorMetricsBus.h>
#include <AzToolsFramework/Slice/SliceUtilities.h>
#include <AzToolsFramework/ToolsComponents/EditorInspectorComponent.h>
#include <AzToolsFramework/API/ComponentEntityObjectBus.h>
#include <AzToolsFramework/Entity/EditorEntitySearchComponent.h>
#include <AzToolsFramework/Entity/EditorEntitySortComponent.h>
#include <AzToolsFramework/Entity/EditorEntityModelComponent.h>
#include <AzToolsFramework/Slice/SliceDependencyBrowserComponent.h>
#include <AzToolsFramework/UI/LegacyFramework/MainWindowSavedState.h>
#include <AzToolsFramework/AssetEditor/AssetEditorWidget.h>
#include <AzToolsFramework/Viewport/ViewportMessages.h>
#include <AzToolsFramework/ViewportSelection/EditorInteractionSystemComponent.h>

#include <QtWidgets/QMessageBox>
AZ_PUSH_DISABLE_WARNING(4251, "-Wunknown-warning-option") // 4251: 'QFileInfo::d_ptr': class 'QSharedDataPointer<QFileInfoPrivate>' needs to have dll-interface to be used by clients of class 'QFileInfo'
#include <QDir>
AZ_POP_DISABLE_OVERRIDE_WARNING
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
// Not possible to use AZCore's operator new overrides until we address the overall problems
// with allocators, or more likely convert AzToolsFramework to a DLL and restrict overloading to
// within the DLL. Since this is currently linked as a lib, overriding new and delete would require
// us to hunt down all static allocs in Sandbox and Qt code.
// For now we'll stick with the CRT new/delete in tools.
//#include <AzCore/Memory/NewAndDelete.inl>

namespace AzToolsFramework
{
    namespace Internal
    {
        static const char* s_engineConfigFileName = "engine.json";
        static const char* s_engineConfigExternalEnginePathKey = "ExternalEnginePath";
        static const char* s_engineConfigEngineVersionKey = "LumberyardVersion";

        static const char* s_startupLogWindow = "Startup";

        template<typename IdContainerType>
        void DeleteEntities(const IdContainerType& entityIds)
        {
            AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

            if (entityIds.empty())
            {
                return;
            }

            AzToolsFramework::EditorMetricsEventBusSelectionChangeHelper selectionChangeMetricsHelper;

            UndoSystem::URSequencePoint* currentUndoBatch = nullptr;
            ToolsApplicationRequests::Bus::BroadcastResult(
                currentUndoBatch, &ToolsApplicationRequests::Bus::Events::GetCurrentUndoBatch);

            bool createdUndo = false;
            if (!currentUndoBatch)
            {
                createdUndo = true;
                ToolsApplicationRequests::Bus::BroadcastResult(
                    currentUndoBatch, &ToolsApplicationRequests::Bus::Events::BeginUndoBatch, "Delete Selected");
                AZ_Assert(currentUndoBatch, "Failed to create new undo batch.");
            }

            UndoSystem::UndoStack* undoStack = nullptr;
            PreemptiveUndoCache* preemptiveUndoCache = nullptr;
            EBUS_EVENT_RESULT(undoStack, ToolsApplicationRequests::Bus, GetUndoStack);
            EBUS_EVENT_RESULT(preemptiveUndoCache, ToolsApplicationRequests::Bus, GetUndoCache);
            AZ_Assert(undoStack, "Failed to retrieve undo stack.");
            AZ_Assert(preemptiveUndoCache, "Failed to retrieve preemptive undo cache.");
            if (undoStack && preemptiveUndoCache)
            {
                // In order to undo DeleteSelected, we have to create a selection command which selects the current selection
                // and then add the deletion as children.
                // Commands always execute themselves first and then their children (when going forwards)
                // and do the opposite when going backwards.
                AzToolsFramework::EntityIdList selection;
                EBUS_EVENT_RESULT(selection, ToolsApplicationRequests::Bus, GetSelectedEntities);
                AzToolsFramework::SelectionCommand* selCommand = aznew AzToolsFramework::SelectionCommand(selection, "Delete Entities");

                // We insert a "deselect all" command before we delete the entities. This ensures the delete operations aren't changing
                // selection state, which triggers expensive UI updates. By deselecting up front, we are able to do those expensive
                // UI updates once at the start instead of once for each entity.
                {
                    AzToolsFramework::EntityIdList deselection;
                    AzToolsFramework::SelectionCommand* deselectAllCommand = aznew AzToolsFramework::SelectionCommand(deselection, "Deselect Entities");
                    deselectAllCommand->SetParent(selCommand);
                }

                {
                    AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzToolsFramework, "Internal::DeleteEntities:UndoCaptureAndPurgeEntities");
                    for (const auto& entityId : entityIds)
                    {
                        AZ::Entity* entity = NULL;
                        EBUS_EVENT_RESULT(entity, AZ::ComponentApplicationBus, FindEntity, entityId);

                        if (entity)
                        {
                            EntityDeleteCommand* command = aznew EntityDeleteCommand(static_cast<AZ::u64>(entityId));
                            command->Capture(entity);
                            command->SetParent(selCommand);
                        }

                        preemptiveUndoCache->PurgeCache(entityId);

                        // send metrics event here for delete
                        EBUS_EVENT(AzToolsFramework::EditorMetricsEventsBus, EntityDeleted, entityId);
                    }
                }

                selCommand->SetParent(currentUndoBatch);
                {
                    AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzToolsFramework, "Internal::DeleteEntities:RunRedo");
                    selCommand->RunRedo();
                }
            }

            if (createdUndo)
            {
                EBUS_EVENT(ToolsApplicationRequests::Bus, EndUndoBatch);
            }
        }

        struct ToolsApplicationNotificationBusHandler final
            : public ToolsApplicationNotificationBus::Handler
            , public AZ::BehaviorEBusHandler
        {
            AZ_EBUS_BEHAVIOR_BINDER(ToolsApplicationNotificationBusHandler, "{7EB67956-FF86-461A-91E2-7B08279CFACF}", AZ::SystemAllocator,
                EntityRegistered, EntityDeregistered);

            void EntityRegistered(AZ::EntityId entityId) override
            {
                Call(FN_EntityRegistered, entityId);
            }

            void EntityDeregistered(AZ::EntityId entityId) override
            {
                Call(FN_EntityDeregistered, entityId);
            }
        };

    } // Internal

#define AZ_MAX_ENGINE_VERSION_LEN   64
    // Private Implementation class to manage the engine root and version
    // Note: We are not using any AzCore classes because the ToolsApplication
    // initialization happens early on, before the Allocators get instantiated,
    // so we are using Qt privately instead
    class ToolsApplication::EngineConfigImpl
    {
    private:
        friend class ToolsApplication;

        typedef QMap<QString, QString> EngineJsonMap;

        EngineConfigImpl(const char* logWindow, const char* fileName)
            : m_logWindow(logWindow)
            , m_fileName(fileName)
            , m_isEngineExternal(false)
        {
            m_engineRoot[0] = '\0';
            m_engineVersion[0] = '\0';
        }

        char            m_engineRoot[AZ_MAX_PATH_LEN];
        char            m_engineVersion[AZ_MAX_ENGINE_VERSION_LEN];
        bool            m_isEngineExternal;
        EngineJsonMap   m_engineConfigMap;
        const char*     m_logWindow;
        const char*     m_fileName;


        // Read an engine configuration into a map of key/value pairs
        bool ReadEngineConfigIntoMap(QString engineJsonPath, EngineJsonMap& engineJsonMap)
        {
            QFile engineJsonFile(engineJsonPath);
            if (!engineJsonFile.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                AZ_Warning(m_logWindow, false, "Unable to open file '%s' in the current root directory", engineJsonPath.toUtf8().data());
                return false;
            }

            QByteArray engineJsonData = engineJsonFile.readAll();
            engineJsonFile.close();
            QJsonDocument engineJsonDoc(QJsonDocument::fromJson(engineJsonData));
            if (engineJsonDoc.isNull())
            {
                AZ_Warning(m_logWindow, false, "Unable to read file '%s' in the current root directory", engineJsonPath.toUtf8().data());
                return false;
            }

            QJsonObject engineJsonRoot = engineJsonDoc.object();
            for (const QString& configKey : engineJsonRoot.keys())
            {
                QJsonValue configValue = engineJsonRoot[configKey];
                if (configValue.isString() || configValue.isDouble())
                {
                    // Only map strings and numbers, ignore every other type
                    engineJsonMap[configKey] = configValue.toString();
                }
                else
                {
                    AZ_Warning(m_logWindow, false, "Ignoring key '%s' from '%s', unsupported type.", configKey.toUtf8().data(), engineJsonPath.toUtf8().data());
                }
            }
            return true;
        }

        // Initialize the engine config object based on the current
        bool Initialize(const char* currentAppRoot)
        {
            // Start with the app root as the engine root (legacy), but check to see if the engine root
            // is external to the app root
            azstrncpy(m_engineRoot, AZ_ARRAY_SIZE(m_engineRoot), currentAppRoot, strlen(currentAppRoot) + 1);

            // From the appRoot, check and see if we can read any external engine reference in engine.json
            QString engineJsonFileName = QString(m_fileName);
            QString engineJsonFilePath = QDir(currentAppRoot).absoluteFilePath(engineJsonFileName);

            // From the appRoot, check and see if we can read any external engine reference in engine.json
            if (!QFile::exists(engineJsonFilePath))
            {
                AZ_Warning(m_logWindow, false, "Unable to find '%s' in the current root directory.", m_fileName);
                return false;
            }
            if (!ReadEngineConfigIntoMap(engineJsonFilePath, m_engineConfigMap))
            {
                AZ_Warning(m_logWindow, false, "Defaulting root engine path to '%s'", currentAppRoot);
                return false;
            }

            // Read in the local engine version value
            auto localEngineVersionValue = m_engineConfigMap.find(QString(AzToolsFramework::Internal::s_engineConfigEngineVersionKey));
            QString localEngineVersion(localEngineVersionValue.value());
            azstrncpy(m_engineVersion, AZ_ARRAY_SIZE(m_engineVersion), localEngineVersion.toUtf8().data(), localEngineVersion.length() + 1);

            // If the local engine.json does not have a 'ExternalEnginePath';
            auto externalEnginePathValue = m_engineConfigMap.find(QString(AzToolsFramework::Internal::s_engineConfigExternalEnginePathKey));
            if (externalEnginePathValue == m_engineConfigMap.end())
            {
                // If the local engine.json does not have a 'ExternalEnginePath';
                // This current root is the engine root, no further action needed
                return true;    // not an error, do not print a warning in the caller
            }

            if (localEngineVersionValue == m_engineConfigMap.end())
            {
                AZ_Warning(m_logWindow, false, "Engine version not specified in the engine.json file (%s).", engineJsonFilePath.toUtf8().data());
                return false;
            }

            // Validate the 'ExternalEnginePath' value read from the root engine.json
            QString externalEnginePath(externalEnginePathValue.value());
            QDir::cleanPath(externalEnginePath);
            externalEnginePath = externalEnginePath.trimmed();
            if (externalEnginePath.isEmpty())
            {
                externalEnginePath = currentAppRoot;
            }
            else if (!QDir::isAbsolutePath(externalEnginePath))
            {
                AZStd::string resolvedEnginePath(currentAppRoot);
                resolvedEnginePath.append(externalEnginePath.toUtf8().data());
                AzFramework::StringFunc::Path::Normalize(resolvedEnginePath);
                externalEnginePath = resolvedEnginePath.c_str();
            }

            QDir externalEnginePathDir(externalEnginePath);
            if (!externalEnginePathDir.exists())
            {
                AZ_Warning(m_logWindow,  false, "External engine path '%s' is invalid.", externalEnginePath.toUtf8().data());
                return false;
            }

            QString externalEngineJsonPath = externalEnginePathDir.absoluteFilePath(engineJsonFileName);
            if (!QFile::exists(externalEngineJsonPath))
            {
                AZ_Warning(m_logWindow, false, "Unable to find '%s' in the external engine's base directory (%s).", m_fileName, externalEnginePath.toUtf8().data());
                return false;
            }

            // Read the engine.json from the externalenginepath root
            EngineJsonMap   externalEngineJsonMap;
            if (!ReadEngineConfigIntoMap(externalEngineJsonPath, externalEngineJsonMap))
            {
                AZ_Warning(m_logWindow, false, "Unable to read '%s' in the external engine's base directory (%s).", m_fileName, externalEnginePath.toUtf8().data());
                return false;
            }
            auto externalEngineJsonConfigPathValue = externalEngineJsonMap.find(QString(AzToolsFramework::Internal::s_engineConfigExternalEnginePathKey));
            if (externalEngineJsonConfigPathValue != externalEngineJsonMap.end())
            {
                AZ_Warning(m_logWindow, false, "External engine path '%s' is not an engine root path.  It is marked as an external app path  (you can only reference true engine root paths).", externalEnginePath.toUtf8().data());
                return false;
            }
            auto externalEngineVersionValue = externalEngineJsonMap.find(QString(AzToolsFramework::Internal::s_engineConfigEngineVersionKey));
            if (externalEngineVersionValue == externalEngineJsonMap.end())
            {
                AZ_Warning(m_logWindow, false, "Engine version not specified in the external engine folder's %s file (%s).", m_fileName, externalEngineJsonPath.toUtf8().data());
                return false;
            }

            QString externalEngineVersion(externalEngineVersionValue.value());
            if (localEngineVersion.compare(externalEngineVersion, Qt::CaseInsensitive) != 0)
            {
                // Its okay to continue if there is a version mismatch, but show the warning anyways.  This should also be caught by the build scripts
                AZ_Warning(m_logWindow, false, "The external version (%s) does not match the root engine version (%s).  ", externalEngineVersion.toUtf8().data(), localEngineVersion.toUtf8().data());
            }

            azstrncpy(m_engineRoot, AZ_ARRAY_SIZE(m_engineRoot), externalEnginePath.toUtf8().data(), externalEnginePath.length() + 1);
            m_isEngineExternal = true;
            return true;
        }

        const char* GetEngineRoot() const
        {
            return m_engineRoot;
        }

        const char* GetEngineVersion() const
        {
            return m_engineVersion;
        }

        const bool IsEngineExternal() const
        {
            return m_isEngineExternal;
        }
    };


    ToolsApplication::ToolsApplication(int* argc, char*** argv)
        : AzFramework::Application(argc, argv)
        , m_selectionBounds(AZ::Aabb())
        , m_undoStack(nullptr)
        , m_currentBatchUndo(nullptr)
        , m_isDuringUndoRedo(false)
        , m_isInIsolationMode(false)
    {
        ToolsApplicationRequests::Bus::Handler::BusConnect();
        m_engineConfigImpl.reset(new ToolsApplication::EngineConfigImpl(AzToolsFramework::Internal::s_startupLogWindow,
                                                                        AzToolsFramework::Internal::s_engineConfigFileName));
    }

    ToolsApplication::~ToolsApplication()
    {
        ToolsApplicationRequests::Bus::Handler::BusDisconnect();
        Stop();
    }

    void ToolsApplication::CreateStaticModules(AZStd::vector<AZ::Module*>& outModules)
    {
        AzFramework::Application::CreateStaticModules(outModules);

        outModules.emplace_back(aznew AzToolsFrameworkModule);
    }

    AZ::ComponentTypeList ToolsApplication::GetRequiredSystemComponents() const
    {
        AZ::ComponentTypeList components = AzFramework::Application::GetRequiredSystemComponents();

        components.insert(components.end(), {
                azrtti_typeid<EditorEntityContextComponent>(),
                azrtti_typeid<SliceMetadataEntityContextComponent>(),
                azrtti_typeid<EditorEntityFixupComponent>(),
                azrtti_typeid<Components::EditorComponentAPIComponent>(),
                azrtti_typeid<Components::EditorEntityActionComponent>(),
                azrtti_typeid<Components::PropertyManagerComponent>(),
                azrtti_typeid<AzFramework::TargetManagementComponent>(),
                azrtti_typeid<AssetSystem::AssetSystemComponent>(),
                azrtti_typeid<PerforceComponent>(),
                azrtti_typeid<AzToolsFramework::AssetBundleComponent>(),
                azrtti_typeid<AzToolsFramework::ArchiveComponent>(),
                azrtti_typeid<AzToolsFramework::SliceDependencyBrowserComponent>(),
                azrtti_typeid<Components::EditorEntityModelComponent>(),
                azrtti_typeid<AzToolsFramework::EditorInteractionSystemComponent>(),
                azrtti_typeid<Components::EditorEntitySearchComponent>()
            });

        return components;
    }

    void ToolsApplication::Start(const Descriptor& descriptor, const StartupParameters& startupParameters/* = StartupParameters()*/)
    {
        Application::Start(descriptor, startupParameters);
        InitializeEngineConfig();
    }

    void ToolsApplication::Start(const char* applicationDescriptorFile, const StartupParameters& startupParameters/* = StartupParameters()*/)
    {
        Application::Start(applicationDescriptorFile, startupParameters);
        InitializeEngineConfig();
    }

    void ToolsApplication::InitializeEngineConfig()
    {
        AZ_Assert(m_assetRoot[0] != '\0', "m_assetRoot is not initialized");
        if (!m_engineConfigImpl->Initialize(m_assetRoot))
        {
            AZ_Warning(AzToolsFramework::Internal::s_startupLogWindow, false, "Defaulting engine root path to '%s'", m_assetRoot);
        }
    }



    void ToolsApplication::StartCommon(AZ::Entity* systemEntity)
    {
        Application::StartCommon(systemEntity);

        m_undoStack = new UndoSystem::UndoStack(10, nullptr);
    }

    void ToolsApplication::Stop()
    {
        if (m_isStarted)
        {
            FlushUndo();

            m_undoCache.Clear();

            delete m_undoStack;
            m_undoStack = nullptr;

            // Release any memory used by ToolsApplication before Application::Stop() destroys the allocators.
            m_selectedEntities.set_capacity(0);
            m_highlightedEntities.set_capacity(0);
            m_dirtyEntities = {};

            GetSerializeContext()->DestroyEditContext();

            Application::Stop();
        }
    }

    void ToolsApplication::CreateReflectionManager()
    {
        Application::CreateReflectionManager();

        GetSerializeContext()->CreateEditContext();
    }

    void ToolsApplication::Reflect(AZ::ReflectContext* context)
    {
        Application::Reflect(context);

        Components::EditorComponentBase::Reflect(context);
        EditorAssetMimeDataContainer::Reflect(context);
        ComponentAssetMimeDataContainer::Reflect(context);

        AssetBrowser::AssetBrowserEntry::Reflect(context);
        AssetBrowser::RootAssetBrowserEntry::Reflect(context);
        AssetBrowser::FolderAssetBrowserEntry::Reflect(context);
        AssetBrowser::SourceAssetBrowserEntry::Reflect(context);
        AssetBrowser::ProductAssetBrowserEntry::Reflect(context);

        AssetEditor::AssetEditorWidgetUserSettings::Reflect(context);

        QTreeViewWithStateSaving::Reflect(context);
        QWidgetSavedState::Reflect(context);
        SliceUtilities::Reflect(context);

        ComponentModeFramework::ComponentModeDelegate::Reflect(context);

        ViewportInteraction::ViewportInteractionReflect(context);

        if (auto behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->EBus<ToolsApplicationRequestBus>("ToolsApplicationRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation)
                ->Attribute(AZ::Script::Attributes::Category, "Editor")
                ->Attribute(AZ::Script::Attributes::Module, "editor")
                ->Event("CreateNewEntity", &ToolsApplicationRequests::CreateNewEntity)
                ->Event("CreateNewEntityAtPosition", &ToolsApplicationRequests::CreateNewEntityAtPosition)
                ->Event("DeleteEntityById", &ToolsApplicationRequests::DeleteEntityById)
                ->Event("DeleteEntities", &ToolsApplicationRequests::DeleteEntities)
                ->Event("DeleteEntityAndAllDescendants", &ToolsApplicationRequests::DeleteEntityAndAllDescendants)
                ->Event("DeleteEntitiesAndAllDescendants", &ToolsApplicationRequests::DeleteEntitiesAndAllDescendants)
                ;

            behaviorContext->EBus<ToolsApplicationNotificationBus>("ToolsApplicationNotificationBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation)
                ->Attribute(AZ::Script::Attributes::Category, "Editor")
                ->Attribute(AZ::Script::Attributes::Module, "editor")
                ->Handler<Internal::ToolsApplicationNotificationBusHandler>()
                ->Event("EntityRegistered", &ToolsApplicationEvents::EntityRegistered)
                ->Event("EntityDeregistered", &ToolsApplicationEvents::EntityDeregistered)
                ;
        }
    }

    bool ToolsApplication::AddEntity(AZ::Entity* entity)
    {
        const bool result = AzFramework::Application::AddEntity(entity);

        if (result)
        {
            EBUS_EVENT(ToolsApplicationEvents::Bus, EntityRegistered, entity->GetId());
        }

        return result;
    }

    bool ToolsApplication::RemoveEntity(AZ::Entity* entity)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        m_undoCache.PurgeCache(entity->GetId());

        MarkEntityDeselected(entity->GetId());
        SetEntityHighlighted(entity->GetId(), false);

        EBUS_EVENT(ToolsApplicationEvents::Bus, EntityDeregistered, entity->GetId());

        {
            AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzToolsFramework, "ToolsApplication::RemoveEntity:CallApplicationRemoveEntity");
            if (AzFramework::Application::RemoveEntity(entity))
            {
                return true;
            }
        }

        return false;
    }

    void ToolsApplication::PreExportEntity(AZ::Entity& source, AZ::Entity& target)
    {
        AZ::SerializeContext* serializeContext = nullptr;
        EBUS_EVENT_RESULT(serializeContext, AZ::ComponentApplicationBus, GetSerializeContext);
        AZ_Assert(serializeContext, "No serialization context found");

        const AZ::Entity::ComponentArrayType& editorComponents = source.GetComponents();

        // Propagate components from source entity to target, in preparation for exporting target.
        for (AZ::Component* component : editorComponents)
        {
            Components::EditorComponentBase* asEditorComponent =
                azrtti_cast<Components::EditorComponentBase*>(component);

            if (asEditorComponent)
            {
                const size_t oldComponentCount = target.GetComponents().size();

                asEditorComponent->BuildGameEntity(&target);

                // Applying same Id persistence trick as we do in the slice compiler. Once we're off levels,
                // this code all goes away and everything runs through the slice compiler.
                if (target.GetComponents().size() > oldComponentCount)
                {
                    AZ::Component* newComponent = target.GetComponents().back();
                    AZ_Error("Export", asEditorComponent->GetId() != AZ::InvalidComponentId, "For entity \"%s\", component \"%s\" doesn't have a valid component id",
                             source.GetName().c_str(), asEditorComponent->RTTI_GetType().ToString<AZStd::string>().c_str());
                    newComponent->SetId(asEditorComponent->GetId());
                }
            }
            else
            {
                // The component is already runtime-ready. I.e. it is not an editor component.
                // Clone the component and add it to the export entity
                AZ::Component* clonedComponent = serializeContext->CloneObject(component);
                target.AddComponent(clonedComponent);
            }
        }
    }

    void ToolsApplication::PostExportEntity(AZ::Entity& /*source*/, AZ::Entity& /*target*/)
    {
    }

    const char* ToolsApplication::GetCurrentConfigurationName() const
    {
#if defined(_RELEASE)
        return "ReleaseEditor";
#elif defined(_DEBUG)
        return "DebugEditor";
#else
        return "ProfileEditor";
#endif
    }

    void ToolsApplication::MarkEntitySelected(AZ::EntityId entityId)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);
        AZ_Assert(entityId.IsValid(), "Invalid entity Id being marked as selected.");

        EntityIdList::iterator foundIter = AZStd::find(m_selectedEntities.begin(), m_selectedEntities.end(), entityId);
        if (foundIter == m_selectedEntities.end())
        {
            ToolsApplicationEvents::Bus::Broadcast(&ToolsApplicationEvents::BeforeEntitySelectionChanged);

            m_selectedEntities.push_back(entityId);
            EntitySelectionEvents::Bus::Event(entityId, &EntitySelectionEvents::OnSelected);

            const AzToolsFramework::EntityIdList newlySelectedEntities = { entityId };
            ToolsApplicationEvents::Bus::Broadcast(&ToolsApplicationEvents::AfterEntitySelectionChanged, newlySelectedEntities, AzToolsFramework::EntityIdList());
        }
    }

    void ToolsApplication::MarkEntitiesSelected(const EntityIdList& entitiesToSelect)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        ToolsApplicationEvents::Bus::Broadcast(&ToolsApplicationEvents::BeforeEntitySelectionChanged);

        for (AZ::EntityId entityId : entitiesToSelect)
        {
            AZ_Assert(entityId.IsValid(), "Invalid entity Id being marked as selected.");
            m_selectedEntities.push_back(entityId);
            EntitySelectionEvents::Bus::Event(entityId, &EntitySelectionEvents::OnSelected);
        }

        ToolsApplicationEvents::Bus::Broadcast(&ToolsApplicationEvents::AfterEntitySelectionChanged, entitiesToSelect, EntityIdList());
    }

    void ToolsApplication::MarkEntityDeselected(AZ::EntityId entityId)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);
        auto foundIter = AZStd::find(m_selectedEntities.begin(), m_selectedEntities.end(), entityId);
        if (foundIter != m_selectedEntities.end())
        {
            AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzToolsFramework, "ToolsApplication::MarkEntityDeselected:Deselect");
            ToolsApplicationEvents::Bus::Broadcast(&ToolsApplicationEvents::BeforeEntitySelectionChanged);
            m_selectedEntities.erase(foundIter);

            EntitySelectionEvents::Bus::Event(entityId, &EntitySelectionEvents::OnDeselected);
            AzToolsFramework::EntityIdList newlyDeselectedEntities = { entityId };
            ToolsApplicationEvents::Bus::Broadcast(&ToolsApplicationEvents::AfterEntitySelectionChanged, AzToolsFramework::EntityIdList(), newlyDeselectedEntities);
        }
    }

    void ToolsApplication::MarkEntitiesDeselected(const EntityIdList& entitiesToDeselect)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        ToolsApplicationEvents::Bus::Broadcast(&ToolsApplicationEvents::BeforeEntitySelectionChanged);

        EntityIdSet entitySetToDeselect(entitiesToDeselect.begin(), entitiesToDeselect.end());

        for (auto selectedEntityIt = m_selectedEntities.begin(); selectedEntityIt != m_selectedEntities.end(); )
        {
            auto foundIt = entitySetToDeselect.find(*selectedEntityIt);
            if (foundIt == entitySetToDeselect.end())
            {
                // not trying to deselect this entity, advance iterator
                ++selectedEntityIt;
            }
            else
            {
                EntitySelectionEvents::Bus::Event(*selectedEntityIt, &EntitySelectionEvents::OnDeselected);

                // swap with last element and pop back to avoid moving all vector elements, don't advance iterator
                // the order of m_selectedEntities does not matter
                AZStd::iter_swap(selectedEntityIt, m_selectedEntities.rbegin());
                m_selectedEntities.pop_back();
            }
        }

        ToolsApplicationEvents::Bus::Broadcast(&ToolsApplicationEvents::AfterEntitySelectionChanged, EntityIdList(), entitiesToDeselect);
    }

    void ToolsApplication::SetEntityHighlighted(AZ::EntityId entityId, bool highlighted)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);
        AZ_Assert(entityId.IsValid(), "Attempting to mark an invalid entity as highlighted.");

        auto foundIter = AZStd::find(m_highlightedEntities.begin(), m_highlightedEntities.end(), entityId);
        if (foundIter != m_highlightedEntities.end())
        {
            if (!highlighted)
            {
                AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzToolsFramework, "ToolsApplication::SetEntityHighlighted:RemoveHighlight");
                ToolsApplicationEvents::Bus::Broadcast(&ToolsApplicationEvents::BeforeEntityHighlightingChanged);
                m_highlightedEntities.erase(foundIter);
                ToolsApplicationEvents::Bus::Broadcast(&ToolsApplicationEvents::AfterEntityHighlightingChanged);
            }
        }
        else if (highlighted)
        {
            AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzToolsFramework, "ToolsApplication::SetEntityHighlighted:AddHighlight");
            ToolsApplicationEvents::Bus::Broadcast(&ToolsApplicationEvents::BeforeEntityHighlightingChanged);
            m_highlightedEntities.push_back(entityId);
            ToolsApplicationEvents::Bus::Broadcast(&ToolsApplicationEvents::AfterEntityHighlightingChanged);
        }
    }

    void ToolsApplication::SetSelectedEntities(const EntityIdList& selectedEntities)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        // We're setting the selection set as a batch from an external caller.
        // * Filter out any unselectable entities
        // * Calculate selection/deselection delta so we can notify specific entities only on change.
        // * Filter any duplicates.

        // Filter out any invalid or non-selectable entities
        EntityIdList selectedEntitiesFiltered;
        selectedEntitiesFiltered.reserve(selectedEntities.size());

        // if the new viewport interaction model is enabled we do not want to
        // filter out locked entities as this breaks with the logic of being
        // able to select locked entities in the entity outliner
        if (IsNewViewportInteractionModelEnabled())
        {
            selectedEntitiesFiltered.insert(
                selectedEntitiesFiltered.begin(), selectedEntities.begin(), selectedEntities.end());
        }
        else
        {
            for (AZ::EntityId nowSelectedId : selectedEntities)
            {
                AZ_Assert(nowSelectedId.IsValid(), "Invalid entity Id being marked as selected.");

                selectedEntitiesFiltered.push_back(nowSelectedId);
            }
        }

        EntityIdList newlySelectedIds;
        EntityIdList newlyDeselectedIds;

        // Populate list of newly selected entities
        for (AZ::EntityId nowSelectedId : selectedEntitiesFiltered)
        {
            auto alreadySelectedIter = AZStd::find(m_selectedEntities.begin(), m_selectedEntities.end(), nowSelectedId);

            if (alreadySelectedIter == m_selectedEntities.end())
            {
                newlySelectedIds.push_back(nowSelectedId);
            }
        }

        // Populate list of newly deselected entities
        for (AZ::EntityId currentlySelectedId : m_selectedEntities)
        {
            auto stillSelectedIter = AZStd::find(selectedEntitiesFiltered.begin(), selectedEntitiesFiltered.end(), currentlySelectedId);

            if (stillSelectedIter == selectedEntitiesFiltered.end())
            {
                newlyDeselectedIds.push_back(currentlySelectedId);
            }
        }

        // Apply selection changes in bulk
        if (!newlySelectedIds.empty() || !newlyDeselectedIds.empty())
        {
            ToolsApplicationEvents::Bus::Broadcast(&ToolsApplicationEvents::BeforeEntitySelectionChanged);

            m_selectedEntities.clear();

            // Guarantee no dupes, since external caller is unknown.
            for (AZ::EntityId id : selectedEntitiesFiltered)
            {
                auto foundIter = AZStd::find(m_selectedEntities.begin(), m_selectedEntities.end(), id);
                if (foundIter == m_selectedEntities.end())
                {
                    m_selectedEntities.push_back(id);
                }
            }

            for (AZ::EntityId id : newlySelectedIds)
            {
                EntitySelectionEvents::Bus::Event(id, &EntitySelectionEvents::OnSelected);
            }

            for (AZ::EntityId id : newlyDeselectedIds)
            {
                EntitySelectionEvents::Bus::Event(id, &EntitySelectionEvents::OnDeselected);
            }

            ToolsApplicationEvents::Bus::Broadcast(&ToolsApplicationEvents::AfterEntitySelectionChanged, newlySelectedIds, newlyDeselectedIds);
        }

        // ensure parent expansion happens if necessary, even if selection hasn't changed
        for (AZ::EntityId nowSelectedId : selectedEntities)
        {
            EditorEntityInfoNotificationBus::Broadcast(&EditorEntityInfoNotificationBus::Events::OnEntityInfoUpdatedSelection, nowSelectedId, true);
        }
    }

    EntityIdSet ToolsApplication::GatherEntitiesAndAllDescendents(const EntityIdList& inputEntities)
    {
        EntityIdSet output;
        EntityIdList tempList;
        for (const AZ::EntityId& id : inputEntities)
        {
            output.insert(id);

            tempList.clear();
            EBUS_EVENT_ID_RESULT(tempList, id, AZ::TransformBus, GetAllDescendants);
            output.insert(tempList.begin(), tempList.end());
        }

        return output;
    }

    bool ToolsApplication::IsSelectable(const AZ::EntityId& /*entityId*/)
    {
        return true;
    }

    bool ToolsApplication::IsSelected(const AZ::EntityId& entityId)
    {
        return m_selectedEntities.end() != AZStd::find(m_selectedEntities.begin(), m_selectedEntities.end(), entityId);
    }

    AZ::EntityId ToolsApplication::CreateNewEntity(AZ::EntityId parentId)
    {
        AZ::EntityId createdEntityId;
        EditorRequestBus::BroadcastResult(createdEntityId, &EditorRequests::CreateNewEntity, parentId);

        return createdEntityId;
    }

    AZ::EntityId ToolsApplication::CreateNewEntityAtPosition(const AZ::Vector3& pos, AZ::EntityId parentId)
    {
        AZ::EntityId createdEntityId;
        EditorRequestBus::BroadcastResult(createdEntityId, &EditorRequests::CreateNewEntityAtPosition, pos, parentId);

        return createdEntityId;
    }

    void ToolsApplication::DeleteSelected()
    {
        Internal::DeleteEntities(m_selectedEntities);
    }

    void ToolsApplication::DeleteEntityAndAllDescendants(AZ::EntityId entityId)
    {
        DeleteEntitiesAndAllDescendants({ entityId });
    }

    void ToolsApplication::DeleteEntitiesAndAllDescendants(const EntityIdList& entities)
    {
        const EntityIdSet entitiesAndDescendants = GatherEntitiesAndAllDescendents(entities);
        Internal::DeleteEntities(entitiesAndDescendants);
    }

    bool ToolsApplication::DetachEntities(const AZStd::vector<AZ::EntityId>& entitiesToDetach, AZStd::vector<AZStd::pair<AZ::EntityId, AZ::SliceComponent::EntityRestoreInfo>>& restoreInfos)
    {
        AZStd::vector<AZStd::pair<AZ::Entity*, AZ::SliceComponent::SliceReference*>> pendingSliceChanges;

        AZ::SliceComponent* editorRootSlice = nullptr;
        AzToolsFramework::EditorEntityContextRequestBus::BroadcastResult(editorRootSlice, &AzToolsFramework::EditorEntityContextRequestBus::Events::GetEditorRootSlice);
        AZ_Assert(editorRootSlice, "Failed to retrieve editor root slice.");

        // Gather desired changes without modifying slices or entities 
        for (const AZ::EntityId& entityId : entitiesToDetach)
        {
            AZ::SliceComponent::SliceInstanceAddress sliceAddress(nullptr, nullptr);
            AzFramework::EntityIdContextQueryBus::EventResult(sliceAddress, entityId, &AzFramework::EntityIdContextQueries::GetOwningSlice);

            AZ::SliceComponent::SliceReference* sliceReference = sliceAddress.first;
            AZ::SliceComponent::SliceInstance* sliceInstance = sliceAddress.second;
            if (!sliceReference || !sliceInstance)
            {
                AZ_Error("DetachSliceEntity", false, "Entity with Id %s is not part of a slice. \"Detach\" action cancelled. No slices or entities were modified.", entityId.ToString().c_str());
                return false;
            }

            AZ::Entity* entity = nullptr;
            AZ::ComponentApplicationBus::BroadcastResult(entity, &AZ::ComponentApplicationRequests::FindEntity, entityId);
            AZ_Error("DetachSliceEntity", entity, "Unable to find entity for Entity Id %s. \"Detach\" action cancelled. No slices or entities were modified.", entityId.ToString().c_str());
            if (!entity)
            {
                return false;
            }

            AZ::SliceComponent::EntityRestoreInfo restoreInfo;
            if (editorRootSlice->GetEntityRestoreInfo(entityId, restoreInfo))
            {
                restoreInfos.emplace_back(entityId, restoreInfo);
                pendingSliceChanges.emplace_back(entity, sliceReference);
            }
            else
            {
                AZ_Error("DetachSliceEntity", entity, "Failed to prepare restore information for entity of Id %s. \"Detach\" action cancelled. No slices or entities were modified.", entityId.ToString().c_str());
                return false;
            }
        }

        // Apply pending changes
        for (AZStd::pair<AZ::Entity*, AZ::SliceComponent::SliceReference*>& pendingSliceChange : pendingSliceChanges)
        {
            // Remove entity from current slice instance without deleting the entity. Delete slice instance if the detached entity is the last one 
            // in the slice instance. The slice instance will be reconstructed upon undo.
            bool success = pendingSliceChange.second->GetSliceComponent()->RemoveEntity(pendingSliceChange.first->GetId(), false, true);
            if (success)
            {
                editorRootSlice->AddEntity(pendingSliceChange.first); // Add back as loose entity
            }
            else
            {
                AZ_Error("DetachSliceEntity", success, "Entity with Id %s could not be removed from the slice. The Slice Instance is now in an unknown state, and saving it may result in data loss.", pendingSliceChange.first->GetId().ToString().c_str());
            }
        }

        EditorEntityContextNotificationBus::Broadcast(&EditorEntityContextNotification::OnEditorEntitiesSliceOwnershipChanged, entitiesToDetach);
        return true;
    }

    bool ToolsApplication::FindCommonRoot(const AzToolsFramework::EntityIdSet& entitiesToBeChecked, AZ::EntityId& commonRootEntityId
        , AzToolsFramework::EntityIdList* topLevelEntities)
    {
        // Return value
        bool entitiesHaveCommonRoot = true;

        bool rootFound = false;
        commonRootEntityId.SetInvalid();

        // Finding the common root and top level entities
        for (const AZ::EntityId& id : entitiesToBeChecked)
        {
            AZ_Warning("Slices", AZ::TransformBus::FindFirstHandler(id) != nullptr,
                       "Entity with id %llu is not active, or does not have a transform component. This method is only valid for active entities. with a Transform component",
                       id);

            AZ::EntityId parentId;
            AZ::TransformBus::EventResult(parentId, id, &AZ::TransformBus::Events::GetParentId);

            // If this entity is not a child of entities to be checked
            if (entitiesToBeChecked.find(parentId) == entitiesToBeChecked.end())
            {
                if (topLevelEntities)
                {
                    // Add it to the top level entities
                    topLevelEntities->push_back(id);
                }

                if (rootFound)
                {
                    // If the entities checked until now have a common root and the parent id of this entity is not the same as the common root
                    if (entitiesHaveCommonRoot && (parentId != commonRootEntityId))
                    {
                        // Entities do not have a common root
                        commonRootEntityId.SetInvalid();
                        entitiesHaveCommonRoot = false;
                    }
                }
                else
                {
                    commonRootEntityId = parentId;
                    rootFound = true;
                }
            }
        }

        return entitiesHaveCommonRoot;
    }

    bool ToolsApplication::FindCommonRootInactive(const AzToolsFramework::EntityList& entitiesToBeChecked, AZ::EntityId& commonRootEntityId, AzToolsFramework::EntityList* topLevelEntities)
    {
        // Return value
        bool entitiesHaveCommonRoot = true;

        bool rootFound = false;
        commonRootEntityId.SetInvalid();

        AzToolsFramework::EntityIdSet entityIds;
        for (const auto& entity : entitiesToBeChecked)
        {
            entityIds.insert(entity->GetId());
        }

        // Finding the common root and top level entities
        for (const AZ::Entity* entity : entitiesToBeChecked)
        {
            Components::TransformComponent* transformComponent =
                entity->FindComponent<Components::TransformComponent>();

            if (transformComponent)
            {
                const AZ::EntityId& parentId = transformComponent->GetParentId();
                // If this entity is not a child of entities to be checked
                if (entityIds.find(parentId) == entityIds.end())
                {
                    if (topLevelEntities)
                    {
                        topLevelEntities->push_back(const_cast<AZ::Entity*>(entity));
                    }

                    if (rootFound)
                    {
                        if (entitiesHaveCommonRoot && (parentId != commonRootEntityId))
                        {
                            commonRootEntityId.SetInvalid();
                            entitiesHaveCommonRoot = false;
                        }
                    }
                    else
                    {
                        commonRootEntityId = parentId;
                        rootFound = true;
                    }
                }
            }
        }

        return entitiesHaveCommonRoot;
    }

    void ToolsApplication::FindTopLevelEntityIdsInactive(const EntityIdList& entityIdsToCheck, EntityIdList& topLevelEntityIds)
    {
        for (AZ::EntityId entityId : entityIdsToCheck)
        {
            AZ::Entity* entity = nullptr;
            AZ::ComponentApplicationBus::BroadcastResult(entity, &AZ::ComponentApplicationBus::Events::FindEntity, entityId);
            if (!entity)
            {
                continue;
            }
            Components::TransformComponent* transformComponent = entity->FindComponent<Components::TransformComponent>();
            if (!transformComponent)
            {
                continue;
            }

            bool isTopLevel = true;
            AZ::EntityId parentId = transformComponent->GetParentId();
            while (parentId.IsValid())
            {
                if (AZStd::find(entityIdsToCheck.begin(), entityIdsToCheck.end(), parentId) != entityIdsToCheck.end())
                {
                    isTopLevel = false;
                    break;
                }
                else
                {
                    AZ::Entity* parentEntity = nullptr;
                    AZ::ComponentApplicationBus::BroadcastResult(parentEntity, &AZ::ComponentApplicationBus::Events::FindEntity, parentId);
                    parentId.SetInvalid();
                    if (parentEntity)
                    {
                        transformComponent = parentEntity->FindComponent<Components::TransformComponent>();
                        if (transformComponent)
                        {
                            parentId = transformComponent->GetParentId();
                        }
                    }
                }
            }

            if (isTopLevel)
            {
                topLevelEntityIds.push_back(entityId);
            }
        }
    }

    AZ::SliceComponent::SliceInstanceAddress ToolsApplication::FindCommonSliceInstanceAddress(const EntityIdList& entityIds)
    {
        AZ::SliceComponent::SliceInstanceAddress result;

        if (entityIds.empty())
        {
            return result;
        }

        AzFramework::EntityIdContextQueryBus::EventResult(result, entityIds[0], &AzFramework::EntityIdContextQueryBus::Events::GetOwningSlice);
        for (int index = 1; index < entityIds.size(); index++)
        {   
            AZ::SliceComponent::SliceInstanceAddress sliceAddressTemp;
            AzFramework::EntityIdContextQueryBus::EventResult(sliceAddressTemp, entityIds[index], &AzFramework::EntityIdContextQueryBus::Events::GetOwningSlice);
            if (sliceAddressTemp != result)
            {
                // Entities come from different slice instances.
                return AZ::SliceComponent::SliceInstanceAddress();
            }
        }

        return result;
    }

    AZ::EntityId ToolsApplication::GetRootEntityIdOfSliceInstance(AZ::SliceComponent::SliceInstanceAddress sliceAddress)
    {
        AZ::EntityId result;
        if (!sliceAddress.IsValid())
        {
            return result;
        }

        const AZ::Data::Asset<AZ::SliceAsset>& sliceAsset = sliceAddress.GetReference()->GetSliceAsset();
        AZ::SliceComponent::EntityList sliceAssetEntities;
        sliceAsset.Get()->GetComponent()->GetEntities(sliceAssetEntities);

        AZ::EntityId commonRootEntityId;
        AzToolsFramework::EntityList topLevelEntities;

        FindCommonRootInactive(sliceAssetEntities, commonRootEntityId, &topLevelEntities);
        if (!topLevelEntities.empty())
        {
            const AZ::SliceComponent::EntityIdToEntityIdMap& baseToInstanceEntityIdMap = sliceAddress.GetInstance()->GetEntityIdMap();
            auto foundItr = baseToInstanceEntityIdMap.find(topLevelEntities[0]->GetId());
            if (foundItr != baseToInstanceEntityIdMap.end())
            {
                result = foundItr->second;
            }
        }

        return result;
    }

    bool ToolsApplication::CheckSourceControlConnectionAndRequestEditForFileBlocking(const char* assetPath, const char* progressMessage, const ToolsApplicationRequests::RequestEditProgressCallback& progressCallback)
    {
        AZ::IO::FileIOBase* fileIO = AZ::IO::FileIOBase::GetInstance();
        SourceControlState state = SourceControlState::Disabled;
        SourceControlConnectionRequestBus::BroadcastResult(state, &SourceControlConnectionRequestBus::Events::GetSourceControlState);
        if (state != SourceControlState::Active && (!fileIO || fileIO->IsReadOnly(assetPath)))
        {
            return false;
        }

        return RequestEditForFileBlocking(assetPath, progressMessage, progressCallback);
    }

    bool ToolsApplication::RequestEditForFileBlocking(const char* assetPath, const char* progressMessage, const ToolsApplicationRequests::RequestEditProgressCallback& progressCallback)
    {
        AZ::IO::FileIOBase* fileIO = AZ::IO::FileIOBase::GetInstance();
        if (fileIO && !fileIO->IsReadOnly(assetPath))
        {
            SourceControlCommandBus::Broadcast(&SourceControlCommandBus::Events::RequestEdit, assetPath, true, [](bool, const SourceControlFileInfo&) {});
            return true;
        }

        bool editSuccess = false;
        bool editComplete = false;
        SourceControlCommandBus::Broadcast(&SourceControlCommandBus::Events::RequestEdit, assetPath, true,
            [&editSuccess, &editComplete](bool success, const AzToolsFramework::SourceControlFileInfo& /*info*/)
            {
                editSuccess = success;
                editComplete = true;
            }
        );

        QWidget* mainWindow = nullptr;
        EditorRequests::Bus::BroadcastResult(mainWindow, &EditorRequests::Bus::Events::GetMainWindow);
        AZ_Warning("RequestEdit", mainWindow, "Failed to retrieve main application window.");
        ProgressShield::LegacyShowAndWait(mainWindow, mainWindow ? mainWindow->tr(progressMessage) : QString(progressMessage),
            [&editComplete, &progressCallback](int& current, int& max)
            {
                if (progressCallback)
                {
                    progressCallback(current, max);
                }
                else
                {
                    current = 0;
                    max = 0;
                }
                return editComplete;
            }
        );

        return (fileIO && !fileIO->IsReadOnly(assetPath));
    }

    void ToolsApplication::RequestEditForFile(const char* assetPath, RequestEditResultCallback resultCallback)
    {
        AZ_Error("RequestEdit", resultCallback != 0, "User result callback is required.");

        AZ::IO::FileIOBase* fileIO = AZ::IO::FileIOBase::GetInstance();
        if (fileIO && !fileIO->IsReadOnly(assetPath))
        {
            if (resultCallback)
            {
                resultCallback(true);
            }
            return;
        }

        SourceControlCommandBus::Broadcast(&SourceControlCommandBus::Events::RequestEdit, assetPath, true,
            [resultCallback](bool success, const AzToolsFramework::SourceControlFileInfo& info)
            {
                if (resultCallback)
                {
                    success = !info.IsReadOnly();
                    resultCallback(success);
                }
            }
        );
    }

    void ToolsApplication::CheckSourceControlConnectionAndRequestEditForFile(const char* assetPath, RequestEditResultCallback resultCallback)
    {
        AZ::IO::FileIOBase* fileIO = AZ::IO::FileIOBase::GetInstance();
        SourceControlState state = SourceControlState::Disabled;
        SourceControlConnectionRequestBus::BroadcastResult(state, &SourceControlConnectionRequestBus::Events::GetSourceControlState);
        if (state != SourceControlState::Active && (!fileIO || fileIO->IsReadOnly(assetPath)))
        {
            resultCallback(false);
            return;
        }

        RequestEditForFile(assetPath, resultCallback);
    }

    void ToolsApplication::DeleteEntityById(AZ::EntityId entityId)
    {
        DeleteEntities({ entityId });
    }

    void ToolsApplication::DeleteEntities(const EntityIdList& entities)
    {
        Internal::DeleteEntities(entities);
    }

    void ToolsApplication::AddDirtyEntity(AZ::EntityId entityId)
    {
        // If we're already in undo redo, we don't want the user to have to check for this each time.
        if (m_isDuringUndoRedo)
        {
            return;
        }

        m_dirtyEntities.insert(entityId);

        // Check if this dirty entity is in a layer by walking up its parenting hierarchy.
        // If it's in a layer, mark that layer as having unsaved changes.
        do
        {
            bool isLayerEntity = false;
            AzToolsFramework::Layers::EditorLayerComponentRequestBus::EventResult(
                isLayerEntity,
                entityId,
                &AzToolsFramework::Layers::EditorLayerComponentRequestBus::Events::HasLayer);
            if (isLayerEntity)
            {
                AzToolsFramework::Layers::EditorLayerComponentRequestBus::Event(
                    entityId,
                    &AzToolsFramework::Layers::EditorLayerComponentRequestBus::Events::MarkLayerWithUnsavedChanges);
                break;
            }
            AZ::EntityId parentId;
            AZ::TransformBus::EventResult(
                parentId,
                entityId,
                &AZ::TransformBus::Events::GetParentId);
            if (entityId == parentId)
            {
                // Stop when the first parent layer is found.
                // If this layer is nested in another layer, only the most immediate
                // layer should be marked as dirty. Layer hierarchy and parenting is
                // mostly for visual organization, entities only exist in one layer on disk.
                break;
            }
            entityId = parentId;
        } while (entityId.IsValid());
    }

    int ToolsApplication::RemoveDirtyEntity(AZ::EntityId entityId)
    {
        return static_cast<int>(m_dirtyEntities.erase(entityId));
    }

    void ToolsApplication::UndoPressed()
    {
        if (m_undoStack)
        {
            if (m_undoStack->CanUndo())
            {
                EditorMetricsEventBusSelectionChangeHelper selectionChangeMetricsHelper;

                m_isDuringUndoRedo = true;
                EBUS_EVENT(ToolsApplicationEvents::Bus, BeforeUndoRedo);
                m_undoStack->Undo();
                EBUS_EVENT(ToolsApplicationEvents::Bus, AfterUndoRedo);
                m_isDuringUndoRedo = false;

#if defined(ENABLE_UNDOCACHE_CONSISTENCY_CHECKS)
                ConsistencyCheckUndoCache();
#endif
            }
        }
    }

    void ToolsApplication::RedoPressed()
    {
        if (m_undoStack)
        {
            if (m_undoStack->CanRedo())
            {
                EditorMetricsEventBusSelectionChangeHelper selectionChangeMetricsHelper;

                m_isDuringUndoRedo = true;
                EBUS_EVENT(ToolsApplicationEvents::Bus, BeforeUndoRedo);
                m_undoStack->Redo();
                EBUS_EVENT(ToolsApplicationEvents::Bus, AfterUndoRedo);
                m_isDuringUndoRedo = false;

#if defined(ENABLE_UNDOCACHE_CONSISTENCY_CHECKS)
                ConsistencyCheckUndoCache();
#endif
            }
        }
    }

    void ToolsApplication::FlushUndo()
    {
        if (m_undoStack)
        {
            m_undoStack->Reset();
        }

        if (m_currentBatchUndo)
        {
            delete m_currentBatchUndo;
            m_currentBatchUndo = nullptr;
        }

        m_dirtyEntities.clear();
        m_isDuringUndoRedo = false;
    }

    void ToolsApplication::FlushRedo()
    {
        if (m_undoStack)
        {
            m_undoStack->Slice();
        }
    }

    UndoSystem::URSequencePoint* ToolsApplication::BeginUndoBatch(const char* label)
    {
        if (!m_currentBatchUndo)
        {
            m_currentBatchUndo = aznew UndoSystem::BatchCommand(label, 0);

            // notify Cry undo has started (SandboxIntegrationManager)
            // Only do this at the root level. OnEndUndo will be called at the root
            // level when EndUndoBatch is called.
            EBUS_EVENT(ToolsApplicationEvents::Bus, OnBeginUndo, label);
        }
        else
        {
            UndoSystem::URSequencePoint* current = m_currentBatchUndo;

            m_currentBatchUndo = aznew UndoSystem::BatchCommand(label, 0);
            m_currentBatchUndo->SetParent(current);
        }

        return m_currentBatchUndo;
    }

    UndoSystem::URSequencePoint* ToolsApplication::ResumeUndoBatch(UndoSystem::URSequencePoint* expected, const char* label)
    {
        if (m_currentBatchUndo)
        {
            if (m_undoStack->GetTop() == m_currentBatchUndo)
            {
                m_undoStack->PopTop();
            }

            return m_currentBatchUndo;
        }

        if (m_undoStack)
        {
            const auto ptr = m_undoStack->GetTop();
            if (ptr && ptr == expected)
            {
                m_currentBatchUndo = ptr;
                m_undoStack->PopTop();

                return m_currentBatchUndo;
            }
        }

        return BeginUndoBatch(label);
    }

    /**
     * Iterate over all sequence points for this undo batch, checking in each
     * case if it changed or not - if no change was recorded, discard this undo.
     */
    static bool DidSequencePointChange(const UndoSystem::URSequencePoint* root)
    {
        bool changed = false;
        const UndoSystem::URSequencePoint::ChildVec& children = root->GetChildren();
        if (!children.empty())
        {
            for (const UndoSystem::URSequencePoint* child : children)
            {
                changed = DidSequencePointChange(child);
                if (changed)
                {
                    break;
                }
            }
        }

        return root->Changed() || changed;
    }

    void ToolsApplication::EndUndoBatch()
    {
        AZ_Assert(m_currentBatchUndo, "Cannot end batch - no batch current");

        if (m_currentBatchUndo->GetParent())
        {
            // pop one up
            m_currentBatchUndo = m_currentBatchUndo->GetParent();
        }
        else
        {
            // we're at the root

            // only undo at bottom of scope (first invoked ScopedUndoBatch in
            // chain/hierarchy must go out of scope)
            CreateUndosForDirtyEntities();
            m_dirtyEntities.clear();

            // check if any change was recorded during this undo
            bool changed = DidSequencePointChange(m_currentBatchUndo);

            // notify Cry undo has ended (SandboxIntegrationManager)
            ToolsApplicationEvents::Bus::Broadcast(
                &ToolsApplicationEvents::Bus::Events::OnEndUndo, m_currentBatchUndo->GetName().c_str(), changed);

            // record each undo batch
            if (m_undoStack && changed)
            {
                m_undoStack->Post(m_currentBatchUndo);
            }
            else
            {
                delete m_currentBatchUndo;
            }

#if defined(ENABLE_UNDOCACHE_CONSISTENCY_CHECKS)
            ConsistencyCheckUndoCache();
#endif
            m_currentBatchUndo = nullptr;
        }
    }

    void ToolsApplication::CreateUndosForDirtyEntities()
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        AZ_Assert(!m_isDuringUndoRedo, "Cannot add dirty entities during undo/redo.");
        if (m_dirtyEntities.empty())
        {
            return;
        }

        // If the current undo batch has commands in it, then we have to check that we do not add duplicates
        // However if it starts out empty, we can just add things straight from the Set to the undo batch
        bool mustCheckDuplicates = !m_currentBatchUndo->GetChildren().empty();

        for (AZ::EntityId entityId : m_dirtyEntities)
        {
            AZ::Entity* entity = nullptr;
            EBUS_EVENT_RESULT(entity, AZ::ComponentApplicationBus, FindEntity, entityId);

            if (entity)
            {
                EntityStateCommand* state = nullptr;

                if (mustCheckDuplicates)
                {
                    // Check if this entity is already in the current undo batch
                    state = azdynamic_cast<EntityStateCommand*>(m_currentBatchUndo->Find(
                        static_cast<AZ::u64>(entityId), AZ::AzTypeInfo<EntityStateCommand>::Uuid()));
                }

                if (!state)
                {
                    state = aznew EntityStateCommand(static_cast<AZ::u64>(entityId));
                    state->SetParent(m_currentBatchUndo);

                    // capture initial state of entity (before undo)
                    state->Capture(entity, true);
                }

                // capture last state of entity (after undo) - for redo
                state->Capture(entity, false);
            }

            m_undoCache.UpdateCache(entityId);
        }
    }

    void ToolsApplication::ConsistencyCheckUndoCache()
    {
        for (auto && entityEntry : m_entities)
        {
            m_undoCache.Validate((entityEntry.second)->GetId());
        }
    }

    bool ToolsApplication::IsEntityEditable(AZ::EntityId entityId)
    {
        (void)entityId;

        return true;
    }

    bool ToolsApplication::AreEntitiesEditable(const EntityIdList& entityIds)
    {
        for (AZ::EntityId entityId : entityIds)
        {
            if (!IsEntityEditable(entityId))
            {
                return false;
            }
        }

        return true;
    }

    void ToolsApplication::CheckoutPressed()
    {
    }

    SourceControlFileInfo ToolsApplication::GetSceneSourceControlInfo()
    {
        return SourceControlFileInfo();
    }

    void ToolsApplication::EnterEditorIsolationMode()
    {
        if (!m_isInIsolationMode && m_selectedEntities.size() > 0)
        {
            m_isInIsolationMode = true;

            m_isolatedEntityIdSet.insert(m_selectedEntities.begin(), m_selectedEntities.end());

            // Add also entities in between selected entities along the transform hierarchy chain.
            // For example A has child B and B has child C, and if A and C are isolated, B is too.
            AZStd::vector<AZ::EntityId> inbetweenEntityIds;

            for (const AZ::EntityId entityId : m_selectedEntities)
            {
                bool addInbetweenEntityIds = false;
                inbetweenEntityIds.clear();

                AZ::EntityId parentEntityId;
                AZ::TransformBus::EventResult(parentEntityId, entityId, &AZ::TransformBus::Events::GetParentId);

                while (parentEntityId.IsValid())
                {
                    const EntityIdSet::iterator found = m_isolatedEntityIdSet.find(parentEntityId);
                    if (found != m_isolatedEntityIdSet.end())
                    {
                        addInbetweenEntityIds = true;
                        break;
                    }
                    inbetweenEntityIds.push_back(parentEntityId);

                    const AZ::EntityId currentParentId = parentEntityId;
                    parentEntityId.SetInvalid();
                    AZ::TransformBus::EventResult(parentEntityId, currentParentId, &AZ::TransformBus::Events::GetParentId);
                }

                if (addInbetweenEntityIds)
                {
                    m_isolatedEntityIdSet.insert(inbetweenEntityIds.begin(), inbetweenEntityIds.end());
                }
            }


            for (const AZ::EntityId entityId : m_isolatedEntityIdSet)
            {
                ComponentEntityEditorRequestBus::Event(entityId, &ComponentEntityEditorRequestBus::Events::SetSandBoxObjectIsolated, true);
            }

            ToolsApplicationNotificationBus::Broadcast(&ToolsApplicationNotificationBus::Events::OnEnterEditorIsolationMode);
        }
    }

    void ToolsApplication::ExitEditorIsolationMode()
    {
        if (m_isInIsolationMode)
        {
            m_isInIsolationMode = false;

            for (const AZ::EntityId entityId : m_isolatedEntityIdSet)
            {
                ComponentEntityEditorRequestBus::Event(entityId, &ComponentEntityEditorRequestBus::Events::SetSandBoxObjectIsolated, false);
            }

            m_isolatedEntityIdSet.clear();

            ToolsApplicationNotificationBus::Broadcast(&ToolsApplicationNotificationBus::Events::OnExitEditorIsolationMode);
        }
    }

    bool ToolsApplication::IsEditorInIsolationMode()
    {
        return m_isInIsolationMode;
    }

    const char* ToolsApplication::GetEngineRootPath() const
    {
        return m_engineConfigImpl->GetEngineRoot();
    }

    const char* ToolsApplication::GetEngineVersion() const
    {
        return m_engineConfigImpl->GetEngineVersion();
    }

    bool ToolsApplication::IsEngineRootExternal() const
    {
        return m_engineConfigImpl->IsEngineExternal();
    }

    void ToolsApplication::CreateAndAddEntityFromComponentTags(const AZStd::vector<AZ::Crc32>& requiredTags, const char* entityName)
    {
        if (!entityName || !entityName[0])
        {
            entityName = "ToolsApplication Entity";
        }

        AZ::SerializeContext* context = GetSerializeContext();
        AZ_Assert(context, "Unable to retrieve serialize context. Ensure the application is initialized before attempting to add components by tag.");

        AZ::Entity* entity = aznew AZ::Entity(entityName);
        AZStd::unordered_set<AZ::Uuid> componentsToAddToEntity;
        AZ::Edit::GetComponentUuidsWithSystemComponentTag(context, requiredTags, componentsToAddToEntity);

        for (const AZ::Uuid& typeId : componentsToAddToEntity)
        {
            entity->CreateComponent(typeId);
        }
        entity->Init();
        entity->Activate();
    }

    void ToolsApplication::RunRedoSeparately(UndoSystem::URSequencePoint* redoCommand)
    {
        if (redoCommand)
        {
            m_isDuringUndoRedo = true;
            redoCommand->RunRedo();
            m_isDuringUndoRedo = false;
        }
    }

    AzToolsFramework::ToolsApplicationRequests::ResolveToolPathOutcome ToolsApplication::ResolveConfigToolsPath(const char* toolApplicationName) const
    {
        if (!GetExecutableFolder() || !GetEngineRoot())
        {
            return AZ::Failure(AZStd::string("Cannot use ToolsApplicationRequets::ResolveConfigToolsPath until the component application has finished starting up."));
        }

        AZStd::vector<AZStd::string> toolTargetSearchPaths;

        // Add the current executable's folder to search paths
        {
            AZStd::string localAppPath;
            if (AzFramework::StringFunc::Path::ConstructFull(GetExecutableFolder(), toolApplicationName, localAppPath, true))
            {
                toolTargetSearchPaths.push_back(localAppPath);
            }
        }

        // Add the engine root's Tools/LmbrSetup folder to search paths
        {
#if defined (AZ_PLATFORM_WINDOWS)
            static const char* toolSubFolder = "Tools/LmbrSetup/Win";
#elif AZ_TRAIT_OS_PLATFORM_APPLE
            static const char* toolSubFolder = "Tools/LmbrSetup/Mac";
#else
#error Unsupported Platform for tools
#endif

            AZStd::string engineToolsAppPath;
            AZStd::string setupToolsFolder;
            if (AzFramework::StringFunc::Path::Join(GetEngineRoot(), toolSubFolder, setupToolsFolder))
            {
                if (AzFramework::StringFunc::Path::ConstructFull(setupToolsFolder.c_str(), toolApplicationName, engineToolsAppPath, true))
                {
                    toolTargetSearchPaths.push_back(engineToolsAppPath);
                }
            }
        }

        for (const AZStd::string& searchAbsPath : toolTargetSearchPaths)
        {
            if (AZ::IO::SystemFile::Exists(searchAbsPath.c_str()))
            {
                return AZ::Success(AZStd::string(searchAbsPath));
            }
        }

        return AZ::Failure(AZStd::string::format("Unable to resolve tool application path for '%s'", toolApplicationName));
    }
} // namespace AzToolsFramework
