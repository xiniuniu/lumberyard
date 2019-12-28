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
#include "CloudGemPlayerAccount_precompiled.h"
#include "UserAttributeList.h"

#include <AzCore/base.h>
#include <AzCore/Memory/Memory.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Script/ScriptContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace CloudGemPlayerAccount
{
    void UserAttributeList::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

        if (serializeContext)
        {
            serializeContext->Class<UserAttributeList>()
                ->Version(1);
        }

        AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext)
        {
            behaviorContext->Class<UserAttributeList>()
                ->Method("AddAttribute", &UserAttributeList::AddAttribute)
                ->Method("HasAttribute", &UserAttributeList::HasAttribute)
                ->Method("RemoveAttribute", &UserAttributeList::RemoveAttribute)
                ;
        }
    }

    void UserAttributeList::AddAttribute(const AZStd::string& name)
    {
        m_attributes.insert(name);
    }

    bool UserAttributeList::HasAttribute(const AZStd::string& name) const
    {
        return m_attributes.count(name) > 0;
    }

    void UserAttributeList::RemoveAttribute(const AZStd::string& name)
    {
        m_attributes.erase(name);
    }

    const AttributeSet& UserAttributeList::GetData() const
    {
        return m_attributes;
    }

} // namespace CloudGemPlayerAccount
