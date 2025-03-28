// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_ribbon.h"

#include <QtGui/QHoverEvent>
#include <QtWidgets/QScrollBar>

#include <nx/vms/client/desktop/event_search/widgets/event_tile.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>

#include "private/event_ribbon_p.h"

namespace nx::vms::client::desktop {

EventRibbon::EventRibbon(QWidget* parent):
    base_type(parent, Qt::FramelessWindowHint),
    d(new Private(this))
{
}

EventRibbon::~EventRibbon()
{
}

QAbstractListModel* EventRibbon::model() const
{
    return d->model();
}

void EventRibbon::setModel(QAbstractListModel* model)
{
    d->setModel(model);
}

bool EventRibbon::showDefaultToolTips() const
{
    return d->showDefaultToolTips();
}

void EventRibbon::setShowDefaultToolTips(bool value)
{
    d->setShowDefaultToolTips(value);
}

bool EventRibbon::previewsEnabled() const
{
    return d->previewsEnabled();
}

void EventRibbon::setPreviewsEnabled(bool value)
{
    d->setPreviewsEnabled(value);
}

bool EventRibbon::footersEnabled() const
{
    return d->footersEnabled();
}

void EventRibbon::setFootersEnabled(bool value)
{
    d->setFootersEnabled(value);
}

Qt::ScrollBarPolicy EventRibbon::scrollBarPolicy() const
{
    return d->scrollBarPolicy();
}

void EventRibbon::setScrollBarPolicy(Qt::ScrollBarPolicy value)
{
    d->setScrollBarPolicy(value);
}

void EventRibbon::setInsertionMode(UpdateMode updateMode, bool scrollDown)
{
    d->setInsertionMode(updateMode, scrollDown);
}

void EventRibbon::setRemovalMode(UpdateMode updateMode)
{
    d->setRemovalMode(updateMode);
}

std::chrono::microseconds EventRibbon::highlightedTimestamp() const
{
    return d->highlightedTimestamp();
}

void EventRibbon::setHighlightedTimestamp(std::chrono::microseconds value)
{
    d->setHighlightedTimestamp(value);
}

QSet<QnResourcePtr> EventRibbon::highlightedResources() const
{
    return d->highlightedResources();
}

void EventRibbon::setHighlightedResources(const QSet<QnResourcePtr>& value)
{
    d->setHighlightedResources(value);
}

void EventRibbon::setViewportMargins(int top, int bottom)
{
    d->setViewportMargins(top, bottom);
}

void EventRibbon::ensureVisible(int row)
{
    d->ensureVisible(row);
}

QWidget* EventRibbon::viewportHeader() const
{
    return d->viewportHeader();
}

void EventRibbon::setViewportHeader(QWidget* value)
{
    d->setViewportHeader(value);
}

QScrollBar* EventRibbon::scrollBar() const
{
    return d->scrollBar();
}

QSize EventRibbon::sizeHint() const
{
    return QSize(-1, d->totalHeight());
}

bool EventRibbon::event(QEvent* event)
{
    if (event->type() != QEvent::Wheel)
        return base_type::event(event);

    if (d->scrollBar()->isVisible())
        d->scrollBar()->event(event);

    event->accept();
    return true;
}

int EventRibbon::count() const
{
    return d->countWithoutDummies();
}

int EventRibbon::unreadCount() const
{
    return d->unreadCount();
}

nx::utils::Interval<int> EventRibbon::visibleRange() const
{
    return d->visibleRange();
}

} // namespace nx::vms::client::desktop
