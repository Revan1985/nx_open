// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "area_tooltip_item.h"

#include <array>

#include <QtGui/QPainter>
#include <QtGui/QPaintDevice>
#include <QtGui/QPaintEngine>
#include <QtWidgets/QGraphicsWidget>
#include <QtWidgets/QWidget>

#include <ui/workaround/sharp_pixmap_painting.h>
#include <utils/common/scoped_painter_rollback.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/ui/graphics/painters/highlighted_area_text_painter.h>
#include <nx/vms/client/desktop/common/utils/painter_transform_scale_stripper.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/figure/figure.h>
#include <nx/vms/client/desktop/ini.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr qreal kRoundingRadius = 2;
static constexpr QSizeF kArrowSize(8, 4);
static constexpr qreal kArrowHalfSize = kArrowSize.width() / 2;
static constexpr qreal kArrowMargin = 6;
static constexpr QMarginsF kTooltipMargins(6, 4, 6, 4);

struct ArrowPosition
{
    static constexpr int kNoEdge = 0;

    int edge = kNoEdge;
    QRectF geometry;
};

qreal getTopPosition(const QRectF& tooltipRect, const QRectF& objectRect)
{
    return qBound(
        tooltipRect.top() + kArrowMargin,
        qBound(objectRect.top() - kArrowHalfSize, objectRect.top(), objectRect.bottom() - kArrowHalfSize),
        tooltipRect.bottom() - kArrowMargin - kArrowSize.width());
}

qreal getLeftPosition(const QRectF& tooltipRect, const QRectF& objectRect)
{
    return qBound(
        tooltipRect.left() + kArrowMargin,
        qBound(objectRect.left() - kArrowHalfSize, objectRect.left(), objectRect.right() - kArrowHalfSize),
        tooltipRect.right() - kArrowMargin - kArrowSize.width());
}

ArrowPosition arrowPosition(const QRectF& tooltipRect, const QRectF& objectRect)
{
    ArrowPosition arrowPos;

    if (tooltipRect.left() > objectRect.right())
    {
        arrowPos.edge = Qt::LeftEdge;
        arrowPos.geometry = QRectF(
            tooltipRect.left() - kArrowSize.height(),
            getTopPosition(tooltipRect, objectRect),
            kArrowSize.height(),
            kArrowSize.width());
    }
    else if (tooltipRect.right() < objectRect.left())
    {
        arrowPos.edge = Qt::RightEdge;
        arrowPos.geometry = QRectF(
            tooltipRect.right(),
            getTopPosition(tooltipRect, objectRect),
            kArrowSize.height(),
            kArrowSize.width());
    }
    else if (tooltipRect.top() > objectRect.bottom())
    {
        arrowPos.edge = Qt::TopEdge;
        arrowPos.geometry = QRectF(
            getLeftPosition(tooltipRect, objectRect),
            tooltipRect.top() - kArrowSize.height(),
            kArrowSize.width(),
            kArrowSize.height());
    }
    else if (tooltipRect.bottom() < objectRect.top())
    {
        arrowPos.edge = Qt::BottomEdge;
        arrowPos.geometry = QRectF(
            getLeftPosition(tooltipRect, objectRect),
            tooltipRect.bottom(),
            kArrowSize.width(),
            kArrowSize.height());
    }

    return arrowPos;
}

void paintArrow(QPainter* painter, const QRectF& rect, const Qt::Edge edge)
{
    QPainterPath path;

    std::array<QPointF, 4> points{{
        rect.topLeft(), rect.topRight(), rect.bottomRight(), rect.bottomLeft()
    }};

    auto point = [&points](size_t i) { return points[i % points.size()]; };

    size_t firstCorner = 0;
    switch (edge)
    {
        case Qt::BottomEdge:
            firstCorner = 2;
            break;
        case Qt::LeftEdge:
            firstCorner = 3;
            break;
        case Qt::RightEdge:
            firstCorner = 1;
            break;
        default:
            break;
    }

    const auto tip = (point(firstCorner) + point(firstCorner + 1)) / 2;
    path.moveTo(tip);
    path.lineTo(point(firstCorner + 2));
    path.lineTo(point(firstCorner + 3));
    path.lineTo(tip);

    painter->drawPath(path);
}

} // namespace

using nx::vms::client::core::Geometry;

struct AreaTooltipItem::Private
{
    AreaTooltipItem* const q;

    QString text;
    QColor backgroundColor;
    figure::FigurePtr figure;
    QRectF targetObjectGeometry;
    QRectF arrowGeometry;

    HighlightedAreaTextPainter textPainter;

    mutable QPixmap textPixmap;
    mutable QPixmap noBackGroundCache;
    mutable QPixmap backgroundCache;

    void ensureTextPixmap(qreal devicePixelRatio) const;
    void invalidateCachedPixmaps() const;
};

void AreaTooltipItem::Private::ensureTextPixmap(qreal devicePixelRatio) const
{
    if (!textPixmap.isNull() && qFuzzyEquals(textPixmap.devicePixelRatioF(), devicePixelRatio))
        return;

    const auto oldRect = q->boundingRect();

    invalidateCachedPixmaps();
    textPixmap = textPainter.paintText(text, devicePixelRatio);

    if (!qFuzzyEquals(oldRect, q->boundingRect()))
        q->prepareGeometryChange();
}

void AreaTooltipItem::Private::invalidateCachedPixmaps() const
{
    textPixmap = {};
    backgroundCache = {};
    noBackGroundCache = {};
}

//-------------------------------------------------------------------------------------------------

AreaTooltipItem::AreaTooltipItem(QGraphicsItem* parent):
    base_type(parent),
    d(new Private{.q = this})
{
}

AreaTooltipItem::~AreaTooltipItem()
{
}

QString AreaTooltipItem::text() const
{
    return d->text;
}

void AreaTooltipItem::setText(const QString& text)
{
    if (d->text == text)
        return;

    prepareGeometryChange();

    d->text = text;
    d->invalidateCachedPixmaps();
}

AreaTooltipItem::Fonts AreaTooltipItem::fonts() const
{
    return d->textPainter.fonts();
}

void AreaTooltipItem::setFonts(const Fonts& fonts)
{
    if (d->textPainter.fonts() == fonts)
        return;

    prepareGeometryChange();
    d->textPainter.setFonts(fonts);
    d->invalidateCachedPixmaps();
}

QColor AreaTooltipItem::textColor() const
{
    return d->textPainter.color();
}

void AreaTooltipItem::setTextColor(const QColor& color)
{
    if (d->textPainter.color() == color)
        return;

    d->textPainter.setColor(color);
    d->invalidateCachedPixmaps();
    update();
}

QColor AreaTooltipItem::backgroundColor() const
{
    return d->backgroundColor;
}

void AreaTooltipItem::setBackgroundColor(const QColor& color)
{
    if (d->backgroundColor == color)
        return;

    d->backgroundColor = color;
    d->invalidateCachedPixmaps();
    update();
}

QRectF AreaTooltipItem::boundingRect() const
{
    if (d->textPixmap.isNull())
        return QRectF();

    const auto pixmapSize = d->textPixmap.size() / d->textPixmap.devicePixelRatio();
    return QRectF(QPoint(), Geometry::dilated(pixmapSize, textMargins()));
}

void AreaTooltipItem::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* /*option*/,
    QWidget* /*widget*/)
{
    d->ensureTextPixmap(painter->paintEngine()->paintDevice()->devicePixelRatioF());

    if (d->textPixmap.isNull())
        return;

    const PainterTransformScaleStripper scaleStripper(painter);
    const auto mappedRect = scaleStripper.mapRect(boundingRect());
    const auto pixelRatio = painter->device()->devicePixelRatio();

    static bool drawDirect = !ini().cacheAnalyticsTooltips;

    if (drawDirect)
    {
        const auto rect = scaleStripper.mapRect(
            Geometry::eroded(boundingRect(), kArrowSize.height()));

        painter->save();
        painter->setClipRect(mappedRect);
        painter->translate(mappedRect.topLeft());

        if (d->backgroundColor.isValid())
        {
            const auto scale = parentWidget()->size();
            auto arrowPos = arrowPosition(
                rect,
                scaleStripper.mapRect(d->figure->boundingRect(scale).translated(-pos())));

            painter->setPen(Qt::NoPen);
            painter->setBrush(d->backgroundColor);
            painter->drawRoundedRect(
                QRectF(rect.topLeft() - mappedRect.topLeft(), rect.size()),
                kRoundingRadius, kRoundingRadius);

            if (arrowPos.edge != ArrowPosition::kNoEdge)
                paintArrow(painter, arrowPos.geometry.translated(-mappedRect.topLeft()), static_cast<Qt::Edge>(arrowPos.edge));
        }

        paintPixmapSharp(
            painter,
            d->textPixmap,
            QPointF(
                kTooltipMargins.left() + kArrowSize.height(),
                kTooltipMargins.top() + kArrowSize.height())
            );

        painter->restore();
        return;
    }

    if (d->backgroundColor.isValid())
    {
        const auto rect = scaleStripper.mapRect(
            Geometry::eroded(boundingRect(), kArrowSize.height()));

        const auto scale = parentWidget()->size();
        auto arrowPos = arrowPosition(
            rect,
            scaleStripper.mapRect(d->figure->boundingRect(scale).translated(-pos())));

        arrowPos.geometry.translate(-mappedRect.topLeft());

        const auto imageSize = (mappedRect.size() * pixelRatio).toSize();

        if (imageSize != d->backgroundCache.size() || arrowPos.geometry != d->arrowGeometry)
        {
            d->arrowGeometry = arrowPos.geometry;

            QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
            image.setDevicePixelRatio(pixelRatio);
            image.fill(Qt::transparent);

            QPainter p(&image);
            p.setPen(Qt::NoPen);
            p.setBrush(d->backgroundColor);
            p.drawRoundedRect(
                QRectF(rect.topLeft() - mappedRect.topLeft(), rect.size()),
                kRoundingRadius, kRoundingRadius);

            if (arrowPos.edge != ArrowPosition::kNoEdge)
                paintArrow(&p, arrowPos.geometry, static_cast<Qt::Edge>(arrowPos.edge));

            paintPixmapSharp(
                &p,
                d->textPixmap,
                QPointF(
                    kTooltipMargins.left() + kArrowSize.height(),
                    kTooltipMargins.top() + kArrowSize.height())
                );

            d->backgroundCache = QPixmap::fromImage(image);
            d->noBackGroundCache = {};
        }

        paintPixmapSharp(painter, d->backgroundCache, mappedRect.topLeft());
    }
    else
    {
        // We need to generate cache image with the exact same size as background
        // and render only text into it to avoid moving text on mouse hover.

        const auto imageSize = (mappedRect.size() * pixelRatio).toSize();

        if (d->noBackGroundCache.size() != imageSize)
        {
            QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
            image.setDevicePixelRatio(pixelRatio);
            image.fill(Qt::transparent);

            QPainter p(&image);
            paintPixmapSharp(
                &p,
                d->textPixmap,
                QPointF(
                    kTooltipMargins.left() + kArrowSize.height(),
                    kTooltipMargins.top() + kArrowSize.height()));

            d->noBackGroundCache = QPixmap::fromImage(image);
            d->backgroundCache = {};
        }

        paintPixmapSharp(painter, d->noBackGroundCache, mappedRect.topLeft());
    }
}

QMarginsF AreaTooltipItem::textMargins() const
{
    return QMarginsF(
        kTooltipMargins.left() + kArrowSize.height(),
        kTooltipMargins.top() + kArrowSize.height(),
        kTooltipMargins.right() + kArrowSize.height(),
        kTooltipMargins.bottom() + kArrowSize.height());
}

void AreaTooltipItem::setFigure(const figure::FigurePtr& figure)
{
    d->figure = figure;
}

} // namespace nx::vms::client::desktop
