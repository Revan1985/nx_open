// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QSet>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::core::analytics::taxonomy { class AnalyticsFilterModel; }

namespace nx::vms::client::desktop::analytics::taxonomy {

static const QString kDateTimeAttributeName = "^/DATE_TIME/^";
static const QString kTitleAttributeName = "^/OBJECT_TITLE/^";
static const QString kCameraAttributeName = "^/CAMERA/^";
static const QString kObjectTypeAttributeName = "^/OBJECT_TYPE/^";

class NX_VMS_CLIENT_DESKTOP_API AttributeDisplayManager: public QObject
{
    Q_OBJECT
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

    Q_PROPERTY(bool cameraVisible READ cameraVisible NOTIFY cameraVisibleChanged)
    Q_PROPERTY(bool objectTypeVisible READ objectTypeVisible NOTIFY objectTypeVisibleChanged)
    Q_PROPERTY(QStringList attributes READ attributes NOTIFY attributesChanged)

public:
    static void registerQmlType();

    enum class Mode { tileView, tableView };
    Q_ENUM(Mode)

    AttributeDisplayManager(Mode mode,
        core::analytics::taxonomy::AnalyticsFilterModel* filterModel);
    virtual ~AttributeDisplayManager() override;

    virtual QStringList attributes() const;
    QStringList builtInAttributes() const;
    virtual QSet<QString> visibleAttributes() const;

    QString displayName(const QString& attribute) const;

    bool cameraVisible() const;
    bool objectTypeVisible() const;

    bool isVisible(const QString& attribute) const;
    void setVisible(const QString& attribute, bool visible);

    bool canBeHidden(const QString& attribute) const;
    bool canBeMoved(const QString& attribute) const;

    void placeAttributeBefore(const QString& attribute, const QString& anchorAttribute);

    virtual QStringList attributesForObjectType(const QStringList& objectTypeIds) const;

    /** Attribute indexes in the `attributes` list, used for external sorting. */
    QHash<QString, int> attributeIndexes() const;

signals:
    void cameraVisibleChanged();
    void objectTypeVisibleChanged();
    void attributeVisibilityChanged(const QString& attribute);
    void attributesChanged();
    void visibleAttributesChanged();

private:
    class Private;
    nx::utils::ImplPtr <Private> d;
};

namespace details {

class Factory: public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE AttributeDisplayManager* create(
        AttributeDisplayManager::Mode mode,
        nx::vms::client::core::analytics::taxonomy::AnalyticsFilterModel* filterModel);
};

} // namespace details

} // namespace nx::vms::client::desktop::analytics::taxonomy
