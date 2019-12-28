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

#include "stdafx.h"

#include "Woodpecker/Driller/FilteredListView.hxx"
#include <Woodpecker/Driller/FilteredListView.moc>
#include <Woodpecker/Driller/ui_FilteredListView.h>

#include <AzCore/std/containers/set.h>

#include <qtextdocument.h>

namespace Driller
{
    /////////////////////
    // FilteredListView
    /////////////////////

    FilteredListView::FilteredListView(QWidget* parent)
        : QWidget(parent)
        , m_gui(new Ui::FilteredListView())
        , m_enableCustomOrdering(true)
    {
        m_gui->setupUi(this);

        m_stringListModel.setStringList(m_stringList);
        m_filteredModel.setSourceModel(&m_stringListModel);

        m_gui->listView->setModel(&m_filteredModel);
        m_gui->listView->setEditTriggers(QAbstractItemView::NoEditTriggers);

        QObject::connect(m_gui->filter, SIGNAL(textChanged(const QString&)), this, SLOT(filterEdited(const QString&)));
        QObject::connect(m_gui->moveUp, SIGNAL(clicked()), this, SLOT(moveSelectionUp()));
        QObject::connect(m_gui->moveDown, SIGNAL(clicked()), this, SLOT(moveSelectionDown()));

        m_gui->moveUp->setAutoDefault(false);
        m_gui->moveDown->setAutoDefault(false);
    }

    FilteredListView::~FilteredListView()
    {
        delete m_gui;
    }

    void FilteredListView::addItem(const char* item)
    {
        m_stringList.push_back(QString(item));
        m_stringListModel.setStringList(m_stringList);
    }

    void FilteredListView::addItems(const QStringList& items)
    {
        m_stringList.append(items);
        m_stringListModel.setStringList(m_stringList);
    }

    void FilteredListView::removeItem(const char* item)
    {
        m_stringList.removeOne(QString(item));
        m_stringListModel.setStringList(m_stringList);
    }

    void FilteredListView::removeItems(const QStringList& items)
    {
        for (const QString& item : items)
        {
            m_stringList.removeOne(item);
        }

        m_stringListModel.setStringList(m_stringList);
    }

    void FilteredListView::clearItems()
    {
        m_stringList.clear();
        m_stringListModel.setStringList(m_stringList);
    }

    void FilteredListView::removeSelected()
    {
        const QModelIndexList& selectedIndexes = m_gui->listView->selectionModel()->selectedIndexes();

        for (const QModelIndex& modelIndex : selectedIndexes)
        {
            QString item = modelIndex.data(Qt::DisplayRole).toString();

            m_stringList.removeOne(item);
        }

        m_gui->listView->selectionModel()->clearSelection();
        m_stringListModel.setStringList(m_stringList);
    }

    const QStringList& FilteredListView::getAllItems() const
    {
        return m_stringList;
    }

    void FilteredListView::getSelectedItems(QStringList& selectedItems) const
    {
        const QModelIndexList& selectedIndexes = m_gui->listView->selectionModel()->selectedIndexes();

        for (const QModelIndex& modelIndex : selectedIndexes)
        {
            QString item = modelIndex.data(Qt::DisplayRole).toString();
            selectedItems.push_back(item);
        }
    }

    void FilteredListView::enableCustomOrdering(bool enabled)
    {
        m_enableCustomOrdering = enabled;

        setButtonsEnabled(m_enableCustomOrdering);
    }

    void FilteredListView::filterEdited(const QString& eventFilter)
    {
        m_gui->listView->selectionModel()->clearSelection();
        m_filteredModel.setFilterRegExp(eventFilter);
        setButtonsEnabled(eventFilter.isEmpty() && m_enableCustomOrdering);
    }

    void FilteredListView::moveSelectionUp()
    {
        const QModelIndexList& selectedIndexes = m_gui->listView->selectionModel()->selectedIndexes();

        AZStd::set<int> sortedIndexes;

        for (const QModelIndex& index : selectedIndexes)
        {
            sortedIndexes.insert(index.row());
        }

        int lastSelectedRow = -1;
        AZStd::unordered_set<int> selectedRows;

        for (int row : sortedIndexes)
        {
            if (row > 0 && row > lastSelectedRow + 1)
            {
                m_stringList.swap(row, row - 1);

                // Update the row to reflect it's new position.
                row = row - 1;
            }

            lastSelectedRow = row;
            selectedRows.insert(row);
        }

        m_gui->listView->selectionModel()->clear();

        m_stringListModel.setStringList(m_stringList);

        for (int row : selectedRows)
        {
            m_gui->listView->selectionModel()->select(m_stringListModel.index(row, 0), QItemSelectionModel::Select);
        }
    }

    void FilteredListView::moveSelectionDown()
    {
        const QModelIndexList& selectedIndexes = m_gui->listView->selectionModel()->selectedIndexes();

        AZStd::set<int> sortedIndexes;

        for (const QModelIndex& index : selectedIndexes)
        {
            sortedIndexes.insert(index.row());
        }

        int lastSelectedRow = m_stringList.size();
        AZStd::unordered_set<int> selectedRows;

        for (AZStd::set<int>::reverse_iterator iter = sortedIndexes.rbegin();
             iter != sortedIndexes.rend();
             ++iter)
        {
            int row = (*iter);

            if (row < m_stringList.size() - 1 && row < lastSelectedRow - 1)
            {
                m_stringList.swap(row, row + 1);
                row = row + 1;
            }

            lastSelectedRow = row;
            selectedRows.insert(row);
        }

        m_gui->listView->selectionModel()->clear();

        m_stringListModel.setStringList(m_stringList);

        for (int row : selectedRows)
        {
            m_gui->listView->selectionModel()->select(m_stringListModel.index(row, 0), QItemSelectionModel::Select);
        }
    }

    void FilteredListView::setButtonsEnabled(bool enabled)
    {
        m_gui->moveDown->setEnabled(enabled);
        m_gui->moveUp->setEnabled(enabled);
    }
}
