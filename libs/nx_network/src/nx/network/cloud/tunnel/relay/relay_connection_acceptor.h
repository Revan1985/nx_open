// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/network/abstract_stream_socket_acceptor.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_notifications.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/reverse_connection_acceptor.h>
#include <nx/network/socket_common.h>
#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>

#include "../../cloud_abstract_connection_acceptor.h"
#include "api/relay_api_client.h"

namespace nx::network::cloud::relay {

namespace detail {

class NX_NETWORK_API ReverseConnection:
    public aio::BasicPollable,
    public AbstractAcceptableReverseConnection
{
    using base_type = aio::BasicPollable;

public:
    ReverseConnection(
        const nx::Url& relayUrl,
        std::optional<std::chrono::milliseconds> connectTimeout,
        std::optional<int> forcedHttpTunnelType);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    void connect(
        ReverseConnectionCompletionHandler handler);

    virtual void waitForOriginatorToStartUsingConnection(
        ReverseConnectionCompletionHandler handler) override;

    const nx::cloud::relay::api::Client& relayClient() const;
    nx::cloud::relay::api::BeginListeningResponse beginListeningResponse() const;
    std::unique_ptr<AbstractStreamSocket> takeSocket();
    void setTimeout(std::optional<std::chrono::milliseconds> timeout);

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unique_ptr<nx::cloud::relay::api::Client> m_relayClient;
    const std::string m_peerName;
    ReverseConnectionCompletionHandler m_connectHandler;
    std::unique_ptr<nx::network::http::AsyncMessagePipeline> m_httpPipeline;
    ReverseConnectionCompletionHandler m_onConnectionActivated;
    std::unique_ptr<AbstractStreamSocket> m_streamSocket;
    nx::cloud::relay::api::BeginListeningResponse m_beginListeningResponse;

    void onConnectionClosed(SystemError::ErrorCode closeReason, bool /*connectionDestroyed*/);

    void onConnectDone(
        nx::cloud::relay::api::ResultCode resultCode,
        nx::cloud::relay::api::BeginListeningResponse response,
        std::unique_ptr<AbstractStreamSocket> streamSocket);

    void dispatchRelayNotificationReceived(nx::network::http::Message message);
    void dispatchRelayNotificationReceivedOnVerificationStage(nx::network::http::Message message);

    void processOpenTunnelNotification(
        nx::cloud::relay::api::OpenTunnelNotification openTunnelNotification);

    void processTestConnectionNotification(
        nx::cloud::relay::api::TestConnectionNotification notification);
};

//-------------------------------------------------------------------------------------------------

/**
 * Opens server-side connections to the relay.
 */
class NX_NETWORK_API ReverseConnector:
    public SimpleReverseConnector<ReverseConnection>
{
    using base_type = SimpleReverseConnector<ReverseConnection>;

public:
    ReverseConnector(const nx::Url& relayUrl);

    /**
     * Note: it is possible that a single call to this method will actually open multiple connections
     * to the relay.
     * The reason is that there can be multiple HTTP tunnel types with the same priority.
     * And, in this case, all succeeded tunnels are provided.
     */
    virtual void connectAsync() override;

    void setConnectTimeout(std::optional<std::chrono::milliseconds> timeout);

private:
    nx::Url m_relayUrl;
    std::optional<std::chrono::milliseconds> m_connectTimeout;
};

} // namespace detail

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API ConnectionAcceptor:
    public AbstractConnectionAcceptor
{
    using base_type = AbstractConnectionAcceptor;

public:
    ConnectionAcceptor(const nx::Url& relayUrl);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void acceptAsync(AcceptCompletionHandler handler) override;
    virtual void cancelIOSync() override;

    virtual std::unique_ptr<AbstractStreamSocket> getNextSocketIfAny() override;

    /**
     * Install a handler to be called when the acceptor establishes a reverse connection to the
     * relay, but before that connection is actively accepting connections from the client.
     * NOTE: must be called before acceptAsync().
     */
    virtual void setConnectionEstablishedHandler(
        AbstractConnectionAcceptor::ConnectionEstablishedHandler handler) override;

    /**
     * Install a handler to be called when an error occurs, e.g. there is an error connecting to
     * the relay. This is a dedicated channel in addition to the result passed via the handler in
     * acceptAsync().
     * NOTE: must be called before acceptAsync().
     */
    virtual void setAcceptErrorHandler(ErrorHandler) override;

    void setConnectTimeout(std::optional<std::chrono::milliseconds> timeout);

protected:
    virtual void stopWhileInAioThread() override;

private:
    const nx::Url m_relayUrl;
    ReverseConnectionAcceptor<detail::ReverseConnection> m_acceptor;
    bool m_started = false;
    AbstractConnectionAcceptor::ConnectionEstablishedHandler m_connectionEstablishedHandler;
    AbstractConnectionAcceptor::ErrorHandler m_acceptErrorHandler;

    void onConnectionEstablished(const detail::ReverseConnection& newConnection);
    void updateAcceptorConfiguration(const detail::ReverseConnection& newConnection);
    void reportAcceptorError(
        SystemError::ErrorCode sysErrorCode,
        std::unique_ptr<detail::ReverseConnection> failedConnection);

    std::unique_ptr<AbstractStreamSocket> toStreamSocket(
        std::unique_ptr<detail::ReverseConnection> connection);
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API ConnectionAcceptorFactory:
    public nx::utils::BasicFactory<
        std::unique_ptr<AbstractConnectionAcceptor>(const nx::Url& /*relayUrl*/)>
{
    using base_type = nx::utils::BasicFactory<
        std::unique_ptr<AbstractConnectionAcceptor>(const nx::Url& /*relayUrl*/)>;

public:
    ConnectionAcceptorFactory();

    static ConnectionAcceptorFactory& instance();

private:
    std::unique_ptr<AbstractConnectionAcceptor> defaultFactoryFunc(const nx::Url &relayUrl);
};

} // namespace nx::network::cloud::relay
