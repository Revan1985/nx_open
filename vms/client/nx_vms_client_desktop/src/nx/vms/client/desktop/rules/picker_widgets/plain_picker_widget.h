// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/common/widgets/alert_label.h>
#include <nx/vms/client/desktop/common/widgets/hint_button.h>

#include "picker_widget.h"

namespace nx::vms::client::desktop::rules {

class MultilineElidedLabel;

/** Base class for the pickers with label on the left side and content on the right. */
class PlainPickerWidget: public PickerWidget
{
    Q_OBJECT

public:
    PlainPickerWidget(const QString& displayName, SystemContext* context, ParamsWidget* parent);

    void setReadOnly(bool value) override;
    void setDisplayName(const QString& displayName);
    void setValidity(const vms::rules::ValidationResult& validationResult) override;

protected:
    MultilineElidedLabel* m_label{nullptr};
    HintButton* m_hintButton{nullptr};
    QWidget* m_contentWidget{nullptr};

private:
    AlertLabel* m_alertLabel{nullptr};
};

} // namespace nx::vms::client::desktop::rules
