// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/data/lookup_list_data.h>
#include <nx/vms/client/desktop/common/dialogs/qml_dialog_wrapper.h>
#include <nx/vms/client/desktop/system_context_aware.h>

#include "lookup_list_model.h"

namespace nx::vms::client::desktop {

class LookupListsDialog:
    public QmlDialogWrapper,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QmlDialogWrapper;

public:
    static void registerQmlTypes();

    LookupListsDialog(SystemContext* systemContext, QWidget* parent = nullptr);
    virtual ~LookupListsDialog() override;

    void setData(nx::vms::api::LookupListDataList data);
    void appendData(nx::vms::api::LookupListDataList data);
    void selectList(const nx::Uuid& listId);
    nx::Uuid selectedListId() const;

    /** Show error message box over the dialog. */
    void showError(const QString& title, const QString& description);

    /** Notify the dialog about saving result. */
    void setSaveResult(bool success);

    Q_INVOKABLE void save(QList<LookupListModel*> data);
    Q_INVOKABLE void createListRequested();

signals:
    void loadCompleted(QList<LookupListModel*> data);
    void appendData(QList<LookupListModel*> data);
    void saveRequested(nx::vms::api::LookupListDataList data);
};

} // namespace nx::vms::client::desktop
