// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "alert_label.h"

#include <QtWidgets/QHBoxLayout>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <ui/common/palette.h>
#include <ui/widgets/word_wrapped_label.h>

namespace{

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kInfoTheme = {
    {QnIcon::Normal, {.primary = "blue10"}}
};

static constexpr auto kInfoIconSize = QSize{20, 20};

NX_DECLARE_COLORIZED_ICON(kInfoIcon, "20x20/Solid/info.svg", kInfoTheme)

} // namespace

namespace nx::vms::client::desktop {

AlertLabel::AlertLabel(QWidget* parent):
    base_class(parent),
    m_alertText(new QnWordWrappedLabel(this)),
    m_alertIcon(new QLabel(this))
{
    setType(Type::warning);
    m_alertIcon->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(m_alertText, &QnWordWrappedLabel::linkActivated, this, &AlertLabel::linkActivated);

    auto layout = new QHBoxLayout(this);
    layout->addWidget(m_alertIcon);
    layout->setAlignment(m_alertIcon, Qt::AlignTop);
    layout->addWidget(m_alertText);
}

void AlertLabel::setText(const QString& text)
{
    m_alertText->setText(text);
}

void AlertLabel::setType(Type type)
{
    switch (type)
    {
        case Type::info:
            m_alertIcon->setPixmap(qnSkin->icon(kInfoIcon).pixmap(kInfoIconSize));
            m_alertIcon->setVisible(true);
            setPaletteColor(m_alertText, QPalette::WindowText, core::colorTheme()->color("light4"));
            setPaletteColor(m_alertText, QPalette::Link, core::colorTheme()->color("light4"));
            setPaletteColor(m_alertText, QPalette::LinkVisited, core::colorTheme()->color("light1"));
            break;
        case Type::warning:
            m_alertIcon->setPixmap(qnSkin->icon(core::kAlertIcon).pixmap(core::kIconSize));
            m_alertIcon->setVisible(true);
            setPaletteColor(m_alertText, QPalette::WindowText, core::colorTheme()->color("yellow_d"));
            setPaletteColor(m_alertText, QPalette::Link, core::colorTheme()->color("yellow_d"));
            setPaletteColor(m_alertText, QPalette::LinkVisited, core::colorTheme()->color("yellow_l"));
            break;
        case Type::error:
            m_alertIcon->setPixmap({});
            m_alertIcon->setVisible(false);
            setPaletteColor(m_alertText, QPalette::WindowText, core::colorTheme()->color("red_d"));
            setPaletteColor(m_alertText, QPalette::Link, core::colorTheme()->color("red_d"));
            setPaletteColor(m_alertText, QPalette::LinkVisited, core::colorTheme()->color("red_l"));
            break;
    }
}

} // namespace nx::vms::client::desktop
