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

#include <EMotionFX/Source/AnimGraphNode.h>
#include <EMotionFX/Source/EMotionFXConfig.h>
#include <EMotionFX/Source/SpringSolver.h>
#include <EMotionFX/Source/SimulatedObjectBus.h>

namespace EMotionFX
{
    class SimulatedObject;
    class SimulatedJoint;

    class EMFX_API BlendTreeSimulatedObjectNode
        : public AnimGraphNode
        , private EMotionFX::SimulatedObjectNotificationBus::Handler
    {
    public:
        AZ_RTTI(BlendTreeSimulatedObjectNode, "{89FF51DF-0CB0-4E7D-9F56-E305C8E94D90}", AnimGraphNode)
        AZ_CLASS_ALLOCATOR_DECL

        enum
        {
            INPUTPORT_POSE = 0,
            INPUTPORT_STIFFNESSFACTOR = 1,
            INPUTPORT_GRAVITYFACTOR = 2,
            INPUTPORT_DAMPINGFACTOR = 3,
            INPUTPORT_WEIGHT = 4,
            OUTPUTPORT_POSE = 0
        };

        enum
        {
            PORTID_INPUT_POSE = 0,
            PORTID_INPUT_WEIGHT = 1,
            PORTID_INPUT_STIFFNESSFACTOR = 2,
            PORTID_INPUT_GRAVITYFACTOR = 3,
            PORTID_INPUT_DAMPINGFACTOR = 4,
            PORTID_OUTPUT_POSE = 0
        };

        struct EMFX_API Simulation
        {
            AZ_CLASS_ALLOCATOR_DECL
            SpringSolver m_solver;
            const SimulatedObject* m_simulatedObject = nullptr;
        };

        class EMFX_API UniqueData
            : public AnimGraphNodeData
        {
            EMFX_ANIMGRAPHOBJECTDATA_IMPLEMENT_LOADSAVE
        public:
            AZ_CLASS_ALLOCATOR_DECL

            UniqueData(AnimGraphNode* node, AnimGraphInstance* animGraphInstance)
                : AnimGraphNodeData(node, animGraphInstance)
                , m_mustUpdate(true)
                , m_isValid(false)
                , m_timePassedInSeconds(0.0f)
            {
            }
            ~UniqueData() override;

        public:
            AZStd::vector<Simulation*> m_simulations;
            float m_timePassedInSeconds;
            bool m_mustUpdate;
            bool m_isValid;
        };

        BlendTreeSimulatedObjectNode();
        ~BlendTreeSimulatedObjectNode();

        void Reinit() override;
        bool InitAfterLoading(AnimGraph* animGraph) override;
        void Rewind(AnimGraphInstance* animGraphInstance) override;

        void OnUpdateUniqueData(AnimGraphInstance* animGraphInstance) override;
        bool GetSupportsVisualization() const override { return true; }
        bool GetHasOutputPose() const override { return true; }
        bool GetSupportsDisable() const override { return true; }
        AZ::Color GetVisualColor() const override { return AZ::Color(1.0f, 0.0f, 0.0f, 1.0f); }
        AnimGraphPose* GetMainOutputPose(AnimGraphInstance* animGraphInstance) const override { return GetOutputPose(animGraphInstance, OUTPUTPORT_POSE)->GetValue(); }

        const char* GetPaletteName() const override;
        AnimGraphObject::ECategory GetPaletteCategory() const override;

        // SimulatedObjectNotifications
        void OnSimulatedObjectChanged() override;
        void SetSimulatedObjectNames(const AZStd::vector<AZStd::string>& simObjectNames);

        static void Reflect(AZ::ReflectContext* context);

    private:
        using PropertyChangeFunction = AZStd::function<void(UniqueData*)>;

        void UpdateUniqueData(AnimGraphInstance* animGraphInstance, UniqueData* uniqueData);
        void Update(AnimGraphInstance* animGraphInstance, float timePassedInSeconds) override;
        void Output(AnimGraphInstance* animGraphInstance) override;
        void AdjustParticles(const SpringSolver::ParticleAdjustFunction& func);
        void OnNumIterationsChanged();
        void OnUpdateRateChanged();
        void OnPropertyChanged(const PropertyChangeFunction& func);
        bool InitSolvers(AnimGraphInstance* animGraphInstance, UniqueData* uniqueData);
        float GetStiffnessFactor(AnimGraphInstance* animGraphInstance) const;
        float GetGravityFactor(AnimGraphInstance* animGraphInstance) const;
        float GetDampingFactor(AnimGraphInstance* animGraphInstance) const;

        AZStd::vector<AZStd::string> m_simulatedObjectNames;
        AZ::u32 m_numIterations = 2;
        AZ::u32 m_updateRate = 60;
        float m_stiffnessFactor = 1.0f;
        float m_gravityFactor = 1.0f;
        float m_dampingFactor = 1.0f;
        bool m_collisionDetection = true;
    };
} // namespace EMotionFX
