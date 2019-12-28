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

#pragma once

#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

#include <ACETypes.h>
#include <AudioControlFilters.h>
#include <QTreeWidgetFilter.h>

#include <QWidget>
#include <QDialog>
#include <QLineEdit>

class QAudioControlsTreeView;
class QDialogButtonBox;
class QAudioControlSortProxy;
class QStandardItemModel;

namespace AudioControls
{
    class CATLControlsModel;
    class CATLControl;

    //-------------------------------------------------------------------------------------------//
    class ATLControlsDialog
        : public QDialog
    {
        Q_OBJECT
    public:
        ATLControlsDialog(QWidget* pParent, EACEControlType eType);
        ~ATLControlsDialog();

    private slots:
        void UpdateSelectedControl();
        void SetTextFilter(QString filter);
        void EnterPressed();
        void StopTrigger();

    public:
        void SetScope(const AZStd::string& sScope);
        const char* ChooseItem(const char* currentValue);
        QSize sizeHint() const override;
        void showEvent(QShowEvent* e) override;

    private:

        QModelIndex FindItem(const AZStd::string_view sControlName);
        void ApplyFilter();
        bool ApplyFilter(const QModelIndex parent);
        bool IsValid(const QModelIndex index);
        bool IsCriteriaMatch(const CATLControl* control) const;
        const CATLControl* GetControlFromModelIndex(const QModelIndex index) const;
        QString GetWindowTitle(EACEControlType type) const;

        // ------------------ QWidget ----------------------------
        bool eventFilter(QObject* pObject, QEvent* pEvent);
        // -------------------------------------------------------

        // Filtering
        QString m_sFilter;
        EACEControlType m_eType;
        AZStd::string m_sScope;

        AZStd::string m_sControlName;
        QAudioControlsTreeView* m_pControlTree;
        QDialogButtonBox* pDialogButtons;
        QLineEdit* m_TextFilterLineEdit;

        QStandardItemModel* m_pTreeModel;
        QAudioControlSortProxy* m_pProxyModel;
        CATLControlsModel* m_pATLModel;
    };
} // namespace AudioControls
