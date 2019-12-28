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

#include <AzCore/EBus/EBus.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/std/parallel/mutex.h>
#include <AzCore/std/string/osstring.h>

namespace AZ
{
    class ComponentApplication;
    class ComponentDescriptor;

    class Entity;
    class EntityId;

    class Module;
    class DynamicModuleHandle;

    class Component;

    class SerializeContext;
    class BehaviorContext;
    class JsonRegistrationContext;

    namespace Internal
    {
        class ComponentFactoryInterface;
    }

    namespace Debug
    {
        class DrillerManager;
    }

    /**
     * @deprecated Use EntitySystemBus
     * Event bus for dispatching component application events to listeners.
     */
    class AZ_DEPRECATED(, "The ComponentApplicationEventBus has been deprecated and will be removed in a future release. Please use the EntitySystemBus instead.")
        ComponentApplicationEvents
        : public AZ::EBusTraits
    {
    public:

        /**
         * @deprecated Use EntitySystemBus
         * Notifies listeners that an entity was added to the application.
         * @param entity The entity that was added to the application.
         */
        AZ_DEPRECATED(, "The ComponentApplicationEventBus has been deprecated and will be removed in a future release. Please use the EntitySystemBus instead.")
        virtual void OnEntityAdded(AZ::Entity* entity) { (void)entity; };

        /**
         * @deprecated Use EntitySystemBus
         * Notifies listeners that an entity was removed from the application.
         * @param entity The entity that was removed from the application.
         */
        AZ_DEPRECATED(, "The ComponentApplicationEventBus has been deprecated and will be removed in a future release. Please use the EntitySystemBus instead.")
        virtual void OnEntityRemoved(const AZ::EntityId& entityId) { (void)entityId; };
    };

    /**
     * @deprecated Use EntitySystemBus
     * Used when dispatching a component application event. 
     */
    DEPRECATE_EBUS(ComponentApplicationEvents, ComponentApplicationEventBus, "The ComponentApplicationEventsBus has been deprecated in favor of using the EntitySystemBus in Lumberyard release 1.18");

    /**
     * Event bus that components use to make requests of the main application.
     * Only one application can exist at a time, which is why this bus 
     * supports only one listener.
     */
    class ComponentApplicationRequests
        : public AZ::EBusTraits
    {
    public:

        /**
         * Destroys the event bus that components use to make requests of the main application.
         */
        virtual ~ComponentApplicationRequests() {}

        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides - application is a singleton
        /**
         * Overrides the default AZ::EBusTraits handler policy to allow one
         * listener only, because only one application can exist at a time.
         */
        static const AZ::EBusHandlerPolicy HandlerPolicy = EBusHandlerPolicy::Single;  // We sort components on m_initOrder.
        /**
         * Overrides the default AZ::EBusTraits mutex type to the AZStd implementation of 
         * a recursive mutex with exclusive ownership semantics. A mutex prevents multiple 
         * threads from accessing shared data simultaneously.
         */
        typedef AZStd::recursive_mutex MutexType;
        //////////////////////////////////////////////////////////////////////////
        /**
         * Registers a component descriptor with the application.
         * @param descriptor A component descriptor.
         */
        virtual void RegisterComponentDescriptor(const ComponentDescriptor* descriptor) = 0;
        /**
         * Unregisters a component descriptor with the application.
         * @param descriptor A component descriptor.
         */
        virtual void UnregisterComponentDescriptor(const ComponentDescriptor* descriptor) = 0;
        /**
         * Gets a pointer to the application.
         * @return A pointer to the application.
         */
        virtual ComponentApplication*   GetApplication() = 0;

        /**
         * Adds an entity to the application's registry.
         * Calling Init() on an entity automatically performs this operation.
         * @param entity A pointer to the entity to add to the application's registry.
         * @return True if the operation succeeded. False if the operation failed.
         */
        virtual bool                    AddEntity(Entity* entity) = 0;
        /**
         * Removes the specified entity from the application's registry.
         * Deleting an entity automatically performs this operation.
         * @param entity A pointer to the entity that will be removed from the application's registry.
         * @return True if the operation succeeded. False if the operation failed.
         */
        virtual bool                    RemoveEntity(Entity* entity) = 0;
        /**
         * Unregisters and deletes the specified entity.
         * @param entity A reference to the entity that will be unregistered and deleted.
         * @return True if the operation succeeded. False if the operation failed.
         */
        virtual bool                    DeleteEntity(const EntityId& id) = 0;
        /**
         * Returns the entity with the matching ID, if the entity is registered with the application.
         * @param entity A reference to the entity that you are searching for.
         * @return A pointer to the entity with the specified entity ID.
         */
        virtual Entity*                 FindEntity(const EntityId& id) = 0;
        /**
         * Returns the name of the entity that has the specified entity ID.
         * Entity names are not unique.
         * This method exists to facilitate better debugging messages.
         * @param entity A reference to the entity whose name you are seeking.
         * @return The name of the entity with the specified entity ID. 
         * If no entity is found for the specified ID, it returns an empty string. 
         */
        virtual AZStd::string           GetEntityName(const EntityId& id) { (void)id; return AZStd::string(); };

        /**
         * The type that AZ::ComponentApplicationRequests::EnumerateEntities uses to
         * pass entity callbacks to the application for enumeration.
         */
        using EntityCallback = AZStd::function<void(Entity*)>;
        /**
         * Enumerates all registered entities and invokes the specified callback for each entity.
         * @param callback A reference to the callback that is invoked for each entity.
         */
        virtual void                    EnumerateEntities(const EntityCallback& callback) = 0;
        /**
         * Returns the serialize context that was registered with the app.
         * @return The serialize context, if there is one. SerializeContext is a class that contains reflection data
         * for serialization and construction of objects.
         */
        virtual class SerializeContext* GetSerializeContext() = 0;
        /**
         * Returns the behavior context that was registered with the app.
         * @return The behavior context, if there is one. BehaviorContext is a class that reflects classes, methods,
         * and EBuses for runtime interaction.
         */
        virtual class BehaviorContext*  GetBehaviorContext() = 0;
        /**
         * Returns the Json Registration context that was registered with the app.
         * @return The Json Registration context, if there is one. JsonRegistrationContext is a class that contains
         * the serializers used by the best-effort json serialization.
         */
        virtual class JsonRegistrationContext* GetJsonRegistrationContext() = 0;
        /**
         * Gets the name of the working root folder that was registered with the app.
         * @return A pointer to the name of the app's root folder, if a root folder was registered.
         */
        virtual const char*             GetAppRoot() = 0;
        /**
         * Gets the path to the directory that contains the application's executable.
         * @return A pointer to the name of the path that contains the application's executable.
         */
        virtual const char*             GetExecutableFolder() const = 0;

        /**
        * Gets the Bin folder name where the application is running from. The folder is relative to the engine root.
        * @return A pointer to the bin folder name.
        */
        virtual const char*             GetBinFolder() const = 0;


        /**
         * Returns a pointer to the driller manager, if driller is enabled.
         * The driller manager manages all active driller sessions and driller factories.
         * @return A pointer to the driller manager. If driller is not enabled,
         * this function returns null.
         */
        virtual Debug::DrillerManager*  GetDrillerManager() = 0;

        /**
         * ResolveModulePath is called whenever LoadDynamicModule wants to resolve a module in order to actually load it.
         * You can override this if you need to load modules from a different path or hijack module loading in some other way.
         * If you do, ensure that you use platform-specific conventions to do so, as this is called by multiple platforms.
         * The default implantation prepends the path to the executable to the module path, but you can override this behavior
         * (Call the base class if you want this behavior to persist in overrides)
         */
        virtual void ResolveModulePath(AZ::OSString& /*modulePath*/) { }
    };

    /**
     * Used by components to make requests of the component application.
     */
    typedef AZ::EBus<ComponentApplicationRequests>  ComponentApplicationBus;
}
