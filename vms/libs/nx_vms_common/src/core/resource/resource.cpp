// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource.h"

#include <QtCore/QMetaObject>

#include <core/resource/camera_advanced_param.h>
#include <core/resource_management/resource_management_ini.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/status_dictionary.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <utils/common/util.h>

using namespace nx::vms::common;

namespace {

QString hidePasswordIfCredentialsPropety(const QString& key, const QString& value)
{
    if (nx::log::showPasswords())
        return value;

    if (key == nx::vms::api::device_properties::kCredentials
        || key == nx::vms::api::device_properties::kDefaultCredentials)
    {
        return value.left(value.indexOf(':')) + ":" + nx::Url::kMaskedPassword;
    }

    return value;
}

} // namespace

// -------------------------------------------------------------------------- //
// QnResource
// -------------------------------------------------------------------------- //
QnResource::QnResource():
    m_mutex(nx::Mutex::Recursive)
{
}

QnResource::~QnResource() = default;

QnResourcePool* QnResource::resourcePool() const
{
    if (auto* context = systemContext())
        return context->resourcePool();

    return nullptr;
}

void QnResource::addToSystemContext(nx::vms::common::SystemContext* systemContext)
{
    if (!NX_ASSERT(systemContext, "Context must exist here"))
        return;

    if (!NX_ASSERT(!this->systemContext(), "Resource already belongs to some System Context"))
        return;

    setSystemContext(systemContext);
}

QnResourcePtr QnResource::toSharedPointer() const
{
    return QnFromThisToShared<QnResource>::toSharedPointer();
}

void QnResource::setForceUsingLocalProperties(bool value)
{
    m_forceUseLocalProperties = value;
}

bool QnResource::useLocalProperties() const
{
    return m_forceUseLocalProperties || getId().isNull();
}

void QnResource::updateInternal(const QnResourcePtr& source, NotifierList& notifiers)
{
    NX_ASSERT(getId() == source->getId() || getId().isNull());
    NX_ASSERT(toSharedPointer(this));

    m_typeId = source->m_typeId;

    if (m_url != source->m_url)
    {
        m_url = source->m_url;
        notifiers << [r = toSharedPointer(this)] { emit r->urlChanged(r); };
    }

    if (m_flags != source->m_flags)
    {
        m_flags = source->m_flags;
        notifiers << [r = toSharedPointer(this)] { emit r->flagsChanged(r); };
    }

    if (m_name != source->m_name)
    {
        m_name = source->m_name;
        notifiers << [r = toSharedPointer(this)] { emit r->nameChanged(r); };
    }

    if (m_parentId != source->m_parentId)
    {
        const auto previousParentId = m_parentId;
        m_parentId = source->m_parentId;
        notifiers <<
            [r = toSharedPointer(this), previousParentId]
            {
                emit r->parentIdChanged(r, previousParentId);
            };
    }

    if (useLocalProperties())
    {
        for (const auto& p: source->getProperties())
            m_locallySavedProperties.emplace(p.name, p.value);
    }
}

void QnResource::update(const QnResourcePtr& source)
{
    NotifierList notifiers;
    {
        // Maintain mutex lock order.
        nx::Mutex* m1 = &m_mutex;
        nx::Mutex* m2 = &source->m_mutex;
        if (m1 > m2)
            std::swap(m1, m2);
        NX_MUTEX_LOCKER mutexLocker1(m1);
        NX_MUTEX_LOCKER mutexLocker2(m2);
        updateInternal(source, notifiers);
    }

    for (const auto& notifier: notifiers)
        notifier();
}

nx::Uuid QnResource::getParentId() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_parentId;
}

void QnResource::setParentId(const nx::Uuid& parent)
{
    nx::Uuid previousParentId;
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        if (m_parentId == parent)
            return;

        previousParentId = m_parentId;
        m_parentId = parent;
    }

    emit parentIdChanged(toSharedPointer(this), previousParentId);
}

QString QnResource::getName() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return m_name;
}

void QnResource::setName(const QString& name)
{
    {
        NX_MUTEX_LOCKER mutexLocker(&m_mutex);

        if (m_name == name)
            return;

        m_name = name;
    }

    emit nameChanged(toSharedPointer(this));
}

Qn::ResourceFlags QnResource::flags() const
{
    // A mutex is not needed, the value is atomically read.
    return m_flags;
}

void QnResource::setFlags(Qn::ResourceFlags flags)
{
    {
        NX_MUTEX_LOCKER mutexLocker(&m_mutex);
        if (m_flags == flags)
            return;

        m_flags = flags;
    }
    emit flagsChanged(toSharedPointer(this));
}

void QnResource::addFlags(Qn::ResourceFlags flags)
{
    {
        NX_MUTEX_LOCKER mutexLocker(&m_mutex);
        flags |= m_flags;
        if (m_flags == flags)
            return;

        m_flags = flags;
    }
    emit flagsChanged(toSharedPointer(this));
}

void QnResource::removeFlags(Qn::ResourceFlags flags)
{
    {
        NX_MUTEX_LOCKER mutexLocker(&m_mutex);
        flags = m_flags & ~flags;
        if (m_flags == flags)
            return;

        m_flags = flags;
    }
    emit flagsChanged(toSharedPointer(this));
}

QnResourcePtr QnResource::getParentResource() const
{
    if (auto* const resourcePool = this->resourcePool())
        return resourcePool->getResourceById(getParentId());

    return {};
}

nx::Uuid QnResource::getTypeId() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return m_typeId;
}

void QnResource::setTypeId(const nx::Uuid& id)
{
    if (!NX_ASSERT(!id.isNull()))
        return;

    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    m_typeId = id;
}

nx::vms::api::ResourceStatus QnResource::getStatus() const
{
    if (auto* context = systemContext())
    {
        auto* const statusDictionary = context->resourceStatusDictionary();
        if (statusDictionary != nullptr)
            return statusDictionary->value(getId());
    }
    return ResourceStatus::undefined;
}

nx::vms::api::ResourceStatus QnResource::getPreviousStatus() const
{
    return m_previousStatus;
}

void QnResource::setStatus(ResourceStatus newStatus, Qn::StatusChangeReason reason)
{
    if (newStatus == ResourceStatus::undefined)
    {
        NX_VERBOSE(this, "Won't change status of resource %1 (%2) because it is 'undefined'",
            getId(), nx::utils::url::hidePassword(getUrl()));
        return;
    }

    if (hasFlags(Qn::removed))
    {
        NX_VERBOSE(this, "Won't change status of resource %1 (%2) because it has a 'removed' flag",
            getId(), nx::utils::url::hidePassword(getUrl()));
        return;
    }

    auto* context = systemContext();
    if (!NX_ASSERT(context))
        return;

    nx::Uuid id = getId();
    ResourceStatus oldStatus = context->resourceStatusDictionary()->value(id);
    if (oldStatus == newStatus)
    {
        NX_VERBOSE(this, "Won't change status of resource %1 (%2) because status (%3) hasn't changed",
            getId(), nx::utils::url::hidePassword(getUrl()), newStatus);
        return;
    }

    NX_DEBUG(this,
        "Status changed %1 -> %2, reason=%3, name=[%4], url=[%5]",
        oldStatus,
        newStatus,
        reason,
        getName(),
        nx::utils::url::hidePassword(getUrl()));
    m_previousStatus = oldStatus;
    context->resourceStatusDictionary()->setValue(id, newStatus);

    // Null pointer if we are changing status in constructor. Signal is not needed in this case.
    if (auto sharedThis = toSharedPointer(this))
    {
        NX_VERBOSE(this, "Signal status change for %1", newStatus);
        emit statusChanged(sharedThis, reason);
    }
}

void QnResource::setIdUnsafe(const nx::Uuid& id)
{
    m_id = id;
}

QString QnResource::getUrl() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return m_url;
}

void QnResource::setUrl(const QString& url)
{
    {
        NX_MUTEX_LOCKER mutexLocker(&m_mutex);
        if (!setUrlUnsafe(url))
            return;
    }

    emit urlChanged(toSharedPointer(this));
}

QString QnResource::getProperty(const QString& key) const
{
    if (useLocalProperties())
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        return getLocalPropertyUnsafe(key);
    }
    else if (auto* context = systemContext(); context && context->resourcePropertyDictionary())
    {
        return context->resourcePropertyDictionary()->value(getId(), key);
    }

    return {};
}

bool QnResource::setProperty(const QString& key, const QString& value, bool markDirty)
{
    if (useLocalProperties())
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        return setLocalPropertyUnsafe(key, value);
    }

    NX_ASSERT(!getId().isNull());
    if (auto* const context = systemContext(); NX_ASSERT(context))
    {
        const auto prevValue = context->resourcePropertyDictionary()->setValue(
            getId(),
            key,
            value,
            markDirty);

        if (prevValue.has_value())
            emitPropertyChanged(key, prevValue.value(), value);

        return prevValue.has_value();
    }

    return false;
}

bool QnResource::updateProperty(
    const QString& key,
    std::function<QString(QString)> updater,
    bool markDirty)
{
    if (!NX_ASSERT(updater))
        return false;

    if (useLocalProperties())
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        return setLocalPropertyUnsafe(key, updater(getLocalPropertyUnsafe(key)));
    }

    if (auto* context = systemContext(); context && context->resourcePropertyDictionary())
    {
        const auto [prevValue, newValue] = context->resourcePropertyDictionary()->updateValue(
            getId(),
            key,
            updater,
            markDirty);

        const bool valueChanged = prevValue != newValue;
        if (valueChanged)
            emitPropertyChanged(key, prevValue, newValue);

        return valueChanged;
    }

    return false;
}

void QnResource::emitPropertyChanged(
    const QString& key, const QString& prevValue, const QString& newValue)
{
    if (key == nx::vms::api::device_properties::kVideoLayout)
        emit videoLayoutChanged(::toSharedPointer(this));

    NX_VERBOSE(this,
        "Changed property '%1' = '%2'",
        key,
        hidePasswordIfCredentialsPropety(key, newValue));
    emit propertyChanged(toSharedPointer(this), key, prevValue, newValue);
}

bool QnResource::setUrlUnsafe(const QString& value)
{
    if (m_url == value)
        return false;

    m_url = value;
    return true;
}

bool QnResource::setLocalPropertyUnsafe(const QString& key, const QString& value)
{
    auto [it, added] = m_locallySavedProperties.emplace(key, value);
    if (!added)
    {
        if (it->second == value)
            return false;
        it->second = value;
    }
    return true;
}

QString QnResource::getLocalPropertyUnsafe(const QString& key) const
{
    if (auto it = m_locallySavedProperties.find(key); it != m_locallySavedProperties.end())
        return it->second;
    return {};
}

nx::vms::api::ResourceParamDataList QnResource::getProperties() const
{
    if (useLocalProperties())
    {
        nx::vms::api::ResourceParamDataList result;
        for (const auto& prop: m_locallySavedProperties)
            result.emplace_back(prop.first, prop.second);
        return result;
    }

    if (auto* const context = systemContext())
        return context->resourcePropertyDictionary()->allProperties(getId());

    return {};
}

bool QnResource::saveProperties()
{
    NX_ASSERT(systemContext() && !getId().isNull());
    if (auto* context = systemContext())
        return context->resourcePropertyDictionary()->saveParams(getId());
    return false;
}

void QnResource::savePropertiesAsync()
{
    NX_ASSERT(systemContext() && !getId().isNull());
    if (auto* context = systemContext())
        context->resourcePropertyDictionary()->saveParamsAsync(getId());
}

// -----------------------------------------------------------------------------

void QnResource::setSystemContext(nx::vms::common::SystemContext* systemContext)
{
    m_systemContext = systemContext;
}

nx::vms::common::SystemContext* QnResource::systemContext() const
{
    if (auto* systemContext = m_systemContext.load())
        return systemContext;

    return nullptr;
}

QString QnResource::idForToStringFromPtr() const
{
    return NX_FMT("%1: %2", getId().toSimpleString(), getName());
}
