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

#include <GraphCanvas/Widgets/StyledItemDelegates/IconDecoratedNameDelegate.h>

namespace GraphCanvas
{
    IconDecoratedNameDelegate::IconDecoratedNameDelegate(QWidget* parent)
    : QStyledItemDelegate(parent)
    {
    }
    
    void IconDecoratedNameDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        bool fallback = true;

        if (option.state & QStyle::State_Selected)
        {
            QVariant iconData = index.data(Qt::DecorationRole);

            if (iconData.canConvert<QPixmap>())
            {
                QPixmap icon = qvariant_cast<QPixmap>(iconData);

                // Magic decorator width offset to make sure the overlaid pixmap is in the right place
                int decoratorWidth = icon.width() + 7;

                QStyleOptionViewItem opt = option;
                initStyleOption(&opt, index);

                // Draw the original widget
                QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
                style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

                // Redraw the decorator to avoid the selection tinting
                QRect decoratorRect = QRect(opt.rect.x(), opt.rect.y(), decoratorWidth, opt.rect.height());
                style->drawItemPixmap(painter, decoratorRect, Qt::AlignCenter, icon);

                fallback = false;
            }
        }

        if(fallback)
        {
            QStyledItemDelegate::paint(painter, option, index);
        }
    }
}