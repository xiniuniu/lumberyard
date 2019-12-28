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
#include <precompiled.h>

#include <GraphCanvas/Components/Connections/ConnectionBus.h>
#include <GraphCanvas/Components/GridBus.h>
#include <GraphCanvas/Components/Nodes/NodeBus.h>
#include <GraphCanvas/Components/SceneBus.h>
#include <GraphCanvas/Components/StyleBus.h>
#include <GraphCanvas/Components/VisualBus.h>
#include <GraphCanvas/Utils/GraphUtils.h>
#include <GraphCanvas/Utils/NodeNudgingController.h>

#include <Editor/View/Widgets/ValidationPanel/GraphValidationDockWidget.h>
#include <Editor/View/Widgets/ValidationPanel/ui_GraphValidationPanel.h>
#include <Editor/View/Widgets/ValidationPanel/GraphValidationDockWidgetBus.h>

#include <Editor/GraphCanvas/GraphCanvasEditorNotificationBusId.h>
#include <Editor/Nodes/NodeUtils.h>
#include <Editor/View/Widgets/VariablePanel/VariableDockWidget.h>
#include <ScriptCanvas/Debugger/ValidationEvents/DataValidation/DataValidationEvents.h>
#include <ScriptCanvas/Debugger/ValidationEvents/ExecutionValidation/ExecutionValidationEvents.h>

#include <ScriptCanvas/Bus/RequestBus.h>
#include <ScriptCanvas/Core/ConnectionBus.h>
#include <ScriptCanvas/Core/NodeBus.h>
#include <ScriptCanvas/GraphCanvas/MappingBus.h>
#include <ScriptCanvas/GraphCanvas/NodeDescriptorBus.h>
#include <ScriptCanvas/Variable/VariableBus.h>
#include <ScriptCanvas/Bus/EditorScriptCanvasBus.h>

namespace ScriptCanvasEditor
{
    /////////////////////////////////////
    // HighlightElementValidationEffect
    /////////////////////////////////////

    HighlightElementValidationEffect::HighlightElementValidationEffect()
    {
        m_templateConfiguration.m_blurRadius = 5;

        m_templateConfiguration.m_pen = QPen();
        m_templateConfiguration.m_pen.setBrush(Qt::red);
        m_templateConfiguration.m_pen.setWidth(5);

        m_templateConfiguration.m_zValue = 0;
        m_templateConfiguration.m_pulseRate = AZStd::chrono::milliseconds(2500);
    }

    HighlightElementValidationEffect::HighlightElementValidationEffect(const QColor& color)
        : HighlightElementValidationEffect()
    {
        m_templateConfiguration.m_pen.setBrush(color);
    }

    HighlightElementValidationEffect::HighlightElementValidationEffect(const GraphCanvas::SceneMemberGlowOutlineConfiguration& glowConfiguration)
        : m_templateConfiguration(glowConfiguration)
    {
    }

    void HighlightElementValidationEffect::AddTarget(const AZ::EntityId& scriptCanvasTargetId)
    {
        AZ::EntityId graphCanvasMemberId;
        SceneMemberMappingRequestBus::EventResult(graphCanvasMemberId, scriptCanvasTargetId, &SceneMemberMappingRequests::GetGraphCanvasEntityId);

        m_targets.emplace_back(graphCanvasMemberId);
    }

    void HighlightElementValidationEffect::DisplayEffect(const GraphCanvas::GraphId& graphId)
    {
        for (const auto& targetId : m_targets)
        {
            GraphCanvas::SceneMemberGlowOutlineConfiguration glowConfiguration = m_templateConfiguration;

            glowConfiguration.m_sceneMember = targetId;
            GraphCanvas::SceneMemberUIRequestBus::EventResult(glowConfiguration.m_zValue, targetId, &GraphCanvas::SceneMemberUIRequests::GetZValue);

            GraphCanvas::GraphicsEffectId effectId;
            GraphCanvas::SceneRequestBus::EventResult(effectId, graphId, &GraphCanvas::SceneRequests::CreateGlowOnSceneMember, glowConfiguration);

            if (effectId.IsValid())
            {
                m_graphicEffectIds.emplace_back(effectId);
            }
        }

        m_graphId = graphId;
    }

    void HighlightElementValidationEffect::CancelEffect()
    {
        for (const auto& graphicsEffectId : m_graphicEffectIds)
        {
            GraphCanvas::SceneRequestBus::Event(m_graphId, &GraphCanvas::SceneRequests::CancelGraphicsEffect, graphicsEffectId);
        }

        m_graphicEffectIds.clear();
    }

    ///////////////////////////////
    // UnusedNodeValidationEffect
    ///////////////////////////////

    constexpr const char* UnusedSelector = ":unused";
    constexpr const char* UnknownUseState = ":partially_unused";

    void UnusedNodeValidationEffect::AddUnusedNode(const AZ::EntityId& scriptCanvasNodeId)
    {
        AZ::EntityId graphCanvasMemberId;
        SceneMemberMappingRequestBus::EventResult(graphCanvasMemberId, scriptCanvasNodeId, &SceneMemberMappingRequests::GetGraphCanvasEntityId);

        auto insertResult = m_rootUnusedNodes.insert(graphCanvasMemberId);

        if (!insertResult.second)
        {
            return;
        }

        m_isDirty = true;

        m_unprocessedIds.insert(graphCanvasMemberId);
    }

    void UnusedNodeValidationEffect::RemoveUnusedNode(const AZ::EntityId& scriptCanvasNodeId)
    {
        AZ::EntityId graphCanvasMemberId;
        SceneMemberMappingRequestBus::EventResult(graphCanvasMemberId, scriptCanvasNodeId, &SceneMemberMappingRequests::GetGraphCanvasEntityId);

        size_t removeCount = m_rootUnusedNodes.erase(graphCanvasMemberId);

        if (removeCount == 0)
        {
            return;
        }

        m_isDirty = true;

        ClearStyleSelectors();

        m_unprocessedIds = m_rootUnusedNodes;
        m_inactiveNodes.clear();
    }

    void UnusedNodeValidationEffect::DisplayEffect(const GraphCanvas::GraphId& grpahId)
    {
        if (!m_isDirty)
        {
            return;
        }

        AZStd::unordered_set< GraphCanvas::NodeId > processedIds;

        m_isDirty = false;

        while (!m_unprocessedIds.empty())
        {
            AZ::EntityId currentMemberId = (*m_unprocessedIds.begin());
            m_unprocessedIds.erase(m_unprocessedIds.begin());

            processedIds.insert(currentMemberId);

            AZStd::vector< GraphCanvas::SlotId > slotIds;
            GraphCanvas::NodeRequestBus::EventResult(slotIds, currentMemberId, &GraphCanvas::NodeRequests::GetSlotIds);
            
            bool isFullyDisabled = true;

            AZStd::unordered_set< GraphCanvas::ConnectionId > connectionsToStylize;

            for (const auto& slotId : slotIds)
            {
                AZStd::vector< GraphCanvas::ConnectionId > connectionIds;
                GraphCanvas::SlotRequestBus::EventResult(connectionIds, slotId, &GraphCanvas::SlotRequests::GetConnections);

                GraphCanvas::ConnectionType connectionType = GraphCanvas::ConnectionType::CT_Invalid;
                GraphCanvas::SlotRequestBus::EventResult(connectionType, slotId, &GraphCanvas::SlotRequests::GetConnectionType);

                GraphCanvas::SlotType slotType = GraphCanvas::SlotTypes::DataSlot;
                GraphCanvas::SlotRequestBus::EventResult(slotType, slotId, &GraphCanvas::SlotRequests::GetSlotType);

                for (const auto& connectionId : connectionIds)
                {
                    if (slotType == GraphCanvas::SlotTypes::DataSlot
                        || connectionType == GraphCanvas::ConnectionType::CT_Output)
                    {
                        connectionsToStylize.insert(connectionId);
                    }

                    if (slotType == GraphCanvas::SlotTypes::ExecutionSlot)
                    {
                        if (connectionType == GraphCanvas::ConnectionType::CT_Output)
                        {
                            GraphCanvas::Endpoint targetEndpoint;
                            GraphCanvas::ConnectionRequestBus::EventResult(targetEndpoint, connectionId, &GraphCanvas::ConnectionRequests::GetTargetEndpoint);

                            if (processedIds.find(targetEndpoint.GetNodeId()) == processedIds.end())
                            {
                                m_unprocessedIds.insert(targetEndpoint.GetNodeId());
                            }
                        }
                        else if (connectionType == GraphCanvas::ConnectionType::CT_Input)
                        {
                            GraphCanvas::Endpoint sourceEndpoint;
                            GraphCanvas::ConnectionRequestBus::EventResult(sourceEndpoint, connectionId, &GraphCanvas::ConnectionRequests::GetSourceEndpoint);

                            // If we find a node that we are unsure about its activation state.
                            // Don't mark ourselves as fully disabled.
                            if (m_inactiveNodes.find(sourceEndpoint.GetNodeId()) == m_inactiveNodes.end())
                            {
                                isFullyDisabled = false;
                            }
                        }
                    }
                }
            }

            const char* selectorState;

            if (isFullyDisabled)
            {
                selectorState = UnusedSelector;
                m_inactiveNodes.insert(currentMemberId);
            }
            else
            {
                selectorState = UnknownUseState;
            }

            ApplySelector(currentMemberId, selectorState);

            for (const auto& connectionId : connectionsToStylize)
            {
                ApplySelector(connectionId, selectorState);
            }
        }
    }

    void UnusedNodeValidationEffect::CancelEffect()
    {
        // The remove node logic handles these updates
        return;
    }

    void UnusedNodeValidationEffect::ClearStyleSelectors()
    {
        while (!m_styleSelectors.empty())
        {
            auto mapIter = m_styleSelectors.begin();
            RemoveSelector(mapIter->first);
        }
    }

    void UnusedNodeValidationEffect::ApplySelector(const AZ::EntityId& memberId, AZStd::string_view styleSelector)
    {
        RemoveSelector(memberId);

        GraphCanvas::StyledEntityRequestBus::Event(memberId, &GraphCanvas::StyledEntityRequests::AddSelectorState, styleSelector.data());
        m_styleSelectors[memberId] = styleSelector;
    }

    void UnusedNodeValidationEffect::RemoveSelector(const AZ::EntityId& memberId)
    {
        auto mapIter = m_styleSelectors.find(memberId);

        if (mapIter != m_styleSelectors.end())
        {
            GraphCanvas::StyledEntityRequestBus::Event(memberId, &GraphCanvas::StyledEntityRequests::RemoveSelectorState, mapIter->second.data());
            m_styleSelectors.erase(mapIter);
        }
    }

    /////////////////////////
    // GraphValidationModel
    /////////////////////////
    
    GraphValidationModel::GraphValidationModel()
        : m_errorIcon(":/ScriptCanvasEditorResources/Resources/error_icon.png")
        , m_warningIcon(":/ScriptCanvasEditorResources/Resources/warning_symbol.png")
        , m_messageIcon(":/ScriptCanvasEditorResources/Resources/message_icon.png")
        , m_autoFixIcon(":/ScriptCanvasEditorResources/Resources/wrench_icon.png")
    {
    }
    
    GraphValidationModel::~GraphValidationModel()
    {
        m_validationResults.ClearResults();
    }
    
    void GraphValidationModel::RunValidation(const AZ::EntityId& scriptCanvasGraphId)
    {
        layoutAboutToBeChanged();

        m_validationResults.ClearResults();
        
        if (scriptCanvasGraphId.IsValid())
        {
            bool validated = false;
            ScriptCanvas::StatusRequestBus::Event(scriptCanvasGraphId, &ScriptCanvas::StatusRequests::ValidateGraph, m_validationResults);
        }
        
        layoutChanged();

        GraphValidatorDockWidgetNotificationBus::Event(ScriptCanvasEditor::AssetEditorId, &GraphValidatorDockWidgetNotifications::OnResultsChanged, m_validationResults.ErrorCount(), m_validationResults.WarningCount());
    }
    
    QModelIndex GraphValidationModel::index(int row, int column, const QModelIndex& parent) const
    {
        if (row < 0 || row >= m_validationResults.GetEvents().size())
        {
            return QModelIndex();
        }
        
        return createIndex(row, column, const_cast<ScriptCanvas::ValidationEvent*>(FindItemForRow(row)));
    }
    
    QModelIndex GraphValidationModel::parent(const QModelIndex& index) const
    {
        return QModelIndex();
    }
    
    int GraphValidationModel::columnCount(const QModelIndex& parent) const
    {
        return ColumnIndex::Count;
    }
    
    int GraphValidationModel::rowCount(const QModelIndex& parent) const
    {
        return static_cast<int>(m_validationResults.GetEvents().size());
    }

    QVariant GraphValidationModel::headerData(int section, Qt::Orientation orientation, int role) const
    {
        if (orientation == Qt::Orientation::Vertical)
        {
            return QVariant();
        }

        if (role == Qt::DisplayRole)
        {
            switch (section)
            {
                case ColumnIndex::Description:
                {
                    return QString("Description");
                }
                break;
                default:
                    break;
            }
        }

        return QVariant();
    }
    
    QVariant GraphValidationModel::data(const QModelIndex& index, int role) const
    {
        const ScriptCanvas::ValidationEvent* validationEvent = FindItemForIndex(index);
        
        if (role == Qt::DisplayRole)
        {
            switch (index.column())
            {
                case ColumnIndex::Description:
                {
                    return QString(validationEvent->GetDescription().data());
                }
                break;
                default:
                    break;
            }
        }
        else if (role == Qt::DecorationRole)
        {
            switch (index.column())
            {
                // We always want the icon on the leftmost column. So doing away with my usual
                // Labelling to keep the spirit of what I'm after(simple table re-ordering).
                case 0:
                {
                    switch (validationEvent->GetSeverity())
                    {
                    case ScriptCanvas::ValidationSeverity::Error:
                    {
                        return m_errorIcon;
                    }
                    break;
                    case ScriptCanvas::ValidationSeverity::Warning:
                    {
                        return m_warningIcon;
                    }
                    break;
                    case ScriptCanvas::ValidationSeverity::Informative:
                    {
                        return m_messageIcon;
                    }
                    break;
                    default:
                        break;
                    }
                }
                break;
                case ColumnIndex::AutoFix:
                {
                    if (validationEvent->CanAutoFix())
                    {
                        return m_autoFixIcon;
                    }
                }
                break;
                default:
                    break;
            }
        }
        else if (role == Qt::ToolTipRole)
        {
            switch (index.column())
            {
            case ColumnIndex::Description:
            {
                return QString("%1 - %2").arg(validationEvent->GetIdentifier().c_str()).arg(validationEvent->GetTooltip().data());
            }
            case ColumnIndex::AutoFix:
            {
                if (validationEvent->CanAutoFix())
                {
                    return "A potential automatic fix can be applied for this issue. Press this button to fix the error.";
                }
            }
            }
        }
        
        return QVariant();
    }
    
    const ScriptCanvas::ValidationEvent* GraphValidationModel::FindItemForIndex(const QModelIndex& index) const
    {
        if (index.isValid())
        {
            return FindItemForRow(index.row());
        }
        
        return nullptr;
    }
    
    const ScriptCanvas::ValidationEvent* GraphValidationModel::FindItemForRow(int row) const
    {
        const auto& validationEvents = m_validationResults.GetEvents();

        if (row < 0 || row >= validationEvents.size())
        {
            return nullptr;
        }
        
        return validationEvents[row];
    }

    const ScriptCanvas::ValidationResults& GraphValidationModel::GetValidationResults() const
    {
        return m_validationResults;
    }
    
    ////////////////////////////////////////
    // GraphValidationSortFilterProxyModel
    ////////////////////////////////////////
    
    GraphValidationSortFilterProxyModel::GraphValidationSortFilterProxyModel()
        : m_severityFilter(ScriptCanvas::ValidationSeverity::Unknown)
    {
        // TODO: Populate the errors from the user settings
    }
    
    bool GraphValidationSortFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
    {
        QAbstractItemModel* model = sourceModel();
        QModelIndex index = model->index(sourceRow, 0, sourceParent);

        const ScriptCanvas::ValidationEvent* currentItem = static_cast<const ScriptCanvas::ValidationEvent*>(index.internalPointer());

        // If our filter is set to all, we can just show the message
        bool showRow = ((m_severityFilter == ScriptCanvas::ValidationSeverity::Unknown) || (currentItem->GetSeverity() == m_severityFilter));

        if (showRow && !m_filter.isEmpty())
        {
            AZStd::string_view descriptionView = currentItem->GetDescription();

            QString description = QString::fromUtf8(descriptionView.data(), static_cast<int>(descriptionView.size()));

            if (description.lastIndexOf(m_regex) < 0)
            {
                QString errorId = QString(currentItem->GetIdentifier().c_str());

                if (errorId.lastIndexOf(m_regex) < 0)
                {
                    showRow = false;
                }
            }
        }
        
        return showRow;
    }

    void GraphValidationSortFilterProxyModel::SetFilter(const QString& filterString)
    {
        if (m_filter != filterString)
        {
            m_filter = filterString;
            m_regex = QRegExp(m_filter, Qt::CaseInsensitive);

            invalidateFilter();
        }
    }

    void GraphValidationSortFilterProxyModel::SetSeverityFilter(ScriptCanvas::ValidationSeverity severityFilter)
    {
        if (m_severityFilter != severityFilter)
        {
            m_severityFilter = severityFilter;

            invalidateFilter();
        }
    }

    ScriptCanvas::ValidationSeverity GraphValidationSortFilterProxyModel::GetSeverityFilter() const
    {
        return m_severityFilter;
    }

    bool GraphValidationSortFilterProxyModel::IsShowingErrors()
    {
        return m_severityFilter == ScriptCanvas::ValidationSeverity::Unknown || m_severityFilter == ScriptCanvas::ValidationSeverity::Error;
    }

    bool GraphValidationSortFilterProxyModel::IsShowingWarnings()
    {
        return m_severityFilter == ScriptCanvas::ValidationSeverity::Unknown || m_severityFilter == ScriptCanvas::ValidationSeverity::Warning;
    }
    
    //////////////////////////////
    // GraphValidationDockWidget
    //////////////////////////////
    
    GraphValidationDockWidget::GraphValidationDockWidget(QWidget* parent /*= nullptr*/)
        : AzQtComponents::StyledDockWidget(parent)
        , ui(new Ui::GraphValidationPanel())
        , m_model(aznew GraphValidationModel())
        , m_proxyModel(aznew GraphValidationSortFilterProxyModel())
    {
        ui->setupUi(this);

        m_proxyModel->setSourceModel(m_model);

        ui->statusTableView->setModel(m_proxyModel);

        ui->statusTableView->horizontalHeader()->setStretchLastSection(false);

        ui->statusTableView->horizontalHeader()->setSectionResizeMode(GraphValidationModel::Description, QHeaderView::ResizeMode::Stretch);
        
        ui->statusTableView->horizontalHeader()->setSectionResizeMode(GraphValidationModel::AutoFix, QHeaderView::ResizeMode::Fixed);
        ui->statusTableView->horizontalHeader()->resizeSection(GraphValidationModel::AutoFix, 32);

        ui->searchWidget->SetFilterInputInterval(AZStd::chrono::milliseconds(250));

        QButtonGroup* buttonGroup = new QButtonGroup(this);
        buttonGroup->setExclusive(true);

        buttonGroup->addButton(ui->allFilter);
        buttonGroup->addButton(ui->errorOnlyFilter);
        buttonGroup->addButton(ui->warningOnlyFilter);

        ui->allFilter->setChecked(true);

        QObject::connect(ui->allFilter, &QPushButton::clicked, this, &GraphValidationDockWidget::OnSeverityFilterChanged);
        QObject::connect(ui->errorOnlyFilter, &QPushButton::clicked, this, &GraphValidationDockWidget::OnSeverityFilterChanged);
        QObject::connect(ui->warningOnlyFilter, &QPushButton::clicked, this, &GraphValidationDockWidget::OnSeverityFilterChanged);

        QObject::connect(ui->runValidation, &QToolButton::clicked, this, &GraphValidationDockWidget::OnRunValidator);
        QObject::connect(ui->fixSelected, &QPushButton::clicked, this, &GraphValidationDockWidget::FixSelected);

        QObject::connect(ui->statusTableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &GraphValidationDockWidget::OnTableSelectionChanged);

        QObject::connect(ui->statusTableView, &QTableView::doubleClicked, this, &GraphValidationDockWidget::FocusOnEvent);
        QObject::connect(ui->statusTableView, &QTableView::clicked, this, &GraphValidationDockWidget::TryAutoFixEvent);

        QObject::connect(ui->searchWidget, &AzQtComponents::FilteredSearchWidget::TextFilterChanged, this, &GraphValidationDockWidget::OnFilterChanged);
        
        GraphCanvas::AssetEditorNotificationBus::Handler::BusConnect(ScriptCanvasEditor::AssetEditorId);

        ui->runValidation->setEnabled(false);
        ui->fixSelected->setEnabled(false);
        ui->fixSelected->setVisible(false);

        UpdateText();
        UpdateSelectedText();
    }

    GraphValidationDockWidget::~GraphValidationDockWidget()
    {
        GraphCanvas::AssetEditorNotificationBus::Handler::BusDisconnect();

        for (const auto& mapPair : m_validationEffects)
        {
            delete mapPair.second;
        }
    }
    
    void GraphValidationDockWidget::OnActiveGraphChanged(const GraphCanvas::GraphId& graphCanvasGraphId)
    {
        GraphCanvas::SceneNotificationBus::Handler::BusDisconnect();
        GraphCanvas::SceneNotificationBus::Handler::BusConnect(graphCanvasGraphId);

        ui->statusTableView->clearSelection();

        GeneralRequestBus::BroadcastResult(m_scriptCanvasGraphId, &GeneralRequests::GetScriptCanvasGraphId, graphCanvasGraphId);
        m_graphCanvasGraphId = graphCanvasGraphId;

        ui->runValidation->setEnabled(m_scriptCanvasGraphId.IsValid());

        // Lazy way of clearing out the results
        m_model->RunValidation(AZ::EntityId());
        UpdateText();
    }

    void GraphValidationDockWidget::OnSelectionChanged()
    {
        ui->statusTableView->clearSelection();
    }

    void GraphValidationDockWidget::OnConnectionDragBegin()
    {
        ui->statusTableView->clearSelection();
    }

    void GraphValidationDockWidget::OnToastInteraction()
    {
        UIRequestBus::Broadcast(&UIRequests::OpenValidationPanel);
    }

    void GraphValidationDockWidget::OnToastDismissed()
    {
        const GraphCanvas::ToastId* toastId = GraphCanvas::ToastNotificationBus::GetCurrentBusId();

        if (toastId)
        {
            GraphCanvas::ToastNotificationBus::MultiHandler::BusDisconnect((*toastId));
        }
    }
    
    void GraphValidationDockWidget::OnRunValidator(bool displayAsNotification)
    {
        ui->statusTableView->clearSelection();

        m_model->RunValidation(m_scriptCanvasGraphId);
        ui->allFilter->click();

        UpdateText();

        if (!displayAsNotification)
        {
            ui->statusTableView->selectAll();
        }
        else if (m_model->GetValidationResults().HasErrors()
                 || m_model->GetValidationResults().HasWarnings())
        {
            GraphCanvas::ViewId viewId;
            GraphCanvas::SceneRequestBus::EventResult(viewId, m_graphCanvasGraphId, &GraphCanvas::SceneRequests::GetViewId);            

            GraphCanvas::ToastType toastType;
            AZStd::string titleLabel = "Validation Issue";
            AZStd::string description = "";

            if (m_model->GetValidationResults().HasErrors())
            {
                toastType = GraphCanvas::ToastType::Error;
                description = AZStd::string::format("%i validation error(s) were found.", m_model->GetValidationResults().ErrorCount());
            }
            else
            {
                toastType = GraphCanvas::ToastType::Warning;
                description = AZStd::string::format("%i validation warning(s) were found.", m_model->GetValidationResults().WarningCount());
            }

            GraphCanvas::ToastConfiguration toastConfiguration(toastType, titleLabel, description);

            toastConfiguration.SetCloseOnClick(true);
            toastConfiguration.SetDuration(AZStd::chrono::milliseconds(5000));

            GraphCanvas::ToastId validationToastId;

            GraphCanvas::ViewRequestBus::EventResult(validationToastId, viewId, &GraphCanvas::ViewRequests::ShowToastNotification, toastConfiguration);

            GraphCanvas::ToastNotificationBus::MultiHandler::BusConnect(validationToastId);
        }
    }
    
    void GraphValidationDockWidget::OnShowErrors()
    {
        ui->errorOnlyFilter->setChecked(true);
        OnSeverityFilterChanged();
    }
    
    void GraphValidationDockWidget::OnShowWarnings()
    {
        ui->warningOnlyFilter->setChecked(true);
        OnSeverityFilterChanged();
    }
    
    void GraphValidationDockWidget::OnTableSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
    {
        // Handle the deselection cases correctly
        for (auto modelIndex : deselected.indexes())
        {
            OnRowDeselected(m_proxyModel->mapToSource(modelIndex).row());
        }

        ui->fixSelected->setEnabled(false);

        // We want everything to be in sync visually.
        // So fake reselect everything to restart everything
        if (!selected.empty())
        {
            for (auto modelIndex : ui->statusTableView->selectionModel()->selectedIndexes())
            {
                if (modelIndex.column() == 0)
                {
                    QModelIndex sourceIndex = m_proxyModel->mapToSource(modelIndex);
                    const ScriptCanvas::ValidationEvent* validationEvent = m_model->FindItemForIndex(sourceIndex);

                    if (validationEvent && validationEvent->CanAutoFix())
                    {
                        ui->fixSelected->setEnabled(true);
                    }

                    OnRowSelected(sourceIndex.row());
                }
            }

            m_unusedNodeValidationEffect.DisplayEffect(m_graphCanvasGraphId);
        }
    }

    void GraphValidationDockWidget::FocusOnEvent(const QModelIndex& modelIndex)
    {
        const ScriptCanvas::ValidationEvent* validationEvent = m_model->FindItemForIndex(m_proxyModel->mapToSource(modelIndex));
        
        AZ::EntityId graphCanvasMemberId;
        QRectF focusArea;

        if (validationEvent->GetIdCrc() == ScriptCanvas::DataValidationIds::ScopedDataConnectionCrc)
        {
            const ScriptCanvas::ScopedDataConnectionEvent* connectionEvent = static_cast<const ScriptCanvas::ScopedDataConnectionEvent*>(validationEvent);

            const AZ::EntityId& scriptCanvasConnectionId = connectionEvent->GetConnectionId();
            
            SceneMemberMappingRequestBus::EventResult(graphCanvasMemberId, scriptCanvasConnectionId, &SceneMemberMappingRequests::GetGraphCanvasEntityId);
        }
        else if (validationEvent->GetIdCrc() == ScriptCanvas::ExecutionValidationIds::UnusedNodeCrc)
        {
            const ScriptCanvas::UnusedNodeEvent* unusedEvent = static_cast<const ScriptCanvas::UnusedNodeEvent*>(validationEvent);

            const AZ::EntityId& scriptCanvasNodeId = unusedEvent->GetNodeId();

            SceneMemberMappingRequestBus::EventResult(graphCanvasMemberId, scriptCanvasNodeId, &SceneMemberMappingRequests::GetGraphCanvasEntityId);
        }

        if (graphCanvasMemberId.IsValid())
        {
            GraphCanvas::FocusConfig focusConfig;

            if (GraphCanvas::GraphUtils::IsNodeGroup(graphCanvasMemberId))
            {
                focusConfig.m_spacingType = GraphCanvas::FocusConfig::SpacingType::GridStep;
                focusConfig.m_spacingAmount = 1;
            }
            else
            {
                focusConfig.m_spacingType = GraphCanvas::FocusConfig::SpacingType::Scalar;
                focusConfig.m_spacingAmount = 2;
            }

            AZStd::vector<AZ::EntityId> memberIds = { graphCanvasMemberId };

            GraphCanvas::GraphUtils::FocusOnElements(memberIds, focusConfig);
        }
    }

    void GraphValidationDockWidget::TryAutoFixEvent(const QModelIndex& modelIndex)
    {
        if (modelIndex.column() != GraphValidationModel::ColumnIndex::AutoFix)
        {
            return;
        }

        const ScriptCanvas::ValidationEvent* validationEvent = m_model->FindItemForIndex(m_proxyModel->mapToSource(modelIndex));

        if (!validationEvent->CanAutoFix())
        {
            return;
        }

        AutoFixEvent(validationEvent);

        OnRunValidator();
    }

    void GraphValidationDockWidget::FixSelected()
    {
        {
            GraphCanvas::ScopedGraphUndoBlocker undoBlocker(m_graphCanvasGraphId);

            for (auto modelIndex : ui->statusTableView->selectionModel()->selectedIndexes())
            {
                if (modelIndex.column() == 0)
                {
                    QModelIndex sourceIndex = m_proxyModel->mapToSource(modelIndex);
                    const ScriptCanvas::ValidationEvent* validationEvent = m_model->FindItemForIndex(sourceIndex);

                    if (validationEvent->CanAutoFix())
                    {
                        AutoFixEvent(validationEvent);
                    }
                }
            }
        }

        GeneralRequestBus::Broadcast(&GeneralRequests::PostUndoPoint, m_scriptCanvasGraphId);

        OnRunValidator();
    }

    void GraphValidationDockWidget::OnSeverityFilterChanged()
    {
        if (ui->allFilter->isChecked())
        {
            // Using unknown as a proxy for all
            m_proxyModel->SetSeverityFilter(ScriptCanvas::ValidationSeverity::Unknown);
        }
        else if (ui->errorOnlyFilter->isChecked())
        {
            m_proxyModel->SetSeverityFilter(ScriptCanvas::ValidationSeverity::Error);
        }
        else if (ui->warningOnlyFilter->isChecked())
        {
            m_proxyModel->SetSeverityFilter(ScriptCanvas::ValidationSeverity::Warning);
        }

        UpdateText();
    }

    void GraphValidationDockWidget::OnFilterChanged(const QString& filterString)
    {
        m_proxyModel->SetFilter(filterString);
    }

    void GraphValidationDockWidget::AutoFixEvent(const ScriptCanvas::ValidationEvent* validationEvent)
    {
        if (validationEvent->GetIdCrc() == ScriptCanvas::DataValidationIds::ScopedDataConnectionCrc)
        {
            AutoFixScopedDataConnection(static_cast<const ScriptCanvas::ScopedDataConnectionEvent*>(validationEvent));
        }
        else if (validationEvent->GetIdCrc() == ScriptCanvas::DataValidationIds::InvalidVariableTypeCrc)
        {
            AutoFixDeleteInvalidVariables(static_cast<const ScriptCanvas::InvalidVariableTypeEvent*>(validationEvent));
        }
        else if (validationEvent->GetIdCrc() == ScriptCanvas::DataValidationIds::ScriptEventVersionMismatchCrc)
        {
            AutoFixScriptEventVersionMismatch(static_cast<const ScriptCanvas::ScriptEventVersionMismatch*>(validationEvent));
        }
        else
        {
            AZ_Error("ScriptCanvas", false, "Cannot auto fix event type %s despite it being marked at auto fixable", validationEvent->GetIdentifier().c_str());
        }
    }

    void GraphValidationDockWidget::AutoFixScriptEventVersionMismatch(const ScriptCanvas::ScriptEventVersionMismatch* scriptEventMismatchEvent)
    {
        {
            GraphCanvas::ScopedGraphUndoBlocker undoBlocker(m_graphCanvasGraphId);

            AZ::EntityId graphCanvasId;
            SceneMemberMappingRequestBus::EventResult(graphCanvasId, scriptEventMismatchEvent->GetNodeId(), &SceneMemberMappingRequests::GetGraphCanvasEntityId);

            // Detach all connections
            GraphCanvas::GraphUtils::DetachNodeAndStitchConnections(graphCanvasId);

            // TODO #lsempe:
            // Notify the node to update to its latest version
            //EditorGraphRequestBus::Event(m_scriptCanvasGraphId, &EditorGraphRequests::UpdateScriptEventVersion, scriptEventMismatchEvent->GetNodeId());
        }

        GeneralRequestBus::Broadcast(&GeneralRequests::PostUndoPoint, m_scriptCanvasGraphId);

    }


    void GraphValidationDockWidget::AutoFixDeleteInvalidVariables(const ScriptCanvas::InvalidVariableTypeEvent* invalidVariableEvent)
    {
        {
            GraphCanvas::ScopedGraphUndoBlocker undoBlocker(m_graphCanvasGraphId);

            AZStd::vector<NodeIdPair> variableNodes;
            EditorGraphRequestBus::EventResult(variableNodes, m_scriptCanvasGraphId, &EditorGraphRequests::GetVariableNodes, invalidVariableEvent->GetVariableId());
            for (auto& variableNode : variableNodes)
            {
                GraphCanvas::GraphUtils::DetachNodeAndStitchConnections(variableNode.m_graphCanvasId);
            }

            ScriptCanvas::GraphVariableManagerRequestBus::Event(m_scriptCanvasGraphId, &ScriptCanvas::GraphVariableManagerRequests::RemoveVariable, invalidVariableEvent->GetVariableId());
        }

        GeneralRequestBus::Broadcast(&GeneralRequests::PostUndoPoint, m_scriptCanvasGraphId);

    }

    void GraphValidationDockWidget::AutoFixScopedDataConnection(const ScriptCanvas::ScopedDataConnectionEvent* connectionEvent)
    {
        AZStd::unordered_set< AZ::EntityId > createdNodes;

        {
            GraphCanvas::ScopedGraphUndoBlocker undoBlocker(m_graphCanvasGraphId);

            const AZ::EntityId& scriptCanvasConnectionId = connectionEvent->GetConnectionId();

            // Information gathering step
            ScriptCanvas::Endpoint scriptCanvasSourceEndpoint;
            ScriptCanvas::ConnectionRequestBus::EventResult(scriptCanvasSourceEndpoint, scriptCanvasConnectionId, &ScriptCanvas::ConnectionRequests::GetSourceEndpoint);

            // Going to match the visual expectation here, and always have it create a new variable and store the value
            // at this point in time.
            ScriptCanvas::VariableId targetVariableId;

            ScriptCanvas::Data::Type variableType = ScriptCanvas::Data::Type::Invalid();
            ScriptCanvas::NodeRequestBus::EventResult(variableType, scriptCanvasSourceEndpoint.GetNodeId(), &ScriptCanvas::NodeRequests::GetSlotDataType, scriptCanvasSourceEndpoint.GetSlotId());

            if (!variableType.IsValid())
            {
                AZ_Error("ScriptCanvas", false, "Could not auto fix latent connection(%s) because connection did not return a valid data type.", scriptCanvasConnectionId.ToString().c_str());
                return;
            }

            const AZStd::string& varName = VariableDockWidget::FindDefaultVariableName(m_scriptCanvasGraphId);

            ScriptCanvas::Datum datum(variableType, ScriptCanvas::Datum::eOriginality::Original);

            AZ::Outcome<ScriptCanvas::VariableId, AZStd::string> outcome = AZ::Failure(AZStd::string());
            ScriptCanvas::GraphVariableManagerRequestBus::EventResult(outcome, m_scriptCanvasGraphId, &ScriptCanvas::GraphVariableManagerRequests::AddVariable, varName, datum);

            if (outcome.IsSuccess())
            {
                targetVariableId = outcome.GetValue();
            }
            else
            {
                AZ_Error("ScriptCanvas", false, "Could not auto fix latent connection(%s) because variable creation failed with the message: %s", scriptCanvasConnectionId.ToString().c_str(), outcome.GetError().c_str());
                return;
            }

            // Convert elements over to GraphCanvas to begin interactions with the visual front end.
            GraphCanvas::ConnectionId graphCanvasConnectionId;
            SceneMemberMappingRequestBus::EventResult(graphCanvasConnectionId, scriptCanvasConnectionId, &SceneMemberMappingRequests::GetGraphCanvasEntityId);

            GraphCanvas::Endpoint sourceEndpoint;
            GraphCanvas::ConnectionRequestBus::EventResult(sourceEndpoint, graphCanvasConnectionId, &GraphCanvas::ConnectionRequests::GetSourceEndpoint);

            GraphCanvas::Endpoint targetEndpoint;
            GraphCanvas::ConnectionRequestBus::EventResult(targetEndpoint, graphCanvasConnectionId, &GraphCanvas::ConnectionRequests::GetTargetEndpoint);

            AZ::EntityId gridId;
            GraphCanvas::SceneRequestBus::EventResult(gridId, m_graphCanvasGraphId, &GraphCanvas::SceneRequests::GetGrid);

            AZ::Vector2 gridStep(0,0);
            GraphCanvas::GridRequestBus::EventResult(gridStep, gridId, &GraphCanvas::GridRequests::GetMinorPitch);

            AZStd::unordered_set< AZ::EntityId > deletedMemberIds;
            deletedMemberIds.insert(graphCanvasConnectionId);
            
            // Inserting the Set into the execution flow
            {
                // Map of all of the execution outs on the latent node to Endpoints.
                AZStd::unordered_multimap< GraphCanvas::Endpoint, GraphCanvas::ConnectionId > sourceExecutionMapping;

                AZStd::vector< GraphCanvas::SlotId > slotIds;
                GraphCanvas::NodeRequestBus::EventResult(slotIds, sourceEndpoint.GetNodeId(), &GraphCanvas::NodeRequests::FindVisibleSlotIdsByType, GraphCanvas::ConnectionType::CT_Output, GraphCanvas::SlotTypes::ExecutionSlot);

                for (const GraphCanvas::SlotId& slotId : slotIds)
                {
                    AZStd::vector< GraphCanvas::ConnectionId > connectionIds;
                    GraphCanvas::SlotRequestBus::EventResult(connectionIds, slotId, &GraphCanvas::SlotRequests::GetConnections);

                    GraphCanvas::Endpoint executionSource(sourceEndpoint.GetNodeId(), slotId);

                    for (const GraphCanvas::ConnectionId& connectionId : connectionIds)
                    {
                        sourceExecutionMapping.insert(AZStd::make_pair(executionSource, connectionId));
                    }
                }

                if (!sourceExecutionMapping.empty())
                {
                    AZ::Vector2 position;
                    GraphCanvas::Endpoint lastEndpoint;
                    AZ::EntityId setVariableGraphCanvasId;

                    AZStd::vector< GraphCanvas::Endpoint> dataEndpoints;
                    dataEndpoints.push_back(sourceEndpoint);

                    GraphCanvas::CreateConnectionsBetweenConfig connectionConfig;
                    connectionConfig.m_connectionType = GraphCanvas::CreateConnectionsBetweenConfig::CreationType::SingleConnection;

                    for (const auto& sourcePair : sourceExecutionMapping)
                    {
                        const GraphCanvas::Endpoint& executionSourceEndpoint = sourcePair.first;
                        const GraphCanvas::ConnectionId& executionTargetConnectionId = sourcePair.second;

                        if (lastEndpoint != executionSourceEndpoint)
                        {
                            if (!lastEndpoint.IsValid())
                            {
                                QGraphicsItem* sourceItem;
                                GraphCanvas::SceneMemberUIRequestBus::EventResult(sourceItem, sourceEndpoint.GetNodeId(), &GraphCanvas::SceneMemberUIRequests::GetRootGraphicsItem);

                                if (sourceItem)
                                {
                                    QRectF sourceBoundingRect = sourceItem->sceneBoundingRect();

                                    position.SetX(sourceBoundingRect.right() + gridStep.GetX());
                                    position.SetY(sourceBoundingRect.top());
                                }
                            }

                            NodeIdPair createdNodePair = Nodes::CreateSetVariableNode(targetVariableId, m_scriptCanvasGraphId);

                            setVariableGraphCanvasId = createdNodePair.m_graphCanvasId;
                            GraphCanvas::SceneRequestBus::Event(m_graphCanvasGraphId, &GraphCanvas::SceneRequests::AddNode, setVariableGraphCanvasId, position);

                            createdNodes.insert(setVariableGraphCanvasId);

                            position += gridStep;

                            connectionConfig.m_createdConnections.clear();
                            GraphCanvas::GraphUtils::CreateConnectionsBetween(dataEndpoints, setVariableGraphCanvasId, connectionConfig);

                            lastEndpoint = executionSourceEndpoint;
                        }

                        GraphCanvas::ConnectionSpliceConfig spliceConfig;
                        spliceConfig.m_allowOpportunisticConnections = false;

                        GraphCanvas::GraphUtils::SpliceNodeOntoConnection(setVariableGraphCanvasId, executionTargetConnectionId, spliceConfig);
                    }
                }
                else
                {
                    NodeIdPair setVariableNodeIdPair = Nodes::CreateSetVariableNode(targetVariableId, m_scriptCanvasGraphId);

                    createdNodes.insert(setVariableNodeIdPair.m_graphCanvasId);

                    QRectF sourceBoundingRect;
                    QGraphicsItem* graphicsItem = nullptr;
                    GraphCanvas::SceneMemberUIRequestBus::EventResult(graphicsItem, sourceEndpoint.GetNodeId(), &GraphCanvas::SceneMemberUIRequests::GetRootGraphicsItem);

                    if (graphicsItem)
                    {
                        sourceBoundingRect = graphicsItem->sceneBoundingRect();
                    }

                    AZ::Vector2 position = AZ::Vector2(sourceBoundingRect.right() + gridStep.GetX(), sourceBoundingRect.top());
                    GraphCanvas::SceneRequestBus::Event(m_graphCanvasGraphId, &GraphCanvas::SceneRequests::AddNode, setVariableNodeIdPair.m_graphCanvasId, position);

                    AZStd::vector< GraphCanvas::Endpoint> endpoints;
                    endpoints.reserve(slotIds.size() + 1);
                    endpoints.emplace_back(sourceEndpoint);

                    for (const GraphCanvas::SlotId& slotId : slotIds)
                    {
                        endpoints.emplace_back(sourceEndpoint.GetNodeId(), slotId);
                    }

                    GraphCanvas::CreateConnectionsBetweenConfig connectionConfig;
                    connectionConfig.m_connectionType = GraphCanvas::CreateConnectionsBetweenConfig::CreationType::FullyConnected;

                    GraphCanvas::GraphUtils::CreateConnectionsBetween(endpoints, setVariableNodeIdPair.m_graphCanvasId, connectionConfig);
                }
            }

            // Inserting the get into the execution flow
            {
                NodeIdPair getVariableNodeIdPair = Nodes::CreateGetVariableNode(targetVariableId, m_scriptCanvasGraphId);

                createdNodes.insert(getVariableNodeIdPair.m_graphCanvasId);

                QRectF targetBoundingRect;
                QGraphicsItem* graphicsItem = nullptr;
                GraphCanvas::SceneMemberUIRequestBus::EventResult(graphicsItem, targetEndpoint.GetNodeId(), &GraphCanvas::SceneMemberUIRequests::GetRootGraphicsItem);

                if (graphicsItem)
                {
                    targetBoundingRect = graphicsItem->sceneBoundingRect();
                }

                AZ::Vector2 position = AZ::Vector2(targetBoundingRect.left() - gridStep.GetX(), targetBoundingRect.top());

                QGraphicsItem* newGraphicsItem = nullptr;
                GraphCanvas::SceneMemberUIRequestBus::EventResult(newGraphicsItem, getVariableNodeIdPair.m_graphCanvasId, &GraphCanvas::SceneMemberUIRequests::GetRootGraphicsItem);

                if (newGraphicsItem)
                {
                    position.SetX(position.GetX() - newGraphicsItem->sceneBoundingRect().width());
                }

                GraphCanvas::SceneRequestBus::Event(m_graphCanvasGraphId, &GraphCanvas::SceneRequests::AddNode, getVariableNodeIdPair.m_graphCanvasId, position);

                AZStd::vector< GraphCanvas::SlotId > slotIds;
                GraphCanvas::NodeRequestBus::EventResult(slotIds, targetEndpoint.GetNodeId(), &GraphCanvas::NodeRequests::FindVisibleSlotIdsByType, GraphCanvas::ConnectionType::CT_Input, GraphCanvas::SlotTypes::ExecutionSlot);

                AZStd::vector< GraphCanvas::Endpoint > executionSourceEndpoints;
                AZStd::vector< GraphCanvas::Endpoint > validTargetEndpoints;
                validTargetEndpoints.push_back(targetEndpoint);

                for (const GraphCanvas::SlotId& slotId : slotIds)
                {
                    AZStd::vector< GraphCanvas::ConnectionId > connectionIds;
                    GraphCanvas::SlotRequestBus::EventResult(connectionIds, slotId, &GraphCanvas::SlotRequests::GetConnections);

                    validTargetEndpoints.emplace_back(targetEndpoint.GetNodeId(), slotId);

                    for (const GraphCanvas::ConnectionId& connectionId : connectionIds)
                    {
                        GraphCanvas::Endpoint targetExecutionSourceEndpoint;
                        GraphCanvas::ConnectionRequestBus::EventResult(targetExecutionSourceEndpoint, connectionId, &GraphCanvas::ConnectionRequests::GetSourceEndpoint);

                        executionSourceEndpoints.push_back(targetExecutionSourceEndpoint);

                        deletedMemberIds.insert(connectionId);
                    }
                }

                // Hook up all of the connection inputs
                if (!executionSourceEndpoints.empty())
                {
                    GraphCanvas::CreateConnectionsBetweenConfig config;
                    config.m_connectionType = GraphCanvas::CreateConnectionsBetweenConfig::CreationType::FullyConnected;

                    GraphCanvas::GraphUtils::CreateConnectionsBetween(executionSourceEndpoints, getVariableNodeIdPair.m_graphCanvasId, config);
                }

                // Hook up to the actual target endpoints
                GraphCanvas::CreateConnectionsBetweenConfig config;
                config.m_connectionType = GraphCanvas::CreateConnectionsBetweenConfig::CreationType::SinglePass;
                
                GraphCanvas::GraphUtils::CreateConnectionsBetween(validTargetEndpoints, getVariableNodeIdPair.m_graphCanvasId, config);
            }

            GraphCanvas::SceneRequestBus::Event(m_graphCanvasGraphId, &GraphCanvas::SceneRequests::Delete, deletedMemberIds);
        }

        GeneralRequestBus::Broadcast(&GeneralRequests::PostUndoPoint, m_scriptCanvasGraphId);

        GraphCanvas::NodeNudgingController nudgingController;
        nudgingController.SetGraphId(m_graphCanvasGraphId);

        nudgingController.StartNudging(createdNodes);
        nudgingController.FinalizeNudging();
    }

    void GraphValidationDockWidget::UpdateText()
    {
        // Clear our the text Filter
        ui->searchWidget->SetTextFilter("");
        m_proxyModel->SetFilter("");

        ui->errorOnlyFilter->setText(QString("%1 Errors").arg(m_model->GetValidationResults().ErrorCount()));
        ui->warningOnlyFilter->setText(QString("%1 Warnings").arg(m_model->GetValidationResults().WarningCount()));
    }

    void GraphValidationDockWidget::OnRowSelected(int row)
    {
        auto effectIter = m_validationEffects.find(row);
        if (effectIter != m_validationEffects.end())
        {
            // Restart the effect to maintain visual consistency of the glows.
            effectIter->second->CancelEffect();
            effectIter->second->DisplayEffect(m_graphCanvasGraphId);
            return;
        }

        const ScriptCanvas::ValidationEvent* validationEvent = m_model->FindItemForRow(row);

        if (validationEvent->GetIdCrc() == ScriptCanvas::DataValidationIds::ScopedDataConnectionCrc)
        {
            HighlightElementValidationEffect* highlightEffect = aznew HighlightElementValidationEffect();

            const ScriptCanvas::ScopedDataConnectionEvent* connectionEvent = static_cast<const ScriptCanvas::ScopedDataConnectionEvent*>(validationEvent);
            highlightEffect->AddTarget(connectionEvent->GetConnectionId());

            highlightEffect->DisplayEffect(m_graphCanvasGraphId);

            m_validationEffects[row] = highlightEffect;
        }
        else if (validationEvent->GetIdCrc() == ScriptCanvas::DataValidationIds::InvalidVariableTypeCrc)
        {
            HighlightElementValidationEffect* highlightEffect = aznew HighlightElementValidationEffect();

            const ScriptCanvas::InvalidVariableTypeEvent* invalidTypeEvent = static_cast<const ScriptCanvas::InvalidVariableTypeEvent*>(validationEvent);
            
            AZStd::vector<NodeIdPair> variableNodes;
            EditorGraphRequestBus::EventResult(variableNodes, m_scriptCanvasGraphId, &EditorGraphRequests::GetVariableNodes, invalidTypeEvent->GetVariableId());

            for (auto& variable : variableNodes)
            {
                highlightEffect->AddTarget(variable.m_scriptCanvasId);
            }

            highlightEffect->DisplayEffect(m_graphCanvasGraphId);

            m_validationEffects[row] = highlightEffect;
        }
        else if (validationEvent->GetIdCrc() == ScriptCanvas::ExecutionValidationIds::UnusedNodeCrc)
        {
            HighlightElementValidationEffect* highlightEffect = aznew HighlightElementValidationEffect(Qt::yellow);

            const ScriptCanvas::UnusedNodeEvent* unusedEvent = static_cast<const ScriptCanvas::UnusedNodeEvent*>(validationEvent);
            highlightEffect->AddTarget(unusedEvent->GetNodeId());
            highlightEffect->DisplayEffect(m_graphCanvasGraphId);

            m_unusedNodeValidationEffect.AddUnusedNode(unusedEvent->GetNodeId());

            m_validationEffects[row] = highlightEffect;
        }
        else if (validationEvent->GetIdCrc() == ScriptCanvas::DataValidationIds::ScriptEventVersionMismatchCrc)
        {
            HighlightElementValidationEffect* highlightEffect = aznew HighlightElementValidationEffect();

            const ScriptCanvas::ScriptEventVersionMismatch* scriptEventMismatch = static_cast<const ScriptCanvas::ScriptEventVersionMismatch*>(validationEvent);
            highlightEffect->AddTarget(scriptEventMismatch->GetNodeId());
            highlightEffect->DisplayEffect(m_graphCanvasGraphId);

            m_validationEffects[row] = highlightEffect;
        }

        UpdateSelectedText();
    }

    void GraphValidationDockWidget::OnRowDeselected(int row)
    {
        auto validationIter = m_validationEffects.find(row);

        if (validationIter == m_validationEffects.end())
        {
            return;
        }

        const ScriptCanvas::ValidationEvent* validationEvent = m_model->FindItemForRow(row);

        if (validationEvent->GetIdCrc() == ScriptCanvas::ExecutionValidationIds::UnusedNodeCrc)
        {
            const ScriptCanvas::UnusedNodeEvent* unusedEvent = static_cast<const ScriptCanvas::UnusedNodeEvent*>(validationEvent);
            m_unusedNodeValidationEffect.RemoveUnusedNode(unusedEvent->GetNodeId());
        }

        validationIter->second->CancelEffect();
        delete validationIter->second;

        m_validationEffects.erase(validationIter);

        UpdateSelectedText();
    }

    void GraphValidationDockWidget::UpdateSelectedText()
    {
        int selectedRowsSize = 0;

        for (const QModelIndex& selectedRow : ui->statusTableView->selectionModel()->selectedRows())
        {
            QModelIndex sourceIndex = m_proxyModel->mapToSource(selectedRow);
            
            const ScriptCanvas::ValidationEvent* validationEvent = m_model->FindItemForRow(sourceIndex.row());

            if (validationEvent->CanAutoFix())
            {
                selectedRowsSize++;
            }
        }

        if (selectedRowsSize == 0)
        {
            ui->fixSelectedText->setVisible(false);
        }
        else
        {
            ui->fixSelectedText->setVisible(true);
            ui->fixSelectedText->setText(QString("%1 Selected").arg(selectedRowsSize));
        }
    }

#include <Editor/View/Widgets/ValidationPanel/GraphValidationDockWidget.moc>
}
