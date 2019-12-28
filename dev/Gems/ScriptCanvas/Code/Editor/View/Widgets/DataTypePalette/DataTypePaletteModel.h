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

#include <qabstractitemmodel.h>
#include <QSortFilterProxyModel.h>
#include <QTableView.h>

#include <AzCore/Memory/SystemAllocator.h>

#include <ScriptCanvas/Data/Data.h>

namespace ScriptCanvasEditor
{
    class DataTypePaletteModel
        : public QAbstractTableModel
    {
        Q_OBJECT
    public:

        enum ColumnIndex
        {
            Pinned,
            Type,
            Count
        };

        AZ_CLASS_ALLOCATOR(DataTypePaletteModel, AZ::SystemAllocator, 0);

        DataTypePaletteModel(QObject* parent = nullptr);

        // QAbstractTableModel
        int columnCount(const QModelIndex &parent = QModelIndex()) const override;
        int rowCount(const QModelIndex &parent = QModelIndex()) const override;

        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        Qt::ItemFlags flags(const QModelIndex &index) const override;
        ////

        QItemSelectionRange GetSelectionRangeForRow(int row);

        void ClearTypes();        
        void PopulateVariablePalette(const AZStd::unordered_set< AZ::Uuid >& objectTypes);

        void AddDataType(const AZ::TypeId& dataType);
        void RemoveDataType(const AZ::TypeId& dataType);

        AZ::TypeId FindTypeIdForIndex(const QModelIndex& index) const;
        AZ::TypeId FindTypeIdForTypeName(const AZStd::string& typeName) const;

        QModelIndex FindIndexForTypeId(const AZ::TypeId& typeId) const;
        AZStd::string FindTypeNameForTypeId(const AZ::TypeId& typeId) const;

        void TogglePendingPinChange(const AZ::Uuid& azVarType);
        const AZStd::unordered_set< AZ::Uuid >& GetPendingPinChanges() const;
        void SubmitPendingPinChanges();

    private:

        void AddDataTypeImpl(const AZ::TypeId& dataType);

        QIcon   m_pinIcon;

        AZStd::unordered_set< AZ::Uuid >     m_pinningChanges;

        AZStd::vector<AZ::TypeId> m_variableTypes;
        AZStd::unordered_map<AZStd::string, AZ::TypeId> m_typeNameMapping;        
    };
    
    class DataTypePaletteSortFilterProxyModel
        : public QSortFilterProxyModel
    {
        Q_OBJECT
    public:
        AZ_CLASS_ALLOCATOR(DataTypePaletteSortFilterProxyModel, AZ::SystemAllocator, 0);

        DataTypePaletteSortFilterProxyModel(QObject* parent = nullptr);
        
        // QSortFilterProxyModel
        bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
        bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
        ////
        
        void SetFilter(const QString& filter);

    private:
        QString m_filter;
        QRegExp m_testRegex;
    };
}