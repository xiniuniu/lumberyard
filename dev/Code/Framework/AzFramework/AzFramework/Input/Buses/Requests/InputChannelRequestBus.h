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

#include <AzFramework/Input/Channels/InputChannelId.h>

#include <AzCore/EBus/EBus.h>

#include <AzCore/RTTI/ReflectContext.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace AzFramework
{
    class InputChannel;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    //! EBus interface used to query for available input channels
    class InputChannelRequests : public AZ::EBusTraits
    {
    public:
        ////////////////////////////////////////////////////////////////////////////////////////////
        //! EBus Trait: requests can be addressed to a specific InputChannelId so that they are only
        //! handled by one input channel that has connected to the bus using that unique id, or they
        //! can be broadcast to all input channels that have connected to the bus, regardless of id.
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! EBus Trait: requests should be handled by only one input channel connected to each id
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! EBus Trait: requests should be addressed to a specific channel id / device index pair.
        //! While input channel ids must be unique across different input devices, multiple devices
        //! of the same type can exist, so requests must be addressed using an id/device index pair.
        class BusIdType
        {
        public:
            ////////////////////////////////////////////////////////////////////////////////////////
            // Allocator
            AZ_CLASS_ALLOCATOR(BusIdType, AZ::SystemAllocator, 0);

            ////////////////////////////////////////////////////////////////////////////////////////
            // Type Info
            AZ_TYPE_INFO(BusIdType, "{FA0B740B-8917-4260-B402-05444C985AB5}");

            ////////////////////////////////////////////////////////////////////////////////////////
            // Reflection
            static void Reflect(AZ::ReflectContext* context);

            ////////////////////////////////////////////////////////////////////////////////////////
            //! Constructor
            //! \param[in] channelId Id of the input channel to address requests
            //! \param[in] deviceIndex Index of the input device to address requests
            BusIdType(const InputChannelId& channelId, AZ::u32 deviceIndex = 0)
                : m_channelId(channelId)
                , m_deviceIndex(deviceIndex)
            {}

            ////////////////////////////////////////////////////////////////////////////////////////
            //! Constructor
            //! \param[in] channelName Name of the input channel to address requests
            //! \param[in] deviceIndex Index of the input device to address requests
            BusIdType(const char* channelName, AZ::u32 deviceIndex = 0)
                : m_channelId(channelName)
                , m_deviceIndex(deviceIndex)
            {}

            ////////////////////////////////////////////////////////////////////////////////////////
            ///@{
            //! Equality comparison operator
            //! \param[in] other Another instance of the class to compare for equality
            bool operator==(const BusIdType& other) const;
            bool operator!=(const BusIdType& other) const;
            ///@}

            ////////////////////////////////////////////////////////////////////////////////////////
            // Variables
            InputChannelId m_channelId;   //!< Id of the input channel to address requests
            AZ::u32        m_deviceIndex; //!< Index of the input device to address requests
        };

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Finds a specific input channel given its id and the index of the device that owns it.
        //! This convenience function wraps an EBus call to InputChannelRequests::GetInputChannel.
        //! \param[in] channelId Id of the input channel to find
        //! \param[in] deviceIndex Index of the device that owns the input channel
        //! \return Pointer to the input channel if it was found, nullptr if it was not
        static const InputChannel* FindInputChannel(const InputChannelId& channelId,
                                                    AZ::u32 deviceIndex = 0);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Returns the input channel uniquely identified by the id/index pair used to address calls.
        //! This should never be broadcast otherwise the channel returned will effectively be random.
        //!
        //! Examples:
        //!
        //!     // Get the left mouse button input channel
        //!     const InputChannelRequests::BusIdType requestId(InputDeviceMouse::Button::Left);
        //!     const InputChannel* inputChannel = nullptr;
        //!     InputChannelRequestBus::EventResult(inputChannel,
        //!                                         requestId,
        //!                                         &InputChannelRequests::GetInputChannel);
        //!
        //!     // Get the A button input channel for the gamepad device at index 2
        //!     const InputChannelRequests::BusIdType requestId(InputDeviceGamepad::Button::A, 2);
        //!     const InputChannel* inputChannel = nullptr;
        //!     InputChannelRequestBus::EventResult(inputChannel,
        //!                                         requestId,
        //!                                         &InputChannelRequests::GetInputChannel);
        //!
        //! \return Pointer to the input channel if it exists, nullptr otherwise
        virtual const InputChannel* GetInputChannel() const = 0;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Reset the channel's state, which should broadcast an 'Ended' input notification event
        //! (if the channel is currently active) before returning the channel to the idle state.
        virtual void ResetState() = 0;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Default destructor
        virtual ~InputChannelRequests() = default;
    };
    using InputChannelRequestBus = AZ::EBus<InputChannelRequests>;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    inline bool InputChannelRequests::BusIdType::operator==(const InputChannelRequests::BusIdType& other) const
    {
        return (m_channelId == other.m_channelId) && (m_deviceIndex == other.m_deviceIndex);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    inline bool InputChannelRequests::BusIdType::operator!=(const BusIdType& other) const
    {
        return !(*this == other);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    inline const InputChannel* InputChannelRequests::FindInputChannel(const InputChannelId& channelId,
                                                                      AZ::u32 deviceIndex)
    {
        const InputChannel* inputChannel = nullptr;
        const BusIdType inputChannelRequestId(channelId, deviceIndex);
        InputChannelRequestBus::EventResult(inputChannel,
                                            inputChannelRequestId,
                                            &InputChannelRequests::GetInputChannel);
        return inputChannel;
    }
} // namespace AzFramework

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace AZStd
{
    ////////////////////////////////////////////////////////////////////////////////////////////////
    //! Hash structure specialization for InputChannelRequests::BusIdType
    template<> struct hash<AzFramework::InputChannelRequests::BusIdType>
    {
        inline size_t operator()(const AzFramework::InputChannelRequests::BusIdType& busIdType) const
        {
            size_t hashValue = busIdType.m_channelId.GetNameCrc32();
            AZStd::hash_combine(hashValue, busIdType.m_deviceIndex);
            return hashValue;
        }
    };
} // namespace AZStd
