/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates, or 
* a third party where indicated.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,  
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  
*
*/
#include "StdAfx.h"

#include "PropertyMiscCtrl.h"
#include "QtViewPaneManager.h"
#include "LensFlareEditor/LensFlareEditor.h"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QToolButton>
#include <QtCore/QTimer>
#include <QtUtilWin.h>
#include "GenericSelectItemDialog.h"

UserPropertyEditor::UserPropertyEditor(QWidget *pParent /*= nullptr*/)
    : QWidget(pParent)
    , m_canEdit(false)
    , m_useTree(false)
{
    m_valueLabel = new QLabel;

    QToolButton *mainButton = new QToolButton;
    mainButton->setText("..");
    connect(mainButton, &QToolButton::clicked, this, &UserPropertyEditor::onEditClicked);

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->addWidget(m_valueLabel, 1);
    mainLayout->addWidget(mainButton);
    mainLayout->setContentsMargins(1, 1, 1, 1);
}

void UserPropertyEditor::SetValue(const QString &value, bool notify /*= true*/)
{
    if (m_value != value)
    {
        m_value = value;
        m_valueLabel->setText(m_value);
        if (notify)
        {
            emit ValueChanged(m_value);
        }
    }

}

void UserPropertyEditor::SetData(bool canEdit, bool useTree, const QString &treeSeparator, const QString &dialogTitle, const std::vector<IVariable::IGetCustomItems::SItem>& items)
{
    m_canEdit = canEdit;
    m_useTree = useTree;
    m_treeSeparator = treeSeparator;
    m_dialogTitle = dialogTitle;
    m_items = items;
}

void UserPropertyEditor::onEditClicked()
{
    // call the user supplied callback to fill-in items and get dialog title
    emit RefreshItems();
    if (m_canEdit) // if func didn't veto, show the dialog
    {
        CGenericSelectItemDialog gtDlg;
        if (m_useTree)
        {
            gtDlg.SetMode(CGenericSelectItemDialog::eMODE_TREE);
            if (!m_treeSeparator.isEmpty())
            {
                gtDlg.SetTreeSeparator(m_treeSeparator);
            }
        }
        gtDlg.SetItems(m_items);
        if (m_dialogTitle.isEmpty() == false)
            gtDlg.setWindowTitle(m_dialogTitle);
        gtDlg.PreSelectItem(GetValue());
        if (gtDlg.exec() == QDialog::Accepted)
        {
            QString selectedItemStr = gtDlg.GetSelectedItem();

            if (selectedItemStr.isEmpty() == false)
            {
                SetValue(selectedItemStr);
            }
        }
    }
}


QWidget* UserPopupWidgetHandler::CreateGUI(QWidget *pParent)
{
    UserPropertyEditor* newCtrl = aznew UserPropertyEditor(pParent);
    connect(newCtrl, &UserPropertyEditor::ValueChanged, newCtrl, [newCtrl]()
    {
        EBUS_EVENT(AzToolsFramework::PropertyEditorGUIMessages::Bus, RequestWrite, newCtrl);
    });

    return newCtrl;
}

void UserPopupWidgetHandler::ConsumeAttribute(UserPropertyEditor* GUI, AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName)
{
    Q_UNUSED(GUI);
    Q_UNUSED(attrib);
    Q_UNUSED(attrValue);
    Q_UNUSED(debugName);
}

void UserPopupWidgetHandler::WriteGUIValuesIntoProperty(size_t index, UserPropertyEditor* GUI, property_t& instance, AzToolsFramework::InstanceDataNode* node)
{
    Q_UNUSED(index);
    Q_UNUSED(node);
    CReflectedVarUser val = instance;
    val.m_value = GUI->GetValue().toUtf8().data();
    instance = static_cast<property_t>(val);
}

bool UserPopupWidgetHandler::ReadValuesIntoGUI(size_t index, UserPropertyEditor* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* node)
{
    Q_UNUSED(index);
    Q_UNUSED(node);
    CReflectedVarUser val = instance;

    assert(val.m_itemNames.size() == val.m_itemDescriptions.size());

    std::vector<IVariable::IGetCustomItems::SItem> items(val.m_itemNames.size());
    int i = -1;
    std::generate(items.begin(), items.end(), [&val, &i]() { ++i; return IVariable::IGetCustomItems::SItem(val.m_itemNames[i].c_str(), val.m_itemDescriptions[i].c_str());});

    GUI->SetData(val.m_enableEdit, val.m_useTree, val.m_treeSeparator.c_str(), val.m_dialogTitle.c_str(), items);
    GUI->SetValue(val.m_value.c_str(), false);
    return false;
}

#include <Controls/ReflectedPropertyControl/PropertyMiscCtrl.moc>

LensFlarePropertyWidget::LensFlarePropertyWidget(QWidget *pParent /*= nullptr*/)
    :QWidget(pParent)
{
    m_valueEdit = new QLineEdit;

    QToolButton *mainButton = new QToolButton;
    mainButton->setText("D");
    connect(mainButton, &QToolButton::clicked, this, &LensFlarePropertyWidget::OnEditClicked);
    connect(m_valueEdit, &QLineEdit::editingFinished, m_valueEdit, [this] () {emit ValueChanged(m_valueEdit->text());});

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->addWidget(m_valueEdit, 1);
    mainLayout->addWidget(mainButton);
    mainLayout->setContentsMargins(1, 1, 1, 1);
}

void LensFlarePropertyWidget::SetValue(const QString &value)
{
    m_valueEdit->setText(value);
}

QString LensFlarePropertyWidget::GetValue() const
{ 
    return m_valueEdit->text();
}

void LensFlarePropertyWidget::OnEditClicked()
{
    const QtViewPane *lensFlarePane = GetIEditor()->OpenView(CLensFlareEditor::s_pLensFlareEditorClassName);
    if (!lensFlarePane)
        return;

    CLensFlareEditor *editor = FindViewPane<CLensFlareEditor>(QtUtil::ToQString(CLensFlareEditor::s_pLensFlareEditorClassName));
    if (editor)
        QTimer::singleShot(0, editor, SLOT(OnUpdateTreeCtrl()));
}

QWidget* LensFlareHandler::CreateGUI(QWidget *pParent)
{
    LensFlarePropertyWidget* newCtrl = aznew LensFlarePropertyWidget(pParent);
    connect(newCtrl, &LensFlarePropertyWidget::ValueChanged, newCtrl, [newCtrl]()
    {
        EBUS_EVENT(AzToolsFramework::PropertyEditorGUIMessages::Bus, RequestWrite, newCtrl);
    });

    return newCtrl;
}

void LensFlareHandler::ConsumeAttribute(LensFlarePropertyWidget* GUI, AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName)
{
        Q_UNUSED(GUI); 	Q_UNUSED(attrib); Q_UNUSED(attrValue); Q_UNUSED(debugName);
}

void LensFlareHandler::WriteGUIValuesIntoProperty(size_t index, LensFlarePropertyWidget* GUI, property_t& instance, AzToolsFramework::InstanceDataNode* node)
{
        Q_UNUSED(index);
        Q_UNUSED(node);
        CReflectedVarGenericProperty val = instance;
        val.m_value = GUI->GetValue().toUtf8().data();
        instance = static_cast<property_t>(val);
}

bool LensFlareHandler::ReadValuesIntoGUI(size_t index, LensFlarePropertyWidget* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* node)
{
    Q_UNUSED(index);
    Q_UNUSED(node);
    CReflectedVarGenericProperty val = instance;
    GUI->SetValue(val.m_value.c_str());
    return false;
}



QWidget* FloatCurveHandler::CreateGUI(QWidget *pParent)
{
    CSplineCtrl *cSpline = new CSplineCtrl(pParent);
    cSpline->SetUpdateCallback(functor(*this, &FloatCurveHandler::OnSplineChange));
    cSpline->SetTimeRange(0, 1);
    cSpline->SetValueRange(0, 1);
    cSpline->SetGrid(12, 12);
    cSpline->setFixedHeight(52);
    return cSpline;
}
void FloatCurveHandler::OnSplineChange(CSplineCtrl*)
{
//    EBUS_EVENT(AzToolsFramework::PropertyEditorGUIMessages::Bus, RequestWrite, splineWidget);
}

void FloatCurveHandler::ConsumeAttribute(CSplineCtrl *, AZ::u32, AzToolsFramework::PropertyAttributeReader*, const char*)
{}

void FloatCurveHandler::WriteGUIValuesIntoProperty(size_t index, CSplineCtrl* GUI, property_t& instance, AzToolsFramework::InstanceDataNode* node)
{
    //nothing to do here. the spline itself will have it's new values.
}

bool FloatCurveHandler::ReadValuesIntoGUI(size_t index, CSplineCtrl* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* node)
{
    GUI->SetSpline(reinterpret_cast<ISplineInterpolator*>(instance.m_spline));
    return false;
}


QWidget* ColorCurveHandler::CreateGUI(QWidget *pParent)
{
    CColorGradientCtrl* gradientCtrl = new CColorGradientCtrl(pParent);
    //connect(gradientCtrl, &CColorGradientCtrl::change, [gradientCtrl]()
    //{
    //    EBUS_EVENT(AzToolsFramework::PropertyEditorGUIMessages::Bus, RequestWrite, gradientCtrl);
    //});
    gradientCtrl->SetTimeRange(0, 1);
    gradientCtrl->setFixedHeight(36);
    return gradientCtrl;

}

void ColorCurveHandler::ConsumeAttribute(CColorGradientCtrl*, AZ::u32, AzToolsFramework::PropertyAttributeReader*, const char*)
{}

void ColorCurveHandler::WriteGUIValuesIntoProperty(size_t index, CColorGradientCtrl* GUI, property_t& instance, AzToolsFramework::InstanceDataNode* node)
{}

bool ColorCurveHandler::ReadValuesIntoGUI(size_t index, CColorGradientCtrl* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* node)
{
    GUI->SetSpline(reinterpret_cast<ISplineInterpolator*>(instance.m_spline));
    return false;
}

