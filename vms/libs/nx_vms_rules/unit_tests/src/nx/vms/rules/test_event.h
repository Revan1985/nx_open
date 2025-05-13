// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <analytics/common/object_metadata.h>
#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/rules/basic_event.h>
#include <nx/vms/rules/camera_conflict_list.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/event_filter_fields/state_field.h>
#include <nx/vms/rules/utils/event_details.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/type.h>

namespace nx::vms::rules::test {

// Instant test event with camera, no permissions.
class TestEventInstant: public nx::vms::rules::BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "testInstant")

    Q_PROPERTY(nx::Uuid deviceId MEMBER m_deviceId)
public:
    static ItemDescriptor manifest()
    {
        return ItemDescriptor{
            .id = utils::type<TestEventInstant>(),
            .displayName = TranslatableString("Test event"),
            .flags = {ItemFlag::instant},
            .fields = {
                makeFieldDescriptor<SourceCameraField>(
                    utils::kDeviceIdFieldName,
                    nx::TranslatableString("Camera id"),
                    {},
                    {{"acceptAll", true}}),
            }
        };
    }

    TestEventInstant()
    {
    }

    TestEventInstant(std::chrono::microseconds timestamp, State state = State::instant):
        BasicEvent(timestamp, state)
    {
    }

    virtual QString aggregationKey() const override
    {
        return m_deviceId.toSimpleString();
    }

    virtual QString cacheKey() const override
    {
        return m_cacheKey;
    }

    void setCacheKey(const QString& cacheKey)
    {
        m_cacheKey = cacheKey;
    }

    nx::Uuid m_deviceId;
    QString m_cacheKey;
};

// Test event with permissions. No filter fields. May be used for field/details format tests.
class TestEvent: public nx::vms::rules::BasicEvent
{
    using base_type = BasicEvent;
    Q_OBJECT
    Q_CLASSINFO("type", "testPermissions")

    Q_PROPERTY(nx::Uuid serverId MEMBER m_serverId)
    Q_PROPERTY(nx::Uuid deviceId MEMBER m_deviceId)
    Q_PROPERTY(UuidList deviceIds MEMBER m_deviceIds)

    Q_PROPERTY(int intField MEMBER m_intField)
    Q_PROPERTY(QString text MEMBER m_text)
    Q_PROPERTY(bool boolField MEMBER m_boolField)
    Q_PROPERTY(double floatField MEMBER m_floatField)

    Q_PROPERTY(nx::common::metadata::Attributes attributes MEMBER attributes)
    Q_PROPERTY(nx::vms::event::Level level MEMBER level)
    Q_PROPERTY(nx::vms::api::EventReason reason MEMBER reason)
    Q_PROPERTY(nx::vms::rules::CameraConflictList conflicts MEMBER conflicts)

public:
    static ItemDescriptor manifest()
    {
        return ItemDescriptor{
            .id = utils::type<TestEvent>(),
            .displayName = TranslatableString("Test event with permissions"),
            .description = TranslatableString{"Test Event Description."},
            .flags = {ItemFlag::instant, ItemFlag::prolonged},
            .resources = {
                {rules::utils::kDeviceIdFieldName, {ResourceType::device, Qn::ViewContentPermission}},
                {rules::utils::kDeviceIdsFieldName, {ResourceType::device, Qn::ViewContentPermission}}},
            .readPermissions = nx::vms::api::GlobalPermission::viewLogs,
        };
    }

    TestEvent()
    {
    }

    TestEvent(std::chrono::microseconds timestamp, State state = State::instant):
        BasicEvent(timestamp, state)
    {
    }

    virtual QString aggregationKey() const override
    {
        return m_deviceId.toSimpleString();
    }

    virtual QVariantMap details(
        common::SystemContext* context,
        Qn::ResourceInfoLevel detailLevel) const override
    {
        auto result = base_type::details(context, detailLevel);
        result[utils::kSourceTextDetailName] = "Test resource";
        nx::vms::rules::utils::insertLevel(result, level);
        nx::vms::rules::utils::insertIcon(result, nx::vms::rules::Icon::alert);
        result[nx::vms::rules::utils::kCustomIconDetailName] = "test";
        nx::vms::rules::utils::insertClientAction(result, nx::vms::rules::ClientAction::none);
        result[nx::vms::rules::utils::kUrlDetailName] = "http://localhost";
        result[utils::kDetailingDetailName] = QStringList{"line 1", "line 2"};
        result["number"] = m_intField;
        result["stdString"] = QVariant::fromValue(m_text.toStdString());

        return result;
    }

    nx::Uuid m_serverId;
    nx::Uuid m_deviceId;
    UuidList m_deviceIds;

    int m_intField{};
    QString m_text;
    bool m_boolField{};
    double m_floatField{};

    nx::common::metadata::Attributes attributes;
    nx::vms::event::Level level = {};
    nx::vms::api::EventReason reason = {};
    nx::vms::rules::CameraConflictList conflicts;
};

class TestEventProlonged : public nx::vms::rules::BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "testProlonged")

    Q_PROPERTY(nx::Uuid deviceId MEMBER m_deviceId)

public:
    TestEventProlonged()
    {
    }

    TestEventProlonged(std::chrono::microseconds timestamp, State state = State::instant):
        BasicEvent(timestamp, state)
    {
    }

    static ItemDescriptor manifest()
    {
        return ItemDescriptor{
            .id = utils::type<TestEventProlonged>(),
            .displayName = nx::TranslatableString("Test prolonged event"),
            .flags = {ItemFlag::prolonged},
            .fields = {
                makeFieldDescriptor<StateField>(
                    utils::kStateFieldName, nx::TranslatableString("State field")),
                makeFieldDescriptor<SourceCameraField>(
                    utils::kDeviceIdFieldName,
                    nx::TranslatableString("Camera id"),
                    {},
                    ResourceFilterFieldProperties{
                        .acceptAll = true
                    }.toVariantMap()),
            }
        };
    }

    virtual QString aggregationKey() const override
    {
        return m_deviceId.toSimpleString();
    }

    nx::Uuid m_deviceId;
};

using TestEventInstantPtr = QSharedPointer<TestEventInstant>;
using TestEventPtr = QSharedPointer<TestEvent>;
using TestEventProlongedPtr = QSharedPointer<TestEventProlonged>;

} // namespace nx::vms::rules::test
