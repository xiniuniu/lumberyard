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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#include "CryLegacy_precompiled.h"
#include "MovementBlock_UseExactPositioningBase.h"
#include "MovementActor.h"
#include "MovementUpdateContext.h"
#include "PipeUser.h"
#include "AIBubblesSystem/IAIBubblesSystem.h"

namespace Movement
{
    namespace MovementBlocks
    {
        UseExactPositioningBase::UseExactPositioningBase(
            const CNavPath& path,
            const MovementStyle& style)
            : m_path(path)
            , m_style(style)
            , m_state(Prepare)
            , m_accumulatedPathFollowerFailureTime(0.0f)
        {
        }

        void UseExactPositioningBase::Begin(IMovementActor& actor)
        {
            m_accumulatedPathFollowerFailureTime = 0.0f;
            actor.GetAdapter().SetActorStyle(m_style, m_path);
            m_stuckDetector.Reset();
        }

        void UseExactPositioningBase::End(IMovementActor& actor)
        {
            actor.GetAdapter().ClearMovementState();
        }

        Movement::Block::Status UseExactPositioningBase::Update(const MovementUpdateContext& context)
        {
            if (m_state == Prepare)
            {
                return UpdatePrepare(context);
            }
            else
            {
                assert(m_state == Traverse);
                return UpdateTraverse(context);
            }
        }

        Movement::Block::Status UseExactPositioningBase::UpdatePrepare(const MovementUpdateContext& context)
        {
            // Switch to the smart object state when the time is right
            const EActorTargetPhase targetPhase = context.actor.GetAdapter().GetActorPhase();

            // eATP_None
            //      No request sent yet
            // eATP_Waiting
            //      AIProxy has sent the request to controller (should be the frame after we request it)
            //      Controller will start behaving differently near the target and when it
            //      is near enough it will queue an action. When that happens we go to:
            // eATP_Starting
            //      Action has been queued
            //      Wait now for the controller to say the action entered and notified us
            //      about the finish point
            // eATP_Started,
            //      Action has entered (and we got notified about the finish point)
            //      Wait..for next frame...for some reason...
            // eATP_Playing,
            //      We're at least one frame after the action has entered
            // eATP_StartedAndFinished:
            // eATP_Finished:
            //      LOOPING: the loop has started
            //      ONESHOT: the action has finished
            //      (eATP_StartedAndFinished means the Playing state has been skipped)
            // eATP_Error:
            //      Jonas finds this unacceptable ;)

            if (targetPhase == eATP_Error)
            {
                HandleExactPositioningError(context);
                return Movement::Block::Finished;
            }

            if (targetPhase == eATP_Starting || targetPhase == eATP_Started)
            {
                // Done preparing, traverse it
                context.actor.GetAdapter().ClearMovementState();
                m_state =  Traverse;
                OnTraverseStarted(context);
                return Movement::Block::Running;
            }

            if (targetPhase == eATP_None)
            {
                const TryRequestingExactPositioningResult result = TryRequestingExactPositioning(context);

                switch (result)
                {
                case RequestSucceeded:
                {
                    // Enforce to use the current path.
                    context.actor.GetCallbacks().getPathFunction()->GetParams().inhibitPathRegeneration = true;
                }
                break;
                case RequestDelayed_ContinuePathFollowing:
                    break;
                case RequestDelayed_SkipPathFollowing:
                    return Running;
                case RequestFailed_FinishImmediately:
                    return Finished;
                }
            }

            assert((targetPhase == eATP_None) || (targetPhase == eATP_Waiting));

            PathFollowResult result;
            const bool targetReachable = Movement::Helpers::UpdatePathFollowing(result, context, m_style);

            m_stuckDetector.Update(context);

            if (m_stuckDetector.IsAgentStuck())
            {
                return Movement::Block::CantBeFinished;
            }

            if (!targetReachable)
            {
                // Track how much time we spent in this FollowPath block
                // without being able to reach the movement target.
                m_accumulatedPathFollowerFailureTime += context.updateTime;

                // If it keeps on failing we don't expect to recover.
                // We report back that we can't be finished and the planner
                // will attempt to re-plan for the current situation.
                if (m_accumulatedPathFollowerFailureTime > 3.0f)
                {
                    return Movement::Block::CantBeFinished;
                }
            }

            return Movement::Block::Running;
        }

        Movement::Block::Status UseExactPositioningBase::UpdateTraverse(const MovementUpdateContext& context)
        {
            EActorTargetPhase phase = context.actor.GetAdapter().GetActorPhase();

            if (phase == eATP_None || phase == eATP_StartedAndFinished)
            {
                return Finished;
            }

            return Running;
        }
    }
}