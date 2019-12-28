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

namespace MessagePopup
{
    enum EPopupButtons
    {
        EPopupButtons_Confirm,
        EPopupButtons_YesNo,
        EPopupButtons_NoButtons,
        EPopupButtons_Custom
    };
    enum EPopupKind
    {
        EPopupKind_Generic,
        EPopupKind_Toaster
    };

    static const AZ::u32 InvalidId = -1;

    //////////////////////////////////////////////////////////////////////////
    struct MessagePopupInfo
    {
        MessagePopupInfo() 
            : m_clientData ( nullptr )
            , m_showTime(0.0f)
        {}
        void SetData(void* _clientData, AZStd::function<void(int _button)> _callback, float _showTime)
        {
            m_clientData = _clientData;
            m_callback = _callback;
            m_showTime = _showTime;
        }
        bool IsValid()
        {
            return m_clientData != 0;
        }

        void* m_clientData;
        AZStd::function<void(int _button)> m_callback;
        float m_showTime;
    };


    //////////////////////////////////////////////////////////////////////////
    class MessagePopupRequests
        : public AZ::EBusTraits
    {
    public:
        //function pointer definitions for lua reflection
        typedef AZ::u32(MessagePopupRequests::*ShowToasterFunc)(const AZStd::string& _message, float _showTime);
        typedef AZ::u32(MessagePopupRequests::*ShowPopupFunc)(const AZStd::string& _message, EPopupButtons _buttons);

        // EBusTraits overrides
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        virtual AZ::u32 ShowPopup(const AZStd::string& _message, EPopupButtons _buttons) = 0;
        virtual AZ::u32 ShowPopupWithCallback(
            const AZStd::string& _message, 
            EPopupButtons _buttons,
            AZStd::function<void(int _button)> _callback) = 0;
        virtual AZ::u32 ShowToasterPopup(const AZStd::string& _message, float _showTime) = 0;
        virtual AZ::u32 ShowToasterPopupWithCallback(
            const AZStd::string& _message,
            float _showTime,
            AZStd::function<void(int _button)> _callback) = 0;
        virtual bool HidePopup(AZ::u32 _popupID, int _buttonPressed) = 0;
        virtual AZ::u32 GetNumActivePopups() const = 0;
    };
    using MessagePopupRequestBus = AZ::EBus<MessagePopupRequests>;

    //////////////////////////////////////////////////////////////////////////
    class MessagePopupImpl
        : public AZ::EBusTraits
    {
    public:
        // EBusTraits overrides
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        virtual void OnShowPopup(AZ::u32 _popupID, const AZStd::string& _message, EPopupButtons _buttons, EPopupKind _kind, AZStd::function<void(int _button)> _callback, void** _popupClientID) = 0;
        virtual void OnHidePopup(const MessagePopupInfo& _popupInfo) = 0;
    };
    using MessagePopupImplBus = AZ::EBus<MessagePopupImpl>;

    //////////////////////////////////////////////////////////////////////////
    class MessagePopupNotifications
        : public AZ::ComponentBus
    {
    public:
        //! Notifies listeners about popup closed
        virtual void OnHide(AZ::u32 popupID, int buttonPressed) {}

    };
    using MessagePopupNotificationsBus = AZ::EBus<MessagePopupNotifications>;

} // namespace MessagePopup
