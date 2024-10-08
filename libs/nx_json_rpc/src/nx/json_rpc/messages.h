// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <string>
#include <variant>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/serialization/json.h>
#include <nx/reflect/instrument.h>

// JSON RPC 2.0 Specification: https://www.jsonrpc.org/specification

namespace nx::json_rpc {

using ResponseId = std::variant<QString, int, std::nullptr_t>;

struct NX_JSON_RPC_API Request
{
    /**%apidoc
     * %value "2.0"
     */
    std::string jsonrpc = "2.0";

    /**%apidoc
     * %example rest.v{1-}.servers.info.list
     */
    std::string method;

    /**%apidoc:{std::variant<QJsonObject, QJsonArray>} */
    std::optional<QJsonValue> params;

    std::optional<std::variant<QString, int>> id;

    template<typename T>
    T parseOrThrow() const
    {
        if (!params)
        {
            throw QJson::InvalidParameterException(
                std::pair<QString, QString>("params", "Must be specified"));
        }

        QnJsonContext context;
        context.setStrictMode(true);
        if (method.starts_with("rest."))
            context.setSerializeMapToObject(true);

        return QJson::deserializeOrThrow<T>(&context, *params);
    }

    ResponseId responseId() const
    {
        ResponseId result{std::nullptr_t()};
        if (id)
        {
            if (std::holds_alternative<int>(*id))
                result = std::get<int>(*id);
            else
                result = std::get<QString>(*id);
        }
        return result;
    }
};
#define JsonRpcRequest_Fields (jsonrpc)(method)(params)(id)
QN_FUSION_DECLARE_FUNCTIONS(Request, (json), NX_JSON_RPC_API)
NX_REFLECTION_INSTRUMENT(Request, JsonRpcRequest_Fields);

struct NX_JSON_RPC_API Error
{
    // Standard defined error codes for `code` field.
    // Standard codes: https://www.jsonrpc.org/specification#error_object
    // XML-RPC: https://xmlrpc-epi.sourceforge.net/specs/rfc.fault_codes.php
    enum Code
    {
        parseError = -32700,
        encodingError = -32701,
        charsetError = -32702,

        invalidRequest = -32600,
        methodNotFound = -32601,
        invalidParams = -32602,
        internalError = -32603,

        applicationError = -32500,
        systemError = -32400,
        transportError = -32300,

        serverErrorBegin = -32099,
        serverErrorEnd = -32000,

        reservedErrorBegin = -32768,
        reservedErrorEnd = -32000,
    };

    int code = systemError;
    std::string message;
    std::optional<QJsonValue> data;

    Error() = default;
    Error(int code, std::string message, std::optional<QJsonValue> data = {}):
        code(code), message(std::move(message)), data(std::move(data)) {}
};
#define JsonRpcError_Fields (code)(message)(data)
QN_FUSION_DECLARE_FUNCTIONS(Error, (json), NX_JSON_RPC_API)
NX_REFLECTION_INSTRUMENT(Error, JsonRpcError_Fields);

struct NX_JSON_RPC_API Response
{
    /**%apidoc
     * %value "2.0"
     */
    std::string jsonrpc = "2.0";

    // TODO: Use `JsonRpcResponseId` when apidoctool will support `using`.
    std::variant<QString, int, std::nullptr_t> id{std::nullptr_t()};
    std::optional<QJsonValue> result;
    std::optional<Error> error;

    static Response makeError(ResponseId id, Error error);

    template<typename T>
    static Response makeResult(ResponseId id, T&& data)
    {
        const auto serialized =
            [](auto&& data)
            {
                QJsonValue value;
                QnJsonContext context;
                context.setChronoSerializedAsDouble(true);
                context.setSerializeMapToObject(true);
                QJson::serialize(&context, data, &value);
                return value;
            };
        return {.id = id, .result = serialized(std::move(data))};
    }

    template<typename T>
    T resultOrThrow(bool isSerializeMapToObject = true) const
    {
        if (!result)
        {
            throw QJson::InvalidParameterException(
                std::pair<QString, QString>("result", "Must be provided"));
        }

        QnJsonContext context;
        context.setStrictMode(true);
        if (isSerializeMapToObject)
            context.setSerializeMapToObject(true);
        return QJson::deserializeOrThrow<T>(&context, *result);
    }
};
#define JsonRpcResponse_Fields (jsonrpc)(id)(result)(error)
QN_FUSION_DECLARE_FUNCTIONS(Response, (json), NX_JSON_RPC_API)
NX_REFLECTION_INSTRUMENT(Response, JsonRpcResponse_Fields);

} // namespace nx::json_rpc
