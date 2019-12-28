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

#include <EMotionFX/CommandSystem/Source/CommandManager.h>
#include <EMotionFX/CommandSystem/Source/MotionCommands.h>
#include <EMotionFX/Source/MotionManager.h>
#include <EMotionFX/Source/MotionSystem.h>
#include <EMotionFX/Source/SkeletalMotion.h>
#include <EMotionStudio/EMStudioSDK/Source/EMStudioManager.h>
#include <EMotionStudio/EMStudioSDK/Source/FileManager.h>
#include <EMotionStudio/EMStudioSDK/Source/MainWindow.h>
#include <EMotionStudio/EMStudioSDK/Source/SaveChangedFilesManager.h>
#include <EMotionStudio/Plugins/StandardPlugins/Source/MotionWindow/MotionExtractionWindow.h>
#include <EMotionStudio/Plugins/StandardPlugins/Source/MotionWindow/MotionListWindow.h>
#include <EMotionStudio/Plugins/StandardPlugins/Source/MotionWindow/MotionPropertiesWindow.h>
#include <EMotionStudio/Plugins/StandardPlugins/Source/MotionWindow/MotionRetargetingWindow.h>
#include <EMotionStudio/Plugins/StandardPlugins/Source/MotionWindow/MotionWindowPlugin.h>
#include <MCore/Source/LogManager.h>
#include <MysticQt/Source/ButtonGroup.h>
#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>


namespace EMStudio
{
    class SaveDirtyMotionFilesCallback
        : public SaveDirtyFilesCallback
    {
        MCORE_MEMORYOBJECTCATEGORY(SaveDirtyMotionFilesCallback, MCore::MCORE_DEFAULT_ALIGNMENT, MEMCATEGORY_STANDARDPLUGINS)

    public:
        SaveDirtyMotionFilesCallback(MotionWindowPlugin* plugin)
            : SaveDirtyFilesCallback() { mPlugin = plugin; }
        ~SaveDirtyMotionFilesCallback()                                                     {}

        enum
        {
            TYPE_ID = 0x00000003
        };
        uint32 GetType() const override                                                     { return TYPE_ID; }
        uint32 GetPriority() const override                                                 { return 3; }
        bool GetIsPostProcessed() const override                                            { return false; }

        void GetDirtyFileNames(AZStd::vector<AZStd::string>* outFileNames, AZStd::vector<ObjectPointer>* outObjects) override
        {
            // get the number of motions and iterate through them
            const uint32 numMotions = EMotionFX::GetMotionManager().GetNumMotions();
            for (uint32 i = 0; i < numMotions; ++i)
            {
                EMotionFX::Motion* motion = EMotionFX::GetMotionManager().GetMotion(i);

                if (motion->GetIsOwnedByRuntime())
                {
                    continue;
                }

                // return in case we found a dirty file
                if (motion->GetDirtyFlag())
                {
                    // add the filename to the dirty filenames array
                    outFileNames->push_back(motion->GetFileName());

                    // add the link to the actual object
                    ObjectPointer objPointer;
                    objPointer.mMotion = motion;
                    outObjects->push_back(objPointer);
                }
            }
        }

        int SaveDirtyFiles(const AZStd::vector<AZStd::string>& filenamesToSave, const AZStd::vector<ObjectPointer>& objects, MCore::CommandGroup* commandGroup) override
        {
            MCORE_UNUSED(filenamesToSave);

            const size_t numObjects = objects.size();
            for (size_t i = 0; i < numObjects; ++i)
            {
                // get the current object pointer and skip directly if the type check fails
                ObjectPointer objPointer = objects[i];
                if (objPointer.mMotion == nullptr)
                {
                    continue;
                }

                EMotionFX::Motion* motion = objPointer.mMotion;
                if (mPlugin->SaveDirtyMotion(motion, commandGroup, false) == DirtyFileManager::CANCELED)
                {
                    return DirtyFileManager::CANCELED;
                }
            }

            return DirtyFileManager::FINISHED;
        }

        const char* GetExtension() const override       { return "motion"; }
        const char* GetFileType() const override        { return "motion"; }
        const AZ::Uuid GetFileRttiType() const override
        {
            return azrtti_typeid<EMotionFX::Motion>();
        }

    private:
        MotionWindowPlugin* mPlugin;
    };


    AZStd::vector<EMotionFX::MotionInstance*> MotionWindowPlugin::mInternalMotionInstanceSelection;


    MotionWindowPlugin::MotionWindowPlugin()
        : EMStudio::DockWidgetPlugin()
    {
        mDialogStack                        = nullptr;
        mMotionListWindow                   = nullptr;
        mMotionPropertiesWindow             = nullptr;
        mMotionExtractionWindow             = nullptr;
        mMotionRetargetingWindow            = nullptr;
        mDirtyFilesCallback                 = nullptr;
    }


    MotionWindowPlugin::~MotionWindowPlugin()
    {
        delete mDialogStack;
        ClearMotionEntries();

        // unregister the command callbacks and get rid of the memory
        for (auto callback : m_callbacks)
        {
            GetCommandManager()->RemoveCommandCallback(callback, true);
        }
        m_callbacks.clear();

        GetMainWindow()->GetDirtyFileManager()->RemoveCallback(mDirtyFilesCallback, false);
        delete mDirtyFilesCallback;
    }


    void MotionWindowPlugin::ClearMotionEntries()
    {
        const size_t numEntries = mMotionEntries.size();
        for (size_t i = 0; i < numEntries; ++i)
        {
            delete mMotionEntries[i];
        }
        mMotionEntries.clear();
    }


    EMStudioPlugin* MotionWindowPlugin::Clone()
    {
        return new MotionWindowPlugin();
    }


    bool MotionWindowPlugin::Init()
    {
        //LogInfo("Initializing motion window.");

        // create and register the command callbacks only (only execute this code once for all plugins)
        GetCommandManager()->RegisterCommandCallback<CommandImportMotionCallback>("ImportMotion", m_callbacks, true);
        GetCommandManager()->RegisterCommandCallback<CommandRemoveMotionPostCallback>("RemoveMotion", m_callbacks, false);
        GetCommandManager()->RegisterCommandCallback<CommandSaveMotionAssetInfoCallback>("SaveMotionAssetInfo", m_callbacks, false);
        GetCommandManager()->RegisterCommandCallback<CommandAdjustDefaultPlayBackInfoCallback>("AdjustDefaultPlayBackInfo", m_callbacks, false);
        GetCommandManager()->RegisterCommandCallback<CommandAdjustMotionCallback>("AdjustMotion", m_callbacks, false);
        GetCommandManager()->RegisterCommandCallback<CommandLoadMotionSetCallback>("LoadMotionSet", m_callbacks, false);
        GetCommandManager()->RegisterCommandCallback<CommandKeyframeCompressMotionCallback>("KeyframeCompressMotion", m_callbacks, false);
        GetCommandManager()->RegisterCommandCallback<CommandWaveletCompressMotionCallback>("WaveletCompressMotion", m_callbacks, false);
        GetCommandManager()->RegisterCommandCallback<CommandScaleMotionDataCallback>("ScaleMotionData", m_callbacks, false);
        GetCommandManager()->RegisterCommandCallback<CommandSelectCallback>("Select", m_callbacks, false);

        QSplitter* splitterWidget = new QSplitter(mDock);
        splitterWidget->setOrientation(Qt::Horizontal);
        splitterWidget->setChildrenCollapsible(false);
        mDock->SetContents(splitterWidget);

        // create the motion list stack window
        mMotionListWindow = new MotionListWindow(splitterWidget, this);
        mMotionListWindow->Init();
        splitterWidget->addWidget(mMotionListWindow);

        // reinitialize the motion table entries
        ReInit();

        // create the dialog stack
        assert(mDialogStack == nullptr);
        mDialogStack = new MysticQt::DialogStack(splitterWidget);
        mDialogStack->setMinimumWidth(279);
        splitterWidget->addWidget(mDialogStack);

        // add the motion properties stack window
        mMotionPropertiesWindow = new MotionPropertiesWindow(mDialogStack, this);
        mMotionPropertiesWindow->Init();
        mDialogStack->Add(mMotionPropertiesWindow, "Motion Properties");

        // add the motion extraction stack window
        mMotionExtractionWindow = new MotionExtractionWindow(mDialogStack, this);
        mMotionExtractionWindow->Init();
        mDialogStack->Add(mMotionExtractionWindow, "Motion Extraction");

        // add the motion retargeting stack window
        mMotionRetargetingWindow = new MotionRetargetingWindow(mDialogStack, this);
        mMotionRetargetingWindow->Init();
        mDialogStack->Add(mMotionRetargetingWindow, "Motion Retargeting");

        // connect the window activation signal to refresh if reactivated
        connect(mDock, &MysticQt::DockWidget::visibilityChanged, this, &MotionWindowPlugin::VisibilityChanged);

        // update the new interface and return success
        UpdateInterface();

        // initialize the dirty files callback
        mDirtyFilesCallback = new SaveDirtyMotionFilesCallback(this);
        GetMainWindow()->GetDirtyFileManager()->AddCallback(mDirtyFilesCallback);

        return true;
    }


    bool MotionWindowPlugin::AddMotion(uint32 motionID)
    {
        if (FindMotionEntryByID(motionID) == nullptr)
        {
            EMotionFX::Motion* motion = EMotionFX::GetMotionManager().FindMotionByID(motionID);
            if (motion)
            {
                if (!motion->GetIsOwnedByRuntime())
                {
                    mMotionEntries.push_back(new MotionTableEntry(motion));
                    return mMotionListWindow->AddMotionByID(motionID);
                }
            }
        }

        return false;
    }


    bool MotionWindowPlugin::RemoveMotionByIndex(size_t index)
    {
        const uint32 motionID = mMotionEntries[index]->mMotionID;

        delete mMotionEntries[index];
        mMotionEntries.erase(mMotionEntries.begin() + index);

        return mMotionListWindow->RemoveMotionByID(motionID);
    }


    bool MotionWindowPlugin::RemoveMotionById(uint32 motionID)
    {
        const size_t numMotionEntries = mMotionEntries.size();
        for (size_t i = 0; i < numMotionEntries; ++i)
        {
            if (mMotionEntries[i]->mMotionID == motionID)
            {
                return RemoveMotionByIndex(i);
            }
        }

        return false;
    }


#ifdef _DEBUG
    bool MotionWindowPlugin::VerifyMotions()
    {
        // get the number of motions in the motion library and iterate through them
        const uint32 numLibraryMotions = EMotionFX::GetMotionManager().GetNumMotions();
        for (uint32 i = 0; i < numLibraryMotions; ++i)
        {
            EMotionFX::Motion* motion = EMotionFX::GetMotionManager().GetMotion(i);

            if (motion->GetIsOwnedByRuntime())
            {
                continue;
            }

            // check if the motion table entry at the given index is pointing to the correct motion
            MotionTableEntry* motionEntry = FindMotionEntryByID(motion->GetID());
            if (motionEntry)
            {
                MCore::LogError("MotionWindowPlugin: Motion table entry motion '%s' was not found.", motionEntry->mMotion->GetFileName());
                return false;
            }

            if (motionEntry && motionEntry->mMotion != motion)
            {
                MCore::LogError("MotionWindowPlugin: Motion table entry motion '%s' is not the same as the one in the motion manager '%s'.", motionEntry->mMotion->GetFileName(), motion->GetFileName());
                return false;
            }
        }

        // everything ok
        return true;
    }
#endif


    void MotionWindowPlugin::ReInit()
    {
        // verify if the shown motions in the table widget and the ones inside the motion manager are consistent
    #ifdef _DEBUG
        VerifyMotions();
    #endif

        uint32 i;

        // get the number of motions in the motion library and iterate through them
        const uint32 numLibraryMotions = EMotionFX::GetMotionManager().GetNumMotions();
        for (i = 0; i < numLibraryMotions; ++i)
        {
            // check if we have already added this motion, if not add it
            EMotionFX::Motion* motion = EMotionFX::GetMotionManager().GetMotion(i);
            if (motion->GetIsOwnedByRuntime())
            {
                continue;
            }
            if (FindMotionEntryByID(motion->GetID()) == nullptr)
            {
                mMotionEntries.push_back(new MotionTableEntry(motion));
            }
        }

        // iterate through all motions inside the motion window plugin
        i = 0;
        while (i < mMotionEntries.size())
        {
            MotionTableEntry*   entry   = mMotionEntries[i];
            // check if the motion still is in the motion library, if not also remove it from the motion window plugin
            if (EMotionFX::GetMotionManager().FindMotionIndexByID(entry->mMotionID) == MCORE_INVALIDINDEX32)
            {
                delete mMotionEntries[i];
                mMotionEntries.erase(mMotionEntries.begin() + i);
            }
            else
            {
                i++;
            }
        }

        // update the motion list window
        mMotionListWindow->ReInit();
    }


    void MotionWindowPlugin::UpdateMotions()
    {
        mMotionPropertiesWindow->UpdateMotions();
        mMotionRetargetingWindow->UpdateMotions();
    }


    void MotionWindowPlugin::UpdateInterface()
    {
        AZStd::vector<EMotionFX::MotionInstance*>& motionInstances = GetSelectedMotionInstances();
        const size_t numMotionInstances = motionInstances.size();
        for (size_t i = 0; i < numMotionInstances; ++i)
        {
            EMotionFX::MotionInstance* motionInstance = motionInstances[i];
            EMotionFX::Motion* motion = motionInstance->GetMotion();
            motionInstance->InitFromPlayBackInfo(*motion->GetDefaultPlayBackInfo(), false);

            // security check for motion mirroring, disable motion mirroring in case the node
            EMotionFX::ActorInstance* actorInstance = motionInstance->GetActorInstance();
            EMotionFX::Actor* actor = actorInstance->GetActor();
            if (actor->GetHasMirrorInfo() == false)
            {
                motionInstance->SetMirrorMotion(false);
            }
        }

        if (mMotionPropertiesWindow)
        {
            mMotionPropertiesWindow->UpdateInterface();
        }
        if (mMotionListWindow)
        {
            mMotionListWindow->UpdateInterface();
        }
        if (mMotionExtractionWindow)
        {
            mMotionExtractionWindow->UpdateInterface();
        }
        if (mMotionRetargetingWindow)
        {
            mMotionRetargetingWindow->UpdateInterface();
        }
    }



    void MotionWindowPlugin::VisibilityChanged(bool visible)
    {
        if (visible)
        {
            //mMotionRetargetingWindow->UpdateSelection();
            //mMotionExtractionWindow->UpdateExtractionNodeLabel();
        }
    }


    AZStd::vector<EMotionFX::MotionInstance*>& MotionWindowPlugin::GetSelectedMotionInstances()
    {
        const CommandSystem::SelectionList& selectionList               = CommandSystem::GetCommandManager()->GetCurrentSelection();
        const uint32                        numSelectedActorInstances   = selectionList.GetNumSelectedActorInstances();
        const uint32                        numSelectedMotions          = selectionList.GetNumSelectedMotions();

        mInternalMotionInstanceSelection.clear();

        for (uint32 i = 0; i < numSelectedActorInstances; ++i)
        {
            EMotionFX::ActorInstance*   actorInstance       = selectionList.GetActorInstance(i);
            EMotionFX::MotionSystem*    motionSystem        = actorInstance->GetMotionSystem();
            const uint32    numMotionInstances  = motionSystem->GetNumMotionInstances();

            for (uint32 j = 0; j < numSelectedMotions; ++j)
            {
                EMotionFX::Motion* motion = selectionList.GetMotion(j);

                for (uint32 k = 0; k < numMotionInstances; ++k)
                {
                    EMotionFX::MotionInstance* motionInstance = motionSystem->GetMotionInstance(k);
                    if (motionInstance->GetMotion() == motion)
                    {
                        mInternalMotionInstanceSelection.push_back(motionInstance);
                    }
                }
            }
        }

        return mInternalMotionInstanceSelection;
    }


    MotionWindowPlugin::MotionTableEntry* MotionWindowPlugin::FindMotionEntryByID(uint32 motionID)
    {
        const size_t numMotionEntries = mMotionEntries.size();
        for (size_t i = 0; i < numMotionEntries; ++i)
        {
            MotionTableEntry* entry = mMotionEntries[i];
            if (entry->mMotionID == motionID)
            {
                return entry;
            }
        }

        return nullptr;
    }


    void MotionWindowPlugin::PlayMotion(EMotionFX::Motion* motion)
    {
        AZStd::vector<EMotionFX::Motion*> motions;
        motions.push_back(motion);
        PlayMotions(motions);
    }


    void MotionWindowPlugin::PlayMotions(const AZStd::vector<EMotionFX::Motion*>& motions)
    {
        AZStd::string command, commandParameters;
        MCore::CommandGroup commandGroup("Play motions");

        const size_t numMotions = motions.size();
        for (size_t i = 0; i < numMotions; ++i)
        {
            EMotionFX::Motion*                      motion              = motions[i];
            MotionWindowPlugin::MotionTableEntry*   entry               = FindMotionEntryByID(motion->GetID());
            EMotionFX::PlayBackInfo*                defaultPlayBackInfo = motion->GetDefaultPlayBackInfo();

            // Don't blend in and out of the for previewing animations. We might only see a short bit of it for animations smaller than the blend in/out time.
            defaultPlayBackInfo->mBlendInTime = 0.0f;
            defaultPlayBackInfo->mBlendOutTime = 0.0f;

            commandParameters = CommandSystem::CommandPlayMotion::PlayBackInfoToCommandParameters(defaultPlayBackInfo);

            command = AZStd::string::format("PlayMotion -filename \"%s\" %s", motion->GetFileName(), commandParameters.c_str());
            commandGroup.AddCommandString(command);
        }

        AZStd::string result;
        if (!EMStudio::GetCommandManager()->ExecuteCommandGroup(commandGroup, result))
        {
            AZ_Error("EMotionFX", false, result.c_str());
        }
    }


    void MotionWindowPlugin::StopSelectedMotions()
    {
        const CommandSystem::SelectionList& selection = CommandSystem::GetCommandManager()->GetCurrentSelection();

        // get the number of selected motions
        const uint32 numMotions = selection.GetNumSelectedMotions();
        if (numMotions == 0)
        {
            return;
        }

        // create our remove motion command group
        MCore::CommandGroup commandGroup(AZStd::string::format("Stop motion instances", numMotions).c_str());

        AZStd::string command;
        for (uint32 i = 0; i < numMotions; ++i)
        {
            MotionWindowPlugin::MotionTableEntry* entry = FindMotionEntryByID(selection.GetMotion(i)->GetID());
            if (entry == nullptr)
            {
                AZ_Error("EMotionFX", false, "Cannot find motion table entry for the given motion.");
                continue;
            }

            command = AZStd::string::format("StopMotionInstances -filename \"%s\"", entry->mMotion->GetFileName());
            commandGroup.AddCommandString(command);
        }

        AZStd::string result;
        if (!EMStudio::GetCommandManager()->ExecuteCommandGroup(commandGroup, result))
        {
            AZ_Error("EMotionFX", false, result.c_str());
        }
    }


    void MotionWindowPlugin::Render(RenderPlugin* renderPlugin, EMStudioPlugin::RenderInfo* renderInfo)
    {
        MCommon::RenderUtil* renderUtil = renderInfo->mRenderUtil;

        // make sure the render objects are valid
        if (renderPlugin == nullptr || renderUtil == nullptr)
        {
            return;
        }
        /*
        if (mMotionRetargetingWindow->GetRenderMotionBindPose())
        {
            const CommandSystem::SelectionList& selection = CommandSystem::GetCommandManager()->GetCurrentSelection();

            // get the number of selected actor instances and iterate through them
            const uint32 numActorInstances = selection.GetNumSelectedActorInstances();
            for (uint32 j = 0; j < numActorInstances; ++j)
            {
                EMotionFX::ActorInstance*   actorInstance   = selection.GetActorInstance(j);
                EMotionFX::Actor*           actor           = actorInstance->GetActor();

                // get the number of selected motions and iterate through them
                const uint32 numMotions = selection.GetNumSelectedMotions();
                for (uint32 i = 0; i < numMotions; ++i)
                {
                    EMotionFX::Motion* motion = selection.GetMotion(i);
                    if (motion->GetType() == EMotionFX::SkeletalMotion::TYPE_ID)
                    {
                        EMotionFX::SkeletalMotion* skeletalMotion = (EMotionFX::SkeletalMotion*)motion;

                        EMotionFX::AnimGraphPosePool& posePool = EMotionFX::GetEMotionFX().GetThreadData(0)->GetPosePool();
                        EMotionFX::AnimGraphPose* pose = posePool.RequestPose(m_actorInstance);

                        skeletalMotion->CalcMotionBindPose(actor, pose->GetPose());

                        // for all nodes in the actor
                        const uint32 numNodes = actorInstance->GetNumEnabledNodes();
                        for (uint32 n = 0; n < numNodes; ++n)
                        {
                            EMotionFX::Node* curNode = actor->GetSkeleton()->GetNode(actorInstance->GetEnabledNode(n));

                            // skip root nodes, you could also use curNode->IsRootNode()
                            // but we use the parent index here, as we will reuse it
                            uint32 parentIndex = curNode->GetParentIndex();
                            if (parentIndex == MCORE_INVALIDINDEX32)
                            {
                                AZ::Vector3 startPos = mGlobalMatrices[curNode->GetNodeIndex()].GetTranslation();
                                AZ::Vector3 endPos   = startPos + AZ::Vector3(0.0f, 3.0f, 0.0f);
                                renderUtil->RenderLine(startPos, endPos, MCore::RGBAColor(0.0f, 1.0f, 1.0f));
                            }
                            else
                            {
                                AZ::Vector3 startPos = mGlobalMatrices[curNode->GetNodeIndex()].GetTranslation();
                                AZ::Vector3 endPos   = mGlobalMatrices[parentIndex].GetTranslation();
                                renderUtil->RenderLine(startPos, endPos, MCore::RGBAColor(0.0f, 1.0f, 1.0f));
                            }
                        }

                        posePool.FreePose(pose);
                    }
                }
            }
        }
        */
    }


    // constructor
    MotionWindowPlugin::MotionTableEntry::MotionTableEntry(EMotionFX::Motion* motion)
    {
        mMotionID           = motion->GetID();
        mMotion             = motion;
    }


    int MotionWindowPlugin::OnSaveDirtyMotions()
    {
        return GetMainWindow()->GetDirtyFileManager()->SaveDirtyFiles(SaveDirtyMotionFilesCallback::TYPE_ID);
    }


    int MotionWindowPlugin::SaveDirtyMotion(EMotionFX::Motion* motion, MCore::CommandGroup* commandGroup, bool askBeforeSaving, bool showCancelButton)
    {
        // only process changed files
        if (motion->GetDirtyFlag() == false)
        {
            return DirtyFileManager::NOFILESTOSAVE;
        }

        if (askBeforeSaving)
        {
            EMStudio::GetApp()->setOverrideCursor(QCursor(Qt::ArrowCursor));

            QMessageBox msgBox(GetMainWindow());
            AZStd::string text;

            if (motion->GetFileNameString().empty() == false)
            {
                text = AZStd::string::format("Save changes to '%s'?", motion->GetFileName());
            }
            else if (motion->GetNameString().empty() == false)
            {
                text = AZStd::string::format("Save changes to the motion named '%s'?", motion->GetName());
            }
            else
            {
                text = "Save changes to untitled motion?";
            }

            msgBox.setText(text.c_str());
            msgBox.setWindowTitle("Save Changes");

            if (showCancelButton)
            {
                msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
            }
            else
            {
                msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard);
            }

            msgBox.setDefaultButton(QMessageBox::Save);
            msgBox.setIcon(QMessageBox::Question);

            int messageBoxResult = msgBox.exec();
            switch (messageBoxResult)
            {
            case QMessageBox::Save:
            {
                GetMainWindow()->GetFileManager()->SaveMotion(motion->GetID());
                break;
            }
            case QMessageBox::Discard:
            {
                EMStudio::GetApp()->restoreOverrideCursor();
                return DirtyFileManager::FINISHED;
            }
            case QMessageBox::Cancel:
            {
                EMStudio::GetApp()->restoreOverrideCursor();
                return DirtyFileManager::CANCELED;
            }
            }
        }
        else
        {
            // save without asking first
            GetMainWindow()->GetFileManager()->SaveMotion(motion->GetID());
        }

        return DirtyFileManager::FINISHED;
    }

    //-----------------------------------------------------------------------------------------
    // Command callbacks
    //-----------------------------------------------------------------------------------------

    bool ReInitMotionWindowPlugin()
    {
        EMStudioPlugin* plugin = EMStudio::GetPluginManager()->FindActivePlugin(MotionWindowPlugin::CLASS_ID);
        if (plugin == nullptr)
        {
            return false;
        }

        MotionWindowPlugin* motionWindowPlugin = (MotionWindowPlugin*)plugin;
        motionWindowPlugin->ReInit();

        return true;
    }


    bool CallbackAddMotionByID(uint32 motionID)
    {
        EMStudioPlugin* plugin = EMStudio::GetPluginManager()->FindActivePlugin(MotionWindowPlugin::CLASS_ID);
        if (plugin == nullptr)
        {
            return false;
        }

        MotionWindowPlugin* motionWindowPlugin = (MotionWindowPlugin*)plugin;
        motionWindowPlugin->AddMotion(motionID);
        return true;
    }


    bool CallbackRemoveMotion(uint32 motionID)
    {
        EMStudioPlugin* plugin = EMStudio::GetPluginManager()->FindActivePlugin(MotionWindowPlugin::CLASS_ID);
        if (plugin == nullptr)
        {
            return false;
        }

        MotionWindowPlugin* motionWindowPlugin = (MotionWindowPlugin*)plugin;

        // Note: this has to use the index as the plugin always store a synced copy of all motions and this callback is called after the RemoveMotion command has been applied
        // this means the motion is not in the motion manager anymore
        motionWindowPlugin->RemoveMotionById(motionID);

        // An invalid motion id can be passed in case there is a command group where a remove a motion set is before remove a motion command while the motion was in the motion set.
        // In that case the RemoveMotion command can't fill the motion id for the command object as the motion object is already destructed. The root of this issue is that motion sets don't use
        // the asset system yet, nor are they constructed/destructed using the command system like all other motions.
        // For this case we'll call re-init which cleans entries to non-loaded motions and syncs the UI.
        if (motionID == MCORE_INVALIDINDEX32)
        {
            motionWindowPlugin->ReInit();
        }

        return true;
    }


    bool MotionWindowPlugin::CommandImportMotionCallback::Execute(MCore::Command* command, const MCore::CommandLine& commandLine)
    {
        MCORE_UNUSED(commandLine);
        CommandSystem::CommandImportMotion* importMotionCommand = static_cast<CommandSystem::CommandImportMotion*>(command);
        return CallbackAddMotionByID(importMotionCommand->mOldMotionID);
    }


    bool MotionWindowPlugin::CommandImportMotionCallback::Undo(MCore::Command* command, const MCore::CommandLine& commandLine)
    {
        MCORE_UNUSED(command);
        MCORE_UNUSED(commandLine);

        // calls the RemoveMotion command internally, so the callback from that is already called
        return true;
    }


    bool MotionWindowPlugin::CommandRemoveMotionPostCallback::Execute(MCore::Command* command, const MCore::CommandLine& commandLine)
    {
        MCORE_UNUSED(commandLine);
        CommandSystem::CommandRemoveMotion* removeMotionCommand = static_cast<CommandSystem::CommandRemoveMotion*>(command);
        return CallbackRemoveMotion(removeMotionCommand->mOldMotionID);
    }


    bool MotionWindowPlugin::CommandRemoveMotionPostCallback::Undo(MCore::Command* command, const MCore::CommandLine& commandLine)
    {
        MCORE_UNUSED(command);
        MCORE_UNUSED(commandLine);

        // calls the ImportMotion command internally, so the callback from that is already called
        return true;
    }


    bool MotionWindowPlugin::CommandSaveMotionAssetInfoCallback::Execute(MCore::Command* command, const MCore::CommandLine& commandLine)
    {
        return ReInitMotionWindowPlugin();
    }


    bool MotionWindowPlugin::CommandSaveMotionAssetInfoCallback::Undo(MCore::Command* command, const MCore::CommandLine& commandLine)
    {
        return ReInitMotionWindowPlugin();
    }


    bool MotionWindowPlugin::CommandScaleMotionDataCallback::Execute(MCore::Command* command, const MCore::CommandLine& commandLine)
    {
        if (commandLine.GetValueAsBool("skipInterfaceUpdate", command))
        {
            return true;
        }
        return ReInitMotionWindowPlugin();
    }


    bool MotionWindowPlugin::CommandScaleMotionDataCallback::Undo(MCore::Command* command, const MCore::CommandLine& commandLine)
    {
        if (commandLine.GetValueAsBool("skipInterfaceUpdate", command))
        {
            return true;
        }
        return ReInitMotionWindowPlugin();
    }


    bool MotionWindowPlugin::CommandLoadMotionSetCallback::Execute(MCore::Command* command, const MCore::CommandLine& commandLine)      { MCORE_UNUSED(command); MCORE_UNUSED(commandLine); return ReInitMotionWindowPlugin(); }
    bool MotionWindowPlugin::CommandLoadMotionSetCallback::Undo(MCore::Command* command, const MCore::CommandLine& commandLine)         { MCORE_UNUSED(command); MCORE_UNUSED(commandLine); return ReInitMotionWindowPlugin(); }


    bool MotionWindowPlugin::CommandAdjustMotionCallback::Execute(MCore::Command* command, const MCore::CommandLine& commandLine)
    {
        MCORE_UNUSED(command);
        MCORE_UNUSED(commandLine);
        return ReInitMotionWindowPlugin();
    }


    bool MotionWindowPlugin::CommandAdjustMotionCallback::Undo(MCore::Command* command, const MCore::CommandLine& commandLine)
    {
        MCORE_UNUSED(command);
        MCORE_UNUSED(commandLine);
        return ReInitMotionWindowPlugin();
    }


    bool MotionWindowPlugin::CommandKeyframeCompressMotionCallback::Execute(MCore::Command* command, const MCore::CommandLine& commandLine)     { MCORE_UNUSED(command); MCORE_UNUSED(commandLine); return ReInitMotionWindowPlugin(); }
    bool MotionWindowPlugin::CommandKeyframeCompressMotionCallback::Undo(MCore::Command* command, const MCore::CommandLine& commandLine)        { MCORE_UNUSED(command); MCORE_UNUSED(commandLine); return ReInitMotionWindowPlugin(); }
    bool MotionWindowPlugin::CommandWaveletCompressMotionCallback::Execute(MCore::Command* command, const MCore::CommandLine& commandLine)      { MCORE_UNUSED(command); MCORE_UNUSED(commandLine); return ReInitMotionWindowPlugin(); }
    bool MotionWindowPlugin::CommandWaveletCompressMotionCallback::Undo(MCore::Command* command, const MCore::CommandLine& commandLine)         { MCORE_UNUSED(command); MCORE_UNUSED(commandLine); return ReInitMotionWindowPlugin(); }


    bool UpdateInterfaceMotionWindowPlugin()
    {
        EMStudioPlugin* plugin = EMStudio::GetPluginManager()->FindActivePlugin(MotionWindowPlugin::CLASS_ID);
        if (plugin == nullptr)
        {
            return false;
        }

        MotionWindowPlugin* motionWindowPlugin = (MotionWindowPlugin*)plugin;
        motionWindowPlugin->UpdateInterface();

        return true;
    }


    bool MotionWindowPlugin::CommandAdjustDefaultPlayBackInfoCallback::Execute(MCore::Command* command, const MCore::CommandLine& commandLine)  { MCORE_UNUSED(command); MCORE_UNUSED(commandLine); return UpdateInterfaceMotionWindowPlugin(); }
    bool MotionWindowPlugin::CommandAdjustDefaultPlayBackInfoCallback::Undo(MCore::Command* command, const MCore::CommandLine& commandLine)     { MCORE_UNUSED(command); MCORE_UNUSED(commandLine); return UpdateInterfaceMotionWindowPlugin(); }


    bool MotionWindowPlugin::CommandSelectCallback::Execute(MCore::Command* command, const MCore::CommandLine& commandLine)
    {
        AZ_UNUSED(command);

        if (CommandSystem::CheckIfHasMotionSelectionParameter(commandLine))
        {
            EMStudioPlugin* plugin = EMStudio::GetPluginManager()->FindActivePlugin(MotionWindowPlugin::CLASS_ID);
            if (plugin == nullptr)
            {
                return false;
            }

            MotionWindowPlugin* motionWindowPlugin = static_cast<MotionWindowPlugin*>(plugin);
            motionWindowPlugin->UpdateInterface();
        }

        return true;
    }

    bool MotionWindowPlugin::CommandSelectCallback::Undo(MCore::Command* command, const MCore::CommandLine& commandLine)
    {
        AZ_UNUSED(command);

        if (CommandSystem::CheckIfHasMotionSelectionParameter(commandLine))
        {
            EMStudioPlugin* plugin = EMStudio::GetPluginManager()->FindActivePlugin(MotionWindowPlugin::CLASS_ID);
            if (plugin == nullptr)
            {
                return false;
            }

            MotionWindowPlugin* motionWindowPlugin = static_cast<MotionWindowPlugin*>(plugin);
            motionWindowPlugin->UpdateInterface();
        }

        return true;
    }
} // namespace EMStudio

#include <EMotionFX/Tools/EMotionStudio/Plugins/StandardPlugins/Source/MotionWindow/MotionWindowPlugin.moc>
