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

#include "BehaviorTreeManager.h"
#include "BehaviorTree/XmlLoader.h"
#include "BehaviorTree/NodeFactory.h"

#ifdef USING_BEHAVIOR_TREE_VISUALIZER
#include "BehaviorTree/TreeVisualizer.h"
#endif // USING_BEHAVIOR_TREE_VISUALIZER

#ifdef USING_BEHAVIOR_TREE_EXECUTION_STACKS_FILE_LOG
#include "ExecutionStackFileLogger.h"
#endif

#if defined(COMPILE_WITH_MOVEMENT_SYSTEM_DEBUG)
#include "IAIDebugRenderer.h"
#endif

#include "PipeUser.h"

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
#include <Serialization/ClassFactory.h>
#endif
#include <AzFramework/StringFunc/StringFunc.h>

namespace
{
    static CAIActor* GetAIActor(EntityId entityId)
    {
        IEntity* entity = gEnv->pEntitySystem->GetEntity(entityId);
        IAIObject* aiObject = entity ? entity->GetAI() : NULL;
        return aiObject ? aiObject->CastToCAIActor() : NULL;
    }

    static CPipeUser* GetPipeUser(EntityId entityId)
    {
        IEntity* entity = gEnv->pEntitySystem->GetEntity(entityId);
        IAIObject* aiObject = entity ? entity->GetAI() : NULL;
        return aiObject ? aiObject->CastToCPipeUser() : NULL;
    }
}

namespace BehaviorTree
{
    BehaviorTreeManager::BehaviorTreeManager()
    {
        m_nodeFactory.reset(new NodeFactory);
        BehaviorTreeManagerRequestBus::Handler::BusConnect();
    }

    INodeFactory& BehaviorTreeManager::GetNodeFactory()
    {
        assert(m_nodeFactory.get());
        return *m_nodeFactory.get();
    }

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
    SERIALIZATION_CLASS_NULL(INode, "[ Node ]");

    Serialization::ClassFactory<INode>& BehaviorTreeManager::GetNodeSerializationFactory()
    {
        return Serialization::ClassFactory<INode>::the();
    }
#endif

    BehaviorTreeInstancePtr BehaviorTreeManager::CreateBehaviorTreeInstanceFromDiskCache(const char* behaviorTreeName)
    {
        BehaviorTreeInstancePtr instance = LoadFromCache(behaviorTreeName);

        if (instance.get())
        {
            return instance;
        }

        // In the editor the behavior trees are not cached up front so we
        // need to load the tree into the cache if it wasn't already there.
        LoadFromDiskIntoCache(behaviorTreeName);
        return LoadFromCache(behaviorTreeName);
    }

    BehaviorTreeInstancePtr BehaviorTreeManager::CreateBehaviorTreeInstanceFromXml(const char* behaviorTreeName, XmlNodeRef behaviorTreeXmlNode)
    {
        BehaviorTreeTemplatePtr behaviorTreeTemplate = BehaviorTreeTemplatePtr(new BehaviorTreeTemplate());
        if (LoadBehaviorTreeTemplate(behaviorTreeName, behaviorTreeXmlNode, *(behaviorTreeTemplate.get())))
        {
            return BehaviorTreeInstancePtr(new BehaviorTreeInstance(
                    behaviorTreeTemplate->defaultTimestampCollection,
                    behaviorTreeTemplate->variableDeclarations.GetDefaults(),
                    behaviorTreeTemplate,
                    GetNodeFactory()
                    ));
        }

        return BehaviorTreeInstancePtr();
    }

    BehaviorTreeInstancePtr BehaviorTreeManager::LoadFromCache(const char* behaviorTreeName)
    {
        BehaviorTreeCache::iterator findResult = m_behaviorTreeCache.find(behaviorTreeName);
        if (findResult != m_behaviorTreeCache.end())
        {
            BehaviorTreeTemplatePtr behaviorTreeTemplate = findResult->second;
            return BehaviorTreeInstancePtr(new BehaviorTreeInstance(
                    behaviorTreeTemplate->defaultTimestampCollection,
                    behaviorTreeTemplate->variableDeclarations.GetDefaults(),
                    behaviorTreeTemplate,
                    GetNodeFactory()));
        }

        return BehaviorTreeInstancePtr();
    }

    bool BehaviorTreeManager::LoadBehaviorTreeTemplate(const char* behaviorTreeName, XmlNodeRef behaviorTreeXmlNode, BehaviorTreeTemplate& behaviorTreeTemplate)
    {
#if defined(DEBUG_MODULAR_BEHAVIOR_TREE)
        behaviorTreeTemplate.mbtFilename = behaviorTreeName;
#endif

        if (XmlNodeRef timestampsXml = behaviorTreeXmlNode->findChild("Timestamps"))
        {
            behaviorTreeTemplate.defaultTimestampCollection.LoadFromXml(timestampsXml);
        }

        if (XmlNodeRef variablesXml = behaviorTreeXmlNode->findChild("Variables"))
        {
            behaviorTreeTemplate.variableDeclarations.LoadFromXML(variablesXml, behaviorTreeName);
        }

        if (XmlNodeRef signalsXml = behaviorTreeXmlNode->findChild("SignalVariables"))
        {
            behaviorTreeTemplate.signalHandler.LoadFromXML(behaviorTreeTemplate.variableDeclarations, signalsXml, behaviorTreeName);
        }

        LoadContext context(GetNodeFactory(), behaviorTreeName, behaviorTreeTemplate.variableDeclarations);
        behaviorTreeTemplate.rootNode = XmlLoader().CreateBehaviorTreeRootNodeFromBehaviorTreeXml(behaviorTreeXmlNode, context);

        if (!behaviorTreeTemplate.rootNode)
        {
            return false;
        }

        return true;
    }

    void BehaviorTreeManager::LoadFromDiskIntoCache(const char* behaviorTreeName)
    {
        BehaviorTreeCache::iterator findResult = m_behaviorTreeCache.find(behaviorTreeName);
        if (findResult != m_behaviorTreeCache.end())
        {
            return;
        }

        XmlNodeRef behaviorTreeXmlNode = XmlLoader().LoadBehaviorTreeXmlFile("Scripts/AI/BehaviorTrees/", behaviorTreeName);

        if (!behaviorTreeXmlNode)
        {
            behaviorTreeXmlNode = XmlLoader().LoadBehaviorTreeXmlFile("libs/ai/behavior_trees/", behaviorTreeName);
        }

        if (!behaviorTreeXmlNode)
        {
            behaviorTreeXmlNode = XmlLoader().LoadBehaviorTreeXmlFile("@assets@/", behaviorTreeName);
        }

        if (behaviorTreeXmlNode)
        {
            BehaviorTreeTemplatePtr behaviorTreeTemplate = BehaviorTreeTemplatePtr(new BehaviorTreeTemplate());
            if (LoadBehaviorTreeTemplate(behaviorTreeName, behaviorTreeXmlNode, *(behaviorTreeTemplate.get())))
            {
                m_behaviorTreeCache.insert(BehaviorTreeCache::value_type(behaviorTreeName, behaviorTreeTemplate));
            }
            else
            {
                gEnv->pLog->LogError("Modular behavior tree '%s' failed to load.", behaviorTreeName);
            }
        }
        else
        {
            gEnv->pLog->LogError("Failed to load behavior tree '%s'. Could not load the '%s.xml' file from the 'Scripts/AI/BehaviorTrees/' or 'libs/ai/behavior_trees/' folders", behaviorTreeName, behaviorTreeName);
        }
    }

    void BehaviorTreeManager::Reset()
    {
        StopAllBehaviorTreeInstances();
        m_behaviorTreeCache.clear();
        m_nodeFactory->TrimNodeCreators();
#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
        m_errorStatusTrees.clear();
#endif // DEBUG_MODULAR_BEHAVIOR_TREE
    }

    void BehaviorTreeManager::StopAllBehaviorTreeInstances()
    {
        Instances::iterator it = m_instances.begin();
        Instances::iterator end = m_instances.end();
        for (; it != end; ++it)
        {
            BehaviorTreeInstance& instance = *(it->second.get());
            const AZ::EntityId entityId = it->first;

            BehaviorVariablesContext variables(
                instance.variables
                , instance.behaviorTreeTemplate->variableDeclarations
                , instance.variables.Changed()
                );

            IEntity* entity = gEnv->pEntitySystem->GetEntity(GetLegacyEntityId(entityId));

            UpdateContext context(
                entityId
                , entity
                , variables
                , instance.timestampCollection
                , instance.blackboard
#ifdef USING_BEHAVIOR_TREE_LOG
                , instance.behaviorLog
#endif
                );

            instance.behaviorTreeTemplate->rootNode->Terminate(context);
        }

        m_instances.clear();
    }

#ifdef CRYAISYSTEM_DEBUG
    void BehaviorTreeManager::DrawMemoryInformation()
    {
        const float fontSize = 1.25f;
        const float lineHeight = 11.5f * fontSize;
        const float x = 10.0f;
        float y = 30.0f;

        IAIDebugRenderer* renderer = gEnv->pAISystem->GetAIDebugRenderer();

        const size_t immutableSize = m_nodeFactory->GetSizeOfImmutableDataForAllAllocatedNodes();
        const size_t runtimeSize = m_nodeFactory->GetSizeOfRuntimeDataForAllAllocatedNodes();

        renderer->Draw2dLabel(x, y, fontSize, Col_White, false, "MODULAR BEHAVIOR TREE STATISTICS");
        y += lineHeight;
        renderer->Draw2dLabel(x, y, fontSize, Col_White, false, "Immutable: %" PRISIZE_T " bytes", immutableSize);
        y += lineHeight;
        renderer->Draw2dLabel(x, y, fontSize, Col_White, false, "Runtime: %" PRISIZE_T " bytes", runtimeSize);
        y += lineHeight;
    }
#endif

    bool BehaviorTreeManager::StartModularBehaviorTree(const AZ::EntityId entityId, const char* treeName)
    {
        // we need to lop off the extension
        AZStd::string path(treeName);
        AzFramework::StringFunc::Path::StripExtension(path);
        BehaviorTreeInstancePtr instance = CreateBehaviorTreeInstanceFromDiskCache(path.c_str());
        return StartBehaviorInstance(entityId, instance, path.c_str());
    }

    bool BehaviorTreeManager::StartModularBehaviorTreeFromXml(const AZ::EntityId entityId, const char* treeName, XmlNodeRef treeXml)
    {
        BehaviorTreeInstancePtr instance = CreateBehaviorTreeInstanceFromXml(treeName, treeXml);
        return StartBehaviorInstance(entityId, instance, treeName);
    }

    bool BehaviorTreeManager::StartBehaviorInstance(const AZ::EntityId entityId, BehaviorTreeInstancePtr instance, const char* treeName)
    {
        StopModularBehaviorTree(entityId);
        if (instance.get())
        {
            m_instances.insert(std::make_pair(entityId, instance));
#ifdef USING_BEHAVIOR_TREE_EXECUTION_STACKS_FILE_LOG
            m_executionStackFileLoggerInstances.insert(std::make_pair(entityId, ExecutionStackFileLoggerPtr(new ExecutionStackFileLogger(GetLegacyEntityId(entityId)))));
#endif
            return true;
        }
        else
        {
            gEnv->pLog->LogError("Failed to start modular behavior tree '%s'.", treeName);
            return false;
        }
    }

    void BehaviorTreeManager::StopModularBehaviorTree(const AZ::EntityId entityId)
    {
        Instances::iterator it = m_instances.find(entityId);
        if (it != m_instances.end())
        {
            BehaviorTreeInstance& instance = *(it->second.get());

            BehaviorVariablesContext variables(
                instance.variables
                , instance.behaviorTreeTemplate->variableDeclarations
                , instance.variables.Changed()
                );

            IEntity* entity = gEnv->pEntitySystem->GetEntity(GetLegacyEntityId(entityId));

            UpdateContext context(
                entityId
                , entity
                , variables
                , instance.timestampCollection
                , instance.blackboard
#ifdef USING_BEHAVIOR_TREE_LOG
                , instance.behaviorLog
#endif
                );

            instance.behaviorTreeTemplate->rootNode->Terminate(context);
            m_instances.erase(it);
        }

#ifdef USING_BEHAVIOR_TREE_EXECUTION_STACKS_FILE_LOG
        m_executionStackFileLoggerInstances.erase(entityId);
#endif
    }


    bool BehaviorTreeManager::StartBehaviorTree(const AZ::EntityId& entityId, const char* treeName)
    {
        return StartModularBehaviorTree(entityId, treeName);
    }
    
    void BehaviorTreeManager::StopBehaviorTree(const AZ::EntityId& entityId)
    {
        StopModularBehaviorTree(entityId);
    }

    AZStd::vector<AZ::Crc32> BehaviorTreeManager::GetVariableNameCrcs(const AZ::EntityId& entityId)
    {
        AZStd::vector<AZ::Crc32> returnValue;
        if (BehaviorTreeInstance* behaviorTree = GetBehaviorTree(entityId))
        {
            behaviorTree->behaviorTreeTemplate->variableDeclarations.GetVariableIds(returnValue);
        }
        return returnValue;
    }

    bool BehaviorTreeManager::GetVariableValue(const AZ::EntityId& entityId, AZ::Crc32 variableNameCrc)
    {
        bool returnValue = false;
        if (BehaviorTreeInstance* behaviorTree = GetBehaviorTree(entityId))
        {
            behaviorTree->variables.GetVariable(variableNameCrc, &returnValue);
        }
        return returnValue;
    }

    void BehaviorTreeManager::SetVariableValue(const AZ::EntityId& entityId, AZ::Crc32 variableNameCrc, bool newValue)
    {
        if (BehaviorTreeInstance* behaviorTree = GetBehaviorTree(entityId))
        {
            behaviorTree->variables.SetVariable(variableNameCrc, newValue);
        }
    }

    void BehaviorTreeManager::Update()
    {
        EntityIdVector newErrorStatusTree;

        Instances::iterator it = m_instances.begin();
        Instances::iterator end = m_instances.end();

        for (; it != end; ++it)
        {
            const AZ::EntityId azEntityId = it->first;
            IEntity* agentEntity = nullptr;

            if (IsLegacyEntityId(azEntityId))
            {
                EntityId entityId = GetLegacyEntityId(azEntityId);
                agentEntity = gEnv->pEntitySystem->GetEntity(entityId);
                assert(agentEntity);
                if (!agentEntity)
                {
                    continue;
                }

                CPipeUser* pipeUser = GetPipeUser(entityId);
                if (pipeUser && (!pipeUser->IsEnabled() || pipeUser->IsPaused()))
                {
                    continue;
                }
            }
            BehaviorTreeInstance& instance = *(it->second.get());

            BehaviorVariablesContext variables(instance.variables, instance.behaviorTreeTemplate->variableDeclarations, instance.variables.Changed());
            instance.variables.ResetChanged();

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
            DebugTree debugTree;
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

            UpdateContext updateContext(
                azEntityId
                , agentEntity
                , variables
                , instance.timestampCollection
                , instance.blackboard
#ifdef USING_BEHAVIOR_TREE_LOG
                , instance.behaviorLog
#endif // USING_BEHAVIOR_TREE_LOG
#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
                , &debugTree
#endif // DEBUG_MODULAR_BEHAVIOR_TREE
                );

            const Status behaviorStatus = instance.behaviorTreeTemplate->rootNode->Tick(updateContext);

            IF_UNLIKELY (behaviorStatus == Success || behaviorStatus == Failure)
            {
                string message;
                message.Format("Modular Behavior Tree Error Status: The root node for entity '%s' %s. Having the root succeed or fail is considered undefined behavior and the tree should be designed in a way that the root node is always running.", agentEntity ? agentEntity->GetName() : "", (behaviorStatus == Success ? "SUCCEEDED" : "FAILED"));

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
                DynArray<DebugNodePtr>::const_iterator it = debugTree.GetSucceededAndFailedNodes().begin();
                DynArray<DebugNodePtr>::const_iterator end = debugTree.GetSucceededAndFailedNodes().end();
                for (; it != end; ++it)
                {
                    const BehaviorTree::Node* node = static_cast<const BehaviorTree::Node*>((*it)->node);
                    message.append(stack_string().Format(" (%d) %s.", node->GetXmlLine(), node->GetCreator()->GetTypeName()).c_str());
                }
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

                gEnv->pLog->LogError("%s", message.c_str());

                newErrorStatusTree.push_back(azEntityId);

                continue;
            }

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
            const bool debugThisAgent = GetAISystem()->GetAgentDebugTarget() && (GetAISystem()->GetAgentDebugTarget() == GetLegacyEntityId(azEntityId));
            if (debugThisAgent)
            {
                UpdateDebugVisualization(updateContext, azEntityId, debugTree, instance, agentEntity);
            }

            if (gAIEnv.CVars.LogModularBehaviorTreeExecutionStacks == 1 && debugThisAgent || gAIEnv.CVars.LogModularBehaviorTreeExecutionStacks == 2)
            {
                UpdateExecutionStackLogging(updateContext, azEntityId, debugTree, instance);
            }
#endif // DEBUG_MODULAR_BEHAVIOR_TREE
        }

        for (EntityIdVector::iterator it = newErrorStatusTree.begin(), end = newErrorStatusTree.end(); it != end; ++it)
        {
#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
            m_errorStatusTrees.push_back(*it);
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

            StopModularBehaviorTree(*it);
        }

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
        if (gAIEnv.CVars.DebugDraw > 0)
        {
            for (EntityIdVector::iterator it = m_errorStatusTrees.begin(), end = m_errorStatusTrees.end(); it != end; ++it)
            {
                if (IEntity* entity = gEnv->pEntitySystem->GetEntity(GetLegacyEntityId(*it)))
                {
                    const float color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
                    const Vec3 position = entity->GetPos() + Vec3(0.0f, 0.0f, 2.0f);
                    gEnv->pRenderer->DrawLabelEx(position, 1.5f, color, true, true, "Behavior tree error.");
                }
            }
        }
#endif // DEBUG_MODULAR_BEHAVIOR_TREE
    }

    size_t BehaviorTreeManager::GetTreeInstanceCount() const
    {
        return m_instances.size();
    }

    EntityId BehaviorTreeManager::GetTreeInstanceEntityIdByIndex(const size_t index) const
    {
        assert(index < m_instances.size());
        Instances::const_iterator it = m_instances.begin();
        std::advance(it, index);
        return GetLegacyEntityId(it->first);
    }

    BehaviorTreeInstance* BehaviorTreeManager::GetBehaviorTree(const AZ::EntityId entityId) const
    {
        Instances::const_iterator it = m_instances.find(entityId);
        if (it != m_instances.end())
        {
            return it->second.get();
        }
        return NULL;
    }

    Variables::Collection* BehaviorTreeManager::GetBehaviorVariableCollection_Deprecated(const AZ::EntityId entityId) const
    {
        if (BehaviorTreeInstance* instance = GetBehaviorTree(entityId))
        {
            return &instance->variables;
        }

        return NULL;
    }

    const Variables::Declarations* BehaviorTreeManager::GetBehaviorVariableDeclarations_Deprecated(const AZ::EntityId entityId) const
    {
        if (BehaviorTreeInstance* instance = GetBehaviorTree(entityId))
        {
            return &instance->behaviorTreeTemplate->variableDeclarations;
        }

        return NULL;
    }

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
    void BehaviorTreeManager::UpdateDebugVisualization(UpdateContext updateContext, const AZ::EntityId entityId, DebugTree debugTree, BehaviorTreeInstance& instance, IEntity* agentEntity)
    {
#ifdef USING_BEHAVIOR_TREE_VISUALIZER
        BehaviorTree::TreeVisualizer treeVisualizer(updateContext);
        treeVisualizer.Draw(
            debugTree
            , instance.behaviorTreeTemplate->mbtFilename
            , agentEntity->GetName()
#ifdef USING_BEHAVIOR_TREE_LOG
            , instance.behaviorLog
#endif // USING_BEHAVIOR_TREE_LOG
            , instance.timestampCollection
            , instance.blackboard
#ifdef USING_BEHAVIOR_TREE_EVENT_DEBUGGING
            , instance.eventsLog
#endif // USING_BEHAVIOR_TREE_EVENT_DEBUGGING
            );
#endif // USING_BEHAVIOR_TREE_VISUALIZER

#ifdef DEBUG_VARIABLE_COLLECTION
        Variables::DebugHelper::DebugDraw(true, instance.variables, instance.behaviorTreeTemplate->variableDeclarations);
#endif // DEBUG_VARIABLE_COLLECTION
    }
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

#ifdef USING_BEHAVIOR_TREE_EXECUTION_STACKS_FILE_LOG
    void BehaviorTreeManager::UpdateExecutionStackLogging(UpdateContext updateContext, const AZ::EntityId entityId, DebugTree debugTree, BehaviorTreeInstance& instance)
    {
        ExecutionStackFileLoggerInstances::const_iterator itTreeHistory = m_executionStackFileLoggerInstances.find(entityId);
        if (itTreeHistory != m_executionStackFileLoggerInstances.end())
        {
            ExecutionStackFileLogger* logger = itTreeHistory->second.get();
            logger->LogDebugTree(debugTree, updateContext, instance);
        }
    }
#endif

    void BehaviorTreeManager::HandleEvent(const AZ::EntityId entityId, Event& event)
    {
        assert(entityId.IsValid());

        if (BehaviorTreeInstance* behaviorTreeInstance = GetBehaviorTree(entityId))
        {
            behaviorTreeInstance->behaviorTreeTemplate->signalHandler.ProcessSignal(event.GetCRC(), behaviorTreeInstance->variables);
            behaviorTreeInstance->timestampCollection.HandleEvent(event.GetCRC());
            BehaviorTree::EventContext context(entityId);
            behaviorTreeInstance->behaviorTreeTemplate->rootNode->SendEvent(context, event);
#ifdef USING_BEHAVIOR_TREE_EVENT_DEBUGGING
            behaviorTreeInstance->eventsLog.AddMessage(event.GetName());
#endif // USING_BEHAVIOR_TREE_EVENT_DEBUGGING
        }
#ifdef USING_BEHAVIOR_TREE_EVENT_DEBUGGING
        else
        {
            const char* entityName = "";
            if (IEntity* entity = gEnv->pEntitySystem->GetEntity(GetLegacyEntityId(entityId)))
            {
                entityName = entity->GetName();
            }

            stack_string errorText;
            errorText.Format("BehaviorTreeManager: The event '%s' was not handled because the entity '%s' is not running a behavior tree.", event.GetName(), entityName);
            gEnv->pLog->LogError(errorText.c_str());
        }
#endif // USING_BEHAVIOR_TREE_EVENT_DEBUGGING
    }
}
