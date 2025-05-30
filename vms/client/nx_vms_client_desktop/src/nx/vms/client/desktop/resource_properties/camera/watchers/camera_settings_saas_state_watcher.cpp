// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_settings_saas_state_watcher.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_utils.h>
#include <nx/vms/common/system_settings.h>

#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

using namespace nx::vms::common;

CameraSettingsSaasStateWatcher::CameraSettingsSaasStateWatcher(
    CameraSettingsDialogStore* store,
    SystemContext* systemContext,
    QObject* parent)
    :
    base_type(parent)
{
    if (!NX_ASSERT(store && systemContext))
        return;

    const auto setSaasStateToStore =
        [systemContext, store]
        {
            const auto saasManager = systemContext->saasServiceManager();
            store->setSaasInitialized(saas::saasInitialized(systemContext));
            store->setSaasSuspended(saasManager->saasSuspended());
            store->setSaasShutDown(saasManager->saasShutDown());
        };

    connect(systemContext->saasServiceManager(), &saas::ServiceManager::dataChanged,
        this, setSaasStateToStore);

    connect(systemContext->globalSettings(), &SystemSettings::cloudSettingsChanged,
        this, setSaasStateToStore);

    setSaasStateToStore();
}

} // namespace nx::vms::client::desktop
