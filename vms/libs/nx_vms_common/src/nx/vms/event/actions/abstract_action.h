// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QFlags>
#include <QtCore/QSharedPointer>
#include <QtCore/QVector>

#include <core/resource/resource_fwd.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/event_parameters.h>
#include <utils/common/id.h>

class QnResourcePool;

namespace nx::vms::event {

enum class ActionFlag {
    canUseSourceCamera = 1 << 0,

    // Object Detection is registered as an instant event, which is repeated once per 3 seconds
    // starting 5.1.4. To avoid multiple actions triggered for the single object, there is a
    // workaround implemented: each rule is triggered only once per Object Track Id. But this
    // workaround effectively prevents prolonged actions like Camera Recording or Camera Output
    // from working correctly when user wants "Record while object is detected" scenarios. Thus we
    // need another workaround level to skip "once per rule" check for some types of actions.
    canProlongateInstantAction = 1 << 1,
};

Q_DECLARE_FLAGS(ActionFlags, ActionFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(ActionFlags)

NX_VMS_COMMON_API ActionFlags getFlags(ActionType actionType);

// TODO: #sivanov Fix to resourceTypeRequired: None, Camera, Server, User, etc.
NX_VMS_COMMON_API bool requiresCameraResource(ActionType actionType);
NX_VMS_COMMON_API bool requiresUserResource(ActionType actionType);
NX_VMS_COMMON_API bool requiresServerResource(ActionType actionType);
NX_VMS_COMMON_API bool requiresAdditionalUserResource(ActionType actionType);

NX_VMS_COMMON_API bool canProlongateInstantAction(ActionType actionType);
NX_VMS_COMMON_API bool hasToggleState(ActionType actionType);
NX_VMS_COMMON_API bool canBeInstant(ActionType actionType);
NX_VMS_COMMON_API bool supportsDuration(ActionType actionType);
NX_VMS_COMMON_API bool allowsAggregation(ActionType actionType);
NX_VMS_COMMON_API bool canUseSourceCamera(ActionType actionType);

NX_VMS_COMMON_API bool isActionProlonged(ActionType actionType, const ActionParameters &parameters);

NX_VMS_COMMON_API QList<ActionType> userAvailableActions();
NX_VMS_COMMON_API QList<ActionType> allActions();

/**
 * Base class for business actions
 */
class NX_VMS_COMMON_API AbstractAction
{
protected:
    explicit AbstractAction(const ActionType actionType, const EventParameters& runtimeParams);

public:
    virtual ~AbstractAction();

    ActionType actionType() const;

    /**
     * Resource depend of action type.
     * see: requiresCameraResource()
     * see: requiresUserResource()
     */
    void setResources(const QVector<nx::Uuid>& resources);

    const QVector<nx::Uuid>& getResources() const;

    /** Source resource of the action (including custom for generic events). */
    QnResourceList getSourceResources(const QnResourcePool* resourcePool) const;

    void setParams(const ActionParameters& params);
    const ActionParameters& getParams() const;
    ActionParameters& getParams();

    void setRuntimeParams(const EventParameters& params);
    const EventParameters& getRuntimeParams() const;
    EventParameters& getRuntimeParams();

    void setRuleId(const nx::Uuid& value);
    nx::Uuid getRuleId() const;

    void setToggleState(EventState value);
    EventState getToggleState() const;

    void setReceivedFromRemoteHost(bool value);
    bool isReceivedFromRemoteHost() const;

    /** How much times action was instantiated during the aggregation period. */
    void setAggregationCount(int value);
    int getAggregationCount() const;

    bool isProlonged() const;

    /** Return action unique key for external outfit (port number for output action e.t.c). Do not count resourceId
     * This function help to share physical resources between actions
     * Do not used for instant actions
     */
    virtual QString getExternalUniqKey() const;

    /** Virtual assignment operation. */
    virtual void assign(const AbstractAction& other);

protected:
    ActionType m_actionType;
    EventState m_toggleState;
    bool m_receivedFromRemoteHost;
    QVector<nx::Uuid> m_resources;
    ActionParameters m_params;
    EventParameters m_runtimeParams;
    nx::Uuid m_ruleId; // event rule that generated this action
    int m_aggregationCount;
};

} // namespace nx::vms::event
