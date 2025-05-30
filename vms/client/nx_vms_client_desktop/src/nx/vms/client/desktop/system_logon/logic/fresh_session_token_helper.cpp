// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fresh_session_token_helper.h"

#include <QtCore/QThread>
#include <QtWidgets/QApplication>

#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/oauth_client_constants.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/remote_session.h>
#include <nx/vms/client/desktop/system_logon/ui/oauth_login_dialog.h>
#include <nx/vms/client/desktop/ui/dialogs/session_refresh_dialog.h>

namespace nx::vms::client::desktop {
namespace {

struct ActionTypeInfo
{
    core::OauthClientType clientType;
    bool isImportant = true;
};

ActionTypeInfo info(FreshSessionTokenHelper::ActionType actionType)
{
    using ActionType = FreshSessionTokenHelper::ActionType;
    switch (actionType)
    {
        case ActionType::unbind:
            return {core::OauthClientType::passwordDisconnect};
        case ActionType::backup:
            return {core::OauthClientType::passwordBackup, /* isImportant*/ false};
        case ActionType::restore:
            return {core::OauthClientType::passwordRestore};
        case ActionType::merge:
            return {core::OauthClientType::passwordMerge};
        case ActionType::updateSettings:
            return {core::OauthClientType::passwordApply};
        case ActionType::issueRefreshToken:
            return {core::OauthClientType::loginCloud};
    }

    NX_ASSERT(false, "Unexpected action type");
    return {core::OauthClientType::undefined};
}

} // namespace.

FreshSessionTokenHelper::FreshSessionTokenHelper(QWidget* parent):
    m_parent(parent)
{
    NX_CRITICAL(m_parent);
}

FreshSessionTokenHelper::~FreshSessionTokenHelper()
{
}

common::SessionTokenHelperPtr FreshSessionTokenHelper::makeHelper(
    QWidget* parent,
    const QString& title,
    const QString& mainText,
    const QString& actionText,
    ActionType actionType)
{
    auto result = new FreshSessionTokenHelper(parent);
    result->m_title = title;
    result->m_mainText = mainText;
    result->m_actionText = actionText;
    result->m_actionType = actionType;
    return common::SessionTokenHelperPtr(result);
}

FreshSessionTokenHelper* FreshSessionTokenHelper::makeFreshSessionTokenHelper(
    QWidget* parent,
    const QString& title,
    const QString& mainText,
    const QString& actionText,
    ActionType actionType)
{
    auto result = new FreshSessionTokenHelper(parent);
    result->m_title = title;
    result->m_mainText = mainText;
    result->m_actionText = actionText;
    result->m_actionType = actionType;
    return result;
}

std::optional<nx::network::http::AuthToken> FreshSessionTokenHelper::refreshToken()
{
    m_password = {};

    if (!NX_ASSERT(QThread::currentThread() == qApp->thread()))
        return {};

    if (!m_parent)
        return {};

    nx::network::http::AuthToken token;
    const auto context = appContext()->currentSystemContext();
    auto connection = context->connection();
    if (!connection)
        return {};

    if (connection->userType() == nx::vms::api::UserType::cloud)
    {
        auto cloudAuthData = OauthLoginDialog::login(
            m_parent,
            m_title,
            info(m_actionType).clientType,
            /*sessionAware*/ true,
            connection->moduleInformation().cloudSystemId,
            Qt::WindowStaysOnTopHint
        );

        if (cloudAuthData.empty() || (cloudAuthData.needValidateToken
            && !OauthLoginDialog::validateToken(m_parent, m_title, cloudAuthData.credentials)))
        {
            return {};
        }

        cloudAuthData.credentials.username = qnCloudStatusWatcher->credentials().username;
        qnCloudStatusWatcher->setAuthData(cloudAuthData,
            core::CloudStatusWatcher::AuthMode::update);
        if (auto session = context->session(); session && session->connection())
            session->updateCloudSessionToken();

        token = cloudAuthData.credentials.authToken;
    }
    else
    {
        const auto result = SessionRefreshDialog::refreshSession(
            m_parent,
            m_title,
            m_mainText,
            /*infoText*/ QString(),
            m_actionText,
            info(m_actionType).isImportant,
            /*passwordValidationMode*/ false,
            Qt::WindowStaysOnTopHint
        );

        token = result.token;
        if (!token.empty())
            m_password = result.password;
    }
    NX_ASSERT(token.empty() || token.isBearerToken());

    if (token.empty())
        return {};

    return token;
}

core::CloudAuthData FreshSessionTokenHelper::requestAuthData(std::function<bool()> closeCondition)
{
    if (!NX_ASSERT(QThread::currentThread() == qApp->thread()))
        return {};

    if (!m_parent)
        return {};

    const auto cloudAuthData = OauthLoginDialog::login(
        m_parent,
        m_title,
        info(m_actionType).clientType,
        /*sessionAware*/ false,
        /*cloudSystem*/ {},
        /*flags*/ {},
        closeCondition
    );

    if (cloudAuthData.needValidateToken
        && !OauthLoginDialog::validateToken(m_parent, m_title, cloudAuthData.credentials))
    {
        return {};
    }

    return cloudAuthData;
}

QString FreshSessionTokenHelper::password() const
{
    return m_password;
}

} // namespace nx::vms::client::desktop
