// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>

#include <core/resource/shared_resource_pointer.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/rest/user_access_data.h>
#include <nx/network/websocket/websocket.h>
#include <nx/p2p/transport/i_p2p_transport.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/api/data/tran_state_data.h>
#include <transaction/connection_guard.h>
#include <utils/common/from_this_to_shared.h>

#include "p2p_fwd.h"
#include "transaction_filter.h"

namespace ec2 {
struct QnAbstractTransaction;
}

namespace nx::vms::api { struct PeerData; }

namespace nx {
namespace p2p {

using namespace ec2;
class ConnectionBase;
using P2pConnectionPtr = QnSharedResourcePointer<ConnectionBase>;
using SendCounters = std::array<std::atomic<qint64>, (int(MessageType::counter))>;

//-------------------------------------------------------------------------------------------------

class ConnectionBase: public QObject, public QnFromThisToShared<ConnectionBase>
{
    Q_OBJECT
public:

    static constexpr char kWebsocketUrlPath[] = "/ec2/transactionBus/websocket";

    static constexpr char kHttpBaseUrlPath[] = "/ec2/transactionBus/http";
    static constexpr char kHttpHandshakeUrlPath[] = "/ec2/transactionBus/http/handshake";
    static constexpr char kHttpDataUrlPath[] = "/ec2/transactionBus/http/data";

    enum class State
    {
        NotDefined,
        Connecting,
        Connected,
        Error = 256, //< All greater states are errors too.
        Unauthorized,
        forbidden,
        handshakeError,
    };

    enum class Direction
    {
        incoming,
        outgoing,
    };

    ConnectionBase(
        const nx::Uuid& remoteId,
        nx::vms::api::PeerType remotePeerType,
        const vms::api::PeerDataEx& localPeer,
        const nx::Url& remotePeerUrl,
        const std::chrono::seconds& keepAliveTimeout,
        std::unique_ptr<QObject> opaqueObject,
        nx::network::ssl::AdapterFunc adapterFunc,
        std::unique_ptr<ConnectionLockGuard> connectionLockGuard = nullptr);

    ConnectionBase(
        const vms::api::PeerDataEx& remotePeer,
        const vms::api::PeerDataEx& localPeer,
        P2pTransportPtr p2pTransport,
        std::unique_ptr<QObject> opaqueObject,
        std::unique_ptr<ConnectionLockGuard> connectionLockGuard);

    virtual ~ConnectionBase();

    void gotPostConnection(
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        nx::network::http::Request request);

    static const SendCounters& sendCounters() { return m_sendCounters;  }

    /** Peer that opens this connection */
    Direction direction() const { return m_direction; }

    const nx::network::http::HttpHeaders& responseHeaders() const { return m_responseHeaders; }

    const vms::api::PeerDataEx& localPeer() const { return m_localPeer; }
    const vms::api::PeerDataEx& remotePeer() const { return m_remotePeer; }
    bool isIncoming() const { return m_direction == Direction::incoming;  }
    virtual void updateCredentials(nx::network::http::Credentials) {}

    virtual void setState(State state, const QString& reason);
    State state() const;

    /**
     * @return false if cannot decide whether send tran or not. The caller MUST retry using
     * sendTransaction(const QnTransaction<...>&, ...).
     * true if transaction has been sent or MUST never be sent.
     */
    template<typename... Args>
    bool sendTransaction(
        const QnAbstractTransaction& tran, MessageType messageType, Args&&... args)
    {
        switch (shouldTransactionBeSentToPeer(tran))
        {
            case nx::p2p::FilterResult::allow:
                break;

            case nx::p2p::FilterResult::deny:
                // As if the transaction has been sent.
                m_timer.post([this]() { transactionSkipped(); });
                return true;

            case nx::p2p::FilterResult::deserializedTransactionRequired:
                return false; //< The deserialized transaction is required to decide firmly.
        }

        sendMessage(messageType, std::forward<Args>(args)...);
        return true;
    }

    template<typename T, typename... Args>
    void sendTransaction(const QnTransaction<T>& tran, MessageType messageType, Args&&... args)
    {
        if (shouldTransactionBeSentToPeer(tran) != nx::p2p::FilterResult::deny)
            sendMessage(messageType, std::forward<Args>(args)...);
        else
            m_timer.post([this]() { transactionSkipped(); });
    }

    void sendMessage(MessageType messageType, const nx::Buffer& data);
    void sendMessage(MessageType messageType, const QByteArray& data);
    void sendMessage(const nx::Buffer& data);
    void sendMessage(const QByteArray& data);

    void startConnection();
    void startReading();

    QObject* opaqueObject();

    nx::Url remoteAddr() const;
    void pleaseStopSync();

    virtual bool validateRemotePeerData(const vms::api::PeerDataEx& /*peer*/) const { return true; }

    void addAdditionalRequestHeaders(nx::network::http::HttpHeaders headers);
    void addRequestQueryParams(std::vector<std::pair<QString, QString>> queryParams);

    const IP2PTransport& p2pTransport() const { return *m_p2pTransport; }
    IP2PTransport& p2pTransport() { return *m_p2pTransport; }

    QString idForToStringFromPtr() const;
    QString lastErrorMessage() const { return m_lastErrorMessage; }

    /**
     * Limit the unsent buffer size in bytes. If buffer overflow then connection is closed with error.
     * Note: the size control is applied for messages that was sent after this call only.
     */
    void setMaxSendBufferSize(size_t value);

    size_t maxSendBufferSize() const { return m_maxBufferSize; }
    size_t sendBufferSize() const { return m_dataToSend.dataSize(); }

    static constexpr auto kPingTimeout = std::chrono::seconds(30);

    // For tests.
    static std::chrono::milliseconds pingTimeout();
    static void setNoPingSupportClientHeader(bool value);

    void setFilter(std::unique_ptr<TransactionFilter> transactionFilter)
    {
        m_filter = std::move(transactionFilter);
    }

    template<typename Transaction>
    // requires std::is_base_of<QnAbstractTransaction, Transaction>
    nx::p2p::FilterResult shouldTransactionBeSentToPeer(const Transaction& transaction)
    {
        if (remotePeer().peerType == nx::vms::api::PeerType::oldMobileClient &&
            skipTransactionForMobileClient(transaction.command))
        {
            return nx::p2p::FilterResult::deny;
        }
        else if (remotePeer().peerType == nx::vms::api::PeerType::oldServer)
        {
            return nx::p2p::FilterResult::deny;
        }
        else if (transaction.isLocal() && !remotePeer().isClient())
        {
            return nx::p2p::FilterResult::deny;
        }

        if (transaction.command == ::ec2::ApiCommand::tranSyncRequest ||
            transaction.command == ::ec2::ApiCommand::tranSyncResponse ||
            transaction.command == ::ec2::ApiCommand::tranSyncDone)
        {
            // Always sending "special" transactions. E.g., connection handshake.
            return nx::p2p::FilterResult::allow;
        }

        if (m_filter)
            return m_filter->match(transaction);

        return nx::p2p::FilterResult::allow;
    }

signals:
    void gotMessage(QWeakPointer<ConnectionBase> connection, nx::p2p::MessageType messageType, const nx::Buffer& payload);
    void stateChanged(QWeakPointer<ConnectionBase> connection, ConnectionBase::State state);
    void allDataSent(QWeakPointer<ConnectionBase> connection);

protected:
    virtual bool fillAuthInfo(nx::network::http::AsyncClient* httpClient) = 0;
    void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread);
    void stopWhileInAioThread();

private:
    void cancelConnecting(State state, const QString& reason);

    void onHttpClientDone();

    void onMessageSent(SystemError::ErrorCode errorCode, size_t bytesSent);
    void onNewMessageRead(SystemError::ErrorCode errorCode, size_t bytesRead);

    void handleMessage(const nx::Buffer& message);
    MessageType getMessageType(const nx::Buffer& buffer, const nx::vms::api::PeerData& peer) const;
    void transactionSkipped();
    bool skipTransactionForMobileClient(ApiCommand::Value command);

private:
    enum class CredentialsSource
    {
        userNameAndPassword,
        serverKey,
        none,
    };
protected:
    const Direction m_direction;
private:

    class Dequeue
    {
    public:
        void push_back(const nx::Buffer& data)
        {
            m_dataSize += data.size();
            m_queue.push_back(data);
        }

        void pop_front()
        {
            m_dataSize -= m_queue.front().size();
            m_queue.pop_front();
        }

        const nx::Buffer& front() const { return m_queue.front(); }
        size_t dataSize() const { return m_dataSize; }
        size_t size() const { return m_queue.size(); }
        bool empty() const { return m_queue.empty(); }

    private:
        std::deque<nx::Buffer> m_queue;
        std::atomic<size_t> m_dataSize = 0;
    };

    Dequeue m_dataToSend;
    nx::Buffer m_readBuffer;

    std::unique_ptr<nx::network::http::AsyncClient> m_httpClient;

    vms::api::PeerDataEx m_remotePeer;
    vms::api::PeerDataEx m_localPeer;

    P2pTransportPtr m_p2pTransport;
    std::atomic<State> m_state{State::Connecting};

    nx::Url m_remotePeerUrl;

    static SendCounters m_sendCounters;

    network::aio::Timer m_timer;
    std::chrono::seconds m_keepAliveTimeout;
    std::unique_ptr<QObject> m_opaqueObject;
    std::unique_ptr<ConnectionLockGuard> m_connectionLockGuard;

    int m_sendSequence = 0;
    int m_lastReceivedSequence = 0;

    mutable nx::Mutex m_mutex;

    nx::network::http::HttpHeaders m_additionalRequestHeaders;
    std::vector<std::pair<QString, QString>> m_requestQueryParams;
    nx::network::http::HttpHeaders m_responseHeaders;
    QByteArray m_connectionGuid;
    size_t m_startedClassId = 0;
    QString m_lastErrorMessage;
    int64_t m_extraBufferSize = 0;
    size_t m_maxBufferSize = 0;
    nx::network::aio::Timer m_pongTimer;
    std::unique_ptr<TransactionFilter> m_filter;

    // For tests.
    static bool s_noPingSupportClientHeader;
};

QString toString(ConnectionBase::State value);

} // namespace p2p
} // namespace nx
