// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QStringList>

#include <analytics/common/object_metadata.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/event/event_fwd.h>

namespace nx {
namespace vms {
namespace event {

/**%apidoc
 * Additional information associated with the Event.
 * <br/><br/>
 * ATTENTION: This field is an enquoted JSON string containing a JSON object. Example:
 * <code>"{\\"field1\\": \\"value1\\", \\"field2\\": \\"value2\\"}"</code>
 * <br/><br/>
 * Currently supported fields (others can be added in the future):
 * <ul>
 *     <li>"cameraRefs" - Specifies the list of Devices which are linked to the Event (e.g.
 *     the Event will appear on their timelines), in the form of a list of Device ids (can
 *     be obtained from "id" field via /ec2/getCamerasEx).</li>
 * </ul>
 */
struct EventMetaData
{
    /**
     * Camera list which is associated with the event. EventResourceId may be a POS terminal, but
     * this is a camera list which should be shown with this event.
     */
    std::vector<QString> cameraRefs;

    //! Users that can generate this event.
    std::vector<nx::Uuid> instigators;
    bool allUsers = false;
    nx::vms::api::EventLevel level = nx::vms::api::EventLevel::undefined;

    EventMetaData() = default;
    EventMetaData(const EventMetaData&) = default;
    EventMetaData(EventMetaData&&) = default;
    EventMetaData& operator= (const EventMetaData&) = default;
    EventMetaData& operator= (EventMetaData&&) = default;
    bool operator==(const EventMetaData& other) const = default;
};

#define EventMetaData_Fields (cameraRefs)(instigators)(allUsers)(level)
QN_FUSION_DECLARE_FUNCTIONS(EventMetaData, (ubjson)(json)(xml)(csv_record), NX_VMS_COMMON_API)

NX_REFLECTION_INSTRUMENT(EventMetaData, EventMetaData_Fields)

struct NX_VMS_COMMON_API EventParameters
{
    /**%apidoc[opt]
     * Type of the Event to be created. The default value is
     * <code>userDefinedEvent</code>
     */
    EventType eventType = EventType::undefinedEvent;

    /**%apidoc[opt]
     * Event date and time, as a string containing time in
     * milliseconds since epoch, or a local time formatted like
     * <code>"<i>YYYY</i>-<i>MM</i>-<i>DD</i>T<i>HH</i>:<i>mm</i>:<i>ss</i>.<i>zzz</i>"</code> -
     * the format is auto-detected. If "timestamp" is absent, the current Server date and time
     * is used.
     */
    qint64 eventTimestampUsec = 0;

    /**%apidoc[opt] Event source - id of a Device, or a Server, or a PIR. */
    nx::Uuid eventResourceId;

    /**%apidoc[opt]
     * Name of an arbitrary resource that caused the event. Useful when there is no actual
     * Resource to fill `eventResourceId`. Used when `eventType` equals to `userDefinedEvent` only.
     */
    QString resourceName;

    /**%apidoc[opt] Id of a Server that generated the Event. */
    nx::Uuid sourceServerId;

    /**%apidoc[opt] Used for Reasoned Events as reason code. */
    EventReason reasonCode = EventReason::none;

    /**%apidoc[opt] Used for Input Events only. Identifies the input port.
     * %// TODO: Refactor: inputPortId should not be used for analytics event type id.
     */
    QString inputPortId;

    /**%apidoc[opt]
     * Short Event description. It can be used in a filter in Event Rules to assign actions
     * depending on this text.
     */
    QString caption;

    /**%apidoc[opt]
     * Long Event description. It can be used as a filter in Event Rules to assign actions
     * depending on this text.
     */
    QString description;

    /**%apidoc[opt] */
    EventMetaData metadata;

    /**%apidoc[opt]
     * Flag allows to omit event logging to DB on the server.
     * This event still triggers user notifications
     */
    bool omitDbLogging = false;

    /**%apidoc[opt]
     * String with amount of LDAP sync interval if any, in seconds. Used when `eventType` equals to
     * `ldapSyncIssueEvent` only.
     */
    QString ldapSyncIntervalS;

    /**%apidoc[opt] */
    nx::Uuid analyticsEngineId;

    /**%apidoc[opt]
     * Used for Analytics Events.
     * Takes part in ExternalUniqueKey along with EventTypeId.
     */
    nx::Uuid objectTrackId;

    /**%apidoc[opt]
     * Used for Analytics Events.
     * Makes an additional component in ExternalUniqueKey.
     */
    QString key;

    /**%apidoc[opt] */
    std::vector<nx::common::metadata::Attribute> attributes;

    /**%apidoc[opt]
     * Used for INVIF Profile G archive synchronization progress.
     */
    qreal progress = 0;

    bool operator==(const EventParameters& other) const = default;

    // TODO: #sivanov #vkutin #rvasilenko Consider implementing via std::variant or similar.
    QString getAnalyticsEventTypeId() const;
    void setAnalyticsEventTypeId(const QString& id);

    QString getAnalyticsObjectTypeId() const;
    void setAnalyticsObjectTypeId(const QString& id);

    nx::Uuid getAnalyticsEngineId() const;
    void setAnalyticsEngineId(const nx::Uuid& id);

    QString getTriggerName() const;
    void setTriggerName(const QString& name);
    QString getTriggerIcon() const;
    void setTriggerIcon(const QString& icon);

    nx::vms::api::EventLevels getDiagnosticEventLevels() const;
    void setDiagnosticEventLevels(nx::vms::api::EventLevels levels);

    /** Hash for events aggregation. */
    nx::Uuid getParamsHash() const;

    bool canHaveVideoLink() const;
};

#define EventParameters_Fields \
    (eventType)(eventTimestampUsec)(eventResourceId)(resourceName)(sourceServerId) \
    (reasonCode)(inputPortId)(caption)(description)(metadata)(omitDbLogging)(ldapSyncIntervalS) \
    (analyticsEngineId)(objectTrackId)(key)(attributes)(progress)
QN_FUSION_DECLARE_FUNCTIONS(EventParameters, (ubjson)(json)(xml)(csv_record), NX_VMS_COMMON_API);

NX_REFLECTION_INSTRUMENT(EventParameters, EventParameters_Fields)

bool checkForKeywords(const QString& value, const QString& keywords);

} // namespace event
} // namespace vms
} // namespace nx
