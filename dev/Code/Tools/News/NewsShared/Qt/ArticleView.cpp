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

#include <AzQtComponents/Components/ExtendedLabel.h>

#include "ArticleView.h"
#include "NewsShared/ResourceManagement/ArticleDescriptor.h"
AZ_PUSH_DISABLE_WARNING(4251, "-Wunknown-warning-option") // 4251: 'QVariant::d': struct 'QVariant::Private' needs to have dll-interface to be used by clients of class 'QVariant'
#include "NewsShared/Qt/ui_ArticleView.h"
AZ_POP_DISABLE_WARNING
#include "NewsShared/Qt/ui_PinnedArticleView.h"
#include "NewsShared/ResourceManagement/ResourceManifest.h"
#include "NewsShared/ResourceManagement/Resource.h"

#include <QDesktopServices>
#include <QUrl>

using namespace News;

ArticleView::ArticleView(
    QWidget* parent,
    const ArticleDescriptor& article,
    const ResourceManifest& manifest)
    : QWidget(parent)
    , m_pArticle(new ArticleDescriptor(article))
    , m_manifest(manifest)
    , m_icon(nullptr)
{
    
}

void ArticleView::Update()
{
    Q_ASSERT(m_widgetImageFrame && m_widgetTitle && m_widgetBody);

    auto pResource = m_manifest.FindById(m_pArticle->GetResource().GetId());
    m_pArticle.reset();
    if (pResource)
    {
        m_pArticle = QSharedPointer<ArticleDescriptor>(new ArticleDescriptor(*pResource));
        m_widgetTitle->setText(m_pArticle->GetTitle());
        m_widgetBody->setText(m_pArticle->GetBody());
        auto pImageResource = m_manifest.FindById(m_pArticle->GetImageId());
        if (pImageResource)
        {
            QPixmap pixmap;
            if (pixmap.loadFromData(pImageResource->GetData()))
            {
                if (!m_icon)
                {
                    m_icon = new AzQtComponents::ExtendedLabel(this);
                    m_icon->setStyleSheet("border: none;");
                    m_icon->setAlignment(Qt::AlignCenter);
                    static_cast<QVBoxLayout*>(
                        m_widgetImageFrame->layout())->insertWidget(0, m_icon);
                    connect(m_icon, &AzQtComponents::ExtendedLabel::clicked, this, &ArticleView::articleSelectedSlot);
                }
                m_icon->setPixmap(pixmap.scaled(m_widgetImageFrame->minimumWidth(), 
                    m_widgetImageFrame->minimumHeight(),
                    Qt::KeepAspectRatioByExpanding));
            }
            else
            {
                RemoveIcon();
            }
        }
        else
        {
            RemoveIcon();
        }
    }
}

void ArticleView::mousePressEvent(QMouseEvent* event)
{
    articleSelectedSlot();
}

void ArticleView::RemoveIcon()
{
    if (m_icon)
    {
        delete m_icon;
        m_icon = nullptr;
    }
}

void ArticleView::linkActivatedSlot(const QString& link)
{
    QDesktopServices::openUrl(QUrl(link));
    Q_EMIT linkActivatedSignal(link);
}

void ArticleView::articleSelectedSlot()
{
    Q_EMIT articleSelectedSignal(m_pArticle->GetResource().GetId());
}

QSharedPointer<const ArticleDescriptor> ArticleView::GetArticle() const
{
    return m_pArticle;
}

void ArticleView::SetupViewWidget(QFrame* widgetImageFrame, AzQtComponents::ExtendedLabel* widgetTitle, AzQtComponents::ExtendedLabel* widgetBody)
{
    Q_ASSERT(m_widgetImageFrame == nullptr && m_widgetTitle == nullptr && m_widgetBody == nullptr);

    m_widgetImageFrame = widgetImageFrame;
    m_widgetTitle = widgetTitle;
    m_widgetBody = widgetBody;

    connect(m_widgetTitle, &QLabel::linkActivated, this, &ArticleView::linkActivatedSlot);
    connect(m_widgetBody, &QLabel::linkActivated, this, &ArticleView::linkActivatedSlot);
    connect(m_widgetTitle, &AzQtComponents::ExtendedLabel::clicked, this, &ArticleView::articleSelectedSlot);
    connect(m_widgetBody, &AzQtComponents::ExtendedLabel::clicked, this, &ArticleView::articleSelectedSlot);

    Q_ASSERT(m_widgetImageFrame && m_widgetTitle && m_widgetBody);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// ArticleViewDefaultWidget
////////////////////////////////////////////////////////////////////////////////////////////////////
ArticleViewDefaultWidget::ArticleViewDefaultWidget(QWidget* parent,
    const ArticleDescriptor& article,
    const ResourceManifest& manifest)
    : ArticleView(parent, article, manifest)
    , m_ui(new Ui::ArticleViewWidget())
{
    m_ui->setupUi(this);
    SetupViewWidget(m_ui->imageFrame, m_ui->titleLabel, m_ui->bodyLabel);
    Update();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// ArticleViewPinnedWidget
////////////////////////////////////////////////////////////////////////////////////////////////////
ArticleViewPinnedWidget::ArticleViewPinnedWidget(QWidget* parent,
    const ArticleDescriptor& article,
    const ResourceManifest& manifest)
    : ArticleView(parent, article, manifest)
    , m_ui(new Ui::PinnedArticleViewWidget())
{
    m_ui->setupUi(this);
    SetupViewWidget(m_ui->imageFrame, m_ui->titleLabel, m_ui->bodyLabel);
    Update();
}

#include "NewsShared/Qt/ArticleView.moc"
