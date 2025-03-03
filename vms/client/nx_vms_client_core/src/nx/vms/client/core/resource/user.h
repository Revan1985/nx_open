// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/server_rest_connection_fwd.h>
#include <core/resource/user_resource.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/vms/api/data/user_model.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API UserResource: public QnUserResource
{
    Q_OBJECT
    using base_type = QnUserResource;

public:
    UserResource(nx::vms::api::UserType userType, nx::vms::api::UserExternalId externalId);

    /** Special constructor for compatibility mode. */
    explicit UserResource(nx::vms::api::UserModelV1 data);

    /** Constructor for cross-system contexts as of 6.0 and newer. */
    explicit UserResource(nx::vms::api::UserModelV3 data);

    /** Send request to the currently connected server to save user settings. */
    rest::Handle saveSettings(
        const nx::vms::api::UserSettings& value,
        std::function<void(bool /*success*/, rest::Handle /*handle*/)> callback = {},
        nx::utils::AsyncHandlerExecutor executor = {});

    void setSettings(const nx::vms::api::UserSettings& settings);
    nx::vms::api::UserSettings settings() const;

    virtual bool shouldMaskUser() const override;

    virtual std::map<nx::Uuid, nx::Uuid> mappedOrgGroupIds() const override;
    virtual std::vector<nx::Uuid> siteAndMappedOrgGroupIds() const override;

protected:
    virtual void updateInternal(const QnResourcePtr& source, NotifierList& notifiers) override;

private:
    std::optional<nx::vms::api::UserModelV1> m_overwrittenData;
    /**
     * In VMS, organization groups are just user properties and don't appear in the group list. On
     * the client side, they must be listed as read-only. Since their IDs match predefined groups,
     * they need to be mapped to temporary values for distinction.
     */
    std::map<nx::Uuid, nx::Uuid> m_externalToLocalOrgGroupIds;
    std::vector<nx::Uuid> m_localOrgGroupIds;
};

using UserResourcePtr = QnSharedResourcePointer<UserResource>;

} // namespace nx::vms::client::core
