// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/network/http/http_types.h>
#include <nx/telemetry/http_span.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/stree/attribute_dictionary.h>
#include <nx/utils/string.h>

#include "../abstract_msg_body_source.h"

namespace nx::network::http {

class HttpServerConnection;

struct ConnectionAttrs
{
    // Connection id is unique for the process runtime.
    std::uint64_t id = 0;
    bool isSsl = false;

    // Connection source address. This can be different from the request source address in case of proxy.
    SocketAddress sourceAddr;

    // Address the connection was accepted on.
    SocketAddress localAddr;
};

struct TraceContext
{
    // The trace id is a unique identifier for the request. It should be used in logs to identify
    // all log messages related to the same request.
    std::string traceId;

    // The HTTP header that contains the trace id.
    std::pair<std::string, std::string> header;
};

std::optional<TraceContext> parseTraceContext(const HttpHeaders& headers);

struct NX_NETWORK_API RequestContext
{
    using Attrs = nx::utils::stree::StringAttrDict;

    ConnectionAttrs connectionAttrs;
    std::weak_ptr<http::HttpServerConnection> conn;

    /**
     * The endpoint of the request originator. Note that this is different from the HTTP connection
     * source endpoint if HTTP proxy is inplace. In this case, Forwarded and X-Forwarded-For headers
     * are taken into account.
     */
    SocketAddress clientEndpoint;

    /**
     * Attributes added by chained request handlers while processing the request.
     */
    Attrs attrs;

    http::Request request;

    /**
     * Initialized only if the HTTP handler is registered using MessageBodyDeliveryType::stream.
     */
    std::unique_ptr<AbstractMsgBodySourceWithCache> body;

    /**
     * Request handler path like /object_type/{objectId}/sub_object_type/{subObjectId}.
     */
    std::string pathTemplate;

    /**
     * Parameters, taken from request path.
     * E.g., if handler was registered with path /object_type/{objectId}/sub_object_type/{subObjectId},
     * and request /object_type/id1/sub_object_type/id2 was received.
     * Then this would be (("objectId", "id1"), ("subObjectId", "id2")).
     */
    RequestPathParams requestPathParams;

    /** Request tracing context. */
    std::optional<TraceContext> traceContext;

    nx::telemetry::HttpSpan telemetrySpan;

    RequestContext();

    RequestContext(
        ConnectionAttrs connectionAttrs,
        std::weak_ptr<http::HttpServerConnection> connection,
        const SocketAddress& clientEndpoint,
        Attrs attrs,
        http::Request request,
        std::unique_ptr<AbstractMsgBodySourceWithCache> body = nullptr);

    // Returns traceContext->traceId if traceContext is set, otherwise returns empty string.
    std::string traceId() const;
};

enum class MessageBodyDeliveryType
{
    /**
     * The message body will be delivered to the RequestHandlerWithContext descendant in
     * http::RequestContext.request.messageBody.
     */
    buffer,

    /**
     * The message body will be delivered to the RequestHandlerWithContext descendant in
     * http::RequestContext.body.
     */
    stream,
};

//-------------------------------------------------------------------------------------------------

/**
 * Used to install handlers on some events on HTTP connection.
 * WARNING: There is no way to remove installed event handler.
 * Event handler implementation MUST ensure it does not crash.
 */
class ConnectionEvents
{
public:
    /**
     * Invoked just after sending the response.
     */
    nx::MoveOnlyFunc<void(HttpServerConnection*)> onResponseHasBeenSent;
};

/**
 * Result of HTTP request processing. Contains data for building HTTP response.
 */
struct NX_NETWORK_API RequestResult
{
    http::StatusCode::Value statusCode;

    /**
     * Headers to be added to the response message before sending.
     * These headers override those that are added automatically (e.g., Server, Date, etc..).
     */
    HttpHeaders headers;

    /**
     * The response body.
     */
    std::unique_ptr<http::AbstractMsgBodySource> body;

    ConnectionEvents connectionEvents;

    RequestResult(StatusCode::Value statusCode);

    RequestResult(
        http::StatusCode::Value statusCode,
        std::unique_ptr<http::AbstractMsgBodySource> dataSource);

    RequestResult(
        http::StatusCode::Value statusCode,
        std::unique_ptr<http::AbstractMsgBodySource> dataSource,
        ConnectionEvents connectionEvents);

    RequestResult(
        StatusCode::Value statusCode,
        nx::network::http::HttpHeaders responseHeaders,
        std::unique_ptr<nx::network::http::AbstractMsgBodySource> msgBody);
};

using RequestProcessedHandler = nx::MoveOnlyFunc<void(RequestResult)>;

} // namespace nx::network::http
