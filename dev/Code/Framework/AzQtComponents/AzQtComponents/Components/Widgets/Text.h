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

#include <AzQtComponents/AzQtComponentsAPI.h>

class QLabel;

namespace AzQtComponents
{
    /**
     * Special class to handle styling and painting of Text (labels, to be specific).
     *
     * All of the styling of QLabel objects is done via CSS, in Text.qss
     *
     * There are 4 css text styles that can be added to QLabel objects (Headline, Title, Subtitle, and Paragraph). Use the appropriate
     * method below to apply one.
     *
     * Note that a QLabel without a custom css "class" still gets styled according to the Text.qss file.
     * Note also that Qt doesn't properly support the line-height css property, so if non-default Qt
     * line-height spacing is required for multi-line blocks of text, some extra effort will be required
     * beyond the scope of this class/code.
     *
     * There are also 4 css color styles that can be added to QLabel objects (Primary, Secondary, Highlighted and Black). Use the
     * appropriate method below to apply one.
     *
     **/
    class AZ_QT_COMPONENTS_API Text
    {
    public:

        /*!
        * Applies the Headline styling to a QLabel.
        * Same as
        *   AzQtComponents::Style::addClass(label, "Headline");
        */
        static void addHeadlineStyle(QLabel* text);
        
        /*!
        * Applies the Title styling to a QLabel.
        * Same as
        *   AzQtComponents::Style::addClass(label, "Title");
        */
        static void addTitleStyle(QLabel* text);

        /*!
        * Applies the Subtitle styling to a QLabel.
        * Same as
        *   AzQtComponents::Style::addClass(label, "Subtitle");
        */
        static void addSubtitleStyle(QLabel* text);
        
        /*!
        * Applies the Paragraph styling to a QLabel.
        * Same as
        *   AzQtComponents::Style::addClass(label, "Paragraph");
        */
        static void addParagraphStyle(QLabel* text);

        /*!
        * Applies the Primary color styling to a QLabel.
        * Same as
        *   AzQtComponents::Style::addClass(label, "primaryText");
        */
        static void addPrimaryStyle(QLabel* text);
        
        /*!
        * Applies the Secondary color styling to a QLabel.
        * Same as
        *   AzQtComponents::Style::addClass(label, "secondaryText");
        */
        static void addSecondaryStyle(QLabel* text);

        /*!
        * Applies the Highlighted color styling to a QLabel.
        * Same as
        *   AzQtComponents::Style::addClass(label, "highlightedText");
        */
        static void addHighlightedStyle(QLabel* text);

        /*!
        * Applies the Black color styling to a QLabel.
        * Same as
        *   AzQtComponents::Style::addClass(label, "blackText");
        */
        static void addBlackStyle(QLabel* text);
    };
} // namespace AzQtComponents
