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

#include <AudioControlsEditorPlugin.h>

#include <AudioControl.h>
#include <AudioControlsEditorWindow.h>
#include <AudioControlsLoader.h>
#include <AudioControlsWriter.h>

#include <CryFile.h>
#include <CryPath.h>
#include <Cry_Camera.h>
#include <Include/IResourceSelectorHost.h>

#include <IAudioSystem.h>
#include <IAudioSystemEditor.h>
#include <ImplementationManager.h>

#include <MathConversion.h>
#include <QtViewPaneManager.h>


using namespace AudioControls;
using namespace PathUtil;

CATLControlsModel CAudioControlsEditorPlugin::ms_ATLModel;
QATLTreeModel CAudioControlsEditorPlugin::ms_layoutModel;
AZStd::set<AZStd::string> CAudioControlsEditorPlugin::ms_currentFilenames;
Audio::IAudioProxy* CAudioControlsEditorPlugin::ms_pIAudioProxy = nullptr;
Audio::TAudioControlID CAudioControlsEditorPlugin::ms_nAudioTriggerID = INVALID_AUDIO_CONTROL_ID;
CImplementationManager CAudioControlsEditorPlugin::ms_implementationManager;

//-----------------------------------------------------------------------------------------------//
CAudioControlsEditorPlugin::CAudioControlsEditorPlugin(IEditor* editor)
{
    QtViewOptions options;
    options.canHaveMultipleInstances = true;
    options.sendViewPaneNameBackToAmazonAnalyticsServers = true;
    RegisterQtViewPane<CAudioControlsEditorWindow>(editor, LyViewPane::AudioControlsEditor, LyViewPane::CategoryOther, options);
    RegisterModuleResourceSelectors(GetIEditor()->GetResourceSelectorHost());

    Audio::AudioSystemRequestBus::BroadcastResult(ms_pIAudioProxy, &Audio::AudioSystemRequestBus::Events::GetFreeAudioProxy);

    if (ms_pIAudioProxy)
    {
        ms_pIAudioProxy->Initialize("AudioControlsEditor-Preview");
        ms_pIAudioProxy->SetObstructionCalcType(Audio::eAOOCT_IGNORE);
    }

    ms_implementationManager.LoadImplementation();
    ReloadModels();
    ms_layoutModel.Initialize(&ms_ATLModel);
    GetISystem()->GetISystemEventDispatcher()->RegisterListener(this);
}

//-----------------------------------------------------------------------------------------------//
CAudioControlsEditorPlugin::~CAudioControlsEditorPlugin()
{
    Release();
}

//-----------------------------------------------------------------------------------------------//
void CAudioControlsEditorPlugin::Release()
{
    UnregisterQtViewPane<CAudioControlsEditorWindow>();
    // clear connections before releasing the implementation since they hold pointers to data
    // instantiated from the implementation dll.
    CUndoSuspend suspendUndo;
    ms_ATLModel.ClearAllConnections();
    ms_implementationManager.Release();
    if (ms_pIAudioProxy)
    {
        StopTriggerExecution();
        ms_pIAudioProxy->Release();
    }
    GetISystem()->GetISystemEventDispatcher()->RemoveListener(this);
}

//-----------------------------------------------------------------------------------------------//
void CAudioControlsEditorPlugin::SaveModels()
{
    AudioControls::IAudioSystemEditor* pImpl = ms_implementationManager.GetImplementation();
    if (pImpl)
    {
        CAudioControlsWriter writer(&ms_ATLModel, &ms_layoutModel, pImpl, ms_currentFilenames);
    }
}

//-----------------------------------------------------------------------------------------------//
void CAudioControlsEditorPlugin::ReloadModels()
{
    GetIEditor()->SuspendUndo();
    ms_ATLModel.SetSuppressMessages(true);

    AudioControls::IAudioSystemEditor* pImpl = ms_implementationManager.GetImplementation();
    if (pImpl)
    {
        ms_layoutModel.clear();
        ms_ATLModel.Clear();
        pImpl->Reload();
        CAudioControlsLoader ATLLoader(&ms_ATLModel, &ms_layoutModel, pImpl);
        ATLLoader.LoadAll();
        ms_currentFilenames = ATLLoader.GetLoadedFilenamesList();
    }

    ms_ATLModel.SetSuppressMessages(false);
    GetIEditor()->ResumeUndo();
}

//-----------------------------------------------------------------------------------------------//
void CAudioControlsEditorPlugin::ReloadScopes()
{
    AudioControls::IAudioSystemEditor* pImpl = ms_implementationManager.GetImplementation();
    if (pImpl)
    {
        ms_ATLModel.ClearScopes();
        CAudioControlsLoader ATLLoader(&ms_ATLModel, &ms_layoutModel, pImpl);
        ATLLoader.LoadScopes();
    }
}

//-----------------------------------------------------------------------------------------------//
CATLControlsModel* CAudioControlsEditorPlugin::GetATLModel()
{
    return &ms_ATLModel;
}

//-----------------------------------------------------------------------------------------------//
AudioControls::IAudioSystemEditor* CAudioControlsEditorPlugin::GetAudioSystemEditorImpl()
{
    return ms_implementationManager.GetImplementation();
}

//-----------------------------------------------------------------------------------------------//
QATLTreeModel* CAudioControlsEditorPlugin::GetControlsTree()
{
    return &ms_layoutModel;
}

//-----------------------------------------------------------------------------------------------//
void CAudioControlsEditorPlugin::ExecuteTrigger(const AZStd::string_view sTriggerName)
{
    if (!sTriggerName.empty() && ms_pIAudioProxy)
    {
        StopTriggerExecution();
        Audio::AudioSystemRequestBus::BroadcastResult(ms_nAudioTriggerID, &Audio::AudioSystemRequestBus::Events::GetAudioTriggerID, sTriggerName.data());
        if (ms_nAudioTriggerID != INVALID_AUDIO_CONTROL_ID)
        {
            const CCamera& camera = GetIEditor()->GetSystem()->GetViewCamera();

            Audio::SAudioRequest request;
            request.nFlags = Audio::eARF_PRIORITY_NORMAL;

            const AZ::Transform cameraMatrix = LYTransformToAZTransform(camera.GetMatrix());

            Audio::SAudioListenerRequestData<Audio::eALRT_SET_POSITION> requestData(cameraMatrix);
            requestData.oNewPosition.NormalizeForwardVec();
            requestData.oNewPosition.NormalizeUpVec();
            request.pData = &requestData;
            Audio::AudioSystemRequestBus::Broadcast(&Audio::AudioSystemRequestBus::Events::PushRequest, request);

            ms_pIAudioProxy->SetPosition(cameraMatrix);
            ms_pIAudioProxy->ExecuteTrigger(ms_nAudioTriggerID, eLSM_None);
        }
    }
}

//-----------------------------------------------------------------------------------------------//
void CAudioControlsEditorPlugin::StopTriggerExecution()
{
    if (ms_pIAudioProxy && ms_nAudioTriggerID != INVALID_AUDIO_CONTROL_ID)
    {
        ms_pIAudioProxy->StopTrigger(ms_nAudioTriggerID);
        ms_nAudioTriggerID = INVALID_AUDIO_CONTROL_ID;
    }
}

//-----------------------------------------------------------------------------------------------//
void CAudioControlsEditorPlugin::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
    switch (event)
    {
    case ESYSTEM_EVENT_AUDIO_IMPLEMENTATION_LOADED:
        GetIEditor()->SuspendUndo();
        ms_implementationManager.LoadImplementation();
        GetIEditor()->ResumeUndo();
        break;
    }
}

//-----------------------------------------------------------------------------------------------//
CImplementationManager* CAudioControlsEditorPlugin::GetImplementationManager()
{
    return &ms_implementationManager;
}

//-----------------------------------------------------------------------------------------------//
template<>
REFGUID CQtViewClass<AudioControls::CAudioControlsEditorWindow>::GetClassID()
{
    // {82AD1635-38A6-4642-A801-EAB7A829411B}
    static const GUID guid =
    {
        0x82AD1635, 0x38A6, 0x4642, { 0xA8, 0x01, 0xEA, 0xB7, 0xA8, 0x29, 0x41, 0x1B }
    };
    return guid;
}
