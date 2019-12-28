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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

// Description : the command manager


#ifndef CRYINCLUDE_EDITOR_COMMANDS_COMMANDMANAGER_H
#define CRYINCLUDE_EDITOR_COMMANDS_COMMANDMANAGER_H
#pragma once
#include "Include/ICommandManager.h"

class SANDBOX_API CEditorCommandManager
    : public ICommandManager
{
public:
    enum
    {
        CUSTOM_COMMAND_ID_FIRST = 10000,
        CUSTOM_COMMAND_ID_LAST  = 15000
    };

    CEditorCommandManager();
    ~CEditorCommandManager();

    void RegisterAutoCommands();

    bool AddCommand(CCommand* pCommand, TPfnDeleter deleter = NULL);
    bool UnregisterCommand(const char* module, const char* name);
    bool RegisterUICommand(
        const char* module,
        const char* name,
        const char* description,
        const char* example,
        const Functor0& functor,
        const CCommand0::SUIInfo& uiInfo);
    bool AttachUIInfo(const char* fullCmdName, const CCommand0::SUIInfo& uiInfo);
    bool GetUIInfo(const string& module, const string& name, CCommand0::SUIInfo& uiInfo) const;
    bool GetUIInfo(const string& fullCmdName, CCommand0::SUIInfo& uiInfo) const;
    QString Execute(const string& cmdLine);
    QString Execute(const string& module, const string& name, const CCommand::CArgs& args);
    void Execute(int commandId);
    void GetCommandList(std::vector<string>& cmds) const;
    //! Used in the console dialog
    string AutoComplete(const string& substr) const;
    bool IsRegistered(const char* module, const char* name) const;
    bool IsRegistered(const char* cmdLine) const;
    bool IsRegistered(int commandId) const;
    void SetCommandAvailableInScripting(const string& module, const string& name);
    bool IsCommandAvailableInScripting(const string& module, const string& name) const;
    bool IsCommandAvailableInScripting(const string& fullCmdName) const;
    //! Turning off the warning is needed for reloading the ribbon bar.
    void TurnDuplicateWarningOn() { m_bWarnDuplicate = true; }
    void TurnDuplicateWarningOff() { m_bWarnDuplicate = false; }

protected:
    struct SCommandTableEntry
    {
        CCommand* pCommand;
        TPfnDeleter deleter;
    };

    //! A full command name to an actual command mapping
    typedef std::map<string, SCommandTableEntry> CommandTable;
    CommandTable m_commands;

    //! A command ID to an actual UI command mapping
    //! This table will contain a subset of commands among all registered to the above table.
    typedef std::map<int, CCommand0*> UICommandTable;
    UICommandTable m_uiCommands;
    bool m_bWarnDuplicate;

    static int GenNewCommandId();
    static string GetFullCommandName(const string& module, const string& name);
    static void GetArgsFromString(const string& argsTxt, CCommand::CArgs& argList);
    void LogCommand(const string& fullCmdName, const CCommand::CArgs& args) const;
    QString ExecuteAndLogReturn(CCommand* pCommand, const CCommand::CArgs& args);
};

//! A helper class for an automatic command registration
class SANDBOX_API CAutoRegisterCommandHelper
{
public:
    static CAutoRegisterCommandHelper* GetFirst();

    CAutoRegisterCommandHelper(void(*registerFunc)(CEditorCommandManager &));
    void (* m_registerFunc)(CEditorCommandManager&);
    CAutoRegisterCommandHelper* m_pNext;

private:

    static CAutoRegisterCommandHelper* s_pFirst;
    static CAutoRegisterCommandHelper* s_pLast;
};

#define REGISTER_EDITOR_COMMAND(functionPtr, moduleName, functionName, description, example)                                    \
    void RegisterCommand##moduleName##functionName(CEditorCommandManager & cmdMgr)                                              \
    {                                                                                                                           \
        CommandManagerHelper::RegisterCommand(&cmdMgr, #moduleName, #functionName, description, example, functor(functionPtr)); \
    }                                                                                                                           \
    CAutoRegisterCommandHelper g_AutoRegCmdHelper##moduleName##functionName(RegisterCommand##moduleName##functionName)

#endif // CRYINCLUDE_EDITOR_COMMANDS_COMMANDMANAGER_H
