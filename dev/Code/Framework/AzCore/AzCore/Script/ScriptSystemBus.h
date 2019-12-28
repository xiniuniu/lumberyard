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
#ifndef AZCORE_SCRIPT_SYSTEM_BUS_H
#define AZCORE_SCRIPT_SYSTEM_BUS_H

#include <AzCore/EBus/EBus.h>
#include <AzCore/Script/ScriptAsset.h>
#include <AzCore/Script/ScriptContext.h>

namespace AZ
{
    /**
     * Event for communication with the component main application. There can
     * be only one application at a time. This is why this but is set to support
     * only one client/listener.
     */
    class ScriptSystemRequests
        : public AZ::EBusTraits
    {
    public:
        virtual ~ScriptSystemRequests() {}

        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides - application is a singleton
        static const AZ::EBusHandlerPolicy HandlerPolicy = EBusHandlerPolicy::Single;  // we sort components on m_initOrder
        //////////////////////////////////////////////////////////////////////////

        /**
         *  Add a context with based on user created context, the system will not delete such context.  
         *
         * \param garbageCollectorStep you can pass -1 to use the internal system default step, otherwise you can use any value >=1
         */
        virtual ScriptContext* AddContext(ScriptContext* context, int garbageCollectorStep) = 0;
        /// Add a context with an ID, the system will handle the ownership of the context (delete it when removed and the system deactivates)
        virtual ScriptContext* AddContextWithId(ScriptContextId id) = 0;

        /// Removes a script context (without calling delete on it)
        virtual bool RemoveContext(ScriptContext* context) = 0;
        /// Removes and deletes a script context.
        virtual bool RemoveContextWithId(ScriptContextId id) = 0;

        /// Returns the script context that has been registered with the app, if there is one.
        virtual ScriptContext* GetContext(ScriptContextId id) = 0;

        /// Full GC will be performed
        virtual void GarbageCollect() = 0;

        /// Step GC 
        virtual void GarbageCollectStep(int numberOfSteps) = 0;

        /**
         * Load script asset into the a context.
         * If the load succeeds, the script table will be on top of the stack
         *
         * \param asset     script asset
         * \param id        the id of the context to load the script in
         *
         * \return whether or not the asset load succeeded
         */
        virtual bool Load(const Data::Asset<ScriptAsset>& asset, ScriptContextId id) = 0;

        /**
         * Clear the script asset cached in ScriptSystemComponent.
         */
        virtual void ClearAssetReferences(Data::AssetId assetBaseId) = 0;
    };

    using ScriptSystemRequestBus = AZ::EBus<ScriptSystemRequests>;
}

#endif // AZCORE_SCRIPT_SYSTEM_BUS_H
#pragma once
