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

#include "StdAfx.h"
#include "StringDlg.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>


/////////////////////////////////////////////////////////////////////////////
// StringDlg dialog

StringDlg::StringDlg(const QString &title, QWidget* pParent, bool bFileNameLimitation)
    : QInputDialog(pParent)
    , m_bFileNameLimitation(bFileNameLimitation)
{
    setWindowTitle(title);
    setLabelText("");
}

void StringDlg::accept()
{
    if (m_bFileNameLimitation)
    {
        const QString text = textValue();
        const QString reservedCharacters("<>:\"/\\|?*}");
        foreach(auto reserv, reservedCharacters)
        {
            if (text.contains(reserv))
            {
                QMessageBox::warning(this, tr("Warning"), tr(" This string can't contain the following characters: %1").arg(reserv), QMessageBox::Ok);
                return;
            }
        }
    }
    if (m_Check && !m_Check(textValue()))
        return;

    QInputDialog::accept();
}


//////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CMultiLineStringDlg dialog
StringGroupDlg::StringGroupDlg(const QString &title, QWidget *parent)
    : QDialog(parent)
{
    if (!title.isEmpty())
        setWindowTitle(title);

    m_group = new QLineEdit(this);
    m_string = new QLineEdit(this);

    QFrame *horLine = new QFrame(this);
    horLine->setFrameStyle(QFrame::HLine | QFrame::Plain);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Group"), this));
    layout->addWidget(m_group);
    layout->addWidget(new QLabel(tr("Name"), this));
    layout->addWidget(m_string);
    layout->addWidget(horLine);
    layout->addWidget(buttonBox);
}

void StringGroupDlg::SetString(const QString &str)
{
    m_string->setText(str);
}

QString StringGroupDlg::GetString() const
{
    return m_string->text();
}

void StringGroupDlg::SetGroup(const QString &str)
{
    m_group->setText(str);
}

QString StringGroupDlg::GetGroup() const
{
    return m_group->text();
}
