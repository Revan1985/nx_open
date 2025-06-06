// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage.h"

#include <nx/utils/crypt/symmetrical.h>
#include <nx/utils/log/assert.h>

namespace nx::utils::property_storage {

Storage::Storage(AbstractBackend* backend, QObject* parent):
    QObject(parent),
    m_backend(backend)
{
}

bool Storage::isWritable() const
{
    return !m_readOnly && m_backend->isWritable();
}

void Storage::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;
}

void Storage::load()
{
    for (const auto& property: m_properties)
        loadProperty(property);
}

void Storage::save()
{
    for (const auto& property: m_properties)
        saveProperty(property);
}

void Storage::generateDocumentation(const QString& caption, const QString& description)
{
    static const QString kTitle = "# ";
    static const QString kOption = "## ";

    QStringList lines;
    lines.append(kTitle + caption);
    if (!description.isEmpty())
        lines.append(description);

    for (const auto& property: m_properties)
    {
        lines.append("\n" + kOption + property->name);
        if (!property->description.isEmpty())
            lines.append(property->description);
        const QString defaultValue = property->defaultValueDoc();
        if (!defaultValue.isEmpty())
            lines.append(nx::format("Default value: `%1`.", defaultValue));
    }
    m_backend->writeDocumentation(lines.join("\n"));
}

void Storage::sync()
{
    m_backend->sync();
}

QByteArray Storage::securityKey() const
{
    return m_securityKey;
}

void Storage::setSecurityKey(const QByteArray& value)
{
    m_securityKey = value;
}

AbstractBackend* Storage::backend() const
{
    return m_backend.get();
}

void Storage::registerProperty(BaseProperty* property)
{
    NX_ASSERT(!m_properties.contains(property->name));
    m_properties[property->name] = property;
    connect(property, &BaseProperty::changed, this, &Storage::saveProperty);
}

void Storage::unregisterProperty(BaseProperty* property)
{
    property->disconnect(this);
    m_properties.remove(property->name);
}

bool Storage::exists(const QString& name) const
{
    return m_backend->exists(name);
}

QVariant Storage::value(const QString& name) const
{
    if (const BaseProperty* property = m_properties[name])
        return property->variantValue();
    return {};
}

void Storage::setValue(const QString& name, const QVariant& value)
{
    if (BaseProperty* property = m_properties.value(name))
        property->setVariantValue(value);
}

QHash<QString, BaseProperty*> Storage::properties() const
{
    return m_properties;
}

void Storage::loadProperty(BaseProperty* property)
{
    QString rawValue = readValue(property->name);
    if (property->secure)
        rawValue = crypt::decodeStringFromHexStringAES128CBC(rawValue, m_securityKey);
    property->loadSerializedValue(rawValue);
}

void Storage::saveProperty(BaseProperty* property)
{
    QString rawValue = property->serialized();
    if (property->secure)
        rawValue = crypt::encodeHexStringFromStringAES128CBC(rawValue, m_securityKey);
    writeValue(property->name, rawValue);
}

QString Storage::readValue(const QString& name)
{
    return m_backend->readValue(name);
}

void Storage::writeValue(const QString& name, const QString& value)
{
    if (m_readOnly)
        return;

    m_backend->writeValue(name, value);
}

void Storage::removeValue(const QString& name)
{
    if (m_readOnly)
        return;

    m_backend->removeValue(name);
}

} // namespace nx::utils::property_storage
