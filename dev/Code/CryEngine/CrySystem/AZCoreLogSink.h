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

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Math/Crc.h>
#include <AzCore/Debug/TraceMessageBus.h>
#include <AzCore/std/containers/unordered_map.h>

#include <CryAssert.h>

/**
 * Hook Trace bus so we can funnel AZ asserts, warnings, etc to CryEngine.
 *
 * Note: This is currently owned by CrySystem, because CrySystem owns
 * the logging mechanism for which it is relevant.
 */
class AZCoreLogSink
    : public AZ::Debug::TraceMessageBus::Handler
{
public:
    inline static void Connect()
    {
        GetInstance().m_ignoredAsserts = new IgnoredAssertMap();
        GetInstance().BusConnect();
    }

    inline static void Disconnect()
    {
        GetInstance().BusDisconnect();
        delete GetInstance().m_ignoredAsserts;
    }

    static AZCoreLogSink& GetInstance()
    {
        static AZCoreLogSink s_sink;
        return s_sink;
    }

    static bool IsCryLogReady()
    {
        return gEnv && gEnv->pSystem && gEnv->pLog;
    }

    bool OnPreAssert(const char* fileName, int line, const char* func, const char* message) override
    {
#if defined(USE_CRY_ASSERT) && AZ_LEGACY_CRYSYSTEM_TRAIT_DO_PREASSERT
        AZ::Crc32 crc;
        crc.Add(&line, sizeof(line));
        if (fileName)
        {
            crc.Add(fileName, strlen(fileName));
        }

        bool* ignore = nullptr;
        auto foundIter = m_ignoredAsserts->find(crc);
        if (foundIter == m_ignoredAsserts->end())
        {
            ignore = &((*m_ignoredAsserts)[crc]);
            *ignore = false;
        }
        else
        {
            ignore = &((*m_ignoredAsserts)[crc]);
        }

        if (!(*ignore))
        {
            using namespace AZ::Debug;

            Trace::Output(nullptr, "\n==================================================================\n");
            AZ::OSString outputMsg = AZ::OSString::format("Trace::Assert\n %s(%d): '%s'\n%s\n", fileName, line, func, message);
            Trace::Output(nullptr, outputMsg.c_str());

            // Suppress 3 in stack depth - this function, the bus broadcast that got us here, and Trace::Assert
            Trace::Output(nullptr, "------------------------------------------------\n");
            Trace::PrintCallstack(nullptr, 3);
            Trace::Output(nullptr, "\n==================================================================\n");

            AZ::EnvironmentVariable<bool> inEditorBatchMode = AZ::Environment::FindVariable<bool>("InEditorBatchMode");
            if (!inEditorBatchMode.IsConstructed() || !inEditorBatchMode.Get())
            {
                // Note - CryAssertTrace doesn't actually print any info to logging
                // it just stores the message internally for the message box in CryAssert to use
                CryAssertTrace("%s", message);
                if (CryAssert("Assertion failed", fileName, line, ignore) || Trace::IsDebuggerPresent())
                {
                    Trace::Break();
                }
            }
        }
        else
        {
            CryLogAlways("%s", message);
        }

        return true; // suppress default AzCore behavior.
#else
        AZ_UNUSED(fileName);
        AZ_UNUSED(line);
        AZ_UNUSED(func);
        AZ_UNUSED(message);
        return false; // allow AZCore to do its default behavior.   This usually results in an application shutdown.
#endif
    }

    bool OnPreError(const char* window, const char* fileName, int line, const char* func, const char* message) override
    {
        AZ_UNUSED(fileName);
        AZ_UNUSED(line);
        AZ_UNUSED(func);
        if (!IsCryLogReady())
        {
            return false; // allow AZCore to do its default behavior.
        }
        gEnv->pLog->LogError("(%s) - %s", window, message);
        return true; // suppress default AzCore behavior.
    }

    bool OnPreWarning(const char* window, const char* fileName, int line, const char* func, const char* message) override
    {
        AZ_UNUSED(fileName);
        AZ_UNUSED(line);
        AZ_UNUSED(func);

        if (!IsCryLogReady())
        {
            return false; // allow AZCore to do its default behavior.
        }

        CryWarning(VALIDATOR_MODULE_UNKNOWN, VALIDATOR_WARNING, "(%s) - %s", window, message);
        return true; // suppress default AzCore behavior.
    }

    bool OnOutput(const char* window, const char* message)  override
    {
        if (!IsCryLogReady())
        {
            return false; // allow AZCore to do its default behavior.
        }

        if (window == AZ::Debug::Trace::GetDefaultSystemWindow())
        {
            CryLogAlways("%s", message);
        }
        else
        {
            CryLog("(%s) - %s", window, message);
        }

        return true; // suppress default AzCore behavior.
    }

private:

    using IgnoredAssertMap = AZStd::unordered_map<AZ::Crc32, bool, AZStd::hash<AZ::Crc32>, AZStd::equal_to<AZ::Crc32>, AZ::OSStdAllocator>;
    IgnoredAssertMap* m_ignoredAsserts;
};
