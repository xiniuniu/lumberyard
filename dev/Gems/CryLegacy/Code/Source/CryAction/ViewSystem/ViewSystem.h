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

// Description : View System interfaces.


#ifndef CRYINCLUDE_CRYACTION_VIEWSYSTEM_VIEWSYSTEM_H
#define CRYINCLUDE_CRYACTION_VIEWSYSTEM_VIEWSYSTEM_H
#pragma once

#include "View.h"
#include "IMovieSystem.h"
#include <ILevelSystem.h>
#include <AzFramework/Components/CameraBus.h>



class CViewSystem
    : public IViewSystem
    , public IMovieUser
    , public ILevelSystemListener
    , public Camera::CameraSystemRequestBus::Handler
{
private:

    typedef std::map<unsigned int, IView*> TViewMap;
    typedef std::vector<unsigned int> TViewIdVector;

public:

    //IViewSystem
    virtual IView* CreateView();
    virtual unsigned int AddView(IView* pView) override;
    virtual void RemoveView(IView* pView);
    virtual void RemoveView(unsigned int viewId);

    virtual void SetActiveView(IView* pView);
    virtual void SetActiveView(unsigned int viewId);

    //CameraSystemRequestBus
    AZ::EntityId GetActiveCamera() override { return m_activeViewId ? GetActiveView()->GetLinkedId() : AZ::EntityId(); }

    //utility functions
    virtual IView* GetView(unsigned int viewId);
    virtual IView* GetActiveView();

    virtual unsigned int GetViewId(IView* pView);
    virtual unsigned int GetActiveViewId();

    virtual void Serialize(TSerialize ser);
    virtual void PostSerialize();

    virtual IView* GetViewByEntityId(const AZ::EntityId& id, bool forceCreate);

    virtual float GetDefaultZNear() { return m_fDefaultCameraNearZ; };
    virtual void SetBlendParams(float fBlendPosSpeed, float fBlendRotSpeed, bool performBlendOut) { m_fBlendInPosSpeed = fBlendPosSpeed; m_fBlendInRotSpeed = fBlendRotSpeed; m_bPerformBlendOut = performBlendOut; };
    virtual void SetOverrideCameraRotation(bool bOverride, Quat rotation);
    virtual bool IsPlayingCutScene() const
    {
        return m_cutsceneCount > 0;
    }
    virtual void UpdateSoundListeners();

    virtual void SetDeferredViewSystemUpdate(bool const bDeferred){ m_useDeferredViewSystemUpdate = bDeferred; }
    virtual bool UseDeferredViewSystemUpdate() const { return m_useDeferredViewSystemUpdate; }
    virtual void SetControlAudioListeners(bool const bActive);
    //~IViewSystem

    //IMovieUser
    virtual void SetActiveCamera(const SCameraParams& Params);
    virtual void BeginCutScene(IAnimSequence* pSeq, unsigned long dwFlags, bool bResetFX);
    virtual void EndCutScene(IAnimSequence* pSeq, unsigned long dwFlags);
    virtual void SendGlobalEvent(const char* pszEvent);
    //~IMovieUser

    // ILevelSystemListener
    virtual void OnLevelNotFound(const char* levelName) {};
    virtual void OnLoadingStart(ILevelInfo* pLevel);
    virtual void OnLoadingLevelEntitiesStart(ILevelInfo* pLevel) {};
    virtual void OnLoadingComplete(ILevel* pLevel) {};
    virtual void OnLoadingError(ILevelInfo* pLevel, const char* error) {};
    virtual void OnLoadingProgress(ILevelInfo* pLevel, int progressAmount) {};
    virtual void OnUnloadComplete(ILevel* pLevel);
    //~ILevelSystemListener

    CViewSystem(ISystem* pSystem);
    ~CViewSystem();

    void Release() { delete this; };
    void Update(float frameTime);

    virtual void ForceUpdate(float elapsed) { Update(elapsed); }

    //void RegisterViewClass(const char *name, IView *(*func)());

    bool AddListener(IViewSystemListener* pListener)
    {
        return stl::push_back_unique(m_listeners, pListener);
    }

    bool RemoveListener(IViewSystemListener* pListener)
    {
        return stl::find_and_erase(m_listeners, pListener);
    }

    void GetMemoryUsage(ICrySizer* s) const;

    void ClearAllViews();

private:

    void RemoveViewById(unsigned int viewId);
    void ClearCutsceneViews();
    void DebugDraw();

    ISystem* m_pSystem;

    //TViewClassMap m_viewClasses;
    TViewMap m_views;

    // Listeners
    std::vector<IViewSystemListener*> m_listeners;

    unsigned int m_activeViewId;
    unsigned int m_nextViewIdToAssign;  // next id which will be assigned
    unsigned int m_preSequenceViewId; // viewId before a movie cam dropped in

    unsigned int m_cutsceneViewId;
    unsigned int m_cutsceneCount;

    bool m_bActiveViewFromSequence;

    bool m_bOverridenCameraRotation;
    Quat m_overridenCameraRotation;
    float m_fCameraNoise;
    float m_fCameraNoiseFrequency;

    float m_fDefaultCameraNearZ;
    float m_fBlendInPosSpeed;
    float m_fBlendInRotSpeed;
    bool m_bPerformBlendOut;
    int m_nViewSystemDebug;

    bool m_useDeferredViewSystemUpdate;
    bool m_bControlsAudioListeners;
};

#endif // CRYINCLUDE_CRYACTION_VIEWSYSTEM_VIEWSYSTEM_H
