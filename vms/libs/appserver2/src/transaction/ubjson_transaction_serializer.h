// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QCache>

#include <nx/fusion/serialization/ubjson_functions.h>
#include <nx/p2p/p2p_fwd.h>
#include <nx/p2p/p2p_serialization.h>
#include <transaction/transaction.h>

namespace ec2
{
    /**
     * This class serialized a transaction into a byte array.
     */
    class QnUbjsonTransactionSerializer
    {
    public:
        struct CacheKey
        {
            CacheKey() {}
            CacheKey(const QnAbstractTransaction::PersistentInfo& persistentInfo, const ApiCommand::Value& command): persistentInfo(persistentInfo), command(command) {}
            QnAbstractTransaction::PersistentInfo persistentInfo;
            ApiCommand::Value command = ApiCommand::Value::NotDefined;

            bool operator== (const CacheKey& other) const {
                return persistentInfo == other.persistentInfo && command == other.command;
            }
        };
    public:
        static const int MAX_CACHE_SIZE_BYTES = 512*1024;

        QnUbjsonTransactionSerializer()
        :
            m_cache(MAX_CACHE_SIZE_BYTES)
        {
        }

        void addToCache(const QnAbstractTransaction::PersistentInfo& key, ApiCommand::Value command, const QByteArray& data)
        {
            NX_MUTEX_LOCKER lock( &m_mutex );
            m_cache.insert(CacheKey(key, command), new QByteArray(data), data.size());
        }

        template<class T>
        QByteArray serializedTransaction(const QnTransaction<T>& tran)
        {
            NX_MUTEX_LOCKER lock( &m_mutex );

            // do not cache read-only transactions (they have sequence == 0)
            CacheKey key(tran.persistentInfo, tran.command);
            if (!tran.persistentInfo.isNull() && m_cache.contains(key))
                return *m_cache[key];

            std::unique_ptr<QByteArray> serializedTran( new QByteArray() );
            QnUbjsonWriter<QByteArray> stream(serializedTran.get());
            QnUbjson::serialize( tran, &stream );
            QByteArray result = *serializedTran;
            if( !tran.persistentInfo.isNull() )
            {
                m_cache.insert( key, serializedTran.get(), serializedTran->size() );
                serializedTran.release();
            }

            return result;
        }

        template<class T>
        QByteArray serializedTransactionWithHeader(const QnTransaction<T> &tran, const nx::p2p::TransportHeader& header)
        {
            return serializedTransactionWithHeader(serializedTransaction(tran), header);
        }

        template<class T>
        QByteArray serializedTransactionWithoutHeader(const QnTransaction<T> &tran)
        {
            return serializedTransaction(tran);
        }

        QByteArray serializedTransactionWithHeader(const QByteArray &serializedTran, const nx::p2p::TransportHeader& header)
        {
            return nx::p2p::serializeTransportHeader(header).append(serializedTran);
        }

    private:
        mutable nx::Mutex m_mutex;
        QCache<CacheKey, const QByteArray> m_cache;
    };

    size_t qHash(const QnUbjsonTransactionSerializer::CacheKey &id);

}
