// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <nx/utils/std_string_utils.h>

namespace nx::cloud::db::api {

enum class AccountStatus
{
    invalid = 0,
    awaitingEmailConfirmation = 1,
    activated = 2,
    blocked = 3,
    invited = 4,
};

struct AccountRegistrationSecuritySettings
{
    /**%apidoc Enable/disable HTTP Digest authentication. The HTTP Digest requires storing
     * of MD5(email:realm:password).
     */
    std::optional<bool> httpDigestAuthEnabled;
};

struct AccountRegistrationSettings
{
    /**%apidoc Account security options. */
    AccountRegistrationSecuritySettings security;
};

struct AccountRegistrationData
{
    /**%apidoc Account email. Used as unique user id. */
    std::string email;

    /**%apidoc
     * %deprecated Subject for removal.
     */
    std::string passwordHa1;

    /**%apidoc Account password (plain text). */
    std::string password;

    /**%apidoc Account owner's full name. Deprecated. Use `firstName` and `lastName` instead. */
    std::string fullName;

    /**%apidoc Account owner's first name. */
    std::string firstName;

    /**%apidoc Account owner's last name. */
    std::string lastName;

    std::string customization;

    /**%apidoc Account security options. */
    AccountRegistrationSettings settings;
};

enum class PasswordHashType
{
    invalid = 0,
    ha1md5 = 1,
    scrypt = 2,
};

struct PasswordHash
{
    std::string hash;
    std::unordered_map<std::string, std::string> attributes;
};

struct AccountData
{
    /**%apidoc[readonly] Account id. */
    std::string id;

    /**%apidoc[readonly] User email. Unique for each account. */
    std::string email;

    std::unordered_map<PasswordHashType, PasswordHash> passwordHashes;

    /**%apidoc[readonly] Account owner's full name. Deprecated.
     * Use `firstName` and `lastName` instead.
     */
    std::string fullName;

    /**%apidoc[readonly] Account owner's first name. */
    std::string firstName;

    /**%apidoc[readonly] Account owner's last name. */
    std::string lastName;

    std::string customization;

    /**%apidoc[readonly] Account status. */
    AccountStatus statusCode = AccountStatus::invalid;

    /**%apidoc[readonly] The account registration timestamp (UTC). */
    std::chrono::system_clock::time_point registrationTime;

    /**%apidoc[readonly] The account activation timestamp (UTC).
     * Zero, if the account has not been activated yet.
     */
    std::chrono::system_clock::time_point activationTime;

    /**%apidoc[readonly] Shows if HTTP Digest authentication is allowed for this account.
     * Cannot be enabled simultaneously with the 2FA.
     */
    bool httpDigestAuthEnabled = true;

    /**%apidoc[readonly] Shows if two-factor authentication is required for this account. */
    bool account2faEnabled = false;

    /**%apidoc[readonly] Shows if two-factor authentication key was generated for the account.
     * Set in cross-service requests only
     */
    std::optional<bool> mfaKeyGenerated;

    /**%apidoc Maximum authentication session lifetime. */
    std::optional<std::chrono::seconds> authSessionLifetime;

    /**%apidoc[readonly] Shows if this account belongs to an organization. */
    std::optional<bool> accountBelongsToOrganization;

    /**%apidoc[readonly] Incremented each time critical security attributes are changed.
     * E.g., user password was changed, or user has chosen to log out from all devices.
     */
    int securitySequence = 0;

    /**%apidoc[readonly] The AWS region to which the user belongs. */
    std::string accountLocation;

    /**%apidoc Account locale.
     * ISO 639-1 locale code paired with ISO 3166-1 alpha-2 country code via underscore: `en_GB`, `fr_FR` and so on.
     */
    std::string locale;
};

struct AccountStatusData
{
    /**%apidoc[readonly] Account status. */
    AccountStatus statusCode = AccountStatus::invalid;
};

struct AccountConfirmationCode
{
    /**%apidoc Account confirmation code. */
    std::string code;
};

struct AccountUpdateData
{
    /**%apidoc New account password. */
    std::optional<std::string> password;

    /**%apidoc Current account password. */
    std::optional<std::string> currentPassword;

    /**%apidoc Account owner's full name. Deprecated. Use `firstName` and `lastName` instead. */
    std::optional<std::string> fullName;

    /**%apidoc Account owner's first name. */
    std::optional<std::string> firstName;

    /**%apidoc Account owner's last name. */
    std::optional<std::string> lastName;

    std::optional<std::string> customization;

    /**%apidoc
     * %deprecated Replaced by `password` attribute.
     */
    std::optional<std::string> passwordHa1;

    /**%apidoc
     * %deprecated Replaced by `mfaCode` attribute.
     * One-time password from the authenticator app.
     */
    std::optional<std::string> totp;

    /**%apidoc One-time password from the authenticator app or any other source. */
    std::optional<std::string> mfaCode;

    /**%apidoc Account locale.
     * ISO 639-1 locale code paired with ISO 3166-1 alpha-2 country code via underscore: `en_GB`, `fr_FR` and so on.
     */
    std::optional<std::string> locale;
};

struct AccountOrganizationAttrs
{
    /**%apidoc Shows if this account belongs to an organization. */
    bool belongsToSomeOrganization = false;
};

// Holder structure to send just email in a response.
struct AccountEmail
{
    /**%apidoc Account email. */
    std::string email;
};

struct PasswordResetRequest
{
    /**%apidoc Account email. */
    std::string email;

    /**%apidoc Customization to use for password reset notification. */
    std::optional<std::string> customization = std::nullopt;
};

struct TemporaryCredentialsTimeouts
{
    /**%apidoc Expiration timeout.
     * When automatic prolongation is enabled this period works a bit different:
     * credentials expire if not used for this timeout.
     */
    std::chrono::seconds expirationPeriod = std::chrono::seconds::zero();

    /**%apidoc Enable/disable automatic credentials prolongation.
     * if true, then credentials expiration time is set to currentTime() + prolongationPeriod
     * on each request.
     */
    bool autoProlongationEnabled = false;

    /**%apidoc This value is use to prolongate credentials.
     */
    std::chrono::seconds prolongationPeriod = std::chrono::seconds::zero();
};

struct TemporaryCredentialsParams
{
    /**%apidoc Type of key to issue.
     * The following types are defined:
     * - short
     * - long
     * If empty, then timeouts field has to be specified.
     */
    std::string type;

    /**%apidoc Custom expiration settings.
     * Ignored if type is non-empty.
     */
    TemporaryCredentialsTimeouts timeouts;
};

struct TemporaryCredentials
{
    /**%apidoc Temporary login.
     * Can only be used with the password from the same response.
     */
    std::string login;

    /**%apidoc Temporary password.
     * Can only be used with the login from the same response.
     */
    std::string password;

    /**%apidoc Expiration parameters of the credentials. */
    TemporaryCredentialsTimeouts timeouts;
};

struct AccountSecuritySettings
{
    /**%apidoc Enable/disable HTTP Digest authentication for the account.
     * MUST be accompanied by password.
     */
    std::optional<bool> httpDigestAuthEnabled;

    /**%apidoc Enable/disable two-factor authentication for the account.
     * MUST be accompanied by valid totp field.
     */
    std::optional<bool> account2faEnabled;

    /**%apidoc
     * %deprecated Replaced by `mfaCode` attribute.
     * One-time password from the authenticator app.
     */
    std::optional<std::string> totp;

    /**%apidoc One-time password from the authenticator app.
     * Required and MUST be valid if account2faEnabled is specified.
     */
    std::optional<std::string> mfaCode;

    /**%apidoc Account password. Always required except updating 2fa settings. */
    std::optional<std::string> password;

    /**%apidoc Only used in get requests. True if a totp key was generated for the account. */
    std::optional<bool> totpExistsForAccount;

    /**%apidoc Maximum authentication session lifetime. */
    std::optional<std::chrono::seconds> authSessionLifetime;
};

struct AccountForSharingRequest
{
    std::optional<std::string> nonce;
};

struct AccountForSharing
{
    std::string id;
    std::string email;
    std::string fullName;
    std::string firstName;
    std::string lastName;
    AccountStatus statusCode = AccountStatus::invalid;
    bool account2faEnabled = false;
    std::string intermediateResponse;
    bool remote = false;
    std::string locale;
};

struct AccountNotificationFilterSettings
{
    /**%apidoc List of disabled events. */
    std::set<std::string> eventFilter;

    /**%apidoc List of disabled messages. */
    std::set<std::string> messageFilter;
};

inline std::string makeFullName(std::string_view firstName, std::string_view lastName)
{
    if (firstName.empty() && lastName.empty())
        return "";
    if (firstName.empty())
        return std::string(lastName);
    if (lastName.empty())
        return std::string(firstName);
    return std::string(firstName) + ' ' + std::string(lastName);
}

inline std::pair<std::string, std::string> splitFullName(std::string_view fullName)
{
    auto [nameParts, count] = nx::utils::split_n<2>(fullName, ' ');
    if (count <= 1)
        return std::pair<std::string, std::string>(fullName, "");
    return std::pair<std::string, std::string>(nameParts[0], nameParts[1]);
}

} // namespace nx::cloud::db::api
