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

#include <AzCore/Module/DynamicModuleHandle.h>
#include <AzCore/Module/Environment.h>

namespace AZ
{
    DynamicModuleHandle::DynamicModuleHandle(const char* fileFullName)
        : m_fileName(fileFullName)
    {
    }

    bool DynamicModuleHandle::Load(bool isInitializeFunctionRequired)
    {
        LoadStatus status = LoadModule();
        switch (status) 
        {
            case LoadStatus::LoadFailure:
                return false;
            case LoadStatus::AlreadyLoaded: // intentional fall-through.  This means its already loaded by the operating system.
            case LoadStatus::LoadSuccess:
                break;
        }

        // Call module's initialize function.
        auto initFunc = GetFunction<InitializeDynamicModuleFunction>(InitializeDynamicModuleFunctionName);
        if (initFunc)
        {
            // this module may have already been loaded by the operating system, but we still need to init it ourself if it has not
            // been initialized in this context.  Has it been initialized in THIS context?
            // in order not to collide with other environment variable names, we'll pick one that is likely to be unique to this
            // usage of it by prepending "module_init_token: " to the file name.
            AZ::OSString variableName = AZ::OSString::format("module_init_token: %s", m_fileName.c_str());
            m_initialized = AZ::Environment::FindVariable<bool>(variableName.c_str());

            // note here we are not checking the value of m_initialized, we're checking whether the variable exists or not
            // the first one to create the variable also owns it, so we only care about its existence.
            if (!m_initialized) 
            {
                // if we get here, it means nobody has initialized this module yet.  We will initialize it and create the variable.
                m_initialized = AZ::Environment::CreateVariable<bool>(variableName.c_str());
                initFunc(AZ::Environment::GetInstance());
            }
        }
        else if (isInitializeFunctionRequired)
        {
            AZ_Error("Module", false, "Unable to locate required entry point '%s' within module '%s'.",
                InitializeDynamicModuleFunctionName, m_fileName.c_str());
            Unload();
            return false;
        }

        return true;
    }

    bool DynamicModuleHandle::Unload()
    {
        if (!IsLoaded())
        {
            return false;
        }

        // Call module's uninitialize function

        // note that here also we don't care what the actual value of m_initialized is.  We only care whether or not
        // the environment variable exists.  If it exists, someone initialized this module.
        if (m_initialized)
        {
            // the owner is whoever created the variable, and is also thus the one that initialized it, so it can uninitalize it.
            if (m_initialized.IsOwner()) 
            {
                auto uninitFunc = GetFunction<UninitializeDynamicModuleFunction>(UninitializeDynamicModuleFunctionName);
                AZ_Error("Module", uninitFunc, "Unable to locate required entry point '%s' within module '%s'.",
                    UninitializeDynamicModuleFunctionName, m_fileName.c_str());
                if (uninitFunc)
                {
                    uninitFunc();
                }
            }
            
            m_initialized.Reset(); // drop our ref.
        }

        if (!UnloadModule())
        {
            AZ_Error("Module", false, "Failed to unload module at \"%s\".", m_fileName.c_str());
            return false;
        }

        return true;
    }

#if !defined(AZ_HAS_DLL_SUPPORT)
    // Default create which fails for unsupported platforms
    AZStd::unique_ptr<DynamicModuleHandle> DynamicModuleHandle::Create(const char*)
    {
        AZ_Assert(false, "Dynamic modules are not supported on this platform");
        return nullptr;
    }
#endif
} // namespace AZ
