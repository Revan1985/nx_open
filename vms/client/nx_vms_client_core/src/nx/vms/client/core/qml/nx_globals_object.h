// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QModelIndex>
#include <QtCore/QObject>
#include <QtCore/QPersistentModelIndex>
#include <QtCore/QVariant>
#include <QtGui/QCursor>
#include <QtQml/QJSValue>
#include <QtQuick/QQuickItem>

#include <nx/utils/software_version.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/enums.h>
#include <nx/vms/client/core/time/date_range.h>

class QQuickItemView;

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API NxGlobalsObject: public QObject
{
    Q_OBJECT
    Q_ENUMS(nx::vms::client::core::Enums::ResourceFlags)

public:
    NxGlobalsObject(QObject* parent = nullptr);

    Q_INVOKABLE nx::Url url(const QString& url) const;
    Q_INVOKABLE nx::Url url(const QUrl& url) const;
    Q_INVOKABLE nx::Url urlFromUserInput(const QString& url) const;
    Q_INVOKABLE nx::Url emptyUrl() const;

    Q_INVOKABLE QModelIndex invalidModelIndex() const;
    Q_INVOKABLE QPersistentModelIndex toPersistent(const QModelIndex& index) const;
    Q_INVOKABLE QModelIndex fromPersistent(const QPersistentModelIndex& index) const;
    Q_INVOKABLE QVariant modelData(const QModelIndex& index, const QString& roleName) const;
    Q_INVOKABLE bool isRecursiveChildOf(const QModelIndex& child, const QModelIndex& parent) const;
    Q_INVOKABLE bool hasChildren(const QModelIndex& index) const;
    Q_INVOKABLE Qt::ItemFlags itemFlags(const QModelIndex& index) const;

    Q_INVOKABLE QModelIndex modelFindOne(const QModelIndex& start, const QString& roleName,
        const QVariant& value, Qt::MatchFlags flags) const;
    Q_INVOKABLE QModelIndexList modelFindAll(const QModelIndex& start, const QString& roleName,
        const QVariant& value, Qt::MatchFlags flags) const;

    /** Operations with QModelIndexList in QML are incredibly slow, pass QVariantList instead. */
    Q_INVOKABLE QVariantList toQVariantList(const QModelIndexList& indexList) const;

    Q_INVOKABLE nx::utils::SoftwareVersion softwareVersion(const QString& version) const;

    Q_INVOKABLE QCursor cursor(Qt::CursorShape shape) const;

    Q_INVOKABLE bool ensureFlickableChildVisible(QQuickItem* item);

    Q_INVOKABLE nx::Uuid uuid(const QString& uuid) const;
    Q_INVOKABLE nx::Uuid generateUuid() const;

    Q_INVOKABLE bool isSequence(const QJSValue& value) const;

    Q_INVOKABLE nx::vms::client::core::DateRange dateRange(
        const QDateTime& start, const QDateTime& end) const;

    Q_INVOKABLE QLocale numericInputLocale(const QString& basedOn = "") const;

    Q_INVOKABLE bool fileExists(const QString& path) const;

    Q_INVOKABLE bool mightBeHtml(const QString& text) const;

    Q_INVOKABLE bool isRelevantForPositioners(QQuickItem* item) const;

    Q_INVOKABLE void copyToClipboard(const QString& text) const;

    Q_INVOKABLE QString clipboardText() const;

    Q_INVOKABLE double toDouble(const QVariant& value) const;

    /**
     * Makes search expression with some wildcard capabilities. Asterisk symbol and question marks
     * are supported. All other special symbols are escaped and treated "as is".
     * QRegularExpresion::wildcardToRegularExpression can't be used to support full set of wildcard
     * functionality because it processes some special symbols (like slash) in a special way.
     */
    Q_INVOKABLE QString makeSearchRegExp(const QString& value) const;

    Q_INVOKABLE QString makeSearchRegExpNoAnchors(const QString& value) const;

    /** Returns QRegularExpression::escape(value) */
    Q_INVOKABLE QString escapeRegExp(const QString& value) const;

    Q_INVOKABLE void invokeMethod(QObject* obj, const QString& methodName);

    Q_INVOKABLE QString modifierName(const Qt::KeyboardModifier modifier) const;

    Q_INVOKABLE QString dateInShortFormat(const QDateTime& date);

    Q_INVOKABLE QString timeInShortFormat(const QDateTime& date);

    Q_INVOKABLE QString dateTimeInShortFormat(const QDateTime& dateTime);

    Q_INVOKABLE QString highlightMatch(
        const QString& text, const QRegularExpression& rx, const QColor& color) const;

    Q_INVOKABLE QString toHtmlEscaped(const QString& text) const;

    Q_INVOKABLE QString toPlainText(const QString& value) const;

    /**
     * Returns qnSyncTime->currentTimePoint() but in milliseconds for easier usage with JS Date.
     */
    Q_INVOKABLE qint64 syncTimeCurrentTimePointMs() const;

    /** Returns native string representation of QQuickAction shortcut property. */
    Q_INVOKABLE QString shortcutText(const QVariant& var) const;

    /** Absolutely force laying out items of the specified item view. */
    Q_INVOKABLE void forceLayout(QQuickItemView* view) const;

    /** Returns driver information (see QRhiDriverInfo) for RHI of the specified window. */
    Q_INVOKABLE QVariantMap getDriverInfo(QQuickWindow* window) const;

    static void registerQmlType();
};

} // namespace nx::vms::client::core
