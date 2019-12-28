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

#include "StdAfx.h"
#include "PlainTextEdit.hxx"

#include <UI/UICore/PlainTextEdit.moc>

#include <QtGui/QTextBlock>

namespace AzToolsFramework
{
	QRectF PlainTextEdit::GetBlockBoundingGeometry(const QTextBlock &block) const
	{
		auto result = blockBoundingGeometry(block);
		result.translate(contentOffset());
		return result;
	}

	void PlainTextEdit::ForEachVisibleBlock(AZStd::function<void(QTextBlock& block, const QRectF&)> operation) const
	{
		for (auto block = document()->begin(); block != document()->end(); block = block.next())
		{
			auto blockRect = GetBlockBoundingGeometry(block);
			if (block.isVisible() && blockRect.bottom() > 0 && blockRect.top() < size().height())
			{
				operation(block, blockRect);
			}
		}
	}

	void PlainTextEdit::mouseDoubleClickEvent(QMouseEvent* event)
	{
		auto mousePos = event->localPos();

		QTextBlock blockClicked;
		//any block could be hidden, so have to loop through to be sure
		ForEachVisibleBlock([&](const QTextBlock& block, const QRectF& blockRect) {
			if (mousePos.y() >= blockRect.top() && mousePos.y() <= blockRect.bottom())
				blockClicked = block;
		});
				
		event->ignore();

		if (blockClicked.isValid())
			emit BlockDoubleClicked(event, blockClicked);

		if (!event->isAccepted())
			QPlainTextEdit::mouseDoubleClickEvent(event);
		event->accept();
	}

	void PlainTextEdit::scrollContentsBy(int x, int y)
	{
		QPlainTextEdit::scrollContentsBy(x, y);

		emit Scrolled();
	}
}