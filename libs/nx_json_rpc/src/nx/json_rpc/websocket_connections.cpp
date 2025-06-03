// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "websocket_connections.h"

#include <nx/network/websocket/websocket.h>
#include <nx/reflect/json.h>
#include <nx/utils/json/qt_core_types.h>

#include "websocket_connection.h"

namespace nx::json_rpc {

void WebSocketConnections::executeAsync(
    Connection* connection,
    std::unique_ptr<Executor> executor,
    nx::MoveOnlyFunc<void(Response)> handler)
{
    auto threadIt = connection->threads.insert(connection->threads.begin(), std::thread());
    *threadIt = std::thread(
        [this,
            executor = std::move(executor),
            handler = std::move(handler),
            weakConnection = std::weak_ptr(connection->connection),
            threadIt]() mutable
        {
            auto response =
                [executor = std::move(executor), &weakConnection]()
                {
                    std::promise<Response> promise;
                    auto future = promise.get_future();
                    try
                    {
                        executor->execute(weakConnection,
                            [p = std::move(promise)](auto r) mutable { p.set_value(std::move(r)); });
                        return future.get();
                    }
                    catch (const std::exception& e)
                    {
                        return nx::json_rpc::Response::makeError(executor->responseId(),
                            Error::internalError,
                            NX_FMT("Unhandled exception: %1", e.what()).toStdString());
                    }
                    catch (...)
                    {
                        return nx::json_rpc::Response::makeError(executor->responseId(),
                            Error::internalError,
                            "Unhandled exception");
                    }
                }();
            if (auto connection = weakConnection.lock())
            {
                NX_MUTEX_LOCKER lock(&m_mutex);
                if (auto c = m_connections.find(connection.get()); c != m_connections.end())
                {
                    c->second.connection->post(
                        [handler = std::move(handler), response = std::move(response)]() mutable
                        {
                            handler(std::move(response));
                        });
                    threadIt->detach();
                    c->second.threads.erase(threadIt);
                }
            }
        });
}

void WebSocketConnections::addConnection(std::shared_ptr<WebSocketConnection> connection)
{
    auto connectionPtr = connection.get();
    std::weak_ptr<WebSocketConnection> weakConnection{connection};
    NX_VERBOSE(this, "Add connection %1", connectionPtr);
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_connections.emplace(connectionPtr, Connection{std::move(connection)});
    }
    connectionPtr->start(std::move(weakConnection),
        [this](auto request, auto handler, auto connection)
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            auto it = m_connections.find(connection);
            if (it == m_connections.end())
            {
                lock.unlock();
                NX_DEBUG(this,
                    "The connection %1 is closed, request %2with method %3 is ignored",
                    connection,
                    request.id ? nx::reflect::json::serialize(*request.id) + ' ' : std::string(),
                    request.method);
                handler(Response());
                return;
            }

            for (const auto& executorCreator: m_executorCreators)
            {
                if (executorCreator->isMatched(request.method))
                {
                    executeAsync(
                        &it->second,
                        executorCreator->create(std::move(request)),
                        std::move(handler));
                    return;
                }
            }

            lock.unlock();
            handler(Response::makeError(
                request.responseId(), Error::methodNotFound, "Unsupported method"));
        });
}

void WebSocketConnections::removeConnection(WebSocketConnection* connection)
{
    NX_VERBOSE(this, "Remove connection %1", connection);
    Connection holder;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (auto it = m_connections.find(connection); it != m_connections.end())
        {
            holder = std::move(it->second);
            m_connections.erase(it);
        }
    }
    holder.stop();
}

void WebSocketConnections::updateConnectionGuards(
    WebSocketConnection* connection, std::vector<nx::utils::Guard> guards)
{
    std::vector<nx::utils::Guard> oldGuards;
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (auto it = m_connections.find(connection); it != m_connections.end())
    {
        std::swap(it->second.guards, oldGuards);
        it->second.guards = std::move(guards);
    }
}

std::size_t WebSocketConnections::count() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_connections.size();
}

void WebSocketConnections::clear()
{
    std::unordered_map<WebSocketConnection*, Connection> connections;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_connections.swap(connections);
    }
    for (auto& [_, connection]: connections)
        connection.stop();
}

void WebSocketConnections::Connection::stop()
{
    for (auto& thread: threads)
        thread.detach();
    if (connection)
    {
        auto connectionPtr = connection.get();
        connectionPtr->pleaseStop([connection = std::move(connection)]() {});
    }
}

} // namespace nx::json_rpc
