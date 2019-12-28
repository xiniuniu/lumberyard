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

#include "StdAfx.h"

#include "Particle.h"
#include "ParticleEmitter.h"
#include "ParticleContainer.h"

#ifdef LY_TERRAIN_LEGACY_RUNTIME
#include "terrain.h"
#endif

#include "GeomQuery.h"
#include "MatMan.h"

#define fMAX_NONUNIFORM_TRAVEL  0.25f
#define fCOLLIDE_BUFFER_DIST        0.001f
#define fATTACH_BUFFER_DIST         0.01f
#define fMIN_TEST_AHEAD_MULT        2.5f
#define fSLIDE_TEST_AHEAD_TIME  0.01f
#define WIDTH_TO_HALF_WIDTH 0.5f

namespace
{
    // A measure of how far a midpoint is from the line.
    // Return square of double triangle area.
    float InterpVariance(Vec3 const& v0, Vec3 const& v1, Vec3 const& v2)
    {
        Vec3 v02 = v2 - v0;
        Vec3 v01 = v1 - v0;
        float fT = max(div_min(v01 * v02, v02.len2(), 1.f), 0.f);
        return (v02 * fT - v01).len2();
    }

    float InterpVariance(QuatTS const& qt0, QuatTS const& qt1, QuatTS const& qt2)
    {
        return InterpVariance(qt0.t, qt1.t, qt2.t)
               + InterpVariance(qt0.q.GetColumn2() * qt0.s, qt1.q.GetColumn2() * qt1.s, qt2.q.GetColumn2() * qt2.s);
    }

    float Adjust(float fDensity, float fReduce)
    {
        return 1.0f / (1.0f + (fDensity - 1.0f) * fReduce);
    }
}

//////////////////////////////////////////////////////////////////////////
struct STargetForces
    : SPhysForces
{
    ParticleTarget  target;

    STargetForces()
        : SPhysForces(ZERO) {}
};

//////////////////////////////////////////////////////////////////////////
// CParticle implementation

CParticleSource& CParticle::GetSource() const
{ assert(m_pEmitter && m_pEmitter->NumRefs() > 0); return m_pEmitter->GetSource(); }
CParticleEmitter& CParticle::GetMain() const
{ return GetContainer().GetMain(); }
ResourceParticleParams const& CParticle::GetParams() const
{ return GetContainer().GetParams(); }

//////////////////////////////////////////////////////////////////////////
float CParticle::TravelSlide(SParticleState& state, SSlideInfo& sliding, float fTime, const Vec3& vExtAccel, float fMaxSlide, float fMinStepTime) const
{
    float fTravelTime = 0.f;
    while (fTime > 0.f)
    {
        // Convert wind drag to acceleration.
        float fPenAccel = vExtAccel * sliding.vNormal;
        if (fPenAccel >= 0.f)
        {
            // Not sliding along plane.
            sliding.ClearSliding(sliding.vNormal);
            break;
        }

        GetContainer().GetCounts().ParticlesReiterate += 1.f;

        // There is a force against the sliding plane.
        // Sliding if velocity is currently into the plane, or max bounce distance is less than the border.
        Vec3 vAccel = vExtAccel - sliding.vNormal * fPenAccel;
        float fPenVel = state.m_Vel.vLin * sliding.vNormal;
        state.m_Vel.vLin -= sliding.vNormal * fPenVel;
        float fFrictionAccel = sliding.fFriction * fPenAccel;
        float fVV = state.m_Vel.vLin.GetLengthSquared();
        if (fVV < FLT_EPSILON)
        {
            // Reduce acceleration by friction.
            float fAA = vAccel.GetLengthSquared();
            if (fAA <= sqr(fFrictionAccel))
            {
                // No acceleration to overcome friction.
                fTravelTime += fTime;
                break;
            }
            vAccel *= 1.f + fFrictionAccel * isqrt_tpl(fAA);
        }
        else
        {
            // Reduce acceleration in velocity direction.
            vAccel += state.m_Vel.vLin * (isqrt_tpl(fVV) * fFrictionAccel);
        }

        // Clamp based on estimated slide deviation.
        float fStepTime = fTime;
        float fMaxSlideTime = sqrt_fast_tpl(-2.f * fMaxSlide / fPenAccel);
        float fMaxTime = max(fMaxSlideTime - sliding.fSlidingTime, fMinStepTime);
        if (fMaxTime > FLT_EPSILON)
        {
            fStepTime = min(fStepTime, fMaxTime);

            // Detect slide stoppage, and limit travel time.
            float fVA = state.m_Vel.vLin * vAccel;
            if (fVV < -fVA * fStepTime)
            {
                // Stopped.
                fStepTime = -fVV / fVA;
                state.m_Loc.t += state.m_Vel.vLin * fStepTime + vAccel * (fStepTime * fStepTime * 0.5f);
                state.m_Vel.vLin.zero();
                state.m_Vel.vRot.zero();
                fTravelTime += fTime;
                fTime = 0.f;
            }
            else
            {
                state.m_Loc.t += state.m_Vel.vLin * fStepTime - vAccel * (fStepTime * fStepTime * 0.5f);
                state.m_Vel.vLin += vAccel * fStepTime;
                state.m_Vel.vRot *= fStepTime * div_min(-fVA, fVV, 1.f);
                fTravelTime += fStepTime;
                fTime -= fStepTime;
            }

            sliding.fSlidingTime += fStepTime;
        }

        if (sliding.fSlidingTime >= fMaxSlideTime - 1e-6f)
        {
            // Require retest against entity.
            ray_hit hit;
            Vec3 vTest = vExtAccel.GetNormalized(fMaxSlide);
            GetContainer().GetCounts().ParticlesClip += 1.f;
            IPhysicalEntity* pPhysicalEntityHit = sliding.GetPhysicalEntity();
            if (!SPhysEnviron::PhysicsCollision(hit, state.m_Loc.t - vTest, state.m_Loc.t + vTest,
                fCOLLIDE_BUFFER_DIST, pPhysicalEntityHit ? ENV_COLLIDE_PHYSICS : ENV_TERRAIN, pPhysicalEntityHit))
            {
                sliding.Clear();
                break;
            }

            float fPenPos = (state.m_Loc.t - hit.pt) * hit.n;
            if (fPenPos < 0.f)
            {
                state.m_Loc.t -= fPenPos * hit.n;
            }
            sliding.vNormal = hit.n;
            sliding.fSlidingTime = 0.f;
            float fBounce_;
            GetCollisionParams(hit.surface_idx, fBounce_, sliding.fFriction);
        }
    }

    return fTravelTime;
}

void CParticle::Move(SParticleState& state, float fTime, STargetForces const& forces_ext) const
{
    GetContainer().GetCounts().ParticlesReiterate += 1.f;

    ResourceParticleParams const& params = GetParams();

    float fRelativeAge = GetRelativeAge(fTime * 0.5f);

    ParticleParams::EEmitterShapeType emitterShape = GetEmitter()->GetParams().GetEmitterShape();

    Vec3 currentVelocity(0.0f);
    if (emitterShape == ParticleParams::EEmitterShapeType::CIRCLE ||
        emitterShape == ParticleParams::EEmitterShapeType::SPHERE ||
        emitterShape == ParticleParams::EEmitterShapeType::BOX ||
        emitterShape == ParticleParams::EEmitterShapeType::POINT)
    {
        currentVelocity = params.vVelocity.GetVector(GetEmitter()->GetRelativeAge(), fRelativeAge);
    }

    Vec3 vVelUser = (currentVelocity + m_preEmissionRandomVelocity) * fTime;

    QuatTS locCur(IDENTITY);
    if (emitterShape != ParticleParams::EEmitterShapeType::SPHERE &&
        emitterShape != ParticleParams::EEmitterShapeType::CIRCLE)
    {
        if (params.eMoveRelEmitter != ParticleParams::EMoveRelative::E::No)
        {
            locCur = GetSource().GetLocation();
            vVelUser = locCur.q * vVelUser * locCur.s;
        }

        state.m_Loc.t += vVelUser;
    }
    else
    {
        Quat locPrev(IDENTITY);
        if (params.eMoveRelEmitter != ParticleParams::EMoveRelative::E::No)
        {
            locCur = GetSource().GetLocation();
            locPrev = GetEmitter()->GetRelativeRotation();
            vVelUser *= locCur.s;
        }

        Vec3 origin = state.m_Loc.t - m_originalEmitterLocation;
        origin = locPrev.GetInverted() * origin;

        if (emitterShape == ParticleParams::EEmitterShapeType::SPHERE)
        {
            // x axis moves outwards from the center
            // y axis moves over the horizontal circumference of the circle
            // z axis moves over the vertical circumference of the circle 

            float radius = origin.GetLength() + vVelUser.x;

            if (radius > 0.0f)
            {
                quaternionf quatXY = quaternionf::CreateRotationZ(vVelUser.y / radius);
                quaternionf quatZ = quaternionf::CreateRotationY(vVelUser.z / radius);

                quaternionf rot = quatXY * quatZ;
                origin = (rot*origin).normalized() * radius;
            }
            else
            {
                origin = (origin.normalized() * radius);
            }
        }
        else if (emitterShape == ParticleParams::EEmitterShapeType::CIRCLE)
        {
            // x axis moves outwards from the center
            // y axis moves over the circumference of the circle
            // z axis moves over the up axis

            float radius = Vec2(origin.x, origin.y).GetLength();
            float newRadius = radius + vVelUser.x;

            if (radius > 0.0f && newRadius > 0.0f)
            {
                Vec2 originOffset = Vec2(origin.x, origin.y) / radius * newRadius;
                const float angleInRad = vVelUser.y / newRadius;

                origin.x = originOffset.x * cos_tpl(angleInRad) - originOffset.y * sin_tpl(angleInRad);
                origin.y = originOffset.x * sin_tpl(angleInRad) + originOffset.y * cos_tpl(angleInRad);
            }

            origin += Vec3(0,0, vVelUser.z);
        }

        Vec3 rotatedOrigin = locCur.q * origin;
        state.m_Loc.t = rotatedOrigin + m_originalEmitterLocation;
    }
    
    Vec3 vVelOrig = state.m_Vel.vLin;

    SForceParams forces;
    forces.vAccel = params.vAcceleration + forces_ext.vAccel * params.fGravityScale.GetValueFromMod(m_BaseMods.GravityScale, fRelativeAge);
    forces.vWind = forces_ext.vWind * params.fAirResistance.fWindScale;
    forces.fDrag = params.fAirResistance.GetValueFromMod(m_BaseMods.AirResistance, fRelativeAge);

    if (params.fTurbulence3DSpeed)
    {
        /*
            Random acceleration A (wind velocity times air resistance) applied continuously.
            In time quantum dt, dV = A dt, and dP = A/2 dt^2.
            In larger time periods, avg dv(t) = a dt sqrt(t/dt), and avg dp(t) = a/2 dt sqrt(t/dt) t.
            So effective acceleration ae(t) = dv(t) / t = a dt sqrt(t/dt) / t = a sqrt(dt/t)

            Turbulence3DSpeed parameter indicates average velocity change dV in 1 s = dt,
            therefore, the param is used as the acceleration base, and ae(t) = A / sqrt(t)
        */

        float fTurb = params.fTurbulence3DSpeed.GetValueFromMod(m_BaseMods.Turbulence3DSpeed, fRelativeAge);
        fTurb *= isqrt_tpl(fTime);

        Vec3 vTurbulence;
        for (int a = 0; a < 3; a++)
        {
            CChaosKey key(*(uint32*)&state.m_Loc.t[a]);
            vTurbulence[a] = (key.Jumble(this) * 2.f - 1.f) * fTurb;
        }
        forces.vAccel += vTurbulence;
    }

    // 2D spiral vortex.
    if (params.fTurbulenceSize * params.fTurbulenceSpeed != 0.f)
    {
        // Apply exact rotation differences between this and next step.
        // Do not adjust velocity, this is an artificial spiral motion.
        // However, GetVisualVelocity() returns the effective velocity with vortex motion.
        Vec3 vRotate = VortexRotation(state, false);

        Travel::Travel(state.m_Loc.t, state.m_Vel.vLin, fTime, forces);

        state.m_Loc.t += VortexRotation(state, false, fTime) - vRotate;
    }
    else
    {
        Travel::Travel(state.m_Loc.t, state.m_Vel.vLin, fTime, forces);
    }

    // Compute targeting motion.
    if (forces_ext.target.bTarget)
    {
        state.m_Vel.vLin = (state.m_Vel.vLin + vVelOrig) * 0.5f;
        TargetMovement(forces_ext.target, state, fTime, fRelativeAge);
    }

    if (forces.fDrag * params.fAirResistance.fRotationalDragScale > 0.f)
    {
        state.m_Vel.vRot *= expf(-forces.fDrag * params.fAirResistance.fRotationalDragScale * fTime);
        if (state.m_Vel.vRot.GetLengthSquared() == 0.f)
        {
            state.m_Vel.vRot.zero();
        }
    }
}

#if defined(_DEBUG)
// Debug particles out of static bounds
void CParticle::DebugBounds(SParticleState const& state) const
{
    if (GetCVars()->e_ParticlesDebug & AlphaBit('b'))
    {
        static const Color3B clrDebug(0.99f, 0.01f, 0.15f);
        AABB const& bbStatic = GetContainer().GetStaticBounds();
        if (m_BaseMods.Color != clrDebug && !bbStatic.IsReset() && GetContainer().StaticBoundsStable())
        {
            AABB bbPart(AABB::RESET);
            UpdateBounds(bbPart, state);
            if (!bbStatic.ContainsBox(bbPart))
            {
                AABB bbErr = bbPart;
                bbPart.ClipToBox(bbStatic);
                bbErr.min -= bbPart.min;
                bbErr.max -= bbPart.max;
                non_const(*this).m_BaseMods.Color = clrDebug;
            }
        }
    }
}
#endif

//
// Limit travel time to avoid excessive curvature.
//
float CParticle::MoveLinear(SParticleState& state, SCollisionInfo& coll, float fTime, STargetForces const& forces, float fMaxLinearDev, float fMaxSlideDev, float fMinStepTime) const
{
    // Clamp time based on turbulence params.

    if (coll.Sliding.IsSliding())
    {
        // Move slightly forward, to infer accelerations.
        SParticleState stateTest = state;
        Move(stateTest, fSLIDE_TEST_AHEAD_TIME, forces);

        Vec3 vExtAccel = (stateTest.m_Vel.vLin - state.m_Vel.vLin) / fSLIDE_TEST_AHEAD_TIME;
        float fSlideTime = TravelSlide(state, coll.Sliding, fTime, vExtAccel, fMaxSlideDev, fMinStepTime);
        if (!coll.Sliding.IsSliding())
        {
            coll.Hit.Clear();
        }
        if (fSlideTime > 0.f)
        {
            return fSlideTime;
        }
    }

    SParticleState stateNew = state;

    Move(stateNew, fTime, forces);

    if (fMaxLinearDev < fHUGE * 0.5)
    {
        // If curved path encompasses inflection point relative to last collide normal, split in 2.
        float fNV0 = state.m_Vel.vLin * coll.Sliding.vNormal;
        float fNV1 = stateNew.m_Vel.vLin * coll.Sliding.vNormal;
        if (fNV0 * fNV1 < 0.f)
        {
            // Stop just after inflection point -- increase threshold for further tests.
            float fCorrect = abs(fNV0 / (fNV0 - fNV1));
            fTime *= min(fCorrect + 0.1f, 0.9f);
            stateNew = state;
            Move(stateNew, fTime, forces);
        }

        if (fTime > fMinStepTime)
        {
            Vec3 vDVel = stateNew.m_Vel.vLin - state.m_Vel.vLin;
            Vec3 vDPos = stateNew.m_Loc.t - state.m_Loc.t;
            float fDistSqr = vDPos.len2();
            float fDevSqrN = (vDVel.len2() * fDistSqr - sqr(vDVel * vDPos)) * sqr(fTime * 0.125f);
            if (fDevSqrN > fDistSqr * sqr(fMaxLinearDev))
            {
                // Exceeds linear threshold. Deviation proportional to t
                float fCorrect = fMaxLinearDev * isqrt_fast_tpl(fDevSqrN / fDistSqr);
                float fNewTime = max(fTime * fCorrect * 0.75f, fMinStepTime);
                if (fNewTime < fTime * 0.99f)
                {
                    stateNew = state;
                    fTime = fNewTime;
                    Move(stateNew, fTime, forces);
                }
            }
        }
    }

    state = stateNew;
    return fTime;
}

bool CParticle::SHitInfo::TestHit(ray_hit& hit, const Vec3& vPos0, const Vec3& vPos1, const Vec3& vVel0, const Vec3& vVel1, float fMaxDev, float fRadius) const
{
    if (!HasPath())
    {
        return false;
    }

    if (HasHit())
    {
        // Test against collision plane.
        float fDist0 = (vPos0 - vPos) * vNormal;
        float fDist1 = (vPos1 - vPos) * vNormal - fRadius;
        if (fDist1 < 0.f)
        {
            if (fDist0 >= 0.f)
            {
                hit.dist = fDist0 / (fDist0 - fDist1);
                hit.pt.SetLerp(vPos0, vPos1, hit.dist);
            }
            else
            {
                hit.dist = 0.f;
                hit.pt = vPos;
            }

            // Find inflection point.
            float fV0 = vVel0 * vNormal,
                  fV1 = vVel1 * vNormal;
            if (fV0 > 0.f && fV1 < 0.f)
            {
                /*
                    c =  v0 + p0 - p1
                    d = -v1 - p0 + p1

                    p = p0 * (1-t) + p1 * t + (c * (1-t) + d * t) * t * (1-t) * 4
                        = p0 + (p1-p0) t + (c + (d-c) t) t(1-t)
                */
                float fMid = fV0 / (fV0 - fV1);
                Vec3 vVelMid;
                vVelMid.SetLerp(vVel0, vVel1, fMid);
                Vec3 vPosMid;
                vPosMid.SetLerp(vPos0, vPos1, fMid);
                vPosMid += (vPos0 - vPos1 + vVel0) * (fMid * (1.f - fMid) * (1.f - fMid));
                vPosMid += (vPos1 - vPos0 - vVel1) * (fMid * fMid * (1.f - fMid));
                float fDistMid = (vPosMid - vPos) * vNormal;
                if (fDistMid > 0.f)
                {
                    float fDist = fDistMid / (fDistMid - fDist1);
                    hit.pt.SetLerp(vPosMid, vPos1, fDist);
                    hit.dist = fMid + (1.f - fMid) * fDist;
                    assert(hit.dist >= 0.f && hit.dist <= 1.f);
                }
            }

            if ((hit.pt - vPos).len2() > sqr(fMaxDev))
            {
                hit.dist = 1.f;
                return false;
            }

            hit.n = vNormal;
            hit.pt += hit.n * fRadius;
            hit.surface_idx = nSurfaceIdx;
            hit.pCollider = GetPhysicalEntity();
            return true;
        }
    }

    // Test path proximity for hit or miss.
    hit.dist = 1.f;

    Vec3 vPosRel = vPos1 - vPos;
    float fT = vPosRel * vPathDir;
    if (fT >= -fMaxDev && fT <= fPathLength + fMaxDev)
    {
        if ((vPathDir * fT - vPosRel).len2() <= sqr(fMaxDev))
        {
            return true;
        }
    }

    return false;
}

bool CParticle::CheckCollision(ray_hit& hit, float fStepTime, SParticleUpdateContext const& context, STargetForces const& forces, const SParticleState& stateNew, SCollisionInfo& collNew)
{
    hit.dist = 1.f;
    uint32 nCollideFlags = context.nEnvFlags & ENV_COLLIDE_ANY;
    if (nCollideFlags & ENV_COLLIDE_CACHE && GetCVars()->e_ParticlesObjectCollisions < 3)
    {
        // Cache collisions ahead in extended path.
        if (collNew.Hit.TestHit(hit, m_Loc.t, stateNew.m_Loc.t, m_Vel.vLin, stateNew.m_Vel.vLin, fMAX_COLLIDE_DEVIATION, fCOLLIDE_BUFFER_DIST))
        {
            nCollideFlags &= ~ENV_COLLIDE_CACHE;
        }
        else
        {
            // Path does not match cache.
            // Require new check.
            collNew.Hit.Clear();
            float fTestTime = min((m_fStopAge - m_fAge) * 1.1f, context.fMaxLinearStepTime);
            if (fTestTime >= fStepTime * fMIN_TEST_AHEAD_MULT)
            {
                // Predict travel ahead, ignoring random movement params.
                SParticleState stateTest = *this;
                SCollisionInfo collTest;
                if (m_pCollisionInfo)
                {
                    collTest = *m_pCollisionInfo;
                }
                else
                {
                    collTest.Sliding.vNormal = stateTest.m_Vel.vLin.GetNormalized();
                }

                fTestTime = MoveLinear(stateTest, collTest, fTestTime, forces, fMAX_COLLIDE_DEVIATION, fHUGE, fStepTime);
                if (fTestTime >= fStepTime * fMIN_TEST_AHEAD_MULT && stateTest.m_Loc.t != m_Loc.t)
                {
                    ray_hit hit_cache;
                    GetContainer().GetCounts().ParticlesCollideTest += 1.f;
                    if (SPhysEnviron::PhysicsCollision(hit_cache, m_Loc.t, stateTest.m_Loc.t, 0.f, nCollideFlags & ENV_COLLIDE_CACHE))
                    {
                        collNew.Hit.SetHit(m_Loc.t, hit_cache.pt, hit_cache.n, hit_cache.surface_idx, hit_cache.pCollider);
                    }
                    else
                    {
                        collNew.Hit.SetMiss(m_Loc.t, stateTest.m_Loc.t);
                    }

                    if (collNew.Hit.TestHit(hit, m_Loc.t, stateNew.m_Loc.t, m_Vel.vLin, stateNew.m_Vel.vLin, fMAX_COLLIDE_DEVIATION, fCOLLIDE_BUFFER_DIST))
                    {
                        nCollideFlags &= ~ENV_COLLIDE_CACHE;
                    }
                }
            }
        }
        if (hit.dist < 1.f)
        {
            collNew.Clear();
        }
#ifdef _DEBUG
        else if (!(nCollideFlags & ENV_COLLIDE_CACHE))
        {
            int& iDrawHelpers = gEnv->pPhysicalWorld->GetPhysVars()->iDrawHelpers;
            int iSave = iDrawHelpers;
            iDrawHelpers = 0;
            ray_hit hit2;
            if (SPhysEnviron::PhysicsCollision(hit2, m_Loc.t, stateNew.m_Loc.t, fCOLLIDE_BUFFER_DIST, context.nEnvFlags & ENV_COLLIDE_CACHE))
            {
                GetContainer().GetCounts().ParticlesReject += 1.f;
            }
            iDrawHelpers = iSave;
        }
#endif
    }
    if (nCollideFlags)
    {
        ray_hit hit2;
        GetContainer().GetCounts().ParticlesCollideTest += 1.f;
        if (SPhysEnviron::PhysicsCollision(hit2, m_Loc.t, stateNew.m_Loc.t, fCOLLIDE_BUFFER_DIST, nCollideFlags))
        {
            if (hit2.dist < hit.dist)
            {
                hit = hit2;
            }
        }
    }

    return hit.dist < 1.f;
}

void CParticle::Init(SParticleUpdateContext const& context, float fAge, CParticleSubEmitter* pEmitter, const EmitParticleData& data)
{
    assert(pEmitter);
    PREFAST_ASSUME(pEmitter);

    pEmitter->AddRef();
    m_pEmitter = pEmitter;
    m_pContainer = &pEmitter->GetContainer();
    m_segmentCount = 0;
    CParticleContainer& rContainer = GetContainer();
    ResourceParticleParams const& params = rContainer.GetParams();
    SpawnParams const& spawnParams = GetMain().GetSpawnParams();

    m_originalEmitterLocation = Vec3(0, 0, 0);
    
    // Init allocations.
    m_aPosHistory = 0;
    if (int nSteps = rContainer.GetHistorySteps())
    {
        if ((m_aPosHistory = static_cast<SParticleHistory*>(ParticleObjectAllocator().Allocate(sizeof(SParticleHistory) * nSteps))))
        {
            for (int n = 0; n < nSteps; n++)
            {
                m_aPosHistory[n].SetUnused();
            }
        }
    }

    m_pCollisionInfo = 0;
    if (rContainer.NeedsCollisionInfo())
    {
        bool bCollide = RandomActivate(params.fCollisionFraction);
        if (bCollide && params.fCollisionCutoffDistance)
        {
            const float fDistSq = (data.Location.t - gEnv->p3DEngine->GetRenderingCamera().GetPosition()).GetLengthSquared();
            bCollide = fDistSq <= sqr(params.fCollisionCutoffDistance);
        }

        if (bCollide)
        {
            if ((m_pCollisionInfo = static_cast<SCollisionInfo*>(ParticleObjectAllocator().Allocate(sizeof(SCollisionInfo)))))
            {
                //nMaxCollisionEvents is only used for RigidBodyCollision. All the other physics won't limit collision events count (using 0).
                new (m_pCollisionInfo)SCollisionInfo(params.ePhysicsType == params.ePhysicsType.RigidBody?params.nMaxCollisionEvents:0);
            }
        }
    }

    // Init all base values.
    float fEmissionStrength = pEmitter->GetStrength(-fAge);

    m_fAge = 0.f;
    if (!params.fParticleLifeTime)
    {
        m_fStopAge = max(pEmitter->GetStopAge() - GetSource().GetAge(), 0.f);
    }
    else if (params.GetEmitterShape() == ParticleParams::EEmitterShapeType::BEAM)
    {
        m_fStopAge = data.fBeamAge * Adjust(context.fDensityAdjust, params.fMaintainDensity.fReduceLifeTime);
    }
    else
    {
        m_fStopAge = params.fParticleLifeTime(VRANDOM, fEmissionStrength) * Adjust(context.fDensityAdjust, params.fMaintainDensity.fReduceLifeTime);
    }

    // Init base mods.
    m_BaseMods.SizeX = params.fSizeX.GetVarMod(fEmissionStrength);
    // We copy the size X modifier in case we're maintaining aspect ratio because it just contains the randomness/emitter strength component of the size
    m_BaseMods.SizeY = params.bMaintainAspectRatio ? m_BaseMods.SizeX : static_cast<TFixed<unsigned char, 1>>(params.fSizeY.GetVarMod(fEmissionStrength));
    m_BaseMods.SizeZ = params.bMaintainAspectRatio ? m_BaseMods.SizeX : static_cast<TFixed<unsigned char, 1>>(params.fSizeZ.GetVarMod(fEmissionStrength));
    m_BaseMods.PivotX = params.fPivotX.GetVarMod(fEmissionStrength);
    m_BaseMods.PivotY = params.fPivotY.GetVarMod(fEmissionStrength);
    m_BaseMods.PivotZ = params.fPivotZ.GetVarMod(fEmissionStrength);
    m_BaseMods.StretchOrTail = params.fTailLength ? params.fTailLength.GetVarMod(fEmissionStrength) : params.fStretch.GetVarMod(fEmissionStrength);

    m_BaseMods.AirResistance = params.fAirResistance.GetVarMod(fEmissionStrength);
    m_BaseMods.GravityScale = params.fGravityScale.GetVarMod(fEmissionStrength);
    m_BaseMods.Turbulence3DSpeed = params.fTurbulence3DSpeed.GetVarMod(fEmissionStrength);
    m_BaseMods.TurbulenceSize = params.fTurbulenceSize.GetVarMod(fEmissionStrength);
    m_BaseMods.TurbulenceSpeed = params.fTurbulenceSpeed.GetVarMod(fEmissionStrength);
    m_BaseMods.fTargetRadius = params.TargetAttraction.fRadius.GetVarMod(fEmissionStrength);
    m_BaseMods.RotateRateX = params.vRotationRateX.GetVarMod(fEmissionStrength);
    m_BaseMods.RotateRateY = params.vRotationRateY.GetVarMod(fEmissionStrength);
    m_BaseMods.RotateRateZ = params.vRotationRateZ.GetVarMod(fEmissionStrength);

    m_BaseMods.LightSourceIntensity = params.LightSource.fIntensity.GetVarMod(fEmissionStrength);
    m_BaseMods.LightSourceRadius = params.LightSource.fRadius.GetVarMod(fEmissionStrength);

    m_BaseMods.ColorLerp = params.cColor.GetLerp();
    m_BaseMods.Color = params.cColor.GetVarMod(fEmissionStrength, m_BaseMods.ColorLerp);
    m_BaseMods.Alpha = params.fAlpha.GetVarMod(fEmissionStrength, m_BaseMods.ColorLerp) * context.fDensityAdjust;

    m_initialEmissionStrength = fEmissionStrength;

    //init size scale
    CRY_ASSERT(spawnParams.particleSizeScaleRandom <= 1);
    float scaleRandom = cry_random<float> (0, spawnParams.particleSizeScaleRandom);
    m_sizeScale = spawnParams.particleSizeScale * ((1.f - spawnParams.particleSizeScaleRandom * 0.5f) + scaleRandom);

    m_preEmissionRandomVelocity = Vec3(ZERO);
    if (params.GetEmitterShape() == ParticleParams::EEmitterShapeType::POINT ||
        params.GetEmitterShape() == ParticleParams::EEmitterShapeType::CIRCLE ||
        params.GetEmitterShape() == ParticleParams::EEmitterShapeType::SPHERE ||
        params.GetEmitterShape() == ParticleParams::EEmitterShapeType::BOX)
    {
        m_preEmissionRandomVelocity = params.vVelocityXYZRandom.GetVector(fEmissionStrength);
        m_preEmissionRandomVelocity = m_preEmissionRandomVelocity.CompMul(Vec3(cry_random(-1.0f, 1.0f), cry_random(-1.0f, 1.0f), cry_random(-1.0f, 1.0f)));
    }

    m_nEmitterSequence = pEmitter->GetSequence();
    m_indexInSequence = pEmitter->GetEmitIndex();

    // Tile variant is randomized per stream for connected particles, per particle otherwise.
    if (params.IsConnectedParticles())
    {
        m_nTileVariant = pEmitter->GetChaosKey().Jumble(CChaosKey(&params.TextureTiling.nVariantCount)) * params.TextureTiling.nVariantCount;
    }
    else
    {
        m_nTileVariant = cry_random(0U, params.TextureTiling.nVariantCount - 1);
    }
    m_nTileVariant = params.TextureTiling.nFirstTile + m_nTileVariant * params.TextureTiling.nAnimFramesCount;

    m_bHorizontalFlippedTexture = RandomActivate(params.TextureTiling.fHorizontalFlipChance);
    m_bVerticalFlippedTexture = RandomActivate(params.TextureTiling.fVerticalFlipChance);
    // Tile variant is randomized per stream for connected particles, per particle otherwise.
    if (params.IsConnectedParticles())
    {
        m_nTileVariant = pEmitter->GetChaosKey().Jumble(CChaosKey(&params.TextureTiling.nVariantCount)) * params.TextureTiling.nVariantCount;
    }
    else
    {
        m_nTileVariant = cry_random(0U, params.TextureTiling.nVariantCount - 1);
    }
    m_nTileVariant = params.TextureTiling.nFirstTile + m_nTileVariant * params.TextureTiling.nAnimFramesCount;

    if (data.pStatObj)
    {
        Set(data.pStatObj);
        SetCentered(!params.bNoOffset && params.ePhysicsType != params.ePhysicsType.RigidBody);
    }
    Set(data.pPhysEnt);

    Vec3 emitterLocation = data.Location.t;

    IF (data.pPhysEnt && data.pStatObj, false)
    {
        // Pre-generated physics entity.
        m_pPhysEnt->AddRef();
        pe_params_foreign_data pfd;
        pfd.iForeignData = PHYS_FOREIGN_ID_RIGID_PARTICLE;
        pfd.pForeignData = data.pPhysEnt;
        m_pPhysEnt->SetParams(&pfd);

        // Get initial state.
        GetPhysicsState();

        m_Loc.s = params.fSizeY.GetMaxValue() * data.Location.s * m_sizeScale.y;
    }
    else
    {
        // Init location and velocity, overriding with data params if specified.
        if (!data.bHasLocation || !data.bHasVel)
        {
            InitPos(context, data.Location, fEmissionStrength);
        }
        if (data.bHasLocation)
        {
            m_Loc = data.Location;
        }
        if (data.bHasVel)
        {
            m_Vel = data.Velocity;
        }

        if (m_pCollisionInfo && params.ePhysicsType >= params.ePhysicsType.SimplePhysics)
        {
            // Emitter-generated physical particles.
            Physicalize();
        }
    }

    m_originalEmitterLocation += emitterLocation;

    if (data.pStatObj)
    {
        data.pStatObj->AddRef();
    }

    if (m_pPhysEnt && GetMain().GetVisEnviron().OriginIndoors())
    {
        pe_params_flags pf;
        pf.flagsOR = pef_ignore_ocean;
        m_pPhysEnt->SetParams(&pf);
    }

    Update(context, fAge, true);
}

////////////////////////////////////////////////////////////////////////////
void CParticle::InitPos(SParticleUpdateContext const& context, QuatTS const& loc, float fEmissionStrength)
{
    ResourceParticleParams const& params = GetParams();
    SpawnParams const& spawnParams = GetMain().GetSpawnParams();
    SPhysEnviron const& PhysEnv = GetMain().GetPhysEnviron();
    const CParticleSource& rSource = GetSource();
    ParticleParams::EEmitterShapeType shape = params.GetEmitterShape();

    // Position and orientation.
    // Use main emitter scale for size, not parent particle size.
    m_Loc = loc;
    m_Loc.s = GetMain().GetParticleScale();

    // Emit geom.
    EGeomType eAttachType = params.eSpawnIndirection ? params.eAttachType : spawnParams.eAttachType;
    if (eAttachType != GeomType_None && rSource.GetEmitGeom())
    {
        EGeomForm   eAttachForm = params.eSpawnIndirection ? params.eAttachForm : spawnParams.eAttachForm;
        PosNorm ran = { m_Loc.t, m_Vel.vLin };
        rSource.GetEmitGeom().GetRandomPos(ran, eAttachForm, loc);
        m_Loc.t = ran.vPos;
        m_Vel.vLin = ran.vNorm;
    }
    else
    {
        if (shape == ParticleParams::EEmitterShapeType::ANGLE || 
            shape == ParticleParams::EEmitterShapeType::TRAIL || 
            shape == ParticleParams::EEmitterShapeType::BEAM)
        {
            // Focus direction.
            m_Vel.vLin = GetEmitter()->GetEmitFocusDir(loc, fEmissionStrength, params.bFocusRotatesEmitter ? &m_Loc.q : 0);
        }
    }

    // Velocity part 1. (for point emitter to determine spawn offset)
    if (shape == ParticleParams::EEmitterShapeType::POINT)
    {
        float coneCosHalfRad = cos(DEG2RAD(params.fVelocitySpread(VRANDOM, fEmissionStrength) * 0.5f));

        float z = cry_random(0.0f, 1.0f) * (1 - coneCosHalfRad) + coneCosHalfRad;
        float phi = cry_random(0.0f, 1.0f) * (float)g_PI2;
        float x = sqrt(1 - z * z) * cos_tpl(phi);
        float y = sqrt(1 - z * z) * sin_tpl(phi);

        Vec3 point(x, y, z);

        Vec3 dir = params.vVelocity.GetVector();
        if (!dir.IsZeroFast())
        {
            quaternion quat = quaternion::CreateRotationV0V1(Vec3(0,0,1),dir.normalized());

            point = (quat * point).normalized();
        }

        m_Vel.vLin = point;
    }

    // Emitter Offset.
    m_originalEmitterLocation += params.GetEmitterOffset(context.vEmitBox, context.vEmitScale, fEmissionStrength, params.vSpawnPosOffset, params.vSpawnPosRandomOffset);

    // Particle Position Offset for Shape Emitters.
    Vec3 vLocalPosition(0,0,0);
    {
        const float spawnPosX = params.vSpawnPosXYZ.fX(VRANDOM, fEmissionStrength);
        const float spawnPosY = params.vSpawnPosXYZ.fY(VRANDOM, fEmissionStrength);
        const float spawnPosZ = params.vSpawnPosXYZ.fZ(VRANDOM, fEmissionStrength);

        const float spawnPosRandX = params.vSpawnPosXYZRandom.fX(VRANDOM, fEmissionStrength) * cry_random(-1.0f, 1.0f);
        const float spawnPosRandY = params.vSpawnPosXYZRandom.fY(VRANDOM, fEmissionStrength) * cry_random(-1.0f, 1.0f);
        const float spawnPosRandZ = params.vSpawnPosXYZRandom.fZ(VRANDOM, fEmissionStrength) * cry_random(-1.0f, 1.0f);

        if (shape == ParticleParams::EEmitterShapeType::SPHERE || 
            shape == ParticleParams::EEmitterShapeType::CIRCLE)
        {
            const float incrementRandX = params.vSpawnPosIncrementXYZRandom.fX(VRANDOM, fEmissionStrength) * cry_random(-1.0f, 1.0f);
            const float incrementRandY = params.vSpawnPosIncrementXYZRandom.fY(VRANDOM, fEmissionStrength) * cry_random(-1.0f, 1.0f);
            const float incrementRandZ = params.vSpawnPosIncrementXYZRandom.fZ(VRANDOM, fEmissionStrength) * cry_random(-1.0f, 1.0f);

            const float incrementX = params.vSpawnPosIncrementXYZ.fX(VRANDOM, fEmissionStrength) + incrementRandX;
            const float incrementY = params.vSpawnPosIncrementXYZ.fY(VRANDOM, fEmissionStrength) + incrementRandY;
            const float incrementZ = params.vSpawnPosIncrementXYZ.fZ(VRANDOM, fEmissionStrength) + incrementRandZ;

            const float radius = params.fEmitterSizeDiameter(VRANDOM, fEmissionStrength) * 0.5f;
            const float circumference = gf_PI2 * radius;
            const float percentageToUniformScale = 0.01f;

            Vec3 positionIncrement = m_pEmitter->GetCurrentPosIncrementXYZ();

            if (shape == ParticleParams::EEmitterShapeType::SPHERE)
            {
                // x axis moves outwards from the center
                // y axis moves over the horizontal circumference of the circle
                // z axis moves over the vertical circumference of the circle 

                float integer = 0;
                float scaleOverX = percentageToUniformScale * (incrementX + positionIncrement.x);
                scaleOverX = fmodf(scaleOverX, 1.0f);
                
                // position increment moves outwards
                float positionIncrementX = scaleOverX * radius;

                const float offsetX = -(radius + spawnPosX + spawnPosRandX + positionIncrementX);

                vLocalPosition = Vec3(0, offsetX, 0);

                if (radius > 0.0f)
                {
                    const float percentageOfLengthOverY = circumference * percentageToUniformScale * (positionIncrement.y + incrementY);
                    const float percentageOfLengthOverZ = circumference * percentageToUniformScale * (positionIncrement.z + incrementZ);

                    float yAngleInRad = static_cast<float>(g_PI) * 0.5f - (spawnPosY + spawnPosRandY + percentageOfLengthOverY) / radius;
                    float zAngleInRad = -(spawnPosZ + spawnPosRandZ + percentageOfLengthOverZ) / radius;

                    // Move over y and z axis, creates a figure 8 over time
                    vLocalPosition.x = -cos_tpl(yAngleInRad) * cos_tpl(zAngleInRad) * offsetX;
                    vLocalPosition.y = sin_tpl(yAngleInRad) * offsetX;
                    vLocalPosition.z = -cos_tpl(yAngleInRad) * sin_tpl(zAngleInRad) * offsetX;
                }
            }
            else if (shape == ParticleParams::EEmitterShapeType::CIRCLE)
            {
                // x axis moves outwards from the center
                // y axis moves over the circumference of the circle
                // z axis moves over the up axis

                float scaleOverX = percentageToUniformScale * (incrementX + positionIncrement.x);
                scaleOverX = fmodf(scaleOverX, 1.0f);

                float scaleOverZ = percentageToUniformScale * (incrementZ + positionIncrement.z);
                scaleOverZ = fmodf(scaleOverZ, 1.0f);

                // position increment moves outwards
                float positionIncrementX = scaleOverX * radius;
                float positionIncrementZ = scaleOverZ * radius;
                
                const float offsetX = -(radius + spawnPosX + spawnPosRandX + positionIncrementX);
                const float offsetZ = spawnPosZ + spawnPosRandZ + positionIncrementZ;


                if (radius > 0.0f)
                {
                    const float percentageOfLengthOverY = circumference * percentageToUniformScale * (positionIncrement.y + incrementY);

                    const float angleInRad = (float)g_PI * 0.5f + (spawnPosY + spawnPosRandY + percentageOfLengthOverY) / radius;
                    vLocalPosition.x = cos_tpl(angleInRad) * offsetX;
                    vLocalPosition.y = sin_tpl(angleInRad) * offsetX;
                    vLocalPosition.z = offsetZ;
                }
            }

            m_pEmitter->UpdatePosIncrementXYZ(Vec3(incrementX, incrementY, incrementZ));
        }
        else
        {
            if (shape == ParticleParams::EEmitterShapeType::POINT)
            {
                const float offsetX = spawnPosX + spawnPosRandX;
                const float offsetY = spawnPosY + spawnPosRandY;
                const float offsetZ = spawnPosZ + spawnPosRandZ;

                vLocalPosition = Vec3(offsetX, offsetY, offsetZ);
                vLocalPosition += m_Vel.vLin * params.fSpawnOffset(VRANDOM, fEmissionStrength);
            }
            else if (shape == ParticleParams::EEmitterShapeType::BOX)
            {
                const float emitterSizeX = params.vEmitterSizeXYZ.fX(VRANDOM, fEmissionStrength) * WIDTH_TO_HALF_WIDTH;
                const float emitterSizeY = params.vEmitterSizeXYZ.fY(VRANDOM, fEmissionStrength) * WIDTH_TO_HALF_WIDTH;
                const float emitterSizeZ = params.vEmitterSizeXYZ.fZ(VRANDOM, fEmissionStrength) * WIDTH_TO_HALF_WIDTH;

                const float offsetX = emitterSizeX + spawnPosX + spawnPosRandX;
                const float offsetY = -emitterSizeY + spawnPosY + spawnPosRandY;
                const float offsetZ = -emitterSizeZ + spawnPosZ + spawnPosRandZ;
                vLocalPosition = Vec3(offsetX, offsetY, offsetZ);
            }
        }
    }

    Vec3 vParticlePos = m_originalEmitterLocation + vLocalPosition;

    if (params.bEmitOffsetDir)
    {
        m_Vel.vLin = m_Loc.q * vParticlePos.GetNormalized();
    }

    // Velocity part 2.
    if (shape == ParticleParams::EEmitterShapeType::ANGLE)
    {
        // calculate velocity like normal
        float fPhi = params.fEmitAngle(VRANDOM, fEmissionStrength);
        if ((fPhi > 0.f) && (m_Vel.vLin.len2() > FLT_EPSILON))
        {
            //
            // Adjust angle to create a uniform distribution.
            //
            //      density distribution d(phi) = sin phi
            //      cumulative density c(phi) = Int(0,phi) sin x dx = 1 - cos phi
            //      normalized cn(phi) = (1 - cos phi) / (1 - cos phiMax)
            //      reverse function phi(cn) = acos_tpl(1 + cn(cos phiMax - 1))
            //

            float fPhiMax = params.fEmitAngle.GetMaxValue();
            fPhi /= fPhiMax;
            fPhi = acos_tpl(1.f + fPhi * (cos_tpl(DEG2RAD(fPhiMax)) - 1.f));

            float fTheta = cry_random(0.0f, DEG2RAD(360));

            float fPhiCS[2], fThetaCS[2];
            sincos_tpl(fPhi, &fPhiCS[1], &fPhiCS[0]);
            sincos_tpl(fTheta, &fThetaCS[1], &fThetaCS[0]);

            // Compute perpendicular bases.
            Vec3 vX;
            // Must compare against FLT_EPSILON - if y&z are < FLT_EPSILON then the NormalizeFast will give nan's
            if (fabs_tpl(m_Vel.vLin.z) > FLT_EPSILON)
            {
                vX(0.f, -m_Vel.vLin.z, m_Vel.vLin.y);
            }
            else
            {
                vX(-m_Vel.vLin.y, m_Vel.vLin.x, 0.f);
            }
            vX.NormalizeFast();
            Vec3 vY = m_Vel.vLin ^ vX;

            m_Vel.vLin = m_Vel.vLin * fPhiCS[0] + (vX * fThetaCS[0] + vY * fThetaCS[1]) * fPhiCS[1];
        }
    }

    // To world orientation/scale.
    if (params.eMoveRelEmitter != ParticleParams::EMoveRelative::E::No)
    {
        m_Loc.t += m_Loc.q * vParticlePos * m_Loc.s;

        // we rotate the velocity for the point emitter since its outwards movement is driven by the velocity
        if (shape == ParticleParams::EEmitterShapeType::POINT)
        {
            m_Vel.vLin = m_Loc.q * m_Vel.vLin;
        }
    }
    else
    {
        m_Loc.t += m_Loc.q * m_originalEmitterLocation * m_Loc.s;
        m_Loc.t += vLocalPosition * m_Loc.s;
    }

    // Speed.
    float fSpeed = params.fSpeed(VRANDOM, fEmissionStrength);
    m_Vel.vLin *= fSpeed * spawnParams.fSpeedScale * m_Loc.s;
    if (!params.eMoveRelEmitter && params.fInheritVelocity)
    {
        m_Vel.vLin += rSource.GetVelocityAt(m_Loc.t) * params.fInheritVelocity;
    }

    // Size.
    m_Loc.s *= params.fSizeY.GetValueFromMod(m_BaseMods.SizeY, 0.f) * m_sizeScale.y;

    // Initial orientation.
    if (context.b3DRotation)
    {
        // 3D absolute rotation. Compute quat from init angles, convert to world space.
        Vec3 r = params.vRandomAngles;
        Vec3 vAngles = DEG2RAD(params.vInitAngles + cry_random_componentwise(-r, r));
        m_Loc.q = m_Loc.q * Quat::CreateRotationXYZ(Ang3(vAngles));
        Vec3 vRotationRate;
        vRotationRate.Set(params.vRotationRateX.GetValueFromMod(m_BaseMods.RotateRateX, 0.f)
            , params.vRotationRateY.GetValueFromMod(m_BaseMods.RotateRateY, 0.f)
            , params.vRotationRateZ.GetValueFromMod(m_BaseMods.RotateRateZ, 0.f)
            );
        m_Vel.vRot = m_Loc.q * DEG2RAD(vRotationRate);
        m_fAngle = 0.f;
    }
    else
    {
        // 2D relative rotation about Y.
        m_fAngle = DEG2RAD(params.vInitAngles.y + cry_random(-1.0f, 1.0f) * params.vRandomAngles.y);
        m_Vel.vRot.y = DEG2RAD(params.vRotationRateY.GetValueFromMod(m_BaseMods.RotateRateY, 0.f));
    }

    // Rotate to correct alignment.
    Plane plWater;
    if (params.eFacing == params.eFacing.Water)
    {
        GetMain().GetPhysEnviron().GetWaterPlane(plWater, m_Loc.t);
    }
    UpdateAlignment(*this, context, plWater);
    DebugBounds(*this);
    m_initialOrientation = m_Loc.q;
}

void CParticle::UpdateBounds(AABB& bb, SParticleState const& state) const
{
    ParticleParams const& params = GetParams();
    float fRadius = state.m_Loc.s * GetBaseRadius();
    bb.Add(state.m_Loc.t, fRadius);

    if (m_aPosHistory && m_aPosHistory[0].IsUsed())
    {
        // Add oldest element.
        bb.Add(m_aPosHistory[0].Loc.t, fRadius);
    }
    else if (params.fStretch)
    {
        float fStretch = params.fStretch.GetValueFromMod(m_BaseMods.StretchOrTail, GetRelativeAge());
        Vec3 vStretch = m_Vel.vLin * fStretch;
        bb.Add(state.m_Loc.t + vStretch * (1.f + params.fStretch.fOffsetRatio), fRadius);
        bb.Add(state.m_Loc.t - vStretch * (1.f - params.fStretch.fOffsetRatio), fRadius);
    }
}

void CParticle::OffsetPosition(const Vec3& delta)
{
    SMoveState::OffsetPosition(delta);

    if (m_aPosHistory)
    {
        for (SParticleHistory* pPos = m_aPosHistory + GetContainer().GetHistorySteps() - 1; pPos >= m_aPosHistory; pPos--)
        {
            pPos->Loc.t += delta;
        }
    }
}

Vec3 CParticle::GetVisualVelocity(SParticleState const& nextState, float stepTime) const
{
    const ParticleParams& params = GetParams();
    // Use the same caculation in Move() to get velocity
    Vec3 vVelUser = params.GetVelocityVector(GetEmitter()->GetAge(), GetRelativeAge(stepTime * 0.5f));
    
    // For these shapes, estimating the velocity over time is much simpler than trying to attain velocity by more direct means, given 
    // the complex space transformations involved (i.e. x means radius, y means longitudinal rotation, etc).
    ParticleParams::EEmitterShapeType emitterShape = GetEmitter()->GetParams().GetEmitterShape();
    if (stepTime > FLT_EPSILON && (emitterShape == ParticleParams::EEmitterShapeType::SPHERE || emitterShape == ParticleParams::EEmitterShapeType::CIRCLE))
    {
        Vec3 currentLocation = nextState.m_Loc.t;
        Vec3 prevLocation = m_Loc.t;
        vVelUser = (currentLocation - prevLocation) / stepTime;
    }

    if (params.fTurbulenceSize * params.fTurbulenceSpeed != 0.f)
    {
        return nextState.m_Vel.vLin + VortexRotation(nextState, true, stepTime) + vVelUser;
    }
    else
    {
        return nextState.m_Vel.vLin + vVelUser;
    }
}

#ifdef PARTICLE_EDITOR_FUNCTIONS
void CParticle::UpdateAllocations(int nPrevHistorySteps)
{
    SParticleHistory* aPrevHist = m_aPosHistory;
    if (int nNewSteps = GetContainer().GetHistorySteps())
    {
        if ((m_aPosHistory = static_cast<SParticleHistory*>(ParticleObjectAllocator().Allocate(sizeof(SParticleHistory) * nNewSteps))))
        {
            if (aPrevHist)
            {
                memcpy(m_aPosHistory, aPrevHist, min(nPrevHistorySteps, nNewSteps) * sizeof(*aPrevHist));
            }
            for (int n = nPrevHistorySteps; n < nNewSteps; n++)
            {
                m_aPosHistory[n].SetUnused();
            }
        }
    }
    else
    {
        m_aPosHistory = 0;
    }

    ParticleObjectAllocator().Deallocate(aPrevHist, nPrevHistorySteps * sizeof(SParticleHistory));

    if (GetContainer().NeedsCollisionInfo() && !m_pCollisionInfo)
    {
        if ((m_pCollisionInfo = static_cast<SCollisionInfo*>(ParticleObjectAllocator().Allocate(sizeof(SCollisionInfo)))))
        {
            new (m_pCollisionInfo)SCollisionInfo(GetParams().nMaxCollisionEvents);
        }
    }
}
#endif

void CParticle::AddPosHistory(SParticleState const& stateNew)
{
    // Possibly store current position in history. Check significance against previous positions and stateNew.
    float fRelativeAge = GetRelativeAge();
    float fTailLength = GetParams().fTailLength.GetValueFromMod(m_BaseMods.StretchOrTail, fRelativeAge);

    int nCount = GetContainer().GetHistorySteps();
    while (nCount > 0 && !m_aPosHistory[nCount - 1].IsUsed())
    {
        nCount--;
    }

    // Clear out old entries.
    float fMinAge = m_fAge - fTailLength;
    int nStart = 0;
    while (nStart + 1 < nCount && m_aPosHistory[nStart + 1].fAge < fMinAge)
    {
        nStart++;
    }

    if (nStart > 0)
    {
        for (int n = nStart; n < nCount; n++)
        {
            m_aPosHistory[n - nStart] = m_aPosHistory[n];
        }
        for (int n = nCount - nStart; n < nCount; n++)
        {
            m_aPosHistory[n].SetUnused();
        }
        nCount -= nStart;
    }

    if (nCount == GetContainer().GetHistorySteps())
    {
        // Remove least significant entry.
        int nLeast = nCount;
        float fLeastSignif = fHUGE;
        for (int n = 1; n <= nCount; n++)
        {
            QuatTS aqts[3];
            aqts[0] = m_aPosHistory[n - 1].Loc;

            if (n < nCount)
            {
                aqts[1] = m_aPosHistory[n].Loc;
            }
            else
            {
                aqts[1] = m_Loc;
            }

            if (n + 1 < nCount)
            {
                aqts[2] = m_aPosHistory[n + 1].Loc;
            }
            else if (n + 1 == nCount)
            {
                aqts[2] = m_Loc;
            }
            else
            {
                aqts[2] = stateNew.m_Loc;
            }

            float fSignif = InterpVariance(aqts[0], aqts[1], aqts[2]);
            if (fSignif < fLeastSignif)
            {
                fLeastSignif = fSignif;
                nLeast = n;
            }
        }
        if (nLeast != nCount)
        {
            // Delete entry.
            for (int n = nLeast; n < nCount - 1; n++)
            {
                m_aPosHistory[n] = m_aPosHistory[n + 1];
            }
            nCount--;
        }
    }
    if (nCount < GetContainer().GetHistorySteps())
    {
        m_aPosHistory[nCount].fAge = m_fAge;
        m_aPosHistory[nCount].Loc = m_Loc;
    }
}

void CParticle::Update(SParticleUpdateContext const& context, float fFrameTime, bool bNew)
{
    CParticleContainer& rContainer = GetContainer();
    SPhysEnviron const& PhysEnv = GetMain().GetPhysEnviron();
    ResourceParticleParams const& params = rContainer.GetParams();

    // Process only up to lifetime of particle, and handle negative initial age.
    if (m_fAge + fFrameTime <= 0.f)
    {
        m_fAge += fFrameTime;
        fFrameTime = 0.f;
    }
    else if (m_fAge < 0.f)
    {
        fFrameTime += m_fAge;
        m_fAge = 0.f;
    }
    if (!params.bRemainWhileVisible)
    {
        assert(m_fAge <= m_fStopAge);
        fFrameTime = min(fFrameTime, m_fStopAge - m_fAge);
    }

    if (m_pCollisionInfo && m_pCollisionInfo->Stopped())
    {
        //we need to update age and dynamic bounding box if returns early
        // Particle is stopped
        m_fAge += max(fFrameTime, 0.f);

        // Update dynamic bounds if required.
        if (context.pbbDynamicBounds)
        {
            UpdateBounds(*context.pbbDynamicBounds);
        }
        return;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Move
    ////////////////////////////////////////////////////////////////////////////////////////////////

    IF (m_pPhysEnt, false)
    {
        // Use physics engine to move particles.
        GetPhysicsState();

        // Get collision status.
        pe_status_collisions status;
        coll_history_item item;
        status.pHistory = &item;
        status.age = 1.f;
        status.bClearHistory = 1;

        const bool collisionHappened = (m_pPhysEnt->GetStatus(&status) > 0);
        if (collisionHappened)
        {
            Collide();
            if (m_pCollisionInfo && !m_pCollisionInfo->Collide())
            {
                if (params.eFinalCollision == params.eFinalCollision.Bounce)
                {
                    m_pCollisionInfo->Stop();
                }
            }

            // if on collide option is not bounce we need to kill the particle or stop particle the first time when it collides.
            if (params.eFinalCollision == params.eFinalCollision.Die)
            {
                // Kill particle on collision
                Stop();
                return;
            }
            else if (params.eFinalCollision == params.eFinalCollision.Stop)
            {
                m_pCollisionInfo->Stop();
            }
        }
    }
    else
    {
        uint32 nFlags = context.nEnvFlags & ENV_PHYS_AREA;
        uint32 nCollideFlags = m_pCollisionInfo ? context.nEnvFlags & ENV_COLLIDE_ANY : 0;
        float fMaxStepTime = fFrameTime;

        // Transform emitter-relative particles by emitter movement; unless just emitted, then already current.
        if (params.eMoveRelEmitter && GetEmitter() && !bNew)
        {
            Vec3 vPreTrans;
            QuatTS qtMove;
            if (GetEmitter()->GetMoveRelative(vPreTrans, qtMove))
            {
                m_originalEmitterLocation += vPreTrans + qtMove.t;
                m_Loc.t += vPreTrans;
                Transform(qtMove);
                if (params.eMoveRelEmitter == params.eMoveRelEmitter.YesWithTail && m_aPosHistory)
                {
                    for (SParticleHistory* pPos = m_aPosHistory + GetContainer().GetHistorySteps() - 1; pPos >= m_aPosHistory; pPos--)
                    {
                        if (pPos->IsUsed())
                        {
                            pPos->Loc.t += vPreTrans;
                            pPos->Loc = qtMove * pPos->Loc;
                        }
                    }
                }
            }
        }

        // Get particle target.
        STargetForces forces;
        if (context.bHasTarget)
        {
            GetContainer().GetTarget(forces.target, GetEmitter());
        }

        while (fFrameTime > 0.f)
        {
            // Apply previously computed MaxStepTime.
            float fStepTime = min(fFrameTime, fMaxStepTime);
            fMaxStepTime = fFrameTime;

            if (nFlags)
            {
                PhysEnv.GetForces(forces, m_Loc.t, nFlags);
            }

            SParticleState stateNew = *this;

            stateNew.m_Loc.s = params.fSizeY.GetValueFromMod(m_BaseMods.SizeY, GetRelativeAge(fStepTime)) * GetMain().GetParticleScale() * m_sizeScale.y;

            SCollisionInfo collNew(-1);
            if (m_pCollisionInfo)
            {
                collNew = *m_pCollisionInfo;
            }

            fStepTime = MoveLinear(stateNew, collNew, min(fStepTime, context.fMaxLinearStepTime),
                    forces, context.fMaxLinearDeviation, context.fMaxLinearDeviation, context.fMinStepTime);

            if (params.eMoveRelEmitter != ParticleParams::EMoveRelative::E::No)
            {
                m_pEmitter->SetRelativeRotation(GetSource().GetLocation().q);
            }

            Vec3 vMove = stateNew.m_Loc.t - m_Loc.t;

            if (nFlags & PhysEnv.m_nNonUniformFlags && fStepTime > context.fMinStepTime)
            {
                // Subdivide travel in non-uniform areas.
                float fMoveSqr = vMove.GetLengthSquared();
                if (fMoveSqr > sqr(fMAX_NONUNIFORM_TRAVEL))
                {
                    SPhysForces forces2;
                    PhysEnv.GetForces(forces2, stateNew.m_Loc.t, nFlags & (ENV_GRAVITY | ENV_WIND));

                    // Estimate acceleration difference.
                    Vec3 vErrorEst = forces2.vAccel - forces.vAccel;
                    if (nFlags & ENV_WIND)
                    {
                        float fDrag = params.fAirResistance.GetValueFromMod(m_BaseMods.AirResistance, GetRelativeAge(fStepTime * 0.5f));
                        vErrorEst += (forces2.vWind - forces.vWind) * (params.fAirResistance.fWindScale * fDrag);
                    }

                    float fDevSqr = vErrorEst.GetLengthSquared() * sqr(sqr(fStepTime) * 0.5f);
                    if (fDevSqr > sqr(0.75f * fMAX_NONUNIFORM_TRAVEL))
                    {
                        float fShorten = 0.75f * fMAX_NONUNIFORM_TRAVEL * isqrt_tpl(fDevSqr);
                        fShorten = min(fShorten, 0.75f);
                        fMaxStepTime = max(fStepTime * fShorten, context.fMinStepTime);
                        if (fMaxStepTime < fStepTime * 0.99f)
                        {
                            continue;
                        }
                    }
                }
            }

            if (params.bSpaceLoop)
            {
                // Wrap the particle into camera-aligned BB.
                Vec3 vPosSpaceLoop = stateNew.m_Loc.t - context.SpaceLoop.vCenter;

                for (int a = 0; a < 3; a++)
                {
                    float fPosSpace = vPosSpaceLoop * context.SpaceLoop.vScaledAxes[a];
                    if (abs(fPosSpace) > 1.f)
                    {
                        float fCorrect = floor_tpl(fPosSpace);
                        fCorrect = if_neg_else(fCorrect, fCorrect, fCorrect + 1.f);
                        stateNew.m_Loc.t -= context.SpaceLoop.vScaledAxes[a] * (fCorrect * sqr(context.SpaceLoop.vSize[a]));
                    }
                }
            }

            DebugBounds(stateNew);

            ////////////////////////////////////////////////////////////////////////////////////////////////
            // Simple collision with physics entities
            // Collision strategy:
            //  * Move, with sliding from previous collisions
            //  * Test collision, determine bounce or sliding
            //  * Set sliding state for next frames, within slide distance
            ////////////////////////////////////////////////////////////////////////////////////////////////

            if (collNew.CanCollide() && !vMove.IsZero())
            {
                ray_hit hit;
                if (CheckCollision(hit, fStepTime, context, forces, stateNew, collNew))
                {
                    assert(hit.dist >= 0.f);
                    rContainer.GetCounts().ParticlesCollideHit += 1.f;

                    // Set particle to collision point.
                    // Linearly interpolate velocity based on distance.
                    stateNew.m_Vel.vLin.SetLerp(m_Vel.vLin, stateNew.m_Vel.vLin, hit.dist);
                    stateNew.m_Loc.t = hit.pt;

                    fStepTime = max(fStepTime * hit.dist, context.fMinStepTime);

                    // Rotate to surface normal.
                    RotateToUpVector(stateNew.m_Loc.q, hit.n);
                    stateNew.Collide(fStepTime);

                    if (collNew.CanCollide() && params.eFinalCollision != params.eFinalCollision.Bounce)
                    {
                        // Last collision allowed
                        SetState(stateNew);
                        *m_pCollisionInfo = collNew;
                        m_fAge += fStepTime;

                        if (params.eFinalCollision == params.eFinalCollision.Die)
                        {
                            Stop();
                        }
                        else if (params.eFinalCollision == params.eFinalCollision.Stop)
                        {
                            m_Vel = Velocity3(ZERO);
                            m_pCollisionInfo->Stop();
                        }

                        return;
                    }
                    else
                    {
                        if (hit.dist <= 0.f)
                        {
                            // Disable further collisions this frame
                            nCollideFlags = 0;
                        }

                        // Remove perpendicular velocity component.
                        float fVelPerp = stateNew.m_Vel.vLin * hit.n;
                        if (fVelPerp <= 0.f)
                        {
                            if (m_aPosHistory)
                            {
                                // Store double history steps at bounce.
                                UpdateAlignment(stateNew, context, forces.plWater, fStepTime);
                                stateNew.m_fAge += fStepTime;
                                AddPosHistory(stateNew);
                                SetState(stateNew);
                                fFrameTime -= fStepTime;
                                fStepTime = 0.f;
                            }

                            stateNew.m_Vel.vLin -= hit.n * fVelPerp;

                            // Get phys params from material, or particle params.
                            float fElasticity, fSlidingFriction;
                            GetCollisionParams(hit.surface_idx, fElasticity, fSlidingFriction);

                            float fVelBounce = fVelPerp * -fElasticity;
                            float fAccelPerp = forces.vAccel * hit.n;

                            /*
                            float fDeltaVelTransverse = -fVelPerp * (1.f + fElasticity) * fSlidingFriction;
                            if (fDeltaVelTransverse)
                            {
                                // Apply dynamic friction during collision, regardless of sliding state.
                                // Perpendicular velocity change from bounce = (vp1 - vp0)
                                // Transverse velocity change from friction = (vp1 - vp0) * sliding_friction
                                stateNew.m_Vel.vLin -= stateNew.m_Vel.vLin * div_min(fDeltaVelTransverse, stateNew.m_Vel.vLin.GetLength(), 1.f);
                            }
                            */

                            // Sliding occurs when the bounce distance would be less than the collide buffer.
                            // d = - v^2 / 2a <= dmax
                            // v^2 <= -dmax*2a
                            if (sqr(fVelBounce) <= fCOLLIDE_BUFFER_DIST * fAccelPerp * -2.f)
                            {
                                collNew.Sliding.SetSliding(hit.pCollider, hit.n, fSlidingFriction);
                            }
                            else
                            {
                                // Bouncing.
                                collNew.Sliding.ClearSliding(hit.n);

                                // Bounce the particle, continue for remainder of frame.
                                stateNew.m_Vel.vLin += hit.n * fVelBounce;
                            }
                        }
                        else
                        {
                            // Disable further collisions this frame
                            nCollideFlags = 0;
                        }
                    }
                }
            }

            fStepTime = UpdateAlignment(stateNew, context, forces.plWater, fStepTime);

            // Store computed state.
            if (m_aPosHistory)
            {
                AddPosHistory(stateNew);
            }

            SetState(stateNew);
            if (m_pCollisionInfo)
            {
                *m_pCollisionInfo = collNew;
            }

            fFrameTime -= fStepTime;
            m_fAge += fStepTime;
        }

        rContainer.GetCounts().ParticlesReiterate -= 1.f;
    }

    m_fAge += max(fFrameTime, 0.f);

    // Update dynamic bounds if required.
    if (context.pbbDynamicBounds)
    {
        UpdateBounds(*context.pbbDynamicBounds);
    }
}

float CParticle::UpdateAlignment(SParticleState& state, SParticleUpdateContext const& context, Plane const& plWater, float fStepTime) const
{
    ResourceParticleParams const& params = GetParams();

    // Apply rotation velocity.
    if (fStepTime != 0.f)
    {
        // rotationRate is animated, so update it
        if (!params.vRotationRateX.IsConstant() || !params.vRotationRateY.IsConstant() || !params.vRotationRateZ.IsConstant()
            || m_BaseMods.RotateRateX < 1.0f || m_BaseMods.RotateRateY < 1.0f || m_BaseMods.RotateRateZ < 1.0f)
        {
            f32 age = GetRelativeAge(fStepTime);
            if (context.b3DRotation)
            {
                Vec3 vRotationRate;
                vRotationRate.Set(params.vRotationRateX.GetValueFromMod(m_BaseMods.RotateRateX, age)
                    , params.vRotationRateY.GetValueFromMod(m_BaseMods.RotateRateY, age)
                    , params.vRotationRateZ.GetValueFromMod(m_BaseMods.RotateRateZ, age)
                    );
                state.m_Vel.vRot = m_initialOrientation * DEG2RAD(vRotationRate);
            }
            else
            {
                state.m_Vel.vRot.y = DEG2RAD(params.vRotationRateY.GetValueFromMod(m_BaseMods.RotateRateY, age));
            }
        }

        if (context.b3DRotation)
        {
            // 3D rotation.
            state.m_Loc.q = Quat::exp(state.m_Vel.vRot * (fStepTime * 0.5f)) * state.m_Loc.q;
        }
        else
        {
            // Just angle.
            state.m_fAngle += state.m_Vel.vRot.y * fStepTime;
        }
    }

    Vec3 vNormal(ZERO);

    switch (params.eFacing)
    {
    case ParticleParams::EFacing::Water:
    {
        // Project point and velocity onto plane.
        if (HasWater(plWater))
        {
            float fDist = plWater.DistFromPlane(state.m_Loc.t);
            state.m_Loc.t -= plWater.n * (fDist - params.vSpawnPosOffset.z);
            vNormal = plWater.n;
        }
        else
        {
            // No water, kill it.
            state.Stop();
            return fStepTime;
        }
        break;
    }
    case ParticleParams::EFacing::Terrain:
    {
#ifdef LY_TERRAIN_LEGACY_RUNTIME
        if (CTerrain* const pTerrain = GetTerrain())
        {
            // Project center and velocity onto plane.
            if (state.m_Loc.s > 0.f)
            {
                // Sample terrain around point to get heights and average normal.
                Vec3 avCorner[5];
                for (int c = 0; c < 5; c++)
                {
                    avCorner[c] = state.m_Loc.t;
                    if (c < 4)
                    {
                        avCorner[c].x += ((c & 1) * 2 - 1) * state.m_Loc.s;
                        avCorner[c].y += ((c & 2) - 1) * state.m_Loc.s;
                    }
                    avCorner[c].z = pTerrain->GetBilinearZ(avCorner[c].x, avCorner[c].y);
                }

                // Rotate sprite to average normal of quad.
                vNormal = (avCorner[3] - avCorner[0]) ^ (avCorner[2] - avCorner[1]);
                if (vNormal.z != 0.f)
                {
                    if (vNormal.z < 0.f)
                    {
                        vNormal = -vNormal;
                    }
                    vNormal.Normalize();

                    // Adjust z to match plane.
                    float fZAdd = -fHUGE;
                    for (int c = 0; c < 5; c++)
                    {
                        fZAdd = max(fZAdd, avCorner[c] * vNormal);
                    }
                    state.m_Loc.t.z += (fZAdd - state.m_Loc.t * vNormal) / vNormal.z + fATTACH_BUFFER_DIST;
                }
            }
        }
#endif //#ifdef LY_TERRAIN_LEGACY_RUNTIME
        break;
    }
    case ParticleParams::EFacing::Velocity:
    {
        // Rotate to movement dir.
        vNormal = GetVisualVelocity(state, fStepTime).GetNormalized();
        break;
    }
    case ParticleParams::EFacing::Horizontal:
    {
        vNormal = GetSource().GetLocation().q.GetColumn1();

        // Confine particle motion.
        Vec3 vAdjust = vNormal * (vNormal | (state.m_Loc.t - m_Loc.t));
        state.m_Loc.t -= vAdjust;
        break;
    }
    }

    AlignTo(state, vNormal, fStepTime);

    return fStepTime;
}

void CParticle::AlignTo(SParticleState& state, const Vec3& vNormal, float stepTime) const
{
    const ParticleParams& params = GetParams();

    bool bNormal = vNormal.GetLengthSquared() > 0.f;
    if (params.eFacing != params.eFacing.Velocity)
    {
        if (bNormal)
        {
            // Align velocities to normal
            state.m_Vel.vLin = Quat::CreateRotationV0V1(state.m_Loc.q.GetColumn1(), vNormal) * state.m_Vel.vLin;
            state.m_Vel.vLin -= vNormal * (state.m_Vel.vLin * vNormal);
            state.m_Vel.vRot = vNormal * (state.m_Vel.vRot * vNormal);
        }

        if (params.bOrientToVelocity || m_aPosHistory)
        {
            // Orient in velocity direction.
            Vec3 vOrient = GetVisualVelocity(state, stepTime).GetNormalized();
            vOrient -= vNormal * (vOrient * vNormal);
            if (CheckNormalize(vOrient))
            {
                if (bNormal)
                {
                    // Rotate to align Y and Z axes
                    Matrix33 mat;
                    mat.SetColumn1(vNormal);
                    mat.SetColumn2(vOrient);
                    mat.SetColumn0(vNormal ^ vOrient);
                    state.m_Loc.q = Quat(mat);
                }
                else
                {
                    // Just orient Z axis
                    RotateToUpVector(state.m_Loc.q, vOrient);
                }
                return;
            }
        }
    }

    if (bNormal)
    {
        // Just orient Z axis
        RotateToUpVector(state.m_Loc.q, vNormal);
    }
}

Vec3 CParticle::VortexRotation(SParticleState const& state, bool bVelocity, float fTime) const
{
    // Compute vortex rotational offset at current age.
    // p(t) = TSize e^(i TSpeed t) (t max 1)
    const ParticleParams& params = GetParams();

    float fRelativeAge = state.GetRelativeAge(fTime);
    float fVortexSpeed = DEG2RAD(params.fTurbulenceSpeed.GetValueFromMod(m_BaseMods.TurbulenceSpeed, fRelativeAge));
    float fAngle = fVortexSpeed * (state.m_fAge + fTime);
    float fVortexSize = params.fTurbulenceSize.GetValueFromMod(m_BaseMods.TurbulenceSize, fRelativeAge) * GetMain().GetParticleScale();
    if (bVelocity)
    {
        // Compute the current vortex velocity (not magnitude)
        fAngle += gf_PI * 0.5f;
        fVortexSize *= fVortexSpeed;
    }

    Vec2 vRot;
    sincos_tpl(fAngle, &vRot.y, &vRot.x);
    vRot *= fVortexSize;

    // Scale down vortex size in first half rotation.
    fVortexSize *= div_min(abs(fAngle), gf_PI, 1.f);

    // Choose axes orthogonal to velocity.
    Vec3 const& vAxis = state.m_Vel.vLin;
    Vec3 vX = vAxis.GetOrthogonal();
    Vec3 vY = vX ^ vAxis;

    return vX.GetNormalized(vRot.x) + vY.GetNormalized(vRot.y);
}

void CParticle::TargetMovement(ParticleTarget const& target, SParticleState& state, float fTime, float fRelativeAge) const
{
    const ParticleParams& params = GetParams();

    float fTargetRadius = max(params.TargetAttraction.fRadius.GetValueFromMod(m_BaseMods.fTargetRadius, fRelativeAge) + target.fRadius, 0.f);
    bool bOrbiting = params.TargetAttraction.bOrbit && fTargetRadius > 0.f;

    state.m_Loc.t -= state.m_Vel.vLin * fTime;

    // Decompose current velocity into radial+angular components.
    Vec3 vPos = state.m_Loc.t - target.vTarget;
    Vec3 vVel = state.m_Vel.vLin - target.vVelocity;

    float fDist = vPos.GetLength();
    float fVel = vVel.GetLength();
    Vec3 vRadialDir = vPos.GetNormalized();
    float fRadialVel = vVel | vRadialDir;
    Vec3 vOrbitalVel = vVel - vRadialDir * fRadialVel;
    Vec3 vOrbitalDir = vOrbitalVel.GetNormalized();
    float fOrbitalVel = vOrbitalVel.GetLength();

    // Determine arrival time.
    float fArrivalTime = div_min(fTargetRadius - fDist, fRadialVel, fHUGE);
    if (fArrivalTime < 0.f)
    {
        fArrivalTime = fHUGE;
    }

    // Goal is to reach target radius in a quarter revolution over particle's life.
    float fLife = state.m_fStopAge - state.m_fAge;
    fArrivalTime = div_min(gf_PI * 0.5f * fDist * fLife, fOrbitalVel * state.m_fStopAge, fArrivalTime);

    if (fArrivalTime > fLife)
    {
        if (params.TargetAttraction.bExtendSpeed)
        {
            fArrivalTime = fLife;
        }
    }
    else if (!bOrbiting)
    {
        // Age particle prematurely based on target time.
        state.m_fStopAge = state.m_fAge + fArrivalTime;
    }

    // Execute the orbit.
    float fNewDist;
    if (fArrivalTime > fTime)
    {
        // Change particle direction, maintaining speed.
        fRadialVel = (fTargetRadius - fDist) / fArrivalTime;
        fOrbitalVel = sqrt_tpl(max(0.f, sqr(fVel) - sqr(fRadialVel)));
        fNewDist = fDist + fRadialVel * fTime;
    }
    else
    {
        // Arrives this time step.
        fRadialVel = 0.f;
        const float returnRate = 0.25f;
        fOrbitalVel = returnRate * params.fSpeed + (1.f - returnRate) * fVel;
        fNewDist = fTargetRadius;
    }

    if (fNewDist > 0.f)
    {
        // Orbital travel needed.
        // Compute angular movement, using geometric mean of start and end radii, which is a close approximation.
        float fVelAngle = fOrbitalVel * isqrt_tpl(fDist * fNewDist) * fTime;

        float fCos, fSin;
        sincos_tpl(fVelAngle, &fSin, &fCos);
        state.m_Loc.t += (vRadialDir * (fCos - 1.f) + vOrbitalDir * fSin) * fDist;

        // Recompute polar axes.
        vOrbitalDir = vOrbitalDir * fCos - vRadialDir * fSin;
        vRadialDir = (state.m_Loc.t - target.vTarget).GetNormalized();
    }

    state.m_Loc.t += vRadialDir * (fNewDist - fDist);

    state.m_Vel.vLin = vRadialDir * fRadialVel + vOrbitalDir * fOrbitalVel + target.vVelocity;

    if (params.TargetAttraction.bShrink)
    {
        fDist = fNewDist - fTargetRadius;
        if (fDist < state.m_Loc.s * GetBaseRadius())
        {
            state.m_Loc.s = max(fDist, 0.f) / GetBaseRadius();
        }
    }
    assert(m_Loc.s >= 0.f);
}

void CParticle::GetCollisionParams(int nCollSurfaceIdx, float& fElasticity, float& fDrag) const
{
    // Get phys params from material, or particle params.
    fElasticity = GetParams().fElasticity;
    fDrag = GetParams().fDynamicFriction;

    IMaterialManager* pMatMan = GetMatMan();

    int iSurfaceIndex = GetSurfaceIndex();
    if (iSurfaceIndex > 0)
    {
        ISurfaceType* pSurfPart = pMatMan->GetSurfaceType(iSurfaceIndex);
        if (pSurfPart)
        {
            ISurfaceType::SPhysicalParams const& physPart = pSurfPart->GetPhyscalParams();
            fElasticity = max(fElasticity, physPart.bouncyness);
            fDrag = max(fDrag, physPart.friction);
        }

        // Combine with hit surface params.
        ISurfaceType* pSurfHit = pMatMan->GetSurfaceType(nCollSurfaceIdx);
        if (!pSurfHit)
        {
            pSurfHit = pMatMan->GetDefaultTerrainLayerMaterial()->GetSurfaceType();
        }
        ISurfaceType::SPhysicalParams const& physHit = pSurfHit->GetPhyscalParams();

        fElasticity = clamp_tpl((fElasticity + physHit.bouncyness) * 0.5f, 0.f, 1.f);
        fDrag = max((fDrag + physHit.friction) * 0.5f, 0.f);
    }
}

CParticle::~CParticle()
{
    m_pEmitter->Release();

    IF (m_pPhysEnt, 0)
    {
        GetPhysicalWorld()->DestroyPhysicalEntity(m_pPhysEnt);
    }

    SEmitGeom::Release();

    IF (m_pCollisionInfo, 0)
    {
        m_pCollisionInfo->Clear();
        ParticleObjectAllocator().Deallocate(m_pCollisionInfo, sizeof(SCollisionInfo));
    }

    ParticleObjectAllocator().Deallocate(m_aPosHistory, sizeof(SParticleHistory) * m_pContainer->GetHistorySteps());
}

char GetMinAxis(Vec3 const& vVec)
{
    float x = fabs(vVec.x);
    float y = fabs(vVec.y);
    float z = fabs(vVec.z);

    if (x < y && x < z)
    {
        return 'x';
    }

    if (y < x && y < z)
    {
        return 'y';
    }

    return 'z';
}

#define fSPHERE_VOLUME float(4.f / 3.f * gf_PI)

int CParticle::GetSurfaceIndex() const
{
    ResourceParticleParams const& params = GetParams();
    if (params.sSurfaceType.nIndex)
    {
        // Explicit surface type.
        return params.sSurfaceType.nIndex;
    }
    if (params.pMaterial)
    {
        // Get from material, if defined.
        if (int index = params.pMaterial->GetSurfaceTypeId())
        {
            return index;
        }
    }
    if (IStatObj* pStatObj = GetStatObj())
    {
        // Get from geometry.
        if (phys_geometry* pGeom = pStatObj->GetPhysGeom())
        {
            if (pGeom->surface_idx < pGeom->nMats)
            {
                return pGeom->pMatMapping[pGeom->surface_idx];
            }
            else
            {
                return pGeom->surface_idx;
            }
        }
    }
    return 0;
}

void CParticle::Physicalize()
{
    ResourceParticleParams const& params = GetParams();
    SPhysEnviron const& PhysEnv = GetMain().GetPhysEnviron();

    Vec3 vGravity = PhysEnv.m_UniformForces.vAccel * params.fGravityScale.GetValueFromMod(m_BaseMods.GravityScale) + params.vAcceleration;

    pe_params_pos par_pos;
    par_pos.pos = m_Loc.t;
    par_pos.q = m_Loc.q;

    IStatObj* pStatObj = GetStatObj();

    phys_geometry* pGeom = pStatObj ? pStatObj->GetPhysGeom() : 0;

    m_Loc.s = params.fSizeY.GetMaxValue() * GetMain().GetParticleScale() * m_sizeScale.y;

    if (params.ePhysicsType == params.ePhysicsType.RigidBody)
    {
        if (!pGeom)
        {
            return;
        }

        // Make Physical Rigid Body.
        m_pPhysEnt = GetPhysicalWorld()->CreatePhysicalEntity(PE_RIGID, &par_pos, pStatObj, PHYS_FOREIGN_ID_RIGID_PARTICLE);

        pe_geomparams partpos;

        partpos.density = params.fDensity;
        partpos.scale = m_Loc.s;
        partpos.flagsCollider = geom_colltype_debris;
        partpos.flags &= ~geom_colltype_debris; // don't collide with other particles.

        // Override surface index if specified.
        int idx =
            params.sSurfaceType.nIndex ? params.sSurfaceType.nIndex :
            params.pMaterial ? params.pMaterial->GetSurfaceTypeId() :
            0;
        if (idx)
        {
            partpos.pMatMapping = &idx;
            partpos.nMats = 1;
            partpos.surface_idx = 0;
        }
        m_pPhysEnt->AddGeometry(pGeom, &partpos, 0);

        pe_simulation_params symparams;
        symparams.minEnergy = (0.2f) * (0.2f);
        symparams.damping = symparams.dampingFreefall = params.fAirResistance.GetValueFromMod(m_BaseMods.AirResistance);

        // Note: Customized gravity currently doesn't work for rigid body.
        symparams.gravity = symparams.gravityFreefall = vGravity;
        //symparams.softness = symparams.softnessGroup = 0.003f;
        //symparams.softnessAngular = symparams.softnessAngularGroup = 0.01f;
        symparams.maxLoggedCollisions = params.nMaxCollisionEvents;
        m_pPhysEnt->SetParams(&symparams);

        pe_action_set_velocity velparam;
        velparam.v = m_Vel.vLin;
        velparam.w = m_Vel.vRot;
        m_pPhysEnt->Action(&velparam);
    }
    else if (params.ePhysicsType == params.ePhysicsType.SimplePhysics)
    {
        // Make Physical Particle.
        m_pPhysEnt = GetPhysicalWorld()->CreatePhysicalEntity(PE_PARTICLE, &par_pos);
        pe_params_particle part;

        // Compute particle mass from volume of object.
        part.size = m_Loc.s;

        part.mass = params.fDensity * part.size * part.size * part.size;
        if (pStatObj)
        {
            part.size *= pStatObj->GetRadius() + 0.05f;
            if (pGeom)
            {
                part.mass *= pGeom->V;
            }
            else
            {
                part.mass *= pStatObj->GetAABB().GetVolume() * fSPHERE_VOLUME / 8.f;
            }
        }
        else
        {
            // Assume spherical volume.
            part.mass *= fSPHERE_VOLUME;
        }

        part.thickness = params.fThickness * part.size;
        part.velocity = m_Vel.vLin.GetLength();
        if (part.velocity > 0.f)
        {
            part.heading = m_Vel.vLin / part.velocity;
        }
        part.q0 = m_Loc.q;
        part.wspin = m_Vel.vRot;
        part.q0 = m_Loc.q;

        if (pStatObj)
        {
            Vec3 vSize = pStatObj->GetBoxMax() - pStatObj->GetBoxMin();
            char cMinAxis = GetMinAxis(vSize);
            part.normal = Vec3((cMinAxis == 'x') ? 1.f : 0, (cMinAxis == 'y') ? 1.f : 0, (cMinAxis == 'z') ? 1.f : 0);
        }

        part.surface_idx = GetSurfaceIndex();
        part.flags = /*particle_no_roll|*/ particle_no_path_alignment;
        part.kAirResistance = params.fAirResistance.GetValueFromMod(m_BaseMods.AirResistance);
        part.gravity = vGravity;

        m_pPhysEnt->SetParams(&part);
    }

    // Common settings.
    if (m_pPhysEnt)
    {
        pe_params_flags pf;
        pf.flagsOR = pef_never_affect_triggers;
        pf.flagsOR |= pef_log_collisions;
        m_pPhysEnt->SetParams(&pf);
        m_pPhysEnt->AddRef();
    }
}

void CParticle::GetPhysicsState()
{
    if (m_pPhysEnt)
    {
        pe_status_pos status_pos;
        if (m_pPhysEnt->GetStatus(&status_pos))
        {
            m_Loc.t = status_pos.pos;
            m_Loc.q = status_pos.q;
        }
        pe_status_dynamics status_dyn;
        if (m_pPhysEnt->GetStatus(&status_dyn))
        {
            m_Vel.vLin = status_dyn.v;
            m_Vel.vRot = status_dyn.w;
        }
    }
}

size_t CParticle::GetAllocationSize(const CParticleContainer* pCont)
{
    return pCont->GetHistorySteps() * sizeof(SParticleHistory)
           + (pCont->NeedsCollisionInfo() ? sizeof(SCollisionInfo) : 0);
}

void CParticle::SetBeamInfo(unsigned int segmentCount, Vec3 segmentStep, const float uvScale, bool isEdgeParticle /*= false*/)
{
    //using this as a start the next n particles will be tied together to create a beam
    m_segmentCount = segmentCount;
    m_segmentStep = segmentStep;
    m_isSegmentEdge = isEdgeParticle;
    m_UVOffset = uvScale;
}

bool CParticle::IsSegmentEdge() const
{
    return m_isSegmentEdge;
}

#ifdef _DEBUG

// Test for distribution evenness.
struct CChaosTest
{
    CChaosTest()
    {
        CChaosKey keyBase(0U);
        float f[100];
        for (uint32 i = 0; i < 100; i++)
        {
            f[i] = keyBase.Jumble(CChaosKey(i)) * 1.f;
        }
    }
};

static CChaosTest ChaosTest;

#endif
