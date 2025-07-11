// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_filter_model.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/taxonomy/abstract_resource_support_proxy.h>
#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/client/core/analytics/analytics_taxonomy_manager.h>
#include <nx/vms/client/core/analytics/taxonomy/abstract_state_view_filter.h>
#include <nx/vms/client/core/analytics/taxonomy/attribute_condition_state_view_filter.h>
#include <nx/vms/client/core/analytics/taxonomy/composite_state_view_filter.h>
#include <nx/vms/client/core/analytics/taxonomy/engine_state_view_filter.h>
#include <nx/vms/client/core/analytics/taxonomy/object_type.h>
#include <nx/vms/client/core/analytics/taxonomy/scope_state_view_filter.h>
#include <nx/vms/client/core/qml/qml_ownership.h>

namespace nx::vms::client::core::analytics::taxonomy {

namespace {

class LiveTypeFilter: public AbstractStateViewFilter
{
public:
    LiveTypeFilter(bool excludeLiveTypes, QObject* parent = nullptr);

    virtual QString id() const override;
    virtual QString name() const override { return ""; }

    virtual bool matches(
        const nx::analytics::taxonomy::ObjectType* objectType) const override;

    virtual bool matches(
        const nx::analytics::taxonomy::AbstractAttribute* attribute) const override;

private:
    bool m_liveTypesExcluded = false;
};

LiveTypeFilter::LiveTypeFilter(bool excludeLiveTypes, QObject* parent):
    AbstractStateViewFilter(parent),
    m_liveTypesExcluded(excludeLiveTypes)
{
}

QString LiveTypeFilter::id() const
{
    return QString::number(static_cast<int>(m_liveTypesExcluded));
}

bool LiveTypeFilter::matches(const nx::analytics::taxonomy::ObjectType* objectType) const
{
    return !(m_liveTypesExcluded && (objectType->isLiveOnly() || objectType->isNonIndexable()));
}

bool LiveTypeFilter::matches(const nx::analytics::taxonomy::AbstractAttribute* attribute) const
{
    return true;
}

void mapObjectTypes(
    const std::vector<taxonomy::ObjectType*>& objectTypes,
    QMap<QString, taxonomy::ObjectType*>& outObjectTypesById)
{
    if (objectTypes.empty())
        return;

    for (taxonomy::ObjectType* objectType: objectTypes)
    {
        outObjectTypesById.insert(objectType->id(), objectType);
        mapObjectTypes(objectType->derivedObjectTypes(), outObjectTypesById);
    }
}

QMap<QString, taxonomy::ObjectType*> objectTypesMap(
    const std::vector<taxonomy::ObjectType*>& objectTypes)
{
    QMap<QString, taxonomy::ObjectType*> result;
    mapObjectTypes(objectTypes, result);
    return result;
}

std::vector<nx::analytics::taxonomy::AbstractEngine*> enginesFromFilters(
    const std::vector<AbstractStateViewFilter*>& filters)
{
    std::vector<nx::analytics::taxonomy::AbstractEngine*> engines;
    for (auto filter: filters)
    {
        if (auto engineFilter = dynamic_cast<EngineStateViewFilter*>(filter))
            engines.push_back(engineFilter->engine());
    }
    return engines;
}

std::set<nx::Uuid> deviceIds(const QnVirtualCameraResourceSet& devices)
{
    std::set<nx::Uuid> ids;
    for (const QnVirtualCameraResourcePtr& device: devices)
        ids.insert(device->getId());

    return ids;
}

nx::common::metadata::Attributes filterAttributes(const QVariantMap& values)
{
    nx::common::metadata::Attributes result;
    for (const auto& [name, value]: nx::utils::constKeyValueRange(values))
        result.push_back({name, value.toString()});

    return result;
}

} // namespace

void AnalyticsFilterModel::registerQmlType()
{
    qmlRegisterUncreatableType<AnalyticsFilterModel>("nx.vms.client.core.analytics", 1, 0,
        "AnalyticsFilterModel", "Cannot create instance of AnalyticsFilterModel");
}

AnalyticsFilterModel::AnalyticsFilterModel(TaxonomyManager* taxonomyManager, QObject* parent):
    QObject(parent)
{
    setTaxonomyManager(taxonomyManager);
}

void AnalyticsFilterModel::setTaxonomyManager(TaxonomyManager* taxonomyManager)
{
    if (m_taxonomyManager == taxonomyManager)
        return;

    m_manifestsUpdatedConnection.reset();
    m_currentTaxonomyChangedConnection.reset();
    m_taxonomyManager = taxonomyManager;
    if (!taxonomyManager)
    {
        m_stateViewBuilder.reset();
        return;
    }

    auto onTaxonomyChanged =
        [this]
        {
            if (m_isActive)
                rebuild();

            const auto taxonomy = m_taxonomyManager->currentTaxonomy();

            if (!taxonomy->resourceSupportProxy())
                return;

            m_manifestsUpdatedConnection.reset(connect(
                taxonomy->resourceSupportProxy(),
                &nx::analytics::taxonomy::AbstractResourceSupportProxy::manifestsMaybeUpdated,
                this,
                [this]
                {
                    if (m_isActive)
                        rebuild();
                }));
        };

    m_currentTaxonomyChangedConnection.reset(
        connect(taxonomyManager, &TaxonomyManager::currentTaxonomyChanged,
            this, onTaxonomyChanged));

    onTaxonomyChanged();
}

std::vector<taxonomy::ObjectType*> AnalyticsFilterModel::objectTypes() const
{
    return m_objectTypes;
}

std::vector<nx::analytics::taxonomy::AbstractEngine*> AnalyticsFilterModel::engines() const
{
    return m_engines;
}

void AnalyticsFilterModel::setObjectTypes(const std::vector<taxonomy::ObjectType*>& objectTypes)
{
    const auto typesMap = objectTypesMap(objectTypes);
    if (typesMap == m_objectTypesById)
        return;

    m_objectTypes = objectTypes;
    m_objectTypesById = typesMap;
    emit objectTypesChanged();
}

void AnalyticsFilterModel::setEngines(
    const std::vector<nx::analytics::taxonomy::AbstractEngine*>& engines)
{
    if (m_engines == engines)
        return;

    m_engines = engines;
    m_enginesById.clear();

    for (const auto engine: m_engines)
        m_enginesById.insert(nx::Uuid::fromStringSafe(engine->id()), engine);

    emit enginesChanged();
}

void AnalyticsFilterModel::update(
    nx::analytics::taxonomy::AbstractEngine* engine,
    const QnVirtualCameraResourceSet& devices,
    const QVariantMap& attributeValues,
    bool liveTypesExcluded,
    bool force)
{
    if (!m_taxonomyManager)
        return;

    if (force
        || m_engine != engine
        || m_attributeValues != attributeValues
        || m_devices != devices
        || m_liveTypesExcluded != liveTypesExcluded)
    {
        if (m_engine != engine)
        {
            m_engine = engine;
            emit selectedEngineChanged();
        }

        if (devices != m_devices)
        {
            m_devices = devices;
            emit selectedDevicesChanged();
        }

        if (m_attributeValues != attributeValues)
        {
            m_attributeValues = attributeValues;
            emit selectedAttributeValuesChanged();
        }

        m_liveTypesExcluded = liveTypesExcluded;

        const auto liveTypeFilter =
            new LiveTypeFilter(m_liveTypesExcluded, m_stateViewBuilder.get());

        const auto scopeFilter = new ScopeStateViewFilter(
            m_engine.get(), deviceIds(m_devices), m_stateViewBuilder.get());

        const auto conditionFilter = new AttributeConditionStateViewFilter(
            /*baseFilter*/ nullptr, filterAttributes(m_attributeValues), m_stateViewBuilder.get());

        const auto filter = new CompositeFilter(
            std::vector<AbstractStateViewFilter*>{liveTypeFilter, scopeFilter, conditionFilter},
            m_stateViewBuilder.get());

        taxonomy::StateView* state = m_stateViewBuilder->stateView(filter);
        setObjectTypes(state->objectTypes());
    }
}

void AnalyticsFilterModel::setSelectedEngine(nx::analytics::taxonomy::AbstractEngine* engine)
{
    update(engine, m_devices, m_attributeValues, m_liveTypesExcluded);
}

nx::analytics::taxonomy::AbstractEngine* AnalyticsFilterModel::selectedEngine() const
{
    return m_engine;
}

QnVirtualCameraResourceSet AnalyticsFilterModel::selectedDevices() const
{
    return m_devices;
}

void AnalyticsFilterModel::setSelectedDevices(const QnVirtualCameraResourceSet& devices)
{
    update(m_engine, devices, m_attributeValues, m_liveTypesExcluded);
}

void AnalyticsFilterModel::setSelectedDevices(const std::set<nx::Uuid>& deviceIds)
{
    const auto pool = m_taxonomyManager->resourcePool();
    const auto devices = pool->getResourcesByIds<QnVirtualCameraResource>(deviceIds);
    setSelectedDevices(QnVirtualCameraResourceSet(devices.cbegin(), devices.cend()));
}

void AnalyticsFilterModel::setSelectedAttributeValues(const QVariantMap& values)
{
    update(m_engine, m_devices, values, m_liveTypesExcluded);
}

QVariantMap AnalyticsFilterModel::selectedAttributeValues() const
{
    return m_attributeValues;
}

void AnalyticsFilterModel::setLiveTypesExcluded(bool value)
{
    update(m_engine, m_devices, m_attributeValues, value);
}

ObjectType* AnalyticsFilterModel::objectTypeById(const QString& id) const
{
    return core::withCppOwnership(m_objectTypesById[id]);
}

ObjectType* AnalyticsFilterModel::findFilterObjectType(const QStringList& analyticsObjectTypeIds)
{
    if (analyticsObjectTypeIds.empty())
        return nullptr;

    auto currentObjectType = objectTypeById(analyticsObjectTypeIds.first());
    auto prevObjectType = currentObjectType;
    while (currentObjectType && analyticsObjectTypeIds.contains(currentObjectType->id()))
    {
        prevObjectType = currentObjectType;
        currentObjectType = currentObjectType->baseObjectType();
    }

    return prevObjectType;
}

nx::analytics::taxonomy::AbstractEngine* AnalyticsFilterModel::findEngine(
    const nx::Uuid& engineId) const
{
    return m_enginesById.value(engineId);
}

QStringList AnalyticsFilterModel::getAnalyticsObjectTypeIds(ObjectType* filterObjectType)
{
    if (!filterObjectType)
        return {};

    auto ids = filterObjectType->fullSubtreeTypeIds();

    QStringList result;
    result.reserve(ids.size() + 1);

    // Base filtered object type must present in the ids list on the first position to be able
    // to filter both by base type and any derived type.
    result.push_back(filterObjectType->id());
    std::move(
        std::make_move_iterator(ids.begin()),
        std::make_move_iterator(ids.end()),
        std::back_inserter(result)
    );

    return result;
}

bool AnalyticsFilterModel::isActive() const
{
    return m_isActive;
}

void AnalyticsFilterModel::setActive(bool value)
{
    if (m_isActive != value)
    {
        m_isActive = value;
        emit activeChanged();

        if (m_isActive)
            rebuild();
    }
}

void AnalyticsFilterModel::setSelected(
    nx::analytics::taxonomy::AbstractEngine* engine,
    const QVariantMap& attributeValues)
{
    update(engine, m_devices, attributeValues, m_liveTypesExcluded);
}

void AnalyticsFilterModel::rebuild()
{
    if (!m_taxonomyManager)
        return;

    auto newStateViewBuilder =
        std::make_unique<taxonomy::StateViewBuilder>(m_taxonomyManager->currentTaxonomy());

    // Swap to avoid destroying objects used in QML during update.
    std::swap(m_stateViewBuilder, newStateViewBuilder);

    setEngines(enginesFromFilters(m_stateViewBuilder->engineFilters()));
    update(m_engine, m_devices, m_attributeValues, m_liveTypesExcluded, /*force*/ true);
}

} // namespace nx::vms::client::core::analytics::taxonomy
