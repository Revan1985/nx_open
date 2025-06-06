// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <nx/network/http/http_types.h>
#include <nx/network/jose/jwk.h>
#include <nx/utils/move_only_func.h>

#include "oauth_data.h"
#include "result_code.h"

namespace nx::cloud::db::api {

class OauthManager
{
public:
    static constexpr std::string_view kTokenPrefix = "nxcdb-";
    static constexpr std::string_view k2faRequiredError = "second_factor_required";
    static constexpr std::string_view k2faDisabledForUserError = "2fa_disabled_for_the_user";

    virtual ~OauthManager() = default;

    virtual std::chrono::seconds lastServerTime() const = 0;

    virtual void issueTokenLegacy(
        const IssueTokenRequest& request,
        nx::MoveOnlyFunc<void(ResultCode, IssueTokenResponse)> completionHandler) = 0;

    virtual void issueTokenV1(const IssueTokenRequest& request,
        nx::MoveOnlyFunc<void(ResultCode, IssueTokenResponse)> completionHandler) = 0;

    virtual void issueAuthorizationCode(
        const IssueTokenRequest& request,
        nx::MoveOnlyFunc<void(ResultCode, IssueCodeResponse)> completionHandler) = 0;

    virtual void legacyValidateToken(
        const std::string& token,
        nx::MoveOnlyFunc<void(ResultCode, ValidateTokenResponse)> completionHandler) = 0;

    // Get attributes and claims associated with the token.
    virtual void introspectToken(
        const TokenIntrospectionRequest& request,
        nx::MoveOnlyFunc<void(ResultCode, TokenIntrospectionResponse)> completionHandler) = 0;

    virtual void deleteToken(
        const std::string& token, nx::MoveOnlyFunc<void(ResultCode)> completionHandler) = 0;

    virtual void deleteTokensByClientId(
        const std::string& clientId,
        nx::MoveOnlyFunc<void(ResultCode)> completionHandler) = 0;

    virtual void logout(nx::MoveOnlyFunc<void(ResultCode)> completionHandler) = 0;

    virtual void issueStunToken(const IssueStunTokenRequest& request,
        nx::MoveOnlyFunc<void(ResultCode, IssueStunTokenResponse)> completionHandler) = 0;

    virtual void getJwtPublicKeys(
        nx::MoveOnlyFunc<void(ResultCode, std::vector<nx::network::jwk::Key>)> completionHandler) = 0;

    virtual void getJwtPublicKeyByKid(
        const std::string& kid,
        nx::MoveOnlyFunc<void(ResultCode, nx::network::jwk::Key)> completionHandler) = 0;
};

} // namespace nx::cloud::db::api
