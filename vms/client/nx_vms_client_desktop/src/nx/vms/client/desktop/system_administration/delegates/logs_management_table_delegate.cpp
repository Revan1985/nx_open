// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "logs_management_table_delegate.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/widgets/common/progress_widget.h>

#include "../models/logs_management_model.h"
#include "../widgets/logs_management_widget.h"

namespace nx::vms::client::desktop {

namespace {

static constexpr int kExtraTextMargin = 5;
static constexpr qreal kOpacityForDisabledCheckbox = 0.3;

} // namespace

using Model = LogsManagementModel;

LogsManagementTableDelegate::LogsManagementTableDelegate(
    const LoadingIndicatorPtr& loadingIndicator,
    LogsManagementWidget* parent)
    :
    base_type(parent),
    m_loadingIndicator(loadingIndicator)
{
    parentWidget = parent;
}

void LogsManagementTableDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    switch (index.column())
    {
        case Model::NameColumn:
            paintNameColumn(painter, styleOption, index);
            break;

        case Model::CheckBoxColumn:
            paintCheckBoxColumn(painter, styleOption, index);
            break;

        case Model::LogLevelColumn:
            paintLogLevelColumn(painter, styleOption, index);
            break;

        case Model::StatusColumn:
            paintStatusColumn(painter, styleOption, index);
            break;

        default:
            base_type::paint(painter, styleOption, index);
            break;
    }
}

void LogsManagementTableDelegate::paintLogLevelColumn(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    QStyleOptionViewItem option(styleOption);
    initStyleOption(&option, index);

    QStyle* style = option.widget->style();

    // Obtain sub-element rectangles.
    QRect textRect = style->subElementRect(
        QStyle::SE_ItemViewItemText,
        &option,
        option.widget);
    QRect iconRect = {textRect.left(),
        textRect.top(),
        style::Metrics::kDefaultIconSize,
        style::Metrics::kDefaultIconSize};
    textRect.setLeft(textRect.left() + 10);

    // Paint background.
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    // Draw text.
    drawText(index, textRect, style, option, painter, false);

    // Draw icon.
    if (option.text == "None")
        return;

    QBrush brush;
    if (option.text == "Error" || option.text == "Warning")
        brush = {core::colorTheme()->color("yellow_core"), Qt::SolidPattern};
    else if (option.text == "Info")
        brush = {core::colorTheme()->color("attention.blue"), Qt::SolidPattern};
    else if (option.text == "Debug" || option.text == "Verbose")
        brush = {core::colorTheme()->color("red_core"), Qt::SolidPattern};
    else
        return;

    QRect rect = {iconRect.left(), iconRect.top() + 10, 6, 6};

    QPainterPath circle;
    circle.addEllipse(rect);
    painter->fillPath(circle, brush);
}

void LogsManagementTableDelegate::paintNameColumn(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    QStyleOptionViewItem option(styleOption);
    initStyleOption(&option, index);

    QStyle* style = option.widget->style();
    const int kOffset = -2;

    // Obtain sub-element rectangles.
    QRect textRect = style->subElementRect(
        QStyle::SE_ItemViewItemText,
        &option,
        option.widget);
    textRect.setLeft(textRect.left() + kOffset);
    QRect iconRect = style->subElementRect(
        QStyle::SE_ItemViewItemDecoration,
        &option,
        option.widget);
    iconRect.translate(kOffset, 0);

    // Paint background.
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    // Draw icon.
    if (option.features.testFlag(QStyleOptionViewItem::HasDecoration))
    {
        const auto checkStateData = index.siblingAtColumn(Model::CheckBoxColumn).data(Qt::CheckStateRole);
        const auto enabled = index.data(Model::EnabledRole).toBool();
        if (!checkStateData.isNull() && checkStateData.value<Qt::CheckState>() != Qt::Unchecked)
        {
            option.icon.paint(painter,
                iconRect,
                option.decorationAlignment,
                QnIcon::Pressed,
                (enabled ? QIcon::On : QIcon::Off));
        }
        else
        {
            option.icon.paint(painter,
                iconRect,
                option.decorationAlignment,
                QIcon::Normal,
                (enabled? QIcon::On : QIcon::Off));
        }
    }

    // Draw text.
    drawText(index, textRect, style, option, painter, true);
}

void LogsManagementTableDelegate::paintCheckBoxColumn(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    QStyleOptionViewItem option(styleOption);
    initStyleOption(&option, index);

    QStyle* style = option.widget->style();

    // Paint background.
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    const auto checkStateData = index.data(Qt::CheckStateRole);
    if (checkStateData.isNull())
        return;

    QStyleOptionViewItem checkMarkOption(styleOption);
    checkMarkOption.displayAlignment = Qt::AlignRight | Qt::AlignVCenter;

    checkMarkOption.features = QStyleOptionViewItem::HasCheckIndicator;

    switch (checkStateData.value<Qt::CheckState>())
    {
        case Qt::Checked:
            checkMarkOption.state |= QStyle::State_On;
            checkMarkOption.palette.setColor(
                QPalette::Active, QPalette::Text, core::colorTheme()->color("light1"));
            checkMarkOption.palette.setColor(
                QPalette::Inactive, QPalette::Text, core::colorTheme()->color("light10"));
            break;

        case Qt::PartiallyChecked:
            checkMarkOption.state |= QStyle::State_NoChange;
            break;

        case Qt::Unchecked:
            checkMarkOption.state |= QStyle::State_Off;
            break;
    }

    painter->save();
    if (!index.data(LogsManagementModel::EnabledRole).toBool())
        painter->setOpacity(kOpacityForDisabledCheckbox);

    const auto widget = styleOption.widget;
    checkMarkOption.rect =
        style->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &checkMarkOption, widget);
    style->drawPrimitive(
        QStyle::PE_IndicatorItemViewItemCheck, &checkMarkOption, painter, widget);
    painter->restore();
}

void LogsManagementTableDelegate::paintStatusColumn(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    QStyleOptionViewItem option(styleOption);
    initStyleOption(&option, index);

    QStyle* style = option.widget->style();

    // Paint background.
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    QRect rect = option.rect;
    rect.setSize(core::kIconSize);

    if (index.data(LogsManagementModel::DownloadingRole).toBool())
    {
        painter->drawPixmap(rect, m_loadingIndicator->currentPixmap());
    }
    else
    {
        painter->drawPixmap(
            rect,
            index.data(Qt::DecorationRole).value<QIcon>().pixmap(core::kIconSize));
    }
}

void LogsManagementTableDelegate::drawText(const QModelIndex& index,
    QRect textRect,
    QStyle* style,
    QStyleOptionViewItem option,
    QPainter* painter,
    bool extra) const
{
    const auto enabled = index.data(Model::EnabledRole).toBool();
    QColor textColor = core::colorTheme()->color("light10");
    if (!enabled)
        textColor = core::colorTheme()->color("light10", 77);
    const auto checkStateData = index.siblingAtColumn(Model::CheckBoxColumn).data(Qt::CheckStateRole);
    if (!checkStateData.isNull() && checkStateData.value<Qt::CheckState>() != Qt::Unchecked)
        textColor = core::colorTheme()->color("light1");

    const QString extraInfo = index.data(Model::IpAddressRole).toString();
    QColor ipAddressColor = core::colorTheme()->color("light16");
    if (!enabled)
        ipAddressColor = core::colorTheme()->color("light16", 77);

    const int textPadding = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1; //< As in Qt.
    const int textEnd = textRect.right() - textPadding + 1;

    QPoint textPos = textRect.topLeft()
        + QPoint(textPadding, option.fontMetrics.ascent()
            + std::ceil((textRect.height() - option.fontMetrics.height()) / 2.0));

    if (textEnd > textPos.x())
    {
        const auto devicePixelRatio = painter->device()->devicePixelRatio();

        const auto main = m_textPixmapCache.pixmap(
            option.text,
            option.font,
            textColor,
            devicePixelRatio,
            textEnd - textPos.x() + 1,
            option.textElideMode);

        if (!main.pixmap.isNull())
        {
            painter->drawPixmap(textPos + main.origin, main.pixmap);
            textPos.rx() += main.origin.x() + main.size().width() + kExtraTextMargin;
        }

        if (!extra)
            return;

        if (textEnd > textPos.x() && !main.elided() && !extraInfo.isEmpty())
        {
            option.font.setWeight(QFont::Normal);

            const auto extra = m_textPixmapCache.pixmap(
                extraInfo,
                option.font,
                ipAddressColor,
                devicePixelRatio,
                textEnd - textPos.x(),
                option.textElideMode);

            if (!extra.pixmap.isNull())
                painter->drawPixmap(textPos + extra.origin, extra.pixmap);
        }
    }
}

} // namespace nx::vms::client::desktop
