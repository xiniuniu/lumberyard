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

#include "RadioButtonPage.h"
#include <Gallery/ui_RadioButtonPage.h>

#include <AzQtComponents/Components/Widgets/RadioButton.h>

#include <QMenu>
#include <QCheckBox>

RadioButtonPage::RadioButtonPage(QWidget* parent)
: QWidget(parent)
, ui(new Ui::RadioButtonPage)
{
    ui->setupUi(this);

    QString exampleText = R"(

QRadioButton docs: <a href="http://doc.qt.io/qt-5/qradiobutton.html">http://doc.qt.io/qt-5/qradiobutton.html</a><br/>

<pre>
#include &lt;QRadioButton&gt;

QRadioButton* radioButton;

// Assuming you've created a QRadioButton already (either in code or via .ui file):

// To disable the radio button
radioButton->setEnabled(false);

// To set the radio button to the "on" state
radioButton->setChecked(false);

// disabling a radio button and setting it to "on" can be done in Qt Designer and Creator as well.

</pre>

)";

    ui->exampleText->setHtml(exampleText);
}

RadioButtonPage::~RadioButtonPage()
{
}

#include <Gallery/RadioButtonPage.moc>