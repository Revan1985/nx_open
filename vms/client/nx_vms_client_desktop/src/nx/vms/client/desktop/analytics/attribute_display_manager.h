// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>

namespace nx::vms::client::desktop::analytics::taxonomy {

class AnalyticsFilterModel;

class AttributeDisplayManager: public QObject
{
    Q_OBJECT

    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

    Q_PROPERTY(bool cameraVisible READ cameraVisible NOTIFY cameraVisibleChanged)
    Q_PROPERTY(bool objectTypeVisible READ objectTypeVisible NOTIFY objectTypeVisibleChanged)
    Q_PROPERTY(QStringList attributes READ attributes NOTIFY attributesChanged)

public:
    enum class Mode { tileView, tableView };
    Q_ENUM(Mode)

    AttributeDisplayManager(Mode mode, AnalyticsFilterModel* filterModel);
    virtual ~AttributeDisplayManager() override;

    QStringList attributes() const;
    QStringList builtInAttributes() const;
    QSet<QString> visibleAttributes() const;

    QString displayName(const QString& attribute) const;

    bool cameraVisible() const;
    bool objectTypeVisible() const;

    bool isVisible(const QString& attribute) const;
    void setVisible(const QString& attribute, bool visible);

    bool canBeHidden(const QString& attribute) const;
    bool canBeMoved(const QString& attribute) const;

    void placeAttributeBefore(const QString& attribute, const QString& anchorAttribute);

    QStringList attributesForObjectType(const QStringList& objectTypeIds) const;

signals:
    void cameraVisibleChanged();
    void objectTypeVisibleChanged();
    void attributeVisibilityChanged(const QString& attribute);
    void attributesChanged();
    void visibleAttributesChanged();

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace nx::vms::client::desktop::analytics::taxonomy