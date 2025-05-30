// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_actions_helper.h"

#include <nx/network/http/http_types.h>
#include <nx/utils/log/log_main.h>
#include <nx/vms/client/desktop/common/dialogs/web_view_dialog.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ui/dialogs/analytics_action_settings_dialog.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

void AnalyticsActionsHelper::processResult(
    const api::AnalyticsActionResult& result,
    WindowContext* context,
    const QnResourcePtr& proxyResource,
    std::shared_ptr<AbstractWebAuthenticator> authenticator,
    QWidget* parent)
{
    if (!result.messageToUser.isEmpty())
    {
        QnSessionAwareMessageBox message(parent);
        message.setIcon(QnMessageBox::Icon::Success);
        message.setText(result.messageToUser);
        message.setStandardButtons(QDialogButtonBox::Ok);
        message.setDefaultButton(QDialogButtonBox::NoButton);
        setHelpTopic(&message, HelpTopic::Id::Forced_Empty);
        message.exec();
    }

    if (!result.actionUrl.isEmpty())
    {
        if (result.useProxy && !proxyResource)
            NX_WARNING(NX_SCOPE_TAG, "A Resource is required to proxy %1", result.actionUrl);

        if (result.useDeviceCredentials && !authenticator)
            NX_WARNING(NX_SCOPE_TAG, "Can not authenticate %1", result.actionUrl);

        auto webDialog = createSelfDestructingDialog<QnSessionAware<WebViewDialog>>(parent);
        webDialog->init(
            result.actionUrl,
            /*enableClientApi*/ true,
            context,
            result.useProxy ? proxyResource : QnResourcePtr{},
            result.useDeviceCredentials ? authenticator : nullptr,
            /*checkCertificate*/ !result.useDeviceCredentials);

        webDialog->show();
    }
}

void AnalyticsActionsHelper::requestSettingsMap(
        const QJsonObject& settingsModel,
        std::function<void(std::optional<SettingsValuesMap>)> callback,
        QWidget* parent)
{
    if(!NX_ASSERT(callback))
        return;

    requestSettingsJson(
        settingsModel,
        [callback = std::move(callback)](std::optional<QJsonObject> setingsJson)
        {
            if (!setingsJson)
            {
                callback({});
                return;
            }

            SettingsValuesMap result;
            for (const auto& key: setingsJson->keys())
                result[key] = nx::toString(setingsJson->value(key));

            callback(result);
        },
        parent);
}

void AnalyticsActionsHelper::requestSettingsJson(
    const QJsonObject& settingsModel,
    std::function<void(std::optional<QJsonObject>)> callback,
    QWidget* parent)
{
    if(!NX_ASSERT(callback))
        return;

    if (settingsModel.isEmpty())
    {
        callback(QJsonObject{});
        return;
    }

    AnalyticsActionSettingsDialog::request(settingsModel, std::move(callback), {}, parent);
}

} // namespace nx::vms::client::desktop
