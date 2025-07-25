// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>

#include <gtest/gtest.h>

#include <QtCore/QCoreApplication>

#include <network/system_helpers.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/utils/test_support/test_options.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/protocol_version.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/network/certificate_storage.h>
#include <nx/vms/client/core/network/certificate_verifier.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_connection_factory.h>
#include <nx/vms/client/core/network/server_certificate_validation_level.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/common/network/server_compatibility_validator.h>
#include <utils/common/synctime.h>

#include "private/remote_connection_factory_requests_mock.h"

namespace nx::vms::client::core {
namespace test {

static const std::string kLocalHost = "localhost";

std::string makeCertificateAndKey()
{
    return nx::network::ssl::makeCertificateAndKey(
        nx::network::ssl::X509Name("RemoteConnectionFactoryTest", "US", "Test"), kLocalHost);
}

static const nx::utils::SoftwareVersion kVersion42(4, 2);
static const nx::utils::SoftwareVersion kVersion50(5, 0);
static const nx::utils::SoftwareVersion kVersion51(5, 1);
static const nx::utils::SoftwareVersion kLatestVersion(nx::build_info::vmsVersion());

static const nx::Uuid kExpectedServerId = nx::Uuid::createUuid();
static const QString kCloudSystemId = nx::Uuid::createUuid().toSimpleString();
static const std::string kExpectedServerPem = makeCertificateAndKey();
static const std::string kLocalUserName = "user";
static const std::string kPassword = "password";
static const std::string kLocalToken = "local_token";
static const std::string kCloudUserName = "user@example.com";
static const std::string kCloudToken = "cloud_token";

using UserInfo = RemoteConnectionFactoryRequestsManager::UserInfo;
using Purpose = nx::vms::common::ServerCompatibilityValidator::Purpose;

std::vector<UserInfo> makeDefaultUsers()
{
    return {
        {
            .username = kLocalUserName,
            .password = kPassword,
            .token = kLocalToken,
            .userType = nx::vms::api::UserType::local
        },
        {
            .username = kCloudUserName,
            .password = kPassword,
            .token = kCloudToken,
            .userType = nx::vms::api::UserType::cloud
        }
    };
}

struct TestSystemData
{
    nx::utils::SoftwareVersion version = kLatestVersion;
    int serversCount = 1;
    std::vector<UserInfo> users = makeDefaultUsers();
};

struct TestLogonData
{
    // Note: keep fields in alphabetic order to simplify new tests creation.

    /** Whether expected server id is known. */
    bool hasId = true;

    /** Whether user token exists and is working. */
    bool hasToken = true;

    /** Whether expected server version is known. */
    bool hasVersion = true;

    /** Whether connection is done with cloud credentials and address. */
    bool isCloud = false;

    /** Whether connection is done with ldap credentials. */
    bool isLdap = false;

    /** Whether connection is checked as for mobile client (json compatible with old systems). */
    nx::vms::api::PeerType peerType = nx::vms::api::PeerType::mobileClient;

    /** Purpose of connection (Desktop Client may connect in compatibility mode). */
    Purpose purpose = Purpose::connect;
};

using ConnectionOrError = RemoteConnectionFactory::ConnectionOrError;

class RemoteConnectionFactoryTest: public ::testing::Test
{
    class UserInteractionDelegate: public AbstractRemoteConnectionUserInteractionDelegate
    {
        virtual bool acceptNewCertificate(
            const TargetCertificateInfo& /*certificateInfo*/) override { return true; }

        virtual bool acceptCertificateAfterMismatch(
            const TargetCertificateInfo& /*certificateInfo*/) override { return true; }

        virtual bool acceptCertificatesOfServersInTargetSystem(
            const QList<TargetCertificateInfo>& /*certificatesInfo*/) override { return true; }

        virtual bool request2FaValidation(const nx::network::http::Credentials& /*credentials*/)
            override { return true; }
    };

public:
    RemoteConnectionFactoryTest()
    {
        systemContext.reset(appContext.createSystemContext(
            SystemContext::Mode::unitTests));
        appContext.addSystemContext(systemContext.get());

        requestsManager = std::make_shared<RemoteConnectionFactoryRequestsManager>();

        const auto certificateChain = nx::network::ssl::Certificate::parse(kExpectedServerPem);
        requestsManager->setHandshakeCertificateChain(certificateChain);

        constexpr auto kDefaultCertificateValidationLevel =
            network::server_certificate::ValidationLevel::recommended;
        const QString rootCertificatesPath =
            nx::TestOptions::temporaryDirectoryPath(/*canCreate*/ true) + "/certificates";

        connectionCertificatesStorage = std::make_unique<CertificateStorage>(
            rootCertificatesPath + "/connection", kDefaultCertificateValidationLevel);
        autoGeneratedCertificatesStorage = std::make_unique<CertificateStorage>(
            rootCertificatesPath + "/autogenerated", kDefaultCertificateValidationLevel);

        certificateVerifier = std::make_unique<CertificateVerifier>(
            kDefaultCertificateValidationLevel,
            connectionCertificatesStorage.get(),
            autoGeneratedCertificatesStorage.get());

        RemoteConnectionFactory::CloudCredentialsProvider cloudCredentialsProvider;
        cloudCredentialsProvider.getCredentials =
            []
            {
                return nx::network::http::Credentials(
                    kCloudUserName,
                    nx::network::http::BearerAuthToken(kCloudToken));
            };
        cloudCredentialsProvider.getLogin =
            [] { return kCloudUserName; };
        cloudCredentialsProvider.getDigestPassword =
            [] { return kPassword; };
        cloudCredentialsProvider.is2FaEnabledForUser =
            [] { return false; };

        connectionFactory = std::make_unique<RemoteConnectionFactory>(
            std::move(cloudCredentialsProvider),
            requestsManager,
            certificateVerifier.get(),
            nx::vms::api::PeerType::desktopClient,
            Qn::SerializationFormat::json);

        auto userInteractionDelegate = std::make_unique<UserInteractionDelegate>();
        connectionFactory->setUserInteractionDelegate(std::move(userInteractionDelegate));
    }

    void givenSystem(TestSystemData data = {})
    {
        const nx::Uuid localSystemId = nx::Uuid::createUuid();
        testSystemData = data;
        for (int i = 0; i < data.serversCount; ++i)
        {
            const bool isDefaultServer = (i == 0);

            nx::vms::api::ServerInformationV1 serverInfo;
            if (isDefaultServer)
            {
                serverInfo.id = kExpectedServerId;
                serverInfo.certificatePem = kExpectedServerPem;
                if (data.version >= kVersion51)
                    serverInfo.collectedByThisServer = true;
            }
            else
            {
                serverInfo.id = nx::Uuid::createUuid();
            }
            serverInfo.version = data.version;
            serverInfo.localSystemId = localSystemId;
            serverInfo.cloudSystemId = kCloudSystemId;
            serverInfo.brand = nx::branding::brand();
            serverInfo.customization = nx::branding::customization();
            serverInfo.cloudHost = nx::branding::cloudHost();
            serverInfo.port = ::helpers::kDefaultConnectionPort;
            serverInfo.protoVersion = data.version.major * 1000 + data.version.minor * 100;

            requestsManager->addServer(serverInfo);
        }

        for (auto userInfo: data.users)
        {
            if (data.version <= kVersion42)
                userInfo.token = {};

            requestsManager->addUser(userInfo);
        }
    }

    void givenLogonData(TestLogonData data)
    {
        if (data.hasVersion)
            logonData.expectedServerVersion = testSystemData.version;

        if (data.hasId)
            logonData.expectedServerId = kExpectedServerId;

        if (data.isCloud)
        {
            logonData.credentials.username = kCloudUserName;
            logonData.userType = nx::vms::api::UserType::cloud;
            logonData.expectedCloudSystemId = kCloudSystemId;
            if (data.hasId)
            {
                logonData.address = nx::network::SocketAddress(
                    helpers::serverCloudHost(kCloudSystemId, kExpectedServerId).toStdString());
            }
            else
            {
                logonData.address = nx::network::SocketAddress(kCloudSystemId.toStdString());
            }
        }
        else if (data.isLdap)
        {
            logonData.credentials.username = kLocalUserName;
            logonData.userType = nx::vms::api::UserType::ldap;
            logonData.address = kLocalHost;
            if (data.hasToken)
                logonData.credentials.authToken = nx::network::http::BearerAuthToken(kLocalToken);
            else
                logonData.credentials.authToken = nx::network::http::PasswordAuthToken(kPassword);
        }
        else
        {
            logonData.credentials.username = kLocalUserName;
            logonData.userType = nx::vms::api::UserType::local;
            logonData.address = kLocalHost;
            if (data.hasToken)
                logonData.credentials.authToken = nx::network::http::BearerAuthToken(kLocalToken);
            else
                logonData.credentials.authToken = nx::network::http::PasswordAuthToken(kPassword);
        }

        logonData.purpose = data.purpose;

        common::ServerCompatibilityValidator::initialize(
            data.peerType, Qn::SerializationFormat::json);
    }

    void whenConnectToSystem()
    {
        using namespace std::chrono;

        std::promise<ConnectionOrError> promise;
        auto future = promise.get_future();

        auto process = connectionFactory->connect(logonData,
            [&promise](ConnectionOrError result)
            {
                promise.set_value(result);
            },
            systemContext.get());
        std::future_status status;
        do {
            qApp->processEvents();
            status = future.wait_for(100ms);
        } while (status != std::future_status::ready);
        result = future.get();
    }

    void thenRequestsCountIs(int count)
    {
        EXPECT_EQ(requestsManager->requestsCount(), count);
    }

    void thenConnectionSuccessful()
    {
        ASSERT_TRUE(std::holds_alternative<RemoteConnectionPtr>(result));
        auto connection = std::get_if<RemoteConnectionPtr>(&result);
        ASSERT_TRUE((bool) connection->get());
    }

    void thenAuthenticatedAs(nx::network::http::Credentials credentials)
    {
        if (auto connection = std::get_if<RemoteConnectionPtr>(&result))
        {
            ASSERT_EQ(connection->get()->credentials(), credentials);
        }
        else
        {
            FAIL();
        }
    }

    void thenAuthenticatedAsLocalUser()
    {
        thenAuthenticatedAs(nx::network::http::Credentials(
            kLocalUserName,
            nx::network::http::BearerAuthToken(kLocalToken)));
    }

public:
    // RemoteConnectionFactory uses application secure settings for connection cache.
    ApplicationContext appContext{
        ApplicationContext::Mode::unitTests, Qn::SerializationFormat::json};

    std::unique_ptr<SystemContext> systemContext;

    /** Storage for certificates which were actually used for connection. */
    std::unique_ptr<CertificateStorage> connectionCertificatesStorage;

    /** Storage for server auto-generated certificates. */
    std::unique_ptr<CertificateStorage> autoGeneratedCertificatesStorage;

    std::unique_ptr<CertificateVerifier> certificateVerifier;
    std::unique_ptr<RemoteConnectionFactory> connectionFactory;

    std::shared_ptr<RemoteConnectionFactoryRequestsManager> requestsManager;

    TestSystemData testSystemData;
    nx::vms::client::core::LogonData logonData;
    ConnectionOrError result;

private:
    std::unique_ptr<QnSyncTime> m_syncTime = std::make_unique<QnSyncTime>();
};

TEST_F(RemoteConnectionFactoryTest, localSystemMinimalTest)
{
    givenSystem();
    givenLogonData({});
    whenConnectToSystem();
    thenRequestsCountIs(3); //< serversInfo, userType, loginWithToken
    thenConnectionSuccessful();
    thenAuthenticatedAsLocalUser();
}

TEST_F(RemoteConnectionFactoryTest, localSystemNoTokenTest)
{
    givenSystem();
    givenLogonData({.hasToken = false});
    whenConnectToSystem();
    thenRequestsCountIs(3); //< serversInfo, userType, issueToken
    thenConnectionSuccessful();
    thenAuthenticatedAsLocalUser();
}

TEST_F(RemoteConnectionFactoryTest, localSystemNoUsernameTest)
{
    givenSystem();
    givenLogonData({});
    logonData.credentials.username = {};
    whenConnectToSystem();
    thenRequestsCountIs(3); //< serversInfo, userType, loginWithToken
    thenConnectionSuccessful();
    thenAuthenticatedAsLocalUser();
}

TEST_F(RemoteConnectionFactoryTest, localSystemNoVersionTest)
{
    givenSystem();
    givenLogonData({.hasVersion = false});
    whenConnectToSystem();
    thenRequestsCountIs(4); //< moduleInformation, serversInfo, userType, loginWithToken
    thenConnectionSuccessful();
}

TEST_F(RemoteConnectionFactoryTest, localSystem51MinimalTest)
{
    givenSystem({.version = kVersion51});
    givenLogonData({});
    whenConnectToSystem();
    thenRequestsCountIs(4); //< serversInfo, userType, loginWithToken, userModel
    thenConnectionSuccessful();
    thenAuthenticatedAsLocalUser();
}

TEST_F(RemoteConnectionFactoryTest, localSystem50MinimalTest)
{
    givenSystem({.version = kVersion50});
    givenLogonData({});
    whenConnectToSystem();
    thenRequestsCountIs(4); //< serversInfo, userType, loginWithToken, userModel
    thenConnectionSuccessful();
}

TEST_F(RemoteConnectionFactoryTest, localSystem50MultiServerTest)
{
    givenSystem({.version = kVersion50, .serversCount = 2});
    givenLogonData({});
    whenConnectToSystem();
    thenRequestsCountIs(4); //< serversInfo, userType, loginWithToken, userModel
    thenConnectionSuccessful();
}

TEST_F(RemoteConnectionFactoryTest, localSystem50MultiServerNoIdTest)
{
    givenSystem({.version = kVersion50, .serversCount = 2});
    givenLogonData({.hasId = false});
    whenConnectToSystem();
    thenRequestsCountIs(5); //< serversInfo, moduleInformation, userType, loginWithToken, userModel
    thenConnectionSuccessful();
}

TEST_F(RemoteConnectionFactoryTest, localSystem50NoVersionTest)
{
    givenSystem({.version = kVersion50, .serversCount = 2});
    givenLogonData({.hasVersion = false});
    whenConnectToSystem();
    thenRequestsCountIs(5); //< moduleInformation, serversInfo, userType, loginWithToken, userModel
    thenConnectionSuccessful();
}

TEST_F(RemoteConnectionFactoryTest, localSystem42MinimalTest)
{
    givenSystem({.version = kVersion42});
    givenLogonData({
        .hasToken = false,
        .peerType = api::PeerType::desktopClient,
        .purpose = Purpose::connectInCompatibilityMode
    });
    whenConnectToSystem();
    thenRequestsCountIs(3); //< moduleInformation, loginWithDigest, userModel
    thenConnectionSuccessful();
}

TEST_F(RemoteConnectionFactoryTest, localSystem42NoIdMultipleServersTest)
{
    givenSystem({.version = kVersion42, .serversCount = 2});
    givenLogonData({
        .hasId = false,
        .hasToken = false,
        .peerType = api::PeerType::desktopClient,
        .purpose = Purpose::connectInCompatibilityMode
    });
    whenConnectToSystem();
    thenRequestsCountIs(3); //< moduleInformation, loginWithDigest, userModel
    thenConnectionSuccessful();
}

TEST_F(RemoteConnectionFactoryTest, cloudSystemMinimalTest)
{
    givenSystem();
    givenLogonData({.isCloud = true});
    whenConnectToSystem();
    thenRequestsCountIs(2); //< serversInfo, loginWithToken
    thenConnectionSuccessful();
}

TEST_F(RemoteConnectionFactoryTest, cloudSystemNoIdTest)
{
    givenSystem();
    givenLogonData({.hasId = false, .isCloud = true});
    whenConnectToSystem();
    thenRequestsCountIs(2); //< serversInfo, loginWithToken
    thenConnectionSuccessful();
}

TEST_F(RemoteConnectionFactoryTest, cloudSystemNoIdMultipleServersTest)
{
    givenSystem({.serversCount = 2});
    givenLogonData({.hasId = false, .isCloud = true});
    whenConnectToSystem();
    thenRequestsCountIs(2); //< serversInfo, loginWithToken
    thenConnectionSuccessful();
}

TEST_F(RemoteConnectionFactoryTest, cloudSystem50NoIdTest)
{
    givenSystem({.version = kVersion50});
    givenLogonData({.hasId = false, .isCloud = true});
    whenConnectToSystem();
    thenRequestsCountIs(3); //< serversInfo, loginWithToken, userModel
    thenConnectionSuccessful();
}

TEST_F(RemoteConnectionFactoryTest, cloudSystem50NoIdMultipleServersTest)
{
    givenSystem({.version = kVersion50, .serversCount = 2});
    givenLogonData({.hasId = false, .isCloud = true});
    whenConnectToSystem();
    thenRequestsCountIs(4); //< serversInfo, moduleInformation, loginWithToken, userModel
    thenConnectionSuccessful();
}

TEST_F(RemoteConnectionFactoryTest, cloudSystem42Test)
{
    givenSystem({.version = kVersion42});
    givenLogonData({
        .hasToken = false,
        .isCloud = true,
        .peerType = api::PeerType::desktopClient,
        .purpose = Purpose::connectInCompatibilityMode
    });
    whenConnectToSystem();
    thenRequestsCountIs(3); //< moduleInformation, loginWithDigest, userModel
    thenConnectionSuccessful();
}

// Check scenario when we certainly sure current user is LDAP one, e.g. on reconnect.
TEST_F(RemoteConnectionFactoryTest, ldapUserTest)
{
    givenSystem();
    givenLogonData({.isLdap = true});
    whenConnectToSystem();
    thenConnectionSuccessful();
}

// Check whether compatibility model is not requested for 6.0 systems.

// TODO: desktop client
// TODO: videowall
// TODO: failure tests

} // namespace test
} // namespace nx::vms::client::core
