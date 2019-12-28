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

#ifndef EDITORFRAMEWORKAPPLICATION_H
#define EDITORFRAMEWORKAPPLICATION_H

#include <AzCore/base.h>
#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/UserSettings/UserSettingsProvider.h>
#include "EditorFrameworkAPI.h"

#pragma once

namespace AZ
{
    class SerializeContext;
}

namespace LegacyFramework
{
    struct ApplicationDesc
    {
        HMODULE m_applicationModule;  // only necessary if you want to attach your application as a DLL plugin to another application, hosting it
        bool m_enableGUI; // false if you want none of the QT or GUI functionality to exist.  You cannot use project manager if you do this.
        bool m_enableGridmate; // false if you want to not activate the network communications module.
        bool m_enablePerforce; // false if you want to not activate perforce SCM integration.  note that this will eventually become a plugin anyway
        bool m_enableProjectManager; // false if you want to disable project management.  No project path will be set and the project picker GUI will not appear.
        bool m_shouldRunAssetProcessor; // false if you want to disable auto launching the asset processor.
        bool m_saveUserSettings; // true by default - set it to false if you want not to store usersettings (ie, you have no per-user state because you're something like an asset builder!)

        int m_argc;
        char** m_argv;

        char m_applicationName[_MAX_PATH];

        ApplicationDesc(const char* name = "Application", int argc = 0, char** argv = nullptr);
        ApplicationDesc(const ApplicationDesc& other);
        ApplicationDesc& operator=(const ApplicationDesc& other);
    private:
    };


    class Application
        : public AZ::ComponentApplication
        , protected FrameworkApplicationMessages::Handler
        , protected CoreMessageBus::Handler
    {
        /// Create application, if systemEntityFileName is NULL, we will create with default settings.
    public:

        using CoreMessageBus::Handler::Run;
        virtual int Run(const ApplicationDesc& desc);
        Application();
        virtual ~Application();

    protected:

        // ------------------------------------------------------------------
        // implementation of FrameworkApplicationMessages::Handler
        virtual bool IsRunningInGUIMode() { return m_desc.m_enableGUI; }
        virtual bool RequiresGameProject() { return m_desc.m_enableProjectManager; }
        virtual bool ShouldRunAssetProcessor() { return m_desc.m_shouldRunAssetProcessor; }
        virtual HMODULE GetMainModule();
        virtual const char* GetApplicationName();
        virtual const char* GetApplicationModule();
        virtual const char* GetApplicationDirectory();
        virtual const AzFramework::CommandLine* GetCommandLineParser();
        virtual void TeardownApplicationComponent();
        virtual void RunAssetProcessor() override;
        const char* GetAppRoot() override;
        // ------------------------------------------------------------------

        // ------------------------------------------------------------------
        // implementation of CoreMessageBus::Handler
        virtual void OnProjectSet(const char*  /*pathToProject*/);
        // ------------------------------------------------------------------


        AZ::Entity* Create(const char* systemEntityFileName, const StartupParameters& startupParameters = StartupParameters()) override;
        virtual void Destroy();
        /**
         * Before we reflect for serialization make sure we have the serialize context
         * have EditContext as we are editor and we will need it
         */
        virtual void ReflectSerialize();

        // This is called during the bootstrap and makes all the components we should have for SYSTEM minimal functionality.
        // This happens BEFORE the project is ready.  Think carefully before you add additional system components.  Examples of system components
        // are memory managers, crash reporters, log writers, and the project manager itself which lets you switch to a project.
        virtual void CreateSystemComponents();

        // once the project is ready, then the CreateApplicationComponents() function is called.  Those components are guarinteed
        // to run inside a the context of a "Project" and thus have access to project data such as assets.
        virtual void CreateApplicationComponents();

        // called after the application entity is completed to pass some specifics to generic components.
        virtual void OnApplicationEntityActivated();

        // you must call EnsureComponentCreated and EnsureComponentRemoved functions inside the above function create functions:
        // returns TRUE if the component already existed, FALSE if it had to create one.
        // adds it as a SYSTEM component if its called inside CreateSystemComponents
        // adds it as an APPLICATION component if its called inside CreateApplicationComponents
        // will ERROR if you remove an application component after the application
        bool EnsureComponentCreated(AZ::Uuid componentCRC);

        // returns TRUE if the component existed, FALSE if the component did not exist.
        // will ERROR if you remove a system component after the system is booted
        bool EnsureComponentRemoved(AZ::Uuid componentCRC);

        /**
         * A "convenient" function to place all high level (no other place to put) deprecated reflections.
         * IMPORTANT: Please to time stamp each deprecation so we can remove after some time (we know all
         * old data had been converted)
         */
        virtual void ReflectSerializeDeprecated();

        /**
         * This is the function that will be called instantly after the memory
         * manager is created. This is where we should register all core component
         * factories that will participate in the loading of the bootstrap file
         * or all factories in general.
         * When you create your own application this is where you should FIRST call
         * ComponentApplication::RegisterCoreComponents and then register the application
         * specific core components.
         */
        virtual void RegisterCoreComponents();
        AZ::Entity* m_ptrSystemEntity;

        virtual int GetDesiredExitCode() override { return m_desiredExitCode; }
        virtual void SetDesiredExitCode(int code) override { m_desiredExitCode = code; }
        virtual bool GetAbortRequested() override { return m_abortRequested; }
        virtual void SetAbortRequested() override { m_abortRequested = true; }
        virtual AZStd::string GetApplicationGlobalStoragePath() override;
        virtual bool IsMaster() override { return m_isMaster; }

        virtual bool IsAppConfigWritable() override;

        AZ::Entity* m_applicationEntity;

    private:
        void CreateApplicationComponent();
        void OnApplicationEntityLoaded(void* classPtr, const AZ::Uuid& classId, const AZ::SerializeContext* sc);
        void SaveApplicationEntity();

        char m_applicationModule[_MAX_PATH];
        char m_appRoot[AZ_MAX_PATH_LEN];
        int m_desiredExitCode;
        bool m_isMaster;
        volatile bool m_abortRequested; // if you CTRL+C in a console app, this becomes true.  its up to you to check...
        char m_applicationFilePath[_MAX_PATH];
        ApplicationDesc m_desc;
        AzFramework::CommandLine* m_ptrCommandLineParser;
    };
}

#endif // EDITORFRAMEWORKAPPLICATION_H