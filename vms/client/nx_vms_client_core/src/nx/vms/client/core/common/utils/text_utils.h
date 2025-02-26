// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

class QTextDocument;
class QWidget;

namespace nx::vms::client::core {
namespace text_utils {

/** Limit document height, adding specified text (default to "...") as the last visible line. */
void NX_VMS_CLIENT_CORE_API elideDocumentHeight(
    QTextDocument* document,
    int maxHeight,
    const QString& tail = "...");

/** Limit document lines, adding specified text (default to "...") as the last visible line. */
void NX_VMS_CLIENT_CORE_API elideDocumentLines(
    QTextDocument* document,
    int maxLines,
    bool trimLastLine = false,
    const QString& tail = "...");

/** Limit text width, adding specified text (default to "...") at the end. */
void NX_VMS_CLIENT_CORE_API elideTextRight(
    QTextDocument* document,
    int width,
    const QString& tail = "...");

/**
 * Shortens the text to fit the given width or the widget's width if not provided,
 * truncating from the right and appending a tail.
 * Uses the widget's font and right margin for proper fitting.
 */
QString NX_VMS_CLIENT_CORE_API elideTextRight(
    QWidget* widget,
    const QString& text,
    int width = -1,
    const QString& tail = "...");

} // namespace text_utils
} // namespace nx::vms::client::core
