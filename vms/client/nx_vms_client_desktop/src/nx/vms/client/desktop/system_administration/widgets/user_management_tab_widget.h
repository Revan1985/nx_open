// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/window_context_aware.h>
#include <ui/widgets/common/abstract_preferences_widget.h>

namespace nx::vms::common { class UserGroupManager; }

namespace nx::vms::client::desktop {

class UserManagementTabWidget: public QnAbstractPreferencesWidget, public WindowContextAware
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    explicit UserManagementTabWidget(
        WindowContext* context,
        QWidget* parent = nullptr);
    virtual ~UserManagementTabWidget() override;

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;
    virtual bool hasChanges() const override;
    virtual void discardChanges() override;
    virtual bool isNetworkRequestRunning() const override;

    void resetWarnings();

    void manageDigestUsers();

protected:
    virtual void showEvent(QShowEvent* event) override;

private:
    void updateTabVisibility();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
