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
#include <QObject>
#include <QHash>
#include <QString>
#include <QScopedPointer>
#include <QSet>
#include <QStack>

class QFileSystemWatcher;
class QRegExp;

namespace AzQtComponents
{
    class StyleManager;

    class AZ_QT_COMPONENTS_API StyleSheetCache
        : public QObject
    {
        Q_OBJECT

        friend StyleManager;

        explicit StyleSheetCache(QObject* parent);
        ~StyleSheetCache();

    public:
        static const QString& styleSheetExtension();

        void addSearchPaths(const QString& searchPrefix, const QString& pathOnDisk, const QString& qrcPrefix);
        void setFallbackSearchPaths(const QString& fallbackPrefix, const QString& pathOnDisk, const QString& qrcPrefix);

        QString loadStyleSheet(QString styleFileName);

    public Q_SLOTS:
        void clearCache();

    Q_SIGNALS:
        void styleSheetsChanged();

    private Q_SLOTS:
        void fileOnDiskChanged(const QString& filePath);

    private:
        QString preprocess(QString styleFileName, QString loadedStyleSheet);
        QString findStyleSheetPath(const QString& styleFileName);
        AZ_PUSH_DISABLE_WARNING(4251, "-Wunknown-warning-option") // 4251: 'AzQtComponents::StyleSheetCache::m_styleSheetCache': class 'QHash<QString,QString>' needs to have dll-interface to be used by clients of class 'AzQtComponents::StyleSheetCache'
        QHash<QString, QString> m_styleSheetCache;

        QSet<QString> m_processingFiles;
        QStack<QString> m_processingStack;

        QFileSystemWatcher* m_fileWatcher;

        QScopedPointer<QRegExp> m_importExpression;

        QSet<QString> m_prefixes;

        QString m_fallbackPrefix;
        AZ_POP_DISABLE_WARNING
};

} // namespace AzQtComponents

