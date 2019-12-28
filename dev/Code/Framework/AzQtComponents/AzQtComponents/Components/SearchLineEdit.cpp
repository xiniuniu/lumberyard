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
#include "SearchLineEdit.h"

#include <AzQtComponents/Components/SearchLineEdit.h>

#include <QIcon>
#include <QAction>
#include <QMenu>
#include <QCompleter>

using namespace AzQtComponents;

SearchLineEdit::SearchLineEdit(QWidget* parent)
    : QLineEdit(parent),
      m_errorState(false)
{
    setProperty("class", "SearchLineEdit");
    setStyleSheet("color: rgb(192, 192, 192);");

    m_searchAction = new QAction(QIcon(":/stylesheet/img/16x16/Search.png"), QString(), this);
    m_searchAction->setEnabled(false);
    addAction(m_searchAction, QLineEdit::LeadingPosition);
}

bool SearchLineEdit::errorState() const
{
    return m_errorState;
}

void SearchLineEdit::setMenu(QMenu* menu)
{
    if (!menu)
    {
        return;
    }

    m_menu = menu;

    removeAction(m_searchAction);
    m_searchAction = new QAction(QIcon(":/stylesheet/img/16x16/Search_more.png"), QString(), this);
    m_searchAction->setEnabled(true);
    addAction(m_searchAction, QLineEdit::LeadingPosition);
    connect(m_searchAction, &QAction::triggered, this, &SearchLineEdit::displayMenu);
}

void SearchLineEdit::setIconToolTip(const QString& tooltip)
{
    m_searchAction->setToolTip(tooltip);
}

QString SearchLineEdit::userInputText() const
{
    QString lineEditText = text();

    // The QCompleter doesn't seem to update the completion prefix when you delete anything, only when things are added.
    // To get it to update correctly when the user deletes something, I'm using the combination of things:
    //
    // 1) If we have a completion, that text will be auto filled into the quick filter because of the completion model.
    // So, we will compare those two values, and if they match, we know we want to search using the completion prefix.
    //
    // 2) If they don't match, it means that user deleted something, and the Completer didn't update it's internal state, so we'll just
    // use whatever is in the text box.
    //
    // 3) When the text field is set to empty, the current completion gets invalidated, but the prefix doesn't, so that gets special cased out.
    //
    // Extra fun: If you type in something, "Like" then delete a middle character, "Lie", and then put the k back in. It will auto complete the E
    // visually but the completion prefix will be the entire word.
    if (completer()
        && completer()->currentCompletion().compare(lineEditText, Qt::CaseInsensitive) == 0
        && !lineEditText.isEmpty())
    {
        lineEditText = completer()->completionPrefix();
    }

    return lineEditText;
}

void SearchLineEdit::setErrorState(bool errorState)
{
    if (m_errorState == errorState)
    {
        return;
    }

    m_errorState = errorState;
    adaptColorText();

    setProperty("errorState", errorState);
    emit errorStateChanged(m_errorState);
}

void SearchLineEdit::displayMenu()
{
    if (m_menu)
    {
        const auto rect = QRect(0,0, width(), height());
        const auto actionSelected = m_menu->exec(mapToGlobal(rect.bottomLeft()));
        emit menuEntryClicked(actionSelected);
    }
}

void SearchLineEdit::adaptColorText()
{
    if (m_errorState)
    {
        setStyleSheet("color: rgb(224, 83, 72);");
    }
    else
    {
        setStyleSheet("color: rgb(192, 192, 192);");
    }
}

#include <Components/SearchLineEdit.moc>
