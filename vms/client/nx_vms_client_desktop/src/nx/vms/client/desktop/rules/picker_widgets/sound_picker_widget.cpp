// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "sound_picker_widget.h"

#include <QtWidgets/QHBoxLayout>

#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/server_notification_cache.h>
#include <ui/dialogs/notification_sound_manager_dialog.h>
#include <ui/models/notification_sound_model.h>

namespace nx::vms::client::desktop::rules {

SoundPicker::SoundPicker(
    vms::rules::SoundField* field,
    SystemContext* context,
    ParamsWidget* parent)
    :
    PlainFieldPickerWidget<vms::rules::SoundField>(field, context, parent),
    m_serverNotificationCache{context->serverNotificationCache()}
{
    auto contentLayout = new QHBoxLayout;

    contentLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());
    m_comboBox = new QComboBox;
    m_comboBox->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
    contentLayout->addWidget(m_comboBox);

    m_manageButton = new QPushButton;
    m_manageButton->setText(tr("Manage"));

    contentLayout->addWidget(m_manageButton);

    contentLayout->setStretch(0, 1);

    m_contentWidget->setLayout(contentLayout);

    auto soundModel = m_serverNotificationCache->persistentGuiModel();
    m_comboBox->setModel(soundModel);

    connect(
        m_comboBox,
        &QComboBox::currentIndexChanged,
        this,
        &SoundPicker::onCurrentIndexChanged);

    connect(
        m_manageButton,
        &QPushButton::clicked,
        this,
        &SoundPicker::onManageButtonClicked);
}

void SoundPicker::updateUi()
{
    auto soundModel = m_serverNotificationCache->persistentGuiModel();
    QSignalBlocker blocker{m_comboBox};
    m_comboBox->setCurrentIndex(soundModel->rowByFilename(m_field->value()));

    if (isEdited())
    {
        const auto validity = fieldValidity();
        if (validity.validity == QValidator::State::Invalid)
            setErrorStyle(m_comboBox);
        else
            resetErrorStyle(m_comboBox);

        PlainFieldPickerWidget<vms::rules::SoundField>::updateUi();
    }
}

void SoundPicker::onCurrentIndexChanged(int index)
{
    auto soundModel = m_serverNotificationCache->persistentGuiModel();
    m_field->setValue(soundModel->filenameByRow(index));

    setEdited();
}

void SoundPicker::onManageButtonClicked()
{
    auto dialog = createSelfDestructingDialog<QnNotificationSoundManagerDialog>(this);

    connect(
        dialog,
        &QnNotificationSoundManagerDialog::accepted,
        this,
        [this]
        {
            auto soundModel = m_serverNotificationCache->persistentGuiModel();
            m_comboBox->setCurrentIndex(soundModel->rowByFilename(m_field->value()));
        });

    dialog->show();
}

} // namespace nx::vms::client::desktop::rules
