// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/common/dialogs/qml_dialog_wrapper.h>
#include <nx/vms/client/desktop/rules/model_view/key_value_model.h>
#include <nx/vms/rules/field_types.h>
#include <ui/dialogs/common/session_aware_dialog.h>

namespace nx::vms::client::desktop::rules {

class CustomHttpHeadersDialog: public QmlDialogWrapper, public QnSessionAwareDelegate
{
    Q_OBJECT

public:
    explicit CustomHttpHeadersDialog(QList<vms::rules::KeyValueObject> headers, QWidget* parent);

    const QList<vms::rules::KeyValueObject>& headers() const;

    bool tryClose(bool force) override;

private:
    KeyValueModel* m_headersModel{};
};

} // namespace nx::vms::client::desktop::rules
