// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/network/oauth_client_constants.h>
#include <ui/dialogs/common/dialog.h>

namespace nx::network::http { class Credentials; }
namespace nx::vms::client::core
{
    struct CloudAuthData;
    struct CloudBindData;
    struct CloudTokens;
} // namespace nx::vms::client::core

namespace nx::vms::client::desktop {

class OauthLoginDialogPrivate;

class OauthLoginDialog: public QnDialog
{
    Q_OBJECT
    using base_type = QnDialog;

public:
    /** Helper method for requesting fresh cloud auth data. */
    static nx::vms::client::core::CloudAuthData login(
        QWidget* parent,
        const QString& title,
        core::OauthClientType clientType,
        bool sessionAware,
        const QString& cloudSystem = QString(),
        Qt::WindowFlags flags = {},
        std::function<bool()> closeCondition = {});

    /** Helper method for access token 2FA validation. */
    static bool validateToken(
        QWidget* parent,
        const QString& title,
        const nx::network::http::Credentials& credentials);

    OauthLoginDialog(
        QWidget* parent,
        core::OauthClientType clientType,
        const QString& cloudSystem = QString(),
        Qt::WindowFlags flags = {});

    virtual ~OauthLoginDialog() override;

    const nx::vms::client::core::CloudAuthData& authData() const;
    const nx::vms::client::core::CloudBindData& cloudBindData() const;
    const nx::vms::client::core::CloudTokens& cloudTokens() const;

    void setCredentials(const nx::network::http::Credentials& credentials);
    void setSystemName(const QString& systenName);

signals:
    void authDataReady();
    void bindToCloudDataReady();
    void cloudTokensReady();

private:
    void loadPage();

private:
    nx::utils::ImplPtr<OauthLoginDialogPrivate> d;
};

} // namespace nx::vms::client::desktop
