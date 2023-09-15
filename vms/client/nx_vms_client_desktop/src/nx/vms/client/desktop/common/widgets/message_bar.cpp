// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "message_bar.h"

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QPushButton>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/utils/custom_painted.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>
#include <ui/common/palette.h>

namespace nx::vms::client::desktop {

struct CommonMessageBar::Private
{
    CommonMessageBar* const q;
    QnWordWrappedLabel* const label{new QnWordWrappedLabel(q)};
    QPushButton* const closeButton{
        new QPushButton(qnSkin->icon("banners/close.svg"), QString(), q)};
    QPushButton* icon{new QPushButton(QIcon(), QString(), q)};
};

CommonMessageBar::CommonMessageBar(QWidget* parent, const BarDescription& description):
    base_type(parent),
    d(new Private{.q = this})
{
    setAttribute(Qt::WA_StyledBackground, true);
    horizontalLayout()->addWidget(d->icon);
    horizontalLayout()->addWidget(d->label, 1);
    d->label->setForegroundRole(QPalette::Text);
    d->label->label()->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    d->label->setText(QString());

    horizontalLayout()->addWidget(d->closeButton, 0, Qt::AlignTop);
    d->closeButton->setFlat(true);
    d->closeButton->setFixedSize(20, 20);
    d->closeButton->setToolTip(tr("Close"));
    d->closeButton->setFocusPolicy(Qt::NoFocus);
    d->closeButton->hide();

    d->icon->setFixedSize(20, 20);
    d->icon->setFocusPolicy(Qt::NoFocus);
    d->icon->setFlat(true);

    connect(d->closeButton, &QPushButton::clicked, this, &CommonMessageBar::closeClicked);
    connect(d->closeButton, &QPushButton::clicked, this, &CommonMessageBar::hide);

    init(description);
}

CommonMessageBar::~CommonMessageBar()
{
    // Required here for forward-declared scoped pointer destruction.
}

QString CommonMessageBar::text() const
{
    return d->label->text();
}

void CommonMessageBar::setText(const QString& text)
{
    if (text == this->text())
        return;

    d->label->setText(text);
    setDisplayed(!text.isEmpty());
}

void CommonMessageBar::init(const BarDescription& barDescription)
{
    d->closeButton->setVisible(barDescription.isClosable);
    setPaletteColor(this, QPalette::WindowText, core::colorTheme()->color("light4"));

    QString backgroundColor;
    QString frameColor;
    QIcon icon;
    switch (barDescription.level)
    {
        case BarDescription::BarLevel::Info:
            backgroundColor = "attention.blue_bg";
            frameColor = "attention.blue_dark";
            icon = qnSkin->icon("banners/info.svg");
            break;
        case BarDescription::BarLevel::Warning:
            backgroundColor = "attention.yellow_bg";
            frameColor = "yellow_d1";
            icon = qnSkin->icon("banners/warning.svg");
            break;
        case BarDescription::BarLevel::Error:
            backgroundColor = "attention.red_bg";
            frameColor = "red_d1";
            icon = qnSkin->icon("banners/error.svg");
            break;
    }

    setPaletteColor(this, QPalette::Window, core::colorTheme()->color(backgroundColor));
    setPaletteColor(this, QPalette::Dark, core::colorTheme()->color(frameColor));
    d->icon->setIcon(icon);

    setText(barDescription.text);

    if (barDescription.isMultiLine)
    {
        d->label->setAutoFillBackground(true);
        d->label->setContentsMargins(style::Metrics::kDefaultTopLevelMargin,
            style::Metrics::kStandardPadding,
            style::Metrics::kDefaultTopLevelMargin,
            style::Metrics::kStandardPadding);
    }

    for (const auto& row: barDescription.additionalRows)
    {
        verticalLayout()->addLayout(row);
    }
}

} // namespace nx::vms::client::desktop