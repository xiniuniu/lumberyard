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

#include <AzFramework/Input/Channels/InputChannel.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace AzFramework
{
    ////////////////////////////////////////////////////////////////////////////////////////////////
    //! Class representing input channels that emit one dimensional digital input values.
    //! Examples: game-pad button, keyboard key
    class InputChannelDigital : public InputChannel
    {
    public:
        ////////////////////////////////////////////////////////////////////////////////////////////
        // Allocator
        AZ_CLASS_ALLOCATOR(InputChannelDigital, AZ::SystemAllocator, 0);

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Type Info
        AZ_RTTI(InputChannelDigital, "{07BD463B-0E1C-47B5-849D-3C09F9D1B468}", InputChannel);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Constructor
        //! \param[in] inputChannelId Id of the input channel being constructed
        //! \param[in] inputDevice Input device that owns the input channel
        explicit InputChannelDigital(const InputChannelId& inputChannelId,
                                     const InputDevice& inputDevice);

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Disable copying
        AZ_DISABLE_COPY_MOVE(InputChannelDigital);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Default destructor
        ~InputChannelDigital() override = default;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Access to the digital value of the input channel (1.0f or 0.0f)
        //! \return The current digital value of the input channel
        float GetValue() const override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Access to the digital delta of the input channel (1.0f, -1.0f, or 0.0f)
        //! \return Difference between the current and last reported digital values
        float GetDelta() const override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Process a raw input event, that will in turn update the channel's state based on whether
        //! it's active/engaged or inactive/idle, broadcasting an input event if the channel is left
        //! in a non-idle state. This function (or InputChannel::UpdateState) should only be called
        //! a max of once per channel per frame from InputDeviceRequests::TickInputDevice to ensure
        //! that input channels broadcast no more than one event each frame (and at the same time).
        //! \param[in] rawValue The raw digital value to process
        void ProcessRawInputEvent(bool rawValue);
    };
} // namespace AzFramework
