// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vms_rules_dialog.h"

#include <api/server_rest_connection.h>
#include <nx/vms/api/rules/rule.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/dialogs/week_time_schedule_dialog.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_fields/unique_id_field.h>
#include <nx/vms/rules/utils/api.h>

#include "edit_vms_rule_dialog.h"
#include "model_view/rules_table_model.h"
#include "utils/confirmation_dialogs.h"

namespace nx::vms::client::desktop::rules {

VmsRulesDialog::VmsRulesDialog(QWidget* parent):
    QmlDialogWrapper(
        appContext()->qmlEngine(),
        QUrl("Nx/VmsRules/VmsRulesDialog.qml"),
        /*initialProperties*/ {},
        parent),
    nx::vms::client::desktop::CurrentSystemContextAware{parent},
    m_parentWidget{parent},
    m_rulesTableModel{QmlProperty<RulesTableModel*>(rootObjectHolder(), "rulesTableModel")}
{
    QmlProperty<QObject*>(rootObjectHolder(), "dialog") = this;
}

VmsRulesDialog::~VmsRulesDialog()
{
    QmlProperty<QObject*>(rootObjectHolder(), "dialog") = nullptr;
}

void VmsRulesDialog::setError(const QString& error)
{
    QmlProperty<QString>(rootObjectHolder(), "errorString") = error;
}

void VmsRulesDialog::setFilter(const QString& filter)
{
    QmlProperty<QString>(rootObjectHolder(), "filterText") = filter;
}

void VmsRulesDialog::addRule()
{
    showEditRuleDialog(
        std::make_shared<vms::rules::Rule>(nx::Uuid::createUuid(), systemContext()->vmsRulesEngine()),
        /*isNew*/ true);
}

void VmsRulesDialog::editSchedule(const UuidList& ids)
{
    if (!NX_ASSERT(!ids.empty()))
        return;

    auto engine = systemContext()->vmsRulesEngine();

    QVector<QByteArray> schedules;
    for (auto id: ids)
        schedules.push_back(engine->rule(id)->schedule());

    WeekTimeScheduleDialog dialog(m_parentWidget, /*isEmptyAllowed*/ false);
    dialog.setSchedules(schedules);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const auto schedule = dialog.schedule();
    for (auto id: ids)
    {
        auto clone = engine->cloneRule(id);
        if (!NX_ASSERT(clone))
            return;

        clone->setSchedule(schedule);

        saveRuleImpl(clone);
    }
}

void VmsRulesDialog::duplicateRule(nx::Uuid id)
{
    auto engine = systemContext()->vmsRulesEngine();

    auto clone = engine->cloneRule(id);
    if (!NX_ASSERT(clone))
        return;

    clone->setId(nx::Uuid::createUuid()); //< Change id to prevent rule override on save request.
    if (auto uniqueIdField = clone->eventFilters().at(0)->fieldByType<vms::rules::UniqueIdField>())
        uniqueIdField->setId(nx::Uuid::createUuid()); //< Fix field uniqueness after cloning. TODO: #mmalofeev fix this workaround.

    showEditRuleDialog(clone, /*isNew*/ false);
}

void VmsRulesDialog::editRule(nx::Uuid id)
{
    auto engine = systemContext()->vmsRulesEngine();

    auto clone = engine->cloneRule(id);
    if (!NX_ASSERT(clone))
        return;

    showEditRuleDialog(clone, /*isNew*/ false);
}

void VmsRulesDialog::deleteRules(const UuidList& ids)
{
    if (ConfirmationDialogs::confirmDelete(m_parentWidget, ids.size()))
        deleteRulesImpl(ids);
}

void VmsRulesDialog::setRulesState(const UuidList& ids, bool isEnabled)
{
    auto engine = systemContext()->vmsRulesEngine();

    for (auto id: ids)
    {
        auto clone = engine->cloneRule(id);
        if (!NX_ASSERT(clone))
            return;

        clone->setEnabled(isEnabled);

        saveRuleImpl(clone);
    }
}

void VmsRulesDialog::resetToDefaults()
{
    if (ConfirmationDialogs::confirmReset(m_parentWidget))
        resetToDefaultsImpl();
}

void VmsRulesDialog::openEventLogDialog()
{
    action(menu::OpenEventLogAction)->trigger();
}

void VmsRulesDialog::deleteRulesImpl(const UuidList& ids)
{
    const auto api = systemContext()->connectedServerApi();
    if (!api)
        return;

    for (const auto id: ids)
    {
        api->sendRequest<rest::ServerConnection::ErrorOrEmpty>(
            /*helper*/ nullptr,
            nx::network::http::Method::delete_,
            nx::format("/rest/v4/events/rules/%1", id.toSimpleString()),
            /*params*/ {},
            /*body*/ {},
            [this](
                bool success,
                rest::Handle requestId,
                const rest::ServerConnection::ErrorOrEmpty& response)
            {
                if (success)
                    return;

                const auto& errorString = response.error().errorString;
                NX_WARNING(this, "Delete rule request %1 failed: %2", requestId, errorString);

                setError(tr("Delete rule error: %1").arg(errorString));
            },
            this);
    }
}

void VmsRulesDialog::saveRuleImpl(const std::shared_ptr<vms::rules::Rule>& rule)
{
    const auto api = systemContext()->connectedServerApi();
    if (!api)
        return;

    const auto apiRule = vms::rules::toApi(
        systemContext()->vmsRulesEngine(), serialize(rule.get()));

    api->sendRequest<rest::ServerConnection::ErrorOrEmpty>(
        /*helper*/ nullptr,
        nx::network::http::Method::put,
        nx::format("/rest/v4/events/rules/%1", rule->id().toSimpleString()),
        /*params*/ {},
        QJson::serialized(apiRule),
        [this](
            bool success,
            rest::Handle requestId,
            const rest::ServerConnection::ErrorOrEmpty& response)
        {
            if (success)
                return;

            const auto& errorString = response.error().errorString;
            NX_WARNING(this, "Create rule request %1 failed: %2", requestId, errorString);

            setError(tr("Save rule error: %1").arg(errorString));
        },
        this);
}

void VmsRulesDialog::resetToDefaultsImpl()
{
    const auto api = systemContext()->connectedServerApi();
    if (!api)
        return;

    api->sendRequest<rest::ServerConnection::ErrorOrEmpty>(
        /*helper*/ nullptr,
        nx::network::http::Method::post,
        "/rest/v4/events/rules/*/reset",
        /*params*/ {},
        /*body*/ {},
        [this](
            bool success,
            rest::Handle requestId,
            const rest::ServerConnection::ErrorOrEmpty& response)
        {
            if (success)
                return;

            const auto& errorString = response.error().errorString;
            NX_WARNING(this, "Reset rules request %1 failed: %2", requestId, errorString);

            setError(tr("Reset rules error: %1").arg(errorString));
        },
        this);
}

void VmsRulesDialog::showEditRuleDialog(const std::shared_ptr<vms::rules::Rule>& rule, bool isNew)
{
    auto editVmsRuleDialog = new EditVmsRuleDialog{windowContext()};

    // It is unable to set the VmsRulesDialog as the EditVmsRuleDialog's parent, so we need to
    // manually delete this window along with the VmsRulesDialog.
    connect(
        this,
        &QObject::destroyed,
        this,
        [editVmsRuleDialog = QPointer{editVmsRuleDialog}]
        {
            if (editVmsRuleDialog)
                editVmsRuleDialog->deleteLater();
        });

    connect(
        editVmsRuleDialog, &EditVmsRuleDialog::accepted, this, [this, rule]() { saveRuleImpl(rule); });

    connect(
        editVmsRuleDialog,
        &EditVmsRuleDialog::finished,
        this,
        [this, editVmsRuleDialog, rule](int result)
        {
            if (result == QDialogButtonBox::Ok || result == QDialogButtonBox::Apply)
                saveRuleImpl(rule);
            else if (result == QDialogButtonBox::Reset)
                deleteRulesImpl({rule->id()}); //< Reset means user requested to delete the rule.

            editVmsRuleDialog->deleteLater();
        });

    editVmsRuleDialog->setRule(rule, isNew);
    editVmsRuleDialog->setWindowModality(Qt::ApplicationModal);
    editVmsRuleDialog->show();
}

} // namespace nx::vms::client::desktop::rules
