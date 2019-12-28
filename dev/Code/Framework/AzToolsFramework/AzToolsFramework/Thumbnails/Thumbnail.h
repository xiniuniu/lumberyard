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

#include <AzCore/Component/TickBus.h>

AZ_PUSH_DISABLE_WARNING(4127 4251 4800, "-Wunknown-warning-option") // 4127: conditional expression is constant
                                                                    // 4251: 'QLocale::d': class 'QSharedDataPointer<QLocalePrivate>' needs to have dll-interface to be used by clients of class 'QLocale'
                                                                    // 4800: 'int': forcing value to bool 'true' or 'false' (performance warning)
#include <QObject>
#include <QPixmap>
#include <QFutureWatcher>
AZ_POP_DISABLE_WARNING

namespace AzToolsFramework
{
    namespace Thumbnailer
    {
        //! ThumbnailKey is used to locate thumbnails in thumbnail cache
        /*
            ThumbnailKey contains any kind of identifiable information to retrieve thumbnails (e.g. assetId, assetType, filename, etc.)
            To use thumbnail system, keep reference to your thumbnail key, and retrieve Thumbnail via ThumbnailerRequestsBus
        */
        class ThumbnailKey
            : public QObject
        {
            friend class ThumbnailContext;

            Q_OBJECT
        public:
            AZ_RTTI(ThumbnailKey, "{43F20F6B-333D-4226-8E4F-331A62315255}");

            ThumbnailKey() = default;
            virtual ~ThumbnailKey() = default;

            bool IsReady() const;

            virtual bool UpdateThumbnail();

Q_SIGNALS:
            //! Updated signal is dispatched whenever thumbnail data was changed. Anyone using this thumbnail should listen to this.
            void ThumbnailUpdatedSignal() const;
            //! Force update mapped thumbnails
            void UpdateThumbnailSignal() const;

        private:
            bool m_ready = false;
        };

        typedef QSharedPointer<ThumbnailKey> SharedThumbnailKey;

        #define MAKE_TKEY(type, ...) QSharedPointer<type>(new type(__VA_ARGS__))

        //! Thumbnail is the base class in thumbnailer system.
        /*
            Thumbnail handles storing and updating data for each specific thumbnail
            Thumbnail also emits Updated signal whenever thumbnail data changes, this signal is listened to by every ThumbnailKey that maps to this thumbnail
            Because you should be storing reference to ThumbnailKey and not Thumbnail, connect to ThumbnailKey signal instead
        */
        class Thumbnail
            : public QObject
        {
            Q_OBJECT
        public:
            enum class State
            {
                Unloaded,
                Loading,
                Ready,
                Failed
            };

            Thumbnail(SharedThumbnailKey key, int thumbnailSize);
            ~Thumbnail() override;
            bool operator == (const Thumbnail& other) const;
            void Load();
            virtual QPixmap GetPixmap() const;
            virtual void UpdateTime(float /*deltaTime*/) {}
            SharedThumbnailKey GetKey() const;
            State GetState() const;

Q_SIGNALS:
            void Updated() const;

        public Q_SLOTS:
            virtual void Update() {}

        protected:
            QFutureWatcher<void> m_watcher;
            State m_state;
            int m_thumbnailSize;
            SharedThumbnailKey m_key;
            QPixmap m_pixmap;

            virtual void LoadThread() {}
        };

        typedef QSharedPointer<Thumbnail> SharedThumbnail;

        //! Interface to retrieve thumbnails
        class ThumbnailProvider
        {
        public:
            ThumbnailProvider() = default;
            virtual ~ThumbnailProvider() = default;
            virtual bool GetThumbnail(SharedThumbnailKey key, SharedThumbnail& thumbnail) = 0;
            virtual void SetThumbnailSize(int thumbnailSize) = 0;
        };

        typedef QSharedPointer<ThumbnailProvider> SharedThumbnailProvider;

        //! ThumbnailCache manages thumbnails of specific type, derive your custom provider from this
        /*
            ThumbnailType - type of thumbnails managed
            HasherType - hashing function for storing thumbnail keys in the hashtable
            EqualKey - equality function for storing thumbnail keys in the hashtable
            HasherType and EqualKey need to be provided on individual basis depending on
            what constitutes a unique key and how should the key collection be optimized
        */
        template<typename ThumbnailType, typename HasherType, typename EqualKey>
        class ThumbnailCache
            : public ThumbnailProvider
            , public AZ::TickBus::Handler
        {
        public:
            ThumbnailCache();
            ~ThumbnailCache() override;

            //////////////////////////////////////////////////////////////////////////
            // TickBus
            //////////////////////////////////////////////////////////////////////////
            void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

            bool GetThumbnail(SharedThumbnailKey key, SharedThumbnail& thumbnail) override;

            void SetThumbnailSize(int thumbnailSize) override;

        protected:
            int m_thumbnailSize;
            AZStd::unordered_map<SharedThumbnailKey, SharedThumbnail, HasherType, EqualKey> m_cache;

            //! Check if thumbnail key is handled by this provider, overload in derived class
            virtual bool IsSupportedThumbnail(SharedThumbnailKey key) const = 0;
        };

        #define MAKE_TCACHE(cacheType, ...) QSharedPointer<cacheType>(new cacheType(__VA_ARGS__))
    } // namespace Thumbnailer
} // namespace AzToolsFramework

Q_DECLARE_METATYPE(AzToolsFramework::Thumbnailer::SharedThumbnailKey)

#include <AzToolsFramework/Thumbnails/Thumbnail.inl>