// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "auth_session.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QUrlQuery>

#include <nx/fusion/model_functions.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_types.h>
#include <nx/network/socket_common.h>

namespace nx::network::rest {

namespace {

constexpr char kDelimiter = '$';

nx::Uuid idFromRequest(const nx::network::http::Request& request, const QUrlQuery& query)
{
    auto id = nx::Uuid::fromStringSafe(
        nx::network::http::getHeaderValue(request.headers, Qn::EC2_RUNTIME_GUID_HEADER_NAME));
    if (!id.isNull())
        return id;

    id = nx::Uuid::fromStringSafe(request.getCookieValue(Qn::EC2_RUNTIME_GUID_HEADER_NAME));
    if (!id.isNull())
        return id;

    id = nx::Uuid::fromStringSafe(
        query.queryItemValue(QLatin1String(Qn::EC2_RUNTIME_GUID_HEADER_NAME)));
    if (!id.isNull())
        return id;

    if (const auto nonce = request.getCookieValue("auth"); !nonce.empty())
    {
        return nx::Uuid::fromRfc4122(QCryptographicHash::hash(
            {nonce.data(), (qsizetype) nonce.size()}, QCryptographicHash::Md5));
    }

    return id;
}

} // namespace

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AuthSession, (ubjson)(json)(sql_record), AuthSession_Fields)

AuthSession::AuthSession(
    nx::Uuid id_,
    const QString& userName,
    const nx::network::http::Request& request,
    const nx::network::HostAddress& hostAddress,
    bool authHeaderTrusted)
    :
    id(std::move(id_)),
    userName(userName),
    userHost(authHeaderTrusted
        ? QString::fromStdString(
            nx::network::http::getHeaderValue(request.headers, Qn::USER_HOST_HEADER_NAME))
        : QString())
{
    QString existSession = QString::fromStdString(
        nx::network::http::getHeaderValue(request.headers, Qn::AUTH_SESSION_HEADER_NAME));
    if (!existSession.isEmpty())
    {
        fromString(existSession);
        return;
    }

    const QUrlQuery query(request.requestLine.url.query());
    if (id.isNull())
    {
        id = idFromRequest(request, query);
        if (id.isNull())
        {
            id = nx::Uuid::createUuid();
            isAutoGenerated = true;
        }
    }

    if (userHost.isEmpty())
        userHost = hostAddress.toString().c_str();

    userAgent = query.queryItemValue(QLatin1String(Qn::USER_AGENT_HEADER_NAME));
    if (userAgent.isEmpty())
    {
        userAgent = QString::fromStdString(
            nx::network::http::getHeaderValue(request.headers, Qn::USER_AGENT_HEADER_NAME));
    }
    if (int trimmedPos = userAgent.indexOf('/'); trimmedPos != -1)
    {
        trimmedPos = userAgent.indexOf(' ', trimmedPos);
        userAgent = userAgent.left(trimmedPos);
    }
}

QString AuthSession::toString() const
{
    const auto encoded = [](QString value) { return value.replace(kDelimiter, char('_')); };
    QString result;
    result.append(id.toString(QUuid::WithBraces));
    result.append(kDelimiter);
    result.append(encoded(userName));
    result.append(kDelimiter);
    result.append(encoded(userHost));
    result.append(kDelimiter);
    result.append(encoded(userAgent));

    return result;
}

void AuthSession::fromString(const QString& data)
{
    QStringList params = data.split(kDelimiter);
    if (params.size() > 0)
        id = nx::Uuid(params[0]);
    if (params.size() > 1)
        userName = params[1];
    if (params.size() > 2)
        userHost = params[2];
    if (params.size() > 3)
        userAgent = params[3];
}

void serialize_field(const AuthSession& authData, QVariant* target)
{
    serialize_field(authData.toString(), target);
}

void deserialize_field(const QVariant& value, AuthSession* target)
{
    QString tmp;
    deserialize_field(value, &tmp);
    target->fromString(tmp);
}

} // namespace nx::network::rest
