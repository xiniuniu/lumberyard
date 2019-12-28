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

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Math/Vector2.h>
#include <AzFramework/Input/Channels/InputChannelDigitalWithSharedModifierKeyStates.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
class UiInteractableInterface
    : public AZ::ComponentBus
{
public: // member functions

    virtual ~UiInteractableInterface() {}

    //! Check whether this component can handle the event at the given location
    virtual bool CanHandleEvent(AZ::Vector2 point) = 0;

    //! Called on an interactable component when a pressed event is received over it
    //! \param point, the point at which the event occurred (viewport space)
    //! \param shouldStayActive, output - true if the interactable wants to become the active element for the canvas
    //! \return true if the interactable handled the event
    virtual bool HandlePressed(AZ::Vector2 point, bool& shouldStayActive) = 0;

    //! Called on the currently pressed interactable component when a release event is received
    //! \param point, the point at which the event occurred (viewport space)
    //! \return true if the interactable handled the event
    virtual bool HandleReleased(AZ::Vector2 point) = 0;

    //! Called on an interactable component when a multi-touch pressed event is received over it
    //! \param point, the point at which the event occurred (viewport space)
    //! \param multiTouchIndex, the index of the multi-touch (the 'primary' touch with index 0 is sent to HandlePressed)
    //! \return true if the interactable handled the event
    virtual bool HandleMultiTouchPressed(AZ::Vector2 point, int multiTouchIndex) = 0;

    //! Called on the currently pressed interactable component when a multi-touch release event is received
    //! \param point, the point at which the event occurred (viewport space)
    //! \param multiTouchIndex, the index of the multi-touch (the 'primary' touch with index 0 is sent to HandlePressed)
    //! \return true if the interactable handled the event
    virtual bool HandleMultiTouchReleased(AZ::Vector2 point, int multiTouchIndex) = 0;

    //! Called on an interactable component when an enter pressed event is received
    //! \param shouldStayActive, output - true if the interactable wants to become the active element for the canvas
    //! \return true if the interactable handled the event
    virtual bool HandleEnterPressed(bool& shouldStayActive) { return false; }

    //! Called on the currently pressed interactable component when an enter released event is received
    //! \return true if the interactable handled the event
    virtual bool HandleEnterReleased() { return false; }

    //! Called when the interactable was navigated to via gamepad/keyboard, and auto activation is enabled on the interactable
    //! \return true if the interactable handled the event
    virtual bool HandleAutoActivation() { return false; }

    //! Called on the currently active interactable component when text input is received
    //! \return true if the interactable handled the event
    virtual bool HandleTextInput(const AZStd::string& textUTF8) { return false; };

    //! Called on the currently active interactable component when input is received
    //! \return true if the interactable handled the event
    virtual bool HandleKeyInputBegan(const AzFramework::InputChannel::Snapshot& inputSnapshot, AzFramework::ModifierKeyMask activeModifierKeys) { return false; }

    //! Called on the currently active interactable component when a mouse/touch position event is received
    //! \param point, the current mouse/touch position (viewport space)
    virtual void InputPositionUpdate(AZ::Vector2 point) {};

    //! Called on the currently pressed interactable component when a multi-touch position event is received
    //! \param point, the current mouse/touch position (viewport space)
    //! \param multiTouchIndex, the index of the multi-touch (the 'primary' touch with index 0 is sent to HandlePressed)
    virtual void MultiTouchPositionUpdate(AZ::Vector2 point, int multiTouchIndex) {};

    //! Returns true if this interactable supports taking active status when a drag is started on a child
    //! interactble AND the given drag startPoint would be a valid drag start point
    //! \param point, the start point of the drag (which would be on a child interactable) (viewport space)
    virtual bool DoesSupportDragHandOff(AZ::Vector2 startPoint) { return false; }

    //! Called on a parent of the currently active interactable element to allow interactables that
    //! contain other interactables to support drags that start on the child.
    //! If this return true the hand-off occured and the caller will no longer be considered the
    //! active interactable by the canvas.
    //! \param currentActiveInteractable, the child element that is the currently active interactable
    //! \param startPoint, the start point of the potential drag (viewport space)
    //! \param currentPoint, the current points of the potential drag (viewport space)
    virtual bool OfferDragHandOff(AZ::EntityId currentActiveInteractable, AZ::Vector2 startPoint, AZ::Vector2 currentPoint, float dragThreshold) { return false; };

    //! Called on the currently active interactable component when the active interactable changes
    virtual void LostActiveStatus() {};

    //! Called when mouse/touch enters the bounds of this interactable
    virtual void HandleHoverStart() = 0;

    //! Called on the currently hovered interactable component when mouse/touch moves outside of bounds
    virtual void HandleHoverEnd() = 0;

    //! Called when a descendant of the interactable becomes the hover interactable by being navigated to
    virtual void HandleDescendantReceivedHoverByNavigation(AZ::EntityId descendantEntityId) {};

    //! Called when the interactable becomes the hover interactable by being navigated to from one of its descendants
    virtual void HandleReceivedHoverByNavigatingFromDescendant(AZ::EntityId descendantEntityId) {};

    //! Query whether the interactable is currently pressed
    virtual bool IsPressed() { return false; }

    //! Enable/disable event handling
    virtual bool IsHandlingEvents() { return true; }
    virtual void SetIsHandlingEvents(bool isHandlingEvents) {}

    //! Enable/disable multi-touch event handling
    virtual bool IsHandlingMultiTouchEvents() { return true; }
    virtual void SetIsHandlingMultiTouchEvents(bool isHandlingMultiTouchEvents) {}

    //! Get/set whether the interactable automatically becomes active when navigated to via gamepad/keyboard
    virtual bool GetIsAutoActivationEnabled() = 0;
    virtual void SetIsAutoActivationEnabled(bool isEnabled) = 0;

public: // static member data

    //! Only one component on a entity can implement the events
    static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
};

typedef AZ::EBus<UiInteractableInterface> UiInteractableBus;

////////////////////////////////////////////////////////////////////////////////////////////////////
class UiInteractableActiveNotifications
    : public AZ::ComponentBus
{
public: // member functions

    virtual ~UiInteractableActiveNotifications() {}

    //! Notify listener that this interactable is no longer active
    virtual void ActiveCancelled() {}

    //! Notify listener that this interactable has given up active status to a new interactable
    virtual void ActiveChanged(AZ::EntityId m_newActiveInteractable, bool shouldStayActive) {}
};

typedef AZ::EBus<UiInteractableActiveNotifications> UiInteractableActiveNotificationBus;

////////////////////////////////////////////////////////////////////////////////////////////////////
//! Interface class that listeners need to implement in order to get notifications when actions are
//! triggered
class UiInteractableNotifications
    : public AZ::ComponentBus
{
public:

    //////////////////////////////////////////////////////////////////////////
    // EBusTraits overrides
    static const bool EnableEventQueue = true;
    //////////////////////////////////////////////////////////////////////////

public: // member functions

    virtual ~UiInteractableNotifications(){}

    //! Called on hover start
    virtual void OnHoverStart() {};

    //! Called on hover end
    virtual void OnHoverEnd() {};

    //! Called on pressed
    virtual void OnPressed() {};

    //! Called on released
    virtual void OnReleased() {};

    //! Called on receiving hover by being navigated to from a descendant
    virtual void OnReceivedHoverByNavigatingFromDescendant(AZ::EntityId descendantEntityId) {};
};

typedef AZ::EBus<UiInteractableNotifications> UiInteractableNotificationBus;
