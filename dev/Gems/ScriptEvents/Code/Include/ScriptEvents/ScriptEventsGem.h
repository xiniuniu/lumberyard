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

#include <AzCore/Component/Component.h>
#include <AzCore/Module/Module.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/Memory/Memory.h>

namespace ScriptEvents
{
    /**
    * The ScriptEvents::Module class coordinates with the application
    * to reflect classes and create system components.
    */
    class Module
        : public AZ::Module
    {
    public:
        AZ_RTTI(Module, "{DD54A1FE-2BDF-412C-AAB8-5A6BE01FE524}", AZ::Module);
        AZ_CLASS_ALLOCATOR(Module, AZ::SystemAllocator, 0);

        Module();
        virtual ~Module() = default;

        AZ::ComponentTypeList GetRequiredSystemComponents() const override;
    };
}
