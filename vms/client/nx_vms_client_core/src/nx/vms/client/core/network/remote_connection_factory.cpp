// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_connection_factory.h"

#include <memory>

#include <QtCore/QPointer>

#include <network/system_helpers.h>
#include <nx/cloud/db/api/oauth_manager.h>
#include <nx/network/address_resolver.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/nx_network_ini.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/reflect/to_string.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/thread/thread_util.h>
#include <nx/vms/api/data/login.h>
#include <nx/vms/api/data/user_model.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/network/cloud_auth_data.h>
#include <nx/vms/common/network/server_compatibility_validator.h>
#include <nx/vms/common/resource/server_host_priority.h>
#include <nx_ec/ec_api_fwd.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>
#include <utils/email/email.h>

#include "abstract_remote_connection_factory_requests.h"
#include "certificate_verifier.h"
#include "network_manager.h"
#include "private/remote_connection_factory_cache.h"
#include "remote_connection.h"
#include "remote_connection_user_interaction_delegate.h"

using namespace ec2;
using ServerCompatibilityValidator = nx::vms::common::ServerCompatibilityValidator;

namespace nx::vms::client::core {

namespace {

static const nx::utils::SoftwareVersion kSimplifiedLoginSupportVersion(5, 1);
static const nx::utils::SoftwareVersion kUserRightsRedesignVersion(6, 0);

/**
 * Digest authentication requires username to be in lowercase.
 */
void ensureUserNameIsLowercaseIfDigest(nx::network::http::Credentials& credentials)
{
    if (credentials.authToken.isPassword())
    {
        const QString username = QString::fromStdString(credentials.username);
        credentials.username = username.toLower().toStdString();
    }
}

RemoteConnectionErrorCode toErrorCode(ServerCompatibilityValidator::Reason reason)
{
    switch (reason)
    {
        case ServerCompatibilityValidator::Reason::binaryProtocolVersionDiffers:
            return RemoteConnectionErrorCode::binaryProtocolVersionDiffers;

        case ServerCompatibilityValidator::Reason::cloudHostDiffers:
            return RemoteConnectionErrorCode::cloudHostDiffers;

        case ServerCompatibilityValidator::Reason::customizationDiffers:
            return RemoteConnectionErrorCode::customizationDiffers;

        case ServerCompatibilityValidator::Reason::versionIsTooLow:
            return RemoteConnectionErrorCode::versionIsTooLow;
    }

    NX_ASSERT(false, "Should never get here");
    return RemoteConnectionErrorCode::internalError;
}

std::optional<std::string> publicKey(const std::string& pem)
{
    if (pem.empty())
        return {};

    auto chain = nx::network::ssl::Certificate::parse(pem);
    if (chain.empty())
        return {};

    return chain[0].publicKey();
};

std::optional<nx::Uuid> deductServerId(const std::vector<nx::vms::api::ServerInformationV1>& info)
{
    if (info.size() == 1)
        return info.begin()->id;

    for (const auto& server: info)
    {
        if (server.collectedByThisServer)
            return server.id;
    }

    return std::nullopt;
}

nx::Url mainServerUrl(const QSet<QString>& remoteAddresses, int port)
{
    std::vector<nx::Url> addresses;
    for (const auto& addressString: remoteAddresses)
    {
        nx::network::SocketAddress sockAddr(addressString.toStdString());

        nx::Url url = nx::network::url::Builder()
            .setScheme(nx::network::http::kSecureUrlSchemeName)
            .setEndpoint(sockAddr)
            .toUrl();
        if (url.port() == 0)
            url.setPort(port);

        addresses.push_back(url);
    }

    if (addresses.empty())
        return {};

    return *std::min_element(addresses.cbegin(), addresses.cend(),
        [](const auto& l, const auto& r)
        {
            using namespace nx::vms::common;
            return serverHostPriority(l.host()) < serverHostPriority(r.host());
        });
}

void tryUpdateUsernameIfEmpty(nx::network::http::Credentials& credentials)
{
    if (!credentials.username.empty() || !credentials.authToken.isBearerToken())
        return;

    credentials.username = usernameFromToken(credentials.authToken.value);

    NX_DEBUG(NX_SCOPE_TAG, "Username is empty, decoded from token: %1", credentials.username);
}

} // namespace

using WeakContextPtr = std::weak_ptr<RemoteConnectionFactory::Context>;

struct RemoteConnectionFactory::Private
{
    using RequestsManager = AbstractRemoteConnectionFactoryRequestsManager;

    ec2::AbstractECConnectionFactory* q;
    const nx::vms::api::PeerType peerType;
    const Qn::SerializationFormat serializationFormat;
    QPointer<CertificateVerifier> certificateVerifier;
    CloudCredentialsProvider cloudCredentialsProvider;
    std::shared_ptr<RequestsManager> requestsManager;
    std::unique_ptr<AbstractRemoteConnectionUserInteractionDelegate> userInteractionDelegate;

    Private(
        ec2::AbstractECConnectionFactory* q,
        nx::vms::api::PeerType peerType,
        Qn::SerializationFormat serializationFormat,
        CertificateVerifier* certificateVerifier,
        CloudCredentialsProvider cloudCredentialsProvider,
        std::shared_ptr<RequestsManager> requestsManager)
        :
        q(q),
        peerType(peerType),
        serializationFormat(serializationFormat),
        certificateVerifier(certificateVerifier),
        cloudCredentialsProvider(std::move(cloudCredentialsProvider)),
        requestsManager(std::move(requestsManager))
    {
        NX_ASSERT(this->cloudCredentialsProvider.getCredentials);
        NX_ASSERT(this->cloudCredentialsProvider.getLogin);
        NX_ASSERT(this->cloudCredentialsProvider.getDigestPassword);
        NX_ASSERT(this->cloudCredentialsProvider.is2FaEnabledForUser);
    }

    RemoteConnectionPtr makeRemoteConnectionInstance(ContextPtr context)
    {
        ConnectionInfo connectionInfo{
            .address = context->address(),
            .credentials = context->credentials(),
            .userType = context->userType(),
            .compatibilityUserModel = context->compatibilityUserModel};

        return std::make_shared<RemoteConnection>(
            peerType,
            context->moduleInformation,
            connectionInfo,
            context->sessionTokenExpirationTime,
            context->certificateCache,
            serializationFormat,
            context->auditId,
            context->systemContext);
    }

    bool executeInUiThreadSync(
        ContextPtr context,
        std::function<bool(AbstractRemoteConnectionUserInteractionDelegate* delegate)> handler)
    {
        using namespace std::chrono_literals;

        auto isAccepted = std::make_shared<std::promise<bool>>();
        auto delegate = context->customUserInteractionDelegate
            ? context->customUserInteractionDelegate.get()
            : userInteractionDelegate.get();

        // Capture context to ensure delegate will not be destroyed by the time handler is run.
        executeInThread(delegate->thread(),
            [context, isAccepted, handler, delegate]()
            {
                isAccepted->set_value(handler(delegate));
            });

        std::future<bool> future = isAccepted->get_future();
        std::future_status status;
        do {
            status = future.wait_for(100ms);
            if (context->terminated)
                return false;

        } while (status != std::future_status::ready);

        return future.get();
    }

    void logInitialInfo(ContextPtr context)
    {
        NX_DEBUG(this, "Initialize connection process.");
        if (!NX_ASSERT(context))
            return;

        NX_DEBUG(this, "Url %1, purpose %2",
            context->logonData.address,
            nx::reflect::toString(context->logonData.purpose));

        NX_DEBUG(this, "User name: %1, type: %2",
            context->logonData.credentials.username, context->logonData.userType);

        if (context->expectedServerId())
            NX_DEBUG(this, "Expect Server ID %1.", *context->expectedServerId());
        else
            NX_DEBUG(this, "Server ID is not known.");

        if (context->expectedServerVersion())
            NX_DEBUG(this, "Expect Server version %1.", *context->expectedServerVersion());
        else
            NX_DEBUG(this, "Server version is not known.");

        if (context->isCloudConnection())
        {
            if (context->expectedCloudSystemId())
                NX_DEBUG(this, "Expect Cloud connect to %1.", *context->expectedCloudSystemId());
            else
                NX_ASSERT(this, "Expect Cloud connect but the System ID is not known yet.");
        }
    }

    bool needToRequestModuleInformationFirst(ContextPtr context) const
    {
        return context && !context->isRestApiSupported();
    };

    /** Check whether System supports REST API by it's version (which must be known already). */
    bool systemSupportsRestApi(ContextPtr context) const
    {
        return context && context->isRestApiSupported();
    }

    /**
     * Validate expected Server ID and store received info in the context.
     */
    void getModuleInformation(ContextPtr context) const
    {
        if (!context)
            return;

        const auto reply = requestsManager->getModuleInformation(context);
        if (context->failed())
            return;

        NX_DEBUG(this, "Fill context info from module information reply.");
        context->handshakeCertificateChain = reply.handshakeCertificateChain;
        context->moduleInformation = reply.moduleInformation;

        // Check whether actual server id matches the one we expected to get (if any).
        if (context->expectedServerId()
            && context->expectedServerId() != context->moduleInformation.id)
        {
            context->setError(RemoteConnectionErrorCode::unexpectedServerId);
            return;
        }

        context->logonData.expectedServerId = reply.moduleInformation.id;
        context->logonData.expectedServerVersion = reply.moduleInformation.version;
        context->logonData.expectedCloudSystemId = reply.moduleInformation.cloudSystemId;

        if (reply.moduleInformation.id.isNull()
            || reply.moduleInformation.version.isNull())
        {
            context->setError(RemoteConnectionErrorCode::networkContentError);
        }
    }

    void getServersInfo(ContextPtr context)
    {
        if (!context)
            return;

        const auto reply = requestsManager->getServersInfo(context);
        if (context->failed())
            return;

        NX_DEBUG(this, "Fill context info from servers info reply.");
        context->handshakeCertificateChain = reply.handshakeCertificateChain;
        context->serversInfo = reply.serversInfo;
        if (reply.serversInfo.empty())
        {
            context->setError(RemoteConnectionErrorCode::networkContentError);
            return;
        }

        if (!context->expectedServerId())
        {
            // Try to deduct server id for the 5.1 Systems or Systems with one server.
            context->logonData.expectedServerId = deductServerId(reply.serversInfo);
        }

        if (!context->expectedCloudSystemId())
        {
            NX_DEBUG(this, "Fill Cloud System ID from servers info");
            context->logonData.expectedCloudSystemId = reply.serversInfo[0].cloudSystemId;
        }
    }

    void ensureExpectedServerId(ContextPtr context)
    {
        if (!context || context->expectedServerId())
            return;

        // We can connect to 5.0 System without knowing actual server id, so we need to get it.
        NX_DEBUG(this, "Cannot deduct Server ID, requesting it additionally.");

        // GET /api/moduleInformation.
        const auto reply = requestsManager->getModuleInformation(context);
        if (!context->failed())
            context->logonData.expectedServerId = reply.moduleInformation.id;
    }

    void processServersInfoList(ContextPtr context)
    {
        if (!context)
            return;

        if (!NX_ASSERT(context->expectedServerId()))
        {
            context->setError(RemoteConnectionErrorCode::internalError);
            return;
        }

        auto currentServerIt = std::find_if(
            context->serversInfo.cbegin(),
            context->serversInfo.cend(),
            [id = *context->expectedServerId()](const auto& server) { return server.id == id; });

        if (currentServerIt == context->serversInfo.cend())
        {
            NX_WARNING(
                this,
                "Server info list does not contain Server %1.",
                *context->expectedServerId());
            context->setError(RemoteConnectionErrorCode::networkContentError);
            return;
        }

        context->moduleInformation = currentServerIt->getModuleInformation();

        // Check that the handshake certificate matches one of the targets's.
        CertificateVerificationResult certificateVerificationResult =
            verifyHandshakeCertificateChain(context->handshakeCertificateChain,
                *currentServerIt,
                context->logonData.address.address.toString());
        if (!certificateVerificationResult.success)
        {
            context->setError(RemoteConnectionErrorCode::certificateRejected);
            return;
        }

        // Prepare the list of mismatched certificates.
        CertificateVerifier::Status targetStatus = verifyTargetCertificate(context);
        QList<TargetCertificateInfo> mismatchedCertificates = collectMismatchedCertificates(context);
        bool targetIsMismatched =
            mismatchedCertificates.contains(getTargetCertificateInfo(context));

        // If the target server certificate has been successfully validated by OS, it should be
        // removed from the list of certificates with errors.
        if (targetStatus == CertificateVerifier::Status::ok && targetIsMismatched)
            mismatchedCertificates.removeAll(getTargetCertificateInfo(context));

        // If the target certificate is expired, it should be added to the list of certificates
        // with errors, even if it matches the pinned one. Note: the current implementation of
        // CertificateVerifier reports expired certificates as "mismatch", but perhaps it should
        // be refactored.
        if (targetStatus == CertificateVerifier::Status::mismatch && !targetIsMismatched)
            mismatchedCertificates.prepend(getTargetCertificateInfo(context));

        bool accepted = true;

        if (!mismatchedCertificates.isEmpty())
        {
            // There are some certificates with errors in the target System.
            auto accept =
                [mismatchedCertificates = std::move(mismatchedCertificates)]
                    (AbstractRemoteConnectionUserInteractionDelegate* delegate)
                {
                    return delegate->acceptCertificatesOfServersInTargetSystem(mismatchedCertificates);
                };

            accepted = context->logonData.userInteractionAllowed
                && executeInUiThreadSync(context, accept);
        }
        else if (targetStatus == CertificateVerifier::Status::notFound)
        {
            // There are no errors, but the target Server is unknown.
            auto accept =
                [this, context]
                    (AbstractRemoteConnectionUserInteractionDelegate* delegate)
                {
                    return delegate->acceptNewCertificate(getTargetCertificateInfo(context));
                };

            accepted = context->logonData.userInteractionAllowed
                && executeInUiThreadSync(context, accept);
        }

        if (!accepted)
            context->setError(RemoteConnectionErrorCode::certificateRejected);

        if (!context->failed())
            storeCertificates(context);
    }

    bool checkCompatibility(ContextPtr context)
    {
        if (context)
        {
            NX_DEBUG(this, "Checking compatibility");
            if (const auto incompatibilityReason = ServerCompatibilityValidator::check(
                context->moduleInformation, context->logonData.purpose))
            {
                context->setError(toErrorCode(*incompatibilityReason));
            }
            else if (context->moduleInformation.isNewSystem())
            {
                context->setError(RemoteConnectionErrorCode::factoryServer);
            }
        }
        return context && !context->failed();
    }

    TargetCertificateInfo getTargetCertificateInfo(ContextPtr context)
    {
        return TargetCertificateInfo{
            context->moduleInformation,
            context->address(),
            context->handshakeCertificateChain};
    }

    CertificateVerifier::Status verifyTargetCertificate(ContextPtr context)
    {
        if (!nx::network::ini().verifyVmsSslCertificates)
            return CertificateVerifier::Status::ok;

        NX_DEBUG(this, "Verify target certificate");
        // Make sure factory is setup correctly.
        if (!NX_ASSERT(context)
            || !NX_ASSERT(certificateVerifier)
            || !NX_ASSERT(userInteractionDelegate))
        {
            context->setError(RemoteConnectionErrorCode::internalError);
            return CertificateVerifier::Status::mismatch; //< Worst option.
        }

        const CertificateVerifier::Status status = certificateVerifier->verifyCertificate(
            context->moduleInformation.id,
            context->handshakeCertificateChain);

        if (status == CertificateVerifier::Status::ok)
            return status;

        const auto serverName = context->address().address.toString();
        NX_DEBUG(
            this,
            "New certificate has been received from %1. "
            "Trying to verify it by system CA certificates.",
            serverName);

        std::string errorMessage;
        if (NX_ASSERT(!context->handshakeCertificateChain.empty())
            && nx::network::ssl::verifyBySystemCertificates(
                context->handshakeCertificateChain, serverName, &errorMessage))
        {
            NX_DEBUG(this, "Certificate verification for %1 is successful.", serverName);
            return CertificateVerifier::Status::ok;
        }

        NX_DEBUG(this, errorMessage);
        return status;
    }

    void verifyAndAcceptTargetCertificate(ContextPtr context, bool certificateIsUserProvided)
    {
        auto status = verifyTargetCertificate(context);

        auto pinTargetServerCertificate =
            [this, context, certificateIsUserProvided]
            {
                if (!NX_ASSERT(!context->handshakeCertificateChain.empty()))
                    return;

                // Server certificates are checked in checkServerCertificateEquality() using
                // REST API. After that we know exactly which type of certificate is used on
                // SSL handshake. For old servers that don't support REST API the certificate
                // is stored as auto-generated.
                certificateVerifier->pinCertificate(
                    context->moduleInformation.id,
                    context->handshakeCertificateChain[0].publicKey(),
                    certificateIsUserProvided
                        ? CertificateVerifier::CertificateType::connection
                        : CertificateVerifier::CertificateType::autogenerated);
            };

        auto accept =
            [this, status, context](AbstractRemoteConnectionUserInteractionDelegate* delegate)
            {
                if (status == CertificateVerifier::Status::notFound)
                {
                    return delegate->acceptNewCertificate(getTargetCertificateInfo(context));
                }
                else if (status == CertificateVerifier::Status::mismatch)
                {
                    return delegate->acceptCertificateAfterMismatch(TargetCertificateInfo{
                        context->moduleInformation,
                        context->address(),
                        context->handshakeCertificateChain});
                }
                return true; //< Status is ok.
            };

        if ((status == CertificateVerifier::Status::ok)
            || (context->logonData.userInteractionAllowed
                && executeInUiThreadSync(context, accept)))
        {
            pinTargetServerCertificate();
        }
        else
        {
            // User rejected the certificate.
            context->setError(RemoteConnectionErrorCode::certificateRejected);
        }
    }

    void fillCloudConnectionCredentials(ContextPtr context,
        const nx::utils::SoftwareVersion& serverVersion)
    {
        if (!NX_ASSERT(context))
            return;

        if (!NX_ASSERT(context->userType() == nx::vms::api::UserType::cloud))
            return;

        auto& credentials = context->logonData.credentials;

        // Use refresh token to issue new session token if server supports OAuth cloud
        // authorization through the REST API.
        if (serverVersion >= RemoteConnection::kRestApiSupportVersion)
        {
            credentials = cloudCredentialsProvider.getCredentials();
        }
        // Current or stored credentials will be passed to compatibility mode client.
        else if (credentials.authToken.empty())
        {
            context->logonData.credentials.username =
                cloudCredentialsProvider.getLogin();
            if (!context->logonData.credentials.authToken.isPassword()
                || context->logonData.credentials.authToken.value.empty())
            {
                context->logonData.credentials.authToken =
                    nx::network::http::PasswordAuthToken(
                        cloudCredentialsProvider.getDigestPassword());
            }
        }
    }

    /**
     * For cloud connections we can get url, containing only system id. In that case each request
     * may potentially be sent to another server, which may lead to undefined behavior. So we are
     * replacing generic url with the exact server's one.
     */
    void pinCloudConnectionAddressIfNeeded(ContextPtr context)
    {
        if (!context || !context->isCloudConnection())
            return;

        const std::string hostname = context->address().address.toString();
        if (nx::network::SocketGlobals::addressResolver().isCloudHostname(hostname))
        {
            const auto fullHostname = context->moduleInformation.id.toSimpleString()
                + '.'
                + context->moduleInformation.cloudSystemId;
            context->logonData.address.address = fullHostname;
            NX_DEBUG(this, "Fixed connection address: %1 -> %2", hostname, fullHostname);
        }
    }

    void loginWithCloudToken(ContextPtr context)
    {
        if (!context)
            return;

        const auto currentSession = requestsManager->getCurrentSession(context);

        // Assuming server cloud connection problems if fresh cloud token is unauthorized
        if (context->failed() && context->error() == RemoteConnectionErrorCode::sessionExpired)
        {
            context->rewriteError(RemoteConnectionErrorCode::cloudUnavailableOnServer);
            return;
        }

        if (!context->failed())
        {
            NX_VERBOSE(this, "Received session info, user: %1, expires in: %2",
                currentSession.username, currentSession.expiresInS);

            const auto tokenExpirationTime =
                qnSyncTime->currentTimePoint() + currentSession.expiresInS;

            if (!context->sessionTokenExpirationTime
                || context->sessionTokenExpirationTime > tokenExpirationTime)
            {
                context->sessionTokenExpirationTime = tokenExpirationTime;
            }
        }
    }

    /**
     * Systems before 6.0 use old permissions model, where resource access is controlled by global
     * permissions and there is a special `isAdmin` field.
     */
    bool isOldPermissionsModelUsed(ContextPtr context)
    {
        if (!context)
            return false;

        if (!context->moduleInformation.version.isNull())
            return context->moduleInformation.version < kUserRightsRedesignVersion;

        return context->expectedServerVersion()
            && *context->expectedServerVersion() < kUserRightsRedesignVersion;
    }

    void loginWithDigest(ContextPtr context)
    {
        if (context)
        {
            ensureUserNameIsLowercaseIfDigest(context->logonData.credentials);
            requestsManager->checkDigestAuthentication(
                context, cloudCredentialsProvider.is2FaEnabledForUser());
        }
    }

    bool canLoginWithSavedToken(ContextPtr context) const
    {
        return context
            && context->credentials().authToken.isBearerToken()
            && !context->credentials().authToken.value.starts_with(api::TemporaryToken::kPrefix);
    }

    void loginWithToken(ContextPtr context)
    {
        if (!context)
            return;

        // In case token is outdated we will receive unauthorized here.
        nx::vms::api::LoginSession currentSession = requestsManager->getCurrentSession(context);
        if (!context->failed())
        {
            NX_VERBOSE(this, "Received session info, user: %1, expires in: %2",
                currentSession.username, currentSession.expiresInS);

            context->logonData.credentials.username = currentSession.username.toStdString();
            context->sessionTokenExpirationTime =
                qnSyncTime->currentTimePoint() + currentSession.expiresInS;
        }
    }

    nx::vms::api::LoginUser verifyLocalUserType(ContextPtr context)
    {
        if (context)
        {
            // Cloud users are not allowed to be here.
            if (!NX_ASSERT(context->userType() != nx::vms::api::UserType::cloud))
            {
                context->setError(RemoteConnectionErrorCode::internalError);
                return {};
            }

            // Username may be not passed if logging in as local or ldap user by token.
            if (context->credentials().username.empty())
            {
                if (const auto token = context->credentials().authToken; token.isBearerToken())
                {
                    if (token.value.starts_with(nx::vms::api::LoginSession::kTokenPrefix))
                        return {.type = context->logonData.userType};

                    if (token.value.starts_with(nx::vms::api::TemporaryToken::kPrefix))
                    {
                        context->logonData.userType = nx::vms::api::UserType::temporaryLocal;
                        return {.type = context->logonData.userType};
                    }
                }
            }

            nx::vms::api::LoginUser loginUserData = requestsManager->getUserType(context);
            if (context->failed())
                return {};

            // Check if expected user type does not match actual. Possible scenarios:
            // * Receive cloud user using the local tile - forbidden, throw error.
            // * Receive ldap user using the local tile - OK, updating actual user type.
            switch (loginUserData.type)
            {
                case nx::vms::api::UserType::cloud:
                    context->setError(RemoteConnectionErrorCode::loginAsCloudUserForbidden);
                    break;
                case nx::vms::api::UserType::ldap:
                    context->logonData.userType = nx::vms::api::UserType::ldap;
                    break;
                default:
                    break;
            }
            return loginUserData;
        }
        return {};
    }

    void processCloudToken(ContextPtr context)
    {
        if (!context || context->failed())
            return;

        NX_DEBUG(this, "Connecting as Cloud User, waiting for the access token.");
        if (!context->cloudToken.valid()) //< Cloud token request must be sent already.
        {
            context->setError(RemoteConnectionErrorCode::internalError);
            return;
        }

        const auto cloudTokenInfo = context->cloudToken.get();
        if (cloudTokenInfo.error)
        {
            context->setError(*cloudTokenInfo.error);
            return;
        }

        const auto& response = cloudTokenInfo.response;
        if (response.error)
        {
            NX_DEBUG(this, "Token response error: %1", *response.error);
            if (*response.error == nx::cloud::db::api::OauthManager::k2faRequiredError)
            {
                auto credentials = context->credentials();
                credentials.authToken =
                    nx::network::http::BearerAuthToken(response.access_token);

                auto validate =
                    [credentials = std::move(credentials)]
                        (AbstractRemoteConnectionUserInteractionDelegate* delegate)
                    {
                        return delegate->request2FaValidation(credentials);
                    };

                const bool validated = context->logonData.userInteractionAllowed
                    && executeInUiThreadSync(context, validate);

                if (validated)
                {
                    context->logonData.credentials.authToken =
                        nx::network::http::BearerAuthToken(response.access_token);
                    context->sessionTokenExpirationTime = response.expires_at;
                }
                else
                {
                    context->setError(RemoteConnectionErrorCode::unauthorized);
                }
            }
            else if (*response.error
                == nx::cloud::db::api::OauthManager::k2faDisabledForUserError)
            {
                context->setError(
                    RemoteConnectionErrorCode::twoFactorAuthOfCloudUserIsDisabled);
            }
            else
            {
                context->setError(RemoteConnectionErrorCode::unauthorized);
            }
        }
        else
        {
            NX_DEBUG(this, "Access token received successfully.");
            context->logonData.credentials.authToken =
                nx::network::http::BearerAuthToken(response.access_token);
            context->sessionTokenExpirationTime = response.expires_at;
        }
    }

    void requestCloudTokenIfPossible(ContextPtr context)
    {
        if (context
            && context->isCloudConnection() //< Actual for the Cloud User only.
            && context->expectedServerVersion() //< Required to get the credentials correctly.
            && context->expectedCloudSystemId() //< Required to set token scope.
            && !context->expectedCloudSystemId()->isEmpty()
            && !context->cloudToken.valid()) //< Whether token is already requested.
        {
            NX_DEBUG(this, "Requesting Cloud access token.");

            fillCloudConnectionCredentials(context, *context->expectedServerVersion());
            // GET /cdb/oauth2/token.
            context->cloudToken = requestsManager->issueCloudToken(
                context,
                *context->expectedCloudSystemId());
        }
    }

    void issueLocalToken(ContextPtr context)
    {
        if (!context)
            return;

        const bool isTemporaryUser =
            context->logonData.userType == nx::vms::api::UserType::temporaryLocal;

        nx::vms::api::LoginSession session = isTemporaryUser
            ? requestsManager->createTemporaryLocalSession(context)
            : requestsManager->createLocalSession(context);

        if (!context->failed())
        {
            context->logonData.credentials.username = session.username.toStdString();
            context->logonData.credentials.authToken =
                nx::network::http::BearerAuthToken(session.token);
            context->sessionTokenExpirationTime =
                qnSyncTime->currentTimePoint() + session.expiresInS;
        }
    }

    void emulateCompatibilityUserModel(ContextPtr context)
    {
        if (!context)
            return;

        context->compatibilityUserModel = nx::vms::api::UserModelV1{
            .isOwner = true,
            .permissions = nx::vms::api::GlobalPermissionDeprecated::admin
        };
        context->compatibilityUserModel->name = QString::fromStdString(
            context->logonData.credentials.username);
    }

    void requestCompatibilityUserPermissions(ContextPtr context)
    {
        if (!context)
            return;

        if (!NX_ASSERT(!context->credentials().username.empty()))
            return;

        const auto userModel = requestsManager->getUserModel(context);
        if (!context->failed())
            context->compatibilityUserModel = userModel;
    }

    struct CertificateVerificationResult
    {
        bool success = false;
        bool isUserProvidedCertificate = false;
    };

    CertificateVerificationResult verifyHandshakeCertificateChain(
        const nx::network::ssl::CertificateChain& handshakeCertificateChain,
        const nx::vms::api::ServerInformationV1& serverInfo,
        const std::string& hostName)
    {
        if (!nx::network::ini().verifyVmsSslCertificates)
            return {.success = true};

        if (handshakeCertificateChain.empty())
            return {.success = false};

        const auto handshakeKey = handshakeCertificateChain[0].publicKey();

        if (handshakeKey == publicKey(serverInfo.userProvidedCertificatePem))
            return {.success = true, .isUserProvidedCertificate = true};

        if (handshakeKey == publicKey(serverInfo.certificatePem))
            return {.success = true};

        std::string errorMessage;
        if (nx::network::ssl::verifyBySystemCertificates(
                handshakeCertificateChain, hostName, &errorMessage))
        {
            NX_VERBOSE(this, "Certificate passed OS check. Hostname: %2, serverId: %3",
            hostName,
            serverInfo.id);
            return {.success = true};
        }

        NX_WARNING(this,
            "The handshake certificate doesn't match any certificate provided by"
            " the server.\nHandshake key: %1.\nOS check: %2, hostname: %3, serverId: %4",
            handshakeKey,
            errorMessage,
            hostName,
            serverInfo.id);

        if (const auto& pem = serverInfo.certificatePem; !pem.empty())
        {
            NX_VERBOSE(this,
                "Server's certificate key: %1\nServer's certificate: %2",
                publicKey(pem),
                pem);
        }

        if (const auto& pem = serverInfo.userProvidedCertificatePem; !pem.empty())
        {
            NX_VERBOSE(this,
                "User provided certificate key: %1\nServer's certificate: %2",
                publicKey(pem),
                pem);
        }

        // Handshake certificate doesn't match target server certificates.
        return {.success = false};
    }

    QList<TargetCertificateInfo> collectMismatchedCertificates(ContextPtr context)
    {
        using CertificateType = CertificateVerifier::CertificateType;

        if (!NX_ASSERT(context))
            return {};

        NX_VERBOSE(this, "Collect mismatched certificates list.");

        QList<TargetCertificateInfo> certificates;

        // Collect Certificates that don't match the pinned ones.
        for (const auto& server: context->serversInfo)
        {
            const auto& serverId = server.id;
            const auto serverUrl = mainServerUrl(server.remoteAddresses, server.port);

            auto processCertificate =
                [&](const std::string& pem, CertificateType type)
                {
                    if (pem.empty())
                        return; //< There is no certificate to process.

                    const auto chain = nx::network::ssl::Certificate::parse(pem);

                    if (chain.empty())
                        return; //< The pem value is invalid.

                    const auto currentKey = chain[0].publicKey();
                    const auto pinnedKey = certificateVerifier->pinnedCertificate(serverId, type);

                    if (!pinnedKey)
                        return; //< Pin a new certificate, since the system itself is trusted.

                    if (currentKey == pinnedKey)
                        return; //< The given certificate matches the pinned one.

                    // Add certificate to the list of those that need User confirmation.
                    certificates << TargetCertificateInfo{
                        server.getModuleInformation(),
                        nx::network::SocketAddress::fromUrl(serverUrl),
                        chain};
                };

            processCertificate(server.certificatePem, CertificateType::autogenerated);
            processCertificate(server.userProvidedCertificatePem, CertificateType::connection);
        }

        return certificates;
    }

    void storeCertificates(ContextPtr context)
    {
        using CertificateType = CertificateVerifier::CertificateType;

        if (!NX_ASSERT(context))
            return;

        NX_VERBOSE(this, "Storing certificate list.");

        // Store the certificates and fill up the cache.
        for (const auto& server: context->serversInfo)
        {
            const auto& serverId = server.id;

            auto storeCertificate =
                [&](const std::string& pem, CertificateType type)
                {
                    if (pem.empty())
                        return; //< There is no certificate to process.

                    const auto chain = nx::network::ssl::Certificate::parse(pem);

                    if (chain.empty())
                        return; //< The pem value is invalid.

                    const auto currentKey = chain[0].publicKey();
                    const auto pinnedKey = certificateVerifier->pinnedCertificate(serverId, type);

                    if (currentKey != pinnedKey)
                        certificateVerifier->pinCertificate(serverId, currentKey, type);

                    context->certificateCache->addCertificate(serverId, currentKey, type);
                };

            storeCertificate(server.certificatePem, CertificateType::autogenerated);
            storeCertificate(server.userProvidedCertificatePem, CertificateType::connection);
        }
    }

    void fixupCertificateCache(ContextPtr context)
    {
        if (!context)
            return;

        NX_VERBOSE(this, "Emulate certificate cache for System without REST API support.");
        if (nx::network::ini().verifyVmsSslCertificates
            && NX_ASSERT(!context->handshakeCertificateChain.empty()))
        {
            context->certificateCache->addCertificate(
                context->moduleInformation.id,
                context->handshakeCertificateChain[0].publicKey(),
                CertificateVerifier::CertificateType::autogenerated);
        }
    }

    /**
     * This method run asynchronously in a separate thread.
     */
    void connectToServerAsync(WeakContextPtr contextPtr)
    {
        auto context =
            [contextPtr]() -> ContextPtr
            {
                if (auto context = contextPtr.lock(); context && !context->failed())
                    return context;
                return {};
            };

        logInitialInfo(context());

        bool hasCachedData = false;
        bool tokenExpired = true;
        if (auto ctx = context())
        {
            using namespace std::chrono;

            hasCachedData = !ctx->serversInfo.empty();
            const microseconds nowTime = qnSyncTime->currentTimePoint();
            const microseconds expirationTime = ctx->sessionTokenExpirationTime.value_or(0s);
            tokenExpired = nowTime >= expirationTime;
        }

        if (auto ctx = context())
            tryUpdateUsernameIfEmpty(ctx->logonData.credentials);

        // Request cloud token asynchronously, as this request may go in parallel with Server api.
        // This requires to know System ID and version, so method will do nothing if we do not have
        // them yet.
        if (tokenExpired)
            requestCloudTokenIfPossible(context());

        // If server version is not known, we should call api/moduleInformation first. Also send
        // this request for 4.2 and older systems as there is no other way to identify them.
        if (needToRequestModuleInformationFirst(context()))
        {
            // GET /api/moduleInformation
            getModuleInformation(context());

            // At this moment we definitely know System ID and version, so request token if not
            // done it yet.
            requestCloudTokenIfPossible(context());
        }

        // For Systems 5.0 and newer we may use /rest/v1/servers/*/info and receive all Servers'
        // certificates in one call. Offline Servers will not be listed if the System is 5.0, so
        // their certificates will be processed in the ServerCertificateWatcher class.
        if (systemSupportsRestApi(context()))
        {
            // GET /rest/v1/servers/*/info
            if (!hasCachedData)
                getServersInfo(context());

            // At this moment we definitely know System ID and version, so request token if not
            // done it yet.
            if (tokenExpired)
                requestCloudTokenIfPossible(context());

            // GET /api/moduleInformation
            // Request actually will be sent for 5.0 multi-server Systems only as in 5.1 we can get
            // Server ID from the servers info list reply.
            ensureExpectedServerId(context());

            // Ensure expected Server is present in the servers info list, copy it's info to the
            // context module information field, verify handshake certificate and process all
            // certificates from the info list.
            //
            // User interaction.
            processServersInfoList(context());
        }
        else if (context()) //< 4.2 and older servers.
        {
            // User interaction.
            verifyAndAcceptTargetCertificate(context(), /*certificateIsUserProvided*/ false);
            fixupCertificateCache(context());
        }

        if (!checkCompatibility(context()))
            return;

        pinCloudConnectionAddressIfNeeded(context());
        if (context() && !context()->isRestApiSupported())
        {
            NX_DEBUG(this, "Login with Digest to the System with no REST API support.");
            loginWithDigest(context()); //< GET /api/moduleInformationAuthenticated

            // GET /ec2/getUsers in this case.
            requestCompatibilityUserPermissions(context());
            return;
        }

        if (peerType == nx::vms::api::PeerType::videowallClient)
        {
            NX_DEBUG(this, "Login as Video Wall.");
            loginWithToken(context()); //< GET /rest/v1/login/sessions/current
            return;
        }

        if (auto ctx = context(); ctx && ctx->isCloudConnection())
        {
            if (tokenExpired)
            {
                processCloudToken(context()); // User Interaction (2FA if needed).
                NX_DEBUG(this, "Login with Cloud Token.");
                loginWithCloudToken(context()); //< GET /rest/v1/login/sessions/current
            }
        }
        else if (context())
        {
            NX_DEBUG(this, "Connecting as Local User, checking whether LDAP is required.");

            // Step is performed for local Users to upgrade them to LDAP if needed - or block
            // cloud login using a Local System tile or Login Dialog.
            //
            // GET /rest/v1/login/users/<username>
            nx::vms::api::LoginUser userType = verifyLocalUserType(context());
            if (userType.methods.testFlag(nx::vms::api::LoginMethod::http))
            {
                NX_DEBUG(this, "Digest authentication is preferred for the User.");

                // Digest is the preferred method as it is the only way to use rtsp instead of
                // rtsps.
                //
                // GET /api/moduleInformationAuthenticated.
                loginWithDigest(context());
            }
            else
            {
                NX_DEBUG(this, "Logging in with a token.");

                // Try to login with an already saved token.
                if (canLoginWithSavedToken(context()))
                    loginWithToken(context()); //< GET /rest/v1/login/sessions/current
                else
                    issueLocalToken(context()); //< GET /rest/v1/login/sessions
            }
        }

        // For the older systems (before user rights redesign) explicitly request current user
        // permissions.
        if (isOldPermissionsModelUsed(context()))
        {
            // GET /rest/v1/users/<username> in this case.
            requestCompatibilityUserPermissions(context());
        }
    }
};

RemoteConnectionFactory::RemoteConnectionFactory(
    CloudCredentialsProvider cloudCredentialsProvider,
    std::shared_ptr<AbstractRemoteConnectionFactoryRequestsManager> requestsManager,
    CertificateVerifier* certificateVerifier,
    nx::vms::api::PeerType peerType,
    Qn::SerializationFormat serializationFormat)
    :
    AbstractECConnectionFactory(),
    d(new Private(
        this,
        peerType,
        serializationFormat,
        certificateVerifier,
        std::move(cloudCredentialsProvider),
        std::move(requestsManager))
    )
{
}

RemoteConnectionFactory::~RemoteConnectionFactory()
{
    shutdown();
}

void RemoteConnectionFactory::setUserInteractionDelegate(
    std::unique_ptr<AbstractRemoteConnectionUserInteractionDelegate> delegate)
{
    d->userInteractionDelegate = std::move(delegate);
}

AbstractRemoteConnectionUserInteractionDelegate*
    RemoteConnectionFactory::userInteractionDelegate() const
{
    return d->userInteractionDelegate.get();
}

void RemoteConnectionFactory::shutdown()
{
    d->requestsManager.reset();
}

RemoteConnectionFactory::ProcessPtr RemoteConnectionFactory::connect(
    LogonData logonData,
    Callback callback,
    SystemContext* systemContext,
    std::unique_ptr<AbstractRemoteConnectionUserInteractionDelegate> customUserInteractionDelegate,
    bool ignoreCachedData)
{
    auto process = std::make_unique<RemoteConnectionProcess>(systemContext);

    process->context->logonData = logonData;
    process->context->customUserInteractionDelegate = std::move(customUserInteractionDelegate);
    process->context->certificateCache = std::make_shared<CertificateCache>();

    process->future = std::async(std::launch::async,
        [this, contextPtr = WeakContextPtr(process->context), callback,
            logonData = std::move(logonData), ignoreCachedData]
        {
            nx::utils::setCurrentThreadName("RemoteConnectionFactoryThread");

            bool useFastConnect = false;

            if (!ignoreCachedData)
            {
                if (auto context = contextPtr.lock(); !context->logonData.authCacheData.empty())
                    useFastConnect = RemoteConnectionFactoryCache::fillContext(context);
            }
            else
            {
                if (auto context = contextPtr.lock())
                    RemoteConnectionFactoryCache::restoreContext(context, logonData);
            }

            NX_DEBUG(this, "Connect fast? %1", useFastConnect);

            d->connectToServerAsync(contextPtr);

            if (useFastConnect)
            {
                bool retryConnection = false; //< Variable is required to unlock context shared ptr.

                if (auto context = contextPtr.lock(); context && context->error())
                {
                    NX_DEBUG(this, "Connection error when using cached data: %1", context->error());
                    RemoteConnectionFactoryCache::restoreContext(context, logonData);
                    retryConnection = true;
                }

                if (retryConnection) //< Try again without cache.
                {
                    NX_DEBUG(this, "Connect - Try again without cache");
                    useFastConnect = false;
                    d->connectToServerAsync(contextPtr);
                }
            }

            if (!contextPtr.lock())
                return;

            QMetaObject::invokeMethod(
                this,
                [this, contextPtr, callback, useFastConnect]()
                {
                    auto context = contextPtr.lock();
                    if (!context)
                        return;

                    if (context->error())
                    {
                        // We can no longer rely on cached information if the connection failed.
                        RemoteConnectionFactoryCache::clearForCloudId(
                            context->moduleInformation.cloudSystemId);
                        callback(*context->error());
                    }
                    else
                    {
                        auto connection = d->makeRemoteConnectionInstance(context);
                        connection->setCached(useFastConnect);
                        RemoteConnectionFactoryCache::startWatchingConnection(
                            connection, context, useFastConnect);
                        callback(connection);
                    }
                },
                Qt::QueuedConnection);
        });
    return process;
}

void RemoteConnectionFactory::destroyAsync(ProcessPtr&& process)
{
    if (!process)
        return;

    auto future = std::move(process->future);
    process.reset();

    std::thread([future = std::move(future)] {}).detach();
}

} // namespace nx::vms::client::core
