// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include "abstract_backend.h"

class QSettings;

namespace nx::utils::property_storage {

class NX_UTILS_API QSettingsBackend: public AbstractBackend
{
public:
    /**
     * Note: backend takes ownership of QSettings object.
     */
    QSettingsBackend(QSettings* settings, const QString& group = QString());
    virtual ~QSettingsBackend() override;

    virtual bool isWritable() const override;
    virtual QString readValue(const QString& name, bool* success = nullptr) override;
    virtual bool writeValue(const QString& name, const QString& value) override;
    virtual bool removeValue(const QString& name) override;
    virtual bool exists(const QString& name) const override;
    virtual bool sync() override;

    QSettings* qSettings() const;

private:
    QScopedPointer<QSettings> m_settings;
    QString m_group;
};

} // namespace nx::utils::property_storage
