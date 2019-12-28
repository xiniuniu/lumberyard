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

#ifndef CRYINCLUDE_EDITOR_LOGFILE_H
#define CRYINCLUDE_EDITOR_LOGFILE_H

#pragma once

#include "ILog.h"
#include <IConsole.h>
#include <stdarg.h>

#define MAX_LOGBUFFER_SIZE 16384

class QTextEdit;
class QListWidget;

//struct IConsole;
//struct ICVar;

//////////////////////////////////////////////////////////////////////////
// Global log functions.
//////////////////////////////////////////////////////////////////////////

// the 'v' versions are for when you've already done your unpack, so that we are not forced to truncate the buffer

//! Displays error message.
SANDBOX_API void Error(const char* format, ...);
SANDBOX_API void ErrorV(const char* format, va_list argList);
//! Log to console and file.
SANDBOX_API void Log(const char* format, ...);
SANDBOX_API void LogV(const char* format, va_list argList);
//! Display Warning dialog.
SANDBOX_API void Warning(const char* format, ...);
SANDBOX_API void WarningV(const char* format, va_list argList);

/*!
 *  CLogFile implements ILog interface.
 */
class SANDBOX_API CLogFile
    : public ILogCallback
{
public:
    static const char* GetLogFileName();
    static void AttachListBox(QListWidget* hWndListBox);
    static void AttachEditBox(QTextEdit* hWndEditBox);

    //! Write to log snapshot of current process memory usage.
    static QString GetMemUsage();

    static void WriteString(const char* pszString);
    static void WriteString(const QString& string) { WriteString(string.toUtf8().data()); }
    static void WriteLine(const char* pszLine);
    static void WriteLine(const QString& string) { WriteLine(string.toUtf8().data()); }
    static void FormatLine(const char* pszMessage, ...);
    static void FormatLineV(const char* pszMessage, va_list argList);

    //////////////////////////////////////////////////////////////////////////
    // ILogCallback
    //////////////////////////////////////////////////////////////////////////
    virtual void OnWrite(const char* sText, IMiniLog::ELogType type) override {};
    virtual void OnWriteToConsole(const char* sText, bool bNewLine) override;
    virtual void OnWriteToFile(const char* sText, bool bNewLine) override;
    //////////////////////////////////////////////////////////////////////////

    // logs some useful information
    // should be called after CryLog() is available
    static void AboutSystem();

private:
    static void OpenFile();

    // Attached control(s)
    static QListWidget* m_hWndListBox;
    static QTextEdit* m_hWndEditBox;
    static bool m_bShowMemUsage;
    static bool m_bIsQuitting;
};

#endif // CRYINCLUDE_EDITOR_LOGFILE_H
