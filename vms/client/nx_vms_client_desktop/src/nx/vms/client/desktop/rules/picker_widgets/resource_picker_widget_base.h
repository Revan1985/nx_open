// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/desktop/style/custom_style.h>

#include "field_picker_widget.h"
#include "picker_widget_utils.h"

namespace nx::vms::client::desktop::rules {

template<typename F>
class ResourcePickerWidgetBase: public PlainFieldPickerWidget<F>
{
    using base = PlainFieldPickerWidget<F>;

public:
    explicit ResourcePickerWidgetBase(SystemContext* context, ParamsWidget* parent):
        base(context, parent)
    {
        auto contentLayout = new QVBoxLayout;

        m_selectButton = new QPushButton;
        m_selectButton->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        contentLayout->addWidget(m_selectButton);

        m_contentWidget->setLayout(contentLayout);

        connect(
            m_selectButton,
            &QPushButton::clicked,
            this,
            &ResourcePickerWidgetBase<F>::onSelectButtonClicked);
    }

    void setValidity(const vms::rules::ValidationResult& validationResult) override
    {
        base::setValidity(validationResult);

        if (validationResult.validity == QValidator::State::Acceptable)
            resetStyle(m_selectButton);
        else
            setWarningStyle(m_selectButton);
    }

protected:
    BASE_COMMON_USINGS

    QPushButton* m_selectButton{nullptr};

    virtual void onSelectButtonClicked() = 0;
};

} // namespace nx::vms::client::desktop::rules
