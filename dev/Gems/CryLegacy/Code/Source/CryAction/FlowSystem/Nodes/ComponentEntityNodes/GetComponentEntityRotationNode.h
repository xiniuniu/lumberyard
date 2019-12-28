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

#include <ISystem.h>
#include <FlowSystem/Nodes/FlowBaseNode.h>

#include <IFlowSystem.h>
#include <Cry_Vector3.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Transform.h>

class ComponentEntityRotationNode
    : public CFlowBaseNode<eNCT_Instanced>
    , public AZ::TransformNotificationBus::Handler
{
    //////////////////////////////////////////////////////////////////////////
    // ComponentEntityRotationNode
    //////////////////////////////////////////////////////////////////////////

public:

    // This constructor only exists for compatibility with construction code outside of this node, it does not need the Activation info
    ComponentEntityRotationNode(SActivationInfo* /*activationInformation*/)
        : m_coordinateSystem(CoordSys::World)
        , m_previousQueryEntityId(FlowEntityId(FlowEntityId::s_invalidFlowEntityID))
    {
    }


    IFlowNodePtr Clone(SActivationInfo* activationInformation) override
    {
        return new ComponentEntityRotationNode(activationInformation);
    }

    void GetConfiguration(SFlowNodeConfig& config) override;

    void ProcessEvent(EFlowEvent evt, SActivationInfo* activationInformation) override;

    void GetMemoryUsage(ICrySizer* sizer) const override
    {
        sizer->Add(*this);
    }

    //////////////////////////////////////////////////////////////////////////////////
    // Transform notification bus handler
    /// Called when the local transform of the attached entity has changed.
    void OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& /*world*/) override;
    //////////////////////////////////////////////////////////////////////////////////

private:

    enum class CoordSys
    {
        Parent = 0,
        World
    };

    //! Stores the entity id of the last entity that was queried
    FlowEntityId m_previousQueryEntityId;

    //! Stores the Coordinate system that the last operation was performed in
    CoordSys m_coordinateSystem;

    enum InputPorts
    {
        Activate = 0,
        CoordSystem
    };

    enum OutputPorts
    {
        CurrentRotation = 0,
        Forward,
        Up,
        Right
    };

    //! Last known transform of the entity that is attached to this node, this is kept current via the ebus
    AZ::Transform m_entityTransform;

    /*
    * Resets the Transform bus handler to use a new entity Id
    * @param triggerEntityId Entity id to be used
    */
    void ResetHandlerForEntityId(FlowEntityId triggerEntityId);
};
