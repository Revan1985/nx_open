// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/fusion/serialization/json_functions.h>
#include <nx/p2p/p2p_connection_base.h>
#include <nx/p2p/p2p_serialization.h>
#include <nx_ec/data/api_fwd.h>

#include "ubjson_transaction_serializer.h"

namespace ec2 {

/**
 * @return true if it was able to verify & send the transaction. false, if the deserialized
 * transaction is required.
 */
using FastFunctionType =
    std::function<bool(Qn::SerializationFormat, const QnAbstractTransaction&, const QByteArray&)>;

//Overload for ubjson transactions
template<typename Function, typename Param>
bool handleTransactionParams(
    AbstractTransactionMessageBus* bus,
    const QByteArray &serializedTransaction,
    QnUbjsonReader<QByteArray> *stream,
    const QnAbstractTransaction &abstractTransaction,
    Function function,
    FastFunctionType fastFunction)
{
    if (fastFunction(Qn::SerializationFormat::ubjson, abstractTransaction, serializedTransaction))
    {
        return true; // process transaction directly without deserialize
    }

    auto transaction = QnTransaction<Param>(abstractTransaction);
    if (!QnUbjson::deserialize(stream, &transaction.params))
    {
        return NX_ASSERT(false,
            "Can't deserialize UBJSON transaction: %1, peer: %2, sequence: %3, timestamp: %4",
            abstractTransaction.command,
            abstractTransaction.peerID,
            abstractTransaction.persistentInfo.sequence,
            abstractTransaction.persistentInfo.timestamp);
    }
    if (!abstractTransaction.persistentInfo.isNull())
        bus->ubjsonTranSerializer()->addToCache(abstractTransaction.persistentInfo, abstractTransaction.command, serializedTransaction);
    function(transaction);
    return true;
}

//Overload for json transactions
template<typename Function, typename Param>
bool handleTransactionParams(
    AbstractTransactionMessageBus* /*bus*/,
    const QByteArray &serializedTransaction,
    const QJsonObject& jsonData,
    const QnAbstractTransaction &abstractTransaction,
    Function function,
    FastFunctionType fastFunction)
{
    if (fastFunction(Qn::SerializationFormat::json, abstractTransaction, serializedTransaction))
    {
        return true; // process transaction directly without deserialize
    }
    auto transaction = QnTransaction<Param>(abstractTransaction);
    if (!QJson::deserialize(jsonData["params"], &transaction.params))
    {
        return NX_ASSERT(false, "Can't deserialize JSON transaction: %1",
            abstractTransaction.command);
    }
    function(transaction);
    return true;
}

#define HANDLE_TRANSACTION_PARAMS_APPLY(_, value, param, ...) \
    case ApiCommand::value : \
        return handleTransactionParams<Function, param>(bus, serializedTransaction, serializationSupport, transaction, function, fastFunction);

template<typename SerializationSupport, typename Function>
bool handleTransaction2(
    AbstractTransactionMessageBus* bus,
    const QnAbstractTransaction& transaction,
    const SerializationSupport& serializationSupport,
    const QByteArray& serializedTransaction,
    const Function& function,
    FastFunctionType fastFunction)
{
    static constexpr int kBroadcastAction = 804;
    static constexpr int kGetKnownPeersSystemTime = 2005;
    if ((int) transaction.command == kBroadcastAction
        || (int) transaction.command == kGetKnownPeersSystemTime)
    {
        NX_VERBOSE(bus, "Ignore deprecated unused transaction %1", transaction.command);
        return true;
    }

    switch (transaction.command)
    {
        TRANSACTION_DESCRIPTOR_LIST(HANDLE_TRANSACTION_PARAMS_APPLY)
    default:
        NX_ASSERT(0, "Unknown transaction command");
    }
    return false;
}

#undef HANDLE_TRANSACTION_PARAMS_APPLY

template<typename Function>
bool handleTransaction(
    AbstractTransactionMessageBus* bus,
    Qn::SerializationFormat tranFormat,
    const QByteArray& serializedTransaction,
    const QJsonValue& json,
    const Function& function,
    FastFunctionType fastFunction)
{
    if (tranFormat == Qn::SerializationFormat::ubjson)
    {
        QnAbstractTransaction transaction;
        QnUbjsonReader<QByteArray> stream(&serializedTransaction);
        if (!QnUbjson::deserialize(&stream, &transaction))
        {
            NX_WARNING(NX_SCOPE_TAG, "Ignore bad transaction data. size=%1.", serializedTransaction.size());
            return false;
        }

        return handleTransaction2(
            bus, transaction, &stream, serializedTransaction, function, fastFunction);
    }

    if (tranFormat == Qn::SerializationFormat::json)
    {
        QnAbstractTransaction transaction;
        if (!QJson::deserialize(json, &transaction))
            return false;

        return handleTransaction2(
            bus, transaction, json.toObject(), serializedTransaction, function, fastFunction);
    }

    return false;
}

template<typename MessageBus, typename Function>
bool handleTransactionWithHeader(
    MessageBus* bus,
    const nx::p2p::P2pConnectionPtr& connection,
    const QByteArray& data,
    Function function,
    nx::Locker<nx::Mutex>* lock)
{
    int headerSize = 0;
    nx::p2p::TransportHeader header;
    QJsonObject json;
    if (connection->remotePeer().dataFormat == Qn::SerializationFormat::ubjson)
    {
        header = nx::p2p::deserializeTransportHeader(data, &headerSize);
    }
    else
    {
        if (!QJson::deserialize(data, &json))
            return false;

        QJson::deserialize(json["header"], &header);
    }

    using namespace std::placeholders;

    return handleTransaction(
        bus,
        connection->remotePeer().dataFormat,
        data.mid(headerSize),
        json["tran"],
        std::bind(function, bus, _1, connection, header, lock),
        [](Qn::SerializationFormat, const QnAbstractTransaction&, const QByteArray&) { return false; });

    return true;
}

} // namespace ec2
