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

#include <AzCore/Memory/SystemAllocator.h>
#include <Editor/InputDialogValidatable.h>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace EMStudio
{
    AZ_CLASS_ALLOCATOR_IMPL(InputDialogValidatable, AZ::SystemAllocator, 0)

    InputDialogValidatable::InputDialogValidatable(QWidget* parent, const QString& labelText, const QRegExp regExp)
        : QDialog(parent)
    {
        QVBoxLayout* layout = new QVBoxLayout();
        layout->addWidget(new QLabel(labelText));

        m_lineEdit = aznew LineEditValidatable(this, regExp);
        layout->addWidget(m_lineEdit);

        m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(m_buttonBox);
        connect(m_buttonBox, &QDialogButtonBox::rejected, this, &InputDialogValidatable::reject);
        connect(m_buttonBox, &QDialogButtonBox::accepted, this, &InputDialogValidatable::OnAccepted);

        connect(m_lineEdit, &LineEditValidatable::TextChanged, this, [this]() {
            // Special case when using the validatable line edit for non-existing objects without a previous value.
            // This is needed because in case of an invalid input the text will be reset to the previous input.
            m_lineEdit->SetPreviousText(m_lineEdit->text());

            QPushButton* okButton = m_buttonBox->button(QDialogButtonBox::Ok);
            okButton->setEnabled(m_lineEdit->IsValid());
        });

        setLayout(layout);
    }

    void InputDialogValidatable::OnAccepted()
    {
        if (m_lineEdit->IsValid())
        {
            emit accept();
        }
    }

    void InputDialogValidatable::SetText(const QString& newText)
    {
        m_lineEdit->setText(newText);
    }

    QString InputDialogValidatable::GetText() const
    {
        return m_lineEdit->text();
    }

    void InputDialogValidatable::SetValidatorFunc(const AZStd::function<bool()>& func)
    {
        m_lineEdit->SetValidatorFunc(func);

        QPushButton* okButton = m_buttonBox->button(QDialogButtonBox::Ok);
        okButton->setEnabled(m_lineEdit->IsValid());
    }
} // namespace EMStudio
