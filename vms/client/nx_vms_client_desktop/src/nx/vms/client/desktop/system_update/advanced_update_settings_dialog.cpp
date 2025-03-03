// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "advanced_update_settings_dialog.h"

#include <QtCore/QScopedValueRollback>
#include <QtQuick/QQuickItem>
#include <QtWidgets/QWidget>

#include <api/server_rest_connection.h>
#include <nx/vms/client/core/utils/qml_property.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_update/client_update_manager.h>
#include <nx/vms/common/system_settings.h>
#include <ui/workbench/workbench_context.h>

using namespace nx::vms::common;

namespace nx::vms::client::desktop {

AdvancedUpdateSettingsDialog::AdvancedUpdateSettingsDialog(QWidget* parent):
    QmlDialogWrapper(
        QUrl("Nx/Dialogs/SystemSettings/AdvancedUpdateSettings.qml"),
        {},
        parent),
    QnSessionAwareDelegate(parent)
{
    QmlProperty<ClientUpdateManager*>(rootObjectHolder(), "clientUpdateManager") =
        workbenchContext()->findInstance<ClientUpdateManager>();

    loadSettings();

    connect(systemSettings(), &SystemSettings::updateNotificationsChanged,
        this, &AdvancedUpdateSettingsDialog::loadSettings);
    notifyAboutUpdates.connectNotifySignal(this, &AdvancedUpdateSettingsDialog::saveSettings);

    connect(systemSettings(), &SystemSettings::targetPersistentUpdateStorageChanged,
        this, &AdvancedUpdateSettingsDialog::loadSettings);
    offlineUpdatesEnabled.connectNotifySignal(this, &AdvancedUpdateSettingsDialog::saveSettings);
}

bool AdvancedUpdateSettingsDialog::tryClose(bool /*force*/)
{
    if (auto api = connectedServerApi(); api && m_currentRequest > 0)
        api->cancelRequest(m_currentRequest);
    m_currentRequest = 0;

    reject();
    return true;
}

void AdvancedUpdateSettingsDialog::loadSettings()
{
    notifyAboutUpdates = systemSettings()->isUpdateNotificationsEnabled();
    const api::PersistentUpdateStorage storage =
        systemSettings()->targetPersistentUpdateStorage();
    offlineUpdatesEnabled = storage.autoSelection || !storage.servers.isEmpty();
}

void AdvancedUpdateSettingsDialog::saveSettings()
{
    if (m_restoring)
        return;

    auto callback =
        [this](bool success,
            rest::Handle requestId,
            rest::ServerConnection::ErrorOrEmpty)
        {
            NX_ASSERT(m_currentRequest == requestId || m_currentRequest == 0);
            m_currentRequest = 0;
            if (!success)
            {
                QScopedValueRollback<bool> rollback(m_restoring, true);
                loadSettings();
            }
        };

    m_currentRequest = connectedServerApi()->patchSystemSettings(
        systemContext()->getSessionTokenHelper(), {{
            .updateNotificationsEnabled = notifyAboutUpdates,
            .targetPersistentUpdateStorage = api::PersistentUpdateStorage{
                .autoSelection = offlineUpdatesEnabled}}},
        callback,
        this);
}

} // namespace nx::vms::client::desktop
