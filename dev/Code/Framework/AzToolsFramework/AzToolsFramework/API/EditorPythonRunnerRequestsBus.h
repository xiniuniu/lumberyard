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

namespace AzToolsFramework
{
    /**
      * Provides a bus to run Python scripts
      */
    class EditorPythonRunnerRequests
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////

        //! executes a Python script using a string
        virtual void ExecuteByString(AZStd::string_view script) { AZ_UNUSED(script); }

        //! executes a Python script using a filename
        virtual void ExecuteByFilename(AZStd::string_view filename) { AZ_UNUSED(filename); }
    };
    using EditorPythonRunnerRequestBus = AZ::EBus<EditorPythonRunnerRequests>;

} // namespace AzToolsFramework

