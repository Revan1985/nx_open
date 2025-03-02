// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QWidget>

#include <nx/vms/common/system_health/message_type.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui { class PopupSettingsWidget; }

namespace nx::vms::client::desktop {

class UserNotificationSettingsManager;

class PopupSettingsWidget: public QnAbstractPreferencesWidget
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    explicit PopupSettingsWidget(
        SystemContext* systemContext,
        QWidget* parent = nullptr);
    ~PopupSettingsWidget();

    virtual void applyChanges() override;
    virtual void loadDataToUi() override;
    virtual bool hasChanges() const override;

private:
    QStringList watchedEvents() const;
    QList<common::system_health::MessageType> watchedMessages() const;

private:
    QScopedPointer<Ui::PopupSettingsWidget> ui;
    QMap<QString, QCheckBox*> m_eventRulesCheckBoxes;
    QMap<common::system_health::MessageType, QCheckBox*> m_systemHealthCheckBoxes;
    bool m_updating;
    UserNotificationSettingsManager* const m_userNotificationSettingsManager;
};

} // namespace nx::vms::client::desktop
