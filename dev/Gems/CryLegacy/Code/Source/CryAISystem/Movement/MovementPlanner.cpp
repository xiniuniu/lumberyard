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
#include "MovementPlanner.h"
#include "MovementActor.h"
#include "MovementUpdateContext.h"
#include "MovementSystem.h"

#include "MovementBlock_FollowPath.h"
#include "MovementBlock_HarshStop.h"
#include "MovementBlock_InstallAgentInCover.h"
#include "MovementBlock_SetupPipeUserCoverInformation.h"
#include "MovementBlock_TurnTowardsPosition.h"
#include "MovementBlock_UninstallAgentFromCover.h"
#include "MovementBlock_UseSmartObject.h"
#include "MovementBlock_UseExactPositioning.h"



namespace Movement
{
    bool GenericPlanner::IsUpdateNeeded() const
    {
        return m_plan.HasBlocks() || m_pathfinderRequestQueued;
    }

    void GenericPlanner::StartWorkingOnRequest(const MovementRequest& request, const MovementUpdateContext& context)
    {
        m_amountOfFailedReplanning = 0;
        StartWorkingOnRequest_Internal(request, context);
    }

    void GenericPlanner::StartWorkingOnRequest_Internal(const MovementRequest& request, const MovementUpdateContext& context)
    {
        assert(IsReadyForNewRequest());

        m_request = request;

        if (request.type == MovementRequest::MoveTo)
        {
            if (!m_request.style.IsMovingAlongDesignedPath())
            {
                // Future: We could path find from a bit further along the current plan.

                context.actor.RequestPathTo(request.destination, request.lengthToTrimFromThePathEnd, request.dangersFlags, request.considerActorsAsPathObstacles);
                m_pathfinderRequestQueued = true;

                //
                // Problem
                //  What if the controller starts planning the path from the current
                //  position while following a path and then keeps executing the plan
                //  and starts using a smart object and then a path comes back?
                //  This is not cool because we can't replace the current plan with
                //  the new one while traversing the smart object.
                //
                // Solution
                //  The controller can only start planning when not traversing a
                //  smart object and it must cut off all upcoming smart object blocks.
                //  This guarantees that when we get the path we'll be running a block
                //  that is interruptible, or no block. Good, this is what we want.
                //

                m_plan.CutOffAfterCurrentBlock();
            }
            else
            {
                // If we are following a path we don't need to request a path.
                // We can directly produce a plan.
                ProducePlan(context);
            }
        }
        else if (request.type == MovementRequest::Stop)
        {
            ProducePlan(context);
        }
        else
        {
            assert(false); // Unsupported request type
        }
    }

    void GenericPlanner::CancelCurrentRequest(MovementActor& actor)
    {
        //
        // The request has been canceled but the plan remains intact.
        // This means that if the actor is running along a path he will keep
        // keep running along that path.
        //
        // The idea is that an actor should only stop if a stop is
        // explicitly requested.
        //
        // Let's see how it works out in practice :)
        //
    }

    IPlanner::Status GenericPlanner::Update(const MovementUpdateContext& context)
    {
        IPlanner::Status status;

        if (m_pathfinderRequestQueued)
        {
            CheckOnPathfinder(context, OUT status);

            if (status.HasPathfinderFailed())
            {
                return status;
            }
        }

        ExecuteCurrentPlan(context, OUT status);

        return status;
    }

    bool GenericPlanner::IsReadyForNewRequest() const
    {
        if (m_pathfinderRequestQueued)
        {
            return false;
        }

        return m_plan.InterruptibleNow();
    }

    void GenericPlanner::GetStatus(MovementRequestStatus& status) const
    {
        if (m_pathfinderRequestQueued)
        {
            status.id = MovementRequestStatus::FindingPath;
        }
        else
        {
            status.id = MovementRequestStatus::ExecutingPlan;
            status.currentBlockIndex = m_plan.GetCurrentBlockIndex();

            for (uint32 i = 0, n = m_plan.GetBlockCount(); i < n; ++i)
            {
                const char* blockName = m_plan.GetBlock(i)->GetName();
                status.blockInfos.push_back(blockName);
            }
        }
    }

    void GenericPlanner::CheckOnPathfinder(const MovementUpdateContext& context, OUT IPlanner::Status& status)
    {
        const PathfinderState state = context.actor.GetPathfinderState();

        const bool pathfinderFinished = (state != StillFinding);
        if (pathfinderFinished)
        {
            m_pathfinderRequestQueued = false;

            if (state == FoundPath)
            {
                ProducePlan(context);
            }
            else
            {
                status.SetPathfinderFailed();
            }
        }
    }

    void GenericPlanner::ExecuteCurrentPlan(const MovementUpdateContext& context, OUT IPlanner::Status& status)
    {
        if (m_plan.HasBlocks())
        {
            const Plan::Status s = m_plan.Execute(context);

            if (s == Plan::Finished)
            {
                status.SetRequestSatisfied();
                m_plan.Clear(context.actor);
            }
            else if (s == Plan::CantBeFinished)
            {
                // The current plan can't be finished. We'll re-plan as soon as
                // we can to see if we can satisfy the request.
                // Note: it could become necessary to add a "retry count".
                ++m_amountOfFailedReplanning;
                if (m_request.style.IsMovingAlongDesignedPath())
                {
                    status.SetMovingAlongPathFailed();
                }
                else if (IsReadyForNewRequest())
                {
                    if (CanReplan(m_request))
                    {
                        context.actor.Log("Movement plan couldn't be finished, re-planning.");
                        const MovementRequest replanRequest = m_request;
                        StartWorkingOnRequest_Internal(replanRequest, context);
                    }
                    else
                    {
                        status.SetReachedMaxAllowedReplans();
                    }
                }
            }
            else
            {
                assert(s == Plan::Running);
            }
        }
    }

    void GenericPlanner::ProducePlan(const MovementUpdateContext& context)
    {
        // Future: It would be good to respect the current plan and patch it
        // up with the newly found path. This would also require that we
        // kick off the path finding process with a predicted future position.

        // We should have shaved off non-interruptible movement blocks in
        // StartWorkingOnRequest before we started path finding.
        assert(m_plan.InterruptibleNow());

        m_plan.Clear(context.actor);

        switch (m_request.type)
        {
        case MovementRequest::MoveTo:
            ProduceMoveToPlan(context);
            break;

        case MovementRequest::Stop:
            ProduceStopPlan(context);
            break;

        default:
            assert(false);
        }

        context.actor.GetAdapter().OnMovementPlanProduced();
    }

    void GenericPlanner::ProduceMoveToPlan(const MovementUpdateContext& context)
    {
        using namespace Movement::MovementBlocks;
        if (m_request.style.IsMovingAlongDesignedPath())
        {
            // If the agent is forced to follow an AIPath then only the
            // FollowPath block is needed
            SShape designedPath;
            if (!context.actor.GetAdapter().GetDesignedPath(designedPath))
            {
                return;
            }

            if (designedPath.shape.empty())
            {
                return;
            }

            TPathPoints fullPath(designedPath.shape.begin(), designedPath.shape.end());
            CNavPath navPath;
            navPath.SetPathPoints(fullPath);
            m_plan.AddBlock(BlockPtr(new FollowPath(navPath, .0f, m_request.style, false)));
        }
        else
        {
            const INavPath* pNavPath = context.actor.GetCallbacks().getPathFunction();
            const TPathPoints fullPath = pNavPath->GetPath();

            if (m_request.style.ShouldTurnTowardsMovementDirectionBeforeMoving())
            {
                if (fullPath.size() >= 2)
                {
                    // Stop moving before we turn. The turn expects to be standing still.
                    m_plan.AddBlock<HarshStop>();

                    // Using the second point of the path. Ideally, we should turn
                    // towards a position once we've processed the path with the
                    // smart path follower to be sure we're really turning towards
                    // the direction we will later move towards. But the path has
                    // been somewhat straightened out already so this should be fine.
                    TPathPoints::const_iterator it = fullPath.begin();
                    ++it;
                    const Vec3 positionToTurnTowards = it->vPos;

                    // Adding a dead-zone here because small inaccuracies
                    // in the positioning will otherwise make the object turn in a
                    // seemingly 'random' direction each time when it is ordered to
                    // move to the spot where it already is at.
                    static const float distanceThreshold = 0.2f;
                    static const float distanceThresholdSq = distanceThreshold * distanceThreshold;
                    const float distanceToTurnPointSq = Distance::Point_PointSq(positionToTurnTowards, context.actor.GetAdapter().GetPhysicsPosition());
                    if (distanceToTurnPointSq >= distanceThresholdSq)
                    {
                        m_plan.AddBlock(BlockPtr(new TurnTowardsPosition(positionToTurnTowards)));
                    }
                }
            }

            if (context.actor.GetAdapter().IsInCover())
            {
                m_plan.AddBlock(BlockPtr(new UninstallAgentFromCover(m_request.style.GetStance())));
            }

            if (m_request.style.IsMovingToCover())
            {
                m_plan.AddBlock<SetupActorCoverInformation>();
            }

            // Go through the full path from start to end and split it up into
            // FollowPath & UseSmartObject blocks.
            TPathPoints::const_iterator first = fullPath.begin();
            TPathPoints::const_iterator curr = fullPath.begin();
            TPathPoints::const_iterator end = fullPath.end();

            AZStd::shared_ptr<UseSmartObject> lastAddedSmartObjectBlock;

            assert(curr != end);

            while (curr != end)
            {
                TPathPoints::const_iterator next = curr;
                ++next;

                const PathPointDescriptor& point = *curr;

                const bool isSmartObject = point.navType == IAISystem::NAV_SMARTOBJECT;
                const bool isCustomObject = point.navType == IAISystem::NAV_CUSTOM_NAVIGATION;
                const bool isLastNode = next == end;

                CNavPath path;

                if (isCustomObject || isSmartObject || isLastNode)
                {
                    // Extract the path between the first point and
                    // the smart object/last node we just found.
                    TPathPoints points;
                    points.assign(first, next);
                    path.SetPathPoints(points);

                    const bool blockAfterThisIsUseExactPositioning = isLastNode && (m_request.style.GetExactPositioningRequest() != 0);
                    const bool blockAfterThisUsesSomeFormOfExactPositioning = isSmartObject || isCustomObject || blockAfterThisIsUseExactPositioning;
                    const float endDistance = blockAfterThisUsesSomeFormOfExactPositioning ? 2.5f : 0.0f; // The value 2.5 meters was used prior to Crysis 3
                    const bool endsInCover = isLastNode && m_request.style.IsMovingToCover();
                    m_plan.AddBlock(BlockPtr(new FollowPath(path, endDistance, m_request.style, endsInCover)));

                    if (lastAddedSmartObjectBlock)
                    {
                        lastAddedSmartObjectBlock->SetUpcomingPath(path);
                        lastAddedSmartObjectBlock->SetUpcomingStyle(m_request.style);
                    }

                    if (blockAfterThisIsUseExactPositioning)
                    {
                        assert(!isSmartObject);
                        assert(!path.Empty());
                        m_plan.AddBlock(BlockPtr(new UseExactPositioning(path, m_request.style)));
                    }
                }

                if (isSmartObject || isCustomObject)
                {
                    assert(!path.Empty());
                    if (isSmartObject)
                    {
                        AZStd::shared_ptr<UseSmartObject> useSmartObjectBlock = AZStd::make_shared<UseSmartObject>(path, point.offMeshLinkData, m_request.style); //(new UseSmartObject(path, point.offMeshLinkData, m_request.style));
                        lastAddedSmartObjectBlock = useSmartObjectBlock;
                        m_plan.AddBlock(useSmartObjectBlock);
                    }
                    else
                    {
                        MovementSystem aiMovementSystem = static_cast<MovementSystem&>(context.movementSystem);
                        m_plan.AddBlock(aiMovementSystem.CreateCustomBlock(path, point.offMeshLinkData, m_request.style));
                    }

                    ++curr;
                    first = curr;
                }
                else
                {
                    ++curr;
                }
            }

            if (m_request.style.IsMovingToCover())
            {
                m_plan.AddBlock<InstallAgentInCover>();
            }
        }
    }

    void GenericPlanner::ProduceStopPlan(const MovementUpdateContext& context)
    {
        using namespace Movement::MovementBlocks;

        m_plan.AddBlock(BlockPtr(new HarshStop()));
    }

    bool GenericPlanner::CanReplan(const MovementRequest& request) const
    {
        return m_amountOfFailedReplanning < s_maxAllowedReplanning;
    }
}