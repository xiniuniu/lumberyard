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
#ifndef TOOLBAR_MANAGER_H
#define TOOLBAR_MANAGER_H

#include <QString>
#include <QVector>
#include <QSettings>
#include <QToolBar>
#include <QSize>

#include <functional>

class ActionManager;
class QChildEvent;
class QDragEnterEvent;
class QDragMoveEvent;
class QDragLeaveEvent;
class QDropEvent;
class ToolbarManager;
class DnDIndicator;

enum StandardToolbar
{
    UndefinedToolbar = -1,
    EditModeToolbar = 0,
    ObjectToolbar,
    EditorsToolbar,
    SubstanceToolbar,
    MiscToolbar
};

enum Role
{
    ActionRole = Qt::UserRole
};

// A QToolBar which DnD support
class EditableQToolBar
    : public QToolBar
{
    Q_OBJECT
public:
    explicit EditableQToolBar(const QString& title, ToolbarManager* manager);
    QAction* ActionForWidget(QWidget* w) const;
protected:
    void childEvent(QChildEvent* ev) override;
    void dragMoveEvent(QDragMoveEvent* ev) override;
    void dragEnterEvent(QDragEnterEvent* ev) override;
    void dragLeaveEvent(QDragLeaveEvent* ev) override;
    void dropEvent(QDropEvent* ev) override;
private:
    QWidget* insertPositionForDrop(QPoint mousePos);
    QList<QWidget*> childWidgetsWithActions() const;
    QAction* actionFromDrop(QDropEvent* ev) const;
    bool eventFilter(QObject* obj, QEvent* ev) override;
    ToolbarManager* const m_toolbarManager;
    ActionManager* const m_actionManager;
    DnDIndicator* const m_dndIndicator;
    friend class DnDIndicator;
};

class AmazonToolbar
{
public:
    typedef QVector<AmazonToolbar> List;

    AmazonToolbar(const QString& name = QString(), const QString& translatedName = QString());
    AmazonToolbar(const AmazonToolbar&) = default;
    AmazonToolbar& operator=(const AmazonToolbar&) = default;

    void InstantiateToolbar(QMainWindow* mainWindow, ToolbarManager* manager);
    bool IsInstantiated() const { return m_toolbar != nullptr; }

    void SetName(const QString& name, const QString& translatedName);

    const QString& GetName() const { return m_name; }
    const QString& GetTranslatedName() const { return m_translatedName; }

    void AddAction(int actionId, int toolbarVersionAdded = 0);

    QToolBar* Toolbar() const { return m_toolbar; }

    void Clear();
    QVector<int> ActionIds() const;

    void SetShowByDefault(bool value) { m_showByDefault = value; }
    void SetShowToggled(bool value) { m_showToggled = value; }
    bool IsShowByDefault() const { return m_showByDefault; }
    bool IsShowToggled() const { return m_showToggled; }

    void CopyActions(const AmazonToolbar& other) { m_actions = other.m_actions; }
    void SetActionsOnInternalToolbar(ActionManager* actionManager);

    void UpdateAllowedAreas();
    static void UpdateAllowedAreas(QToolBar* toolbar);

    const bool IsSame(const AmazonToolbar& other) const;

private:
    QString m_name; // Not translated, for settings keys and such
    QString m_translatedName;
    QToolBar* m_toolbar = nullptr;

    struct ActionData
    {
        int actionId;
        int toolbarVersionAdded;

        bool operator ==(const AmazonToolbar::ActionData& other) const
        {
            return actionId == other.actionId;
        }
    };

    QVector<ActionData> m_actions;
    bool m_showByDefault = true;
    bool m_showToggled = false;
};

class ToolbarManager
{
public:
    explicit ToolbarManager(ActionManager* actionManager, MainWindow* mainWindow);
    ~ToolbarManager();

    AmazonToolbar GetToolbar(int index);
    AmazonToolbar::List GetToolbars() const;
    void RestoreToolbarDefaults(const QString& toolbarName);

    bool Delete(int index);
    bool Rename(int index, const QString& newName);
    int Add(const QString& name);
    void LoadToolbars();
    bool IsCustomToolbar(int index) const;
    bool IsCustomToolbar(const QString& name) const;
    EditableQToolBar* ToolbarParent(QObject* o) const;
    ActionManager* GetActionManager() const;

    void SetIsEditingToolBars(bool);
    bool IsEditingToolBars() const;

    bool DeleteAction(QAction* action, EditableQToolBar*);
    void InsertAction(QAction* action, QWidget* beforeWidget, QAction* beforeAction, EditableQToolBar* toolbar);

    AmazonToolbar GetEditModeToolbar() const;
    AmazonToolbar GetObjectToolbar() const;
    AmazonToolbar GetEditorsToolbar() const;
    AmazonToolbar GetSubstanceToolbar() const;
    AmazonToolbar GetMiscToolbar() const;

private:
    Q_DISABLE_COPY(ToolbarManager);
    bool IsGemEnabled(const QString& uuid, const QString& version) const;
    void SaveToolbars();
    void SaveToolbar(EditableQToolBar* toolbar);
    void InstantiateToolbars();
    void InstantiateToolbar(int index);
    void SanitizeToolbars(const QMap<QString, AmazonToolbar>& previousStandardToolbars);
    void InitializeStandardToolbars();
    void UpdateAllowedAreas(QToolBar* toolbar);
    bool IsDirty(const AmazonToolbar& toolbar) const;

    const AmazonToolbar* FindDefaultToolbar(const QString& toolbarName) const;
    AmazonToolbar* FindToolbar(const QString& toolbarName);

    MainWindow* const m_mainWindow;
    ActionManager* const m_actionManager;
    QSettings m_settings;
    AmazonToolbar::List m_toolbars;
    int m_loadedVersion = 0;
    bool m_isEditingToolBars = false;

    AmazonToolbar::List m_standardToolbars;
};

#endif
