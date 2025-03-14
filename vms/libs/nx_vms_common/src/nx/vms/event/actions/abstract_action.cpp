// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_action.h"

#include <QtCore/QCoreApplication>

#include <api/helpers/camera_id_helper.h>
#include <core/resource/camera_resource.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/event/helpers.h>

namespace nx::vms::event {

bool requiresUserResource(ActionType actionType)
{
    switch (actionType)
    {
        case ActionType::undefinedAction:
        case ActionType::panicRecordingAction:
        case ActionType::cameraOutputAction:
        case ActionType::bookmarkAction:
        case ActionType::cameraRecordingAction:
        case ActionType::diagnosticsAction:
        case ActionType::showPopupAction:
        case ActionType::pushNotificationAction:
        case ActionType::playSoundOnceAction:
        case ActionType::playSoundAction:
        case ActionType::sayTextAction:
        case ActionType::executePtzPresetAction:
        case ActionType::showTextOverlayAction:
        case ActionType::showOnAlarmLayoutAction:
        case ActionType::execHttpRequestAction:
        case ActionType::openLayoutAction:
        case ActionType::fullscreenCameraAction:
        case ActionType::exitFullscreenAction:
        case ActionType::buzzerAction:
            return false;

        case ActionType::acknowledgeAction:
        case ActionType::sendMailAction:
            return true;

        default:
            NX_ASSERT(false, "All action types must be handled.");
            return false;
    }
}

bool hasToggleState(ActionType actionType)
{
    switch (actionType)
    {
        case ActionType::undefinedAction:
        case ActionType::sendMailAction:
        case ActionType::diagnosticsAction:
        case ActionType::showPopupAction:
        case ActionType::pushNotificationAction:
        case ActionType::playSoundOnceAction:
        case ActionType::sayTextAction:
        case ActionType::executePtzPresetAction:
        case ActionType::showOnAlarmLayoutAction:
        case ActionType::execHttpRequestAction:
        case ActionType::acknowledgeAction:
        case ActionType::openLayoutAction:
        case ActionType::fullscreenCameraAction:
        case ActionType::exitFullscreenAction:
            return false;

        case ActionType::cameraOutputAction:
        case ActionType::cameraRecordingAction:
        case ActionType::panicRecordingAction:
        case ActionType::playSoundAction:
        case ActionType::bookmarkAction:
        case ActionType::showTextOverlayAction:
        case ActionType::buzzerAction:
            return true;

        default:
            NX_ASSERT(false, "Unhandled action type: %1", actionType);
            break;
    }
    return false;
}

bool canBeInstant(ActionType actionType)
{
    if (!hasToggleState(actionType))
        return true;

    return supportsDuration(actionType);
}

bool supportsDuration(ActionType actionType)
{
    switch (actionType)
    {
        case ActionType::bookmarkAction:
        case ActionType::showTextOverlayAction:
        case ActionType::cameraOutputAction:
        case ActionType::cameraRecordingAction:
        case ActionType::buzzerAction:
            NX_ASSERT(hasToggleState(actionType),
                "Action %1 should have toggle state to support duration", actionType);
            return true;
        default:
            return false;
    }
}

bool allowsAggregation(ActionType actionType)
{
    switch (actionType)
    {
        case ActionType::bookmarkAction:
        case ActionType::showTextOverlayAction:
        case ActionType::cameraOutputAction:
        case ActionType::playSoundAction:
        case ActionType::fullscreenCameraAction:
        case ActionType::exitFullscreenAction:
            return false;

        default:
            return true;
    }
}

bool isActionProlonged(ActionType actionType, const ActionParameters &parameters)
{
    if (supportsDuration(actionType))
        return parameters.durationMs <= 0;

    return hasToggleState(actionType);
}

//-------------------------------------------------------------------------------------------------

AbstractAction::AbstractAction(const ActionType actionType, const EventParameters& runtimeParams):
    m_actionType(actionType),
    m_toggleState(EventState::undefined),
    m_receivedFromRemoteHost(false),
    m_runtimeParams(runtimeParams),
    m_aggregationCount(1)
{
}

AbstractAction::~AbstractAction()
{
}

ActionType AbstractAction::actionType() const
{
    return m_actionType;
}

EventType AbstractAction::eventType() const
{
    return m_runtimeParams.eventType;
}

void AbstractAction::setResources(const QVector<nx::Uuid>& resources)
{
    m_resources = resources;
}

const QVector<nx::Uuid>& AbstractAction::getResources() const
{
    return m_resources;
}

void AbstractAction::setParams(const ActionParameters& params)
{
    m_params = params;
}

const ActionParameters& AbstractAction::getParams() const
{
    return m_params;
}

ActionParameters& AbstractAction::getParams()
{
    return m_params;
}

void AbstractAction::setRuntimeParams(const EventParameters& params)
{
    m_runtimeParams = params;
}

const EventParameters& AbstractAction::getRuntimeParams() const
{
    return m_runtimeParams;
}

EventParameters& AbstractAction::getRuntimeParams()
{
    return m_runtimeParams;
}

void AbstractAction::setRuleId(const nx::Uuid& value)
{
    m_ruleId = value;
}

nx::Uuid AbstractAction::getRuleId() const
{
    return m_ruleId;
}

void AbstractAction::setToggleState(EventState value)
{
    m_toggleState = value;
}

EventState AbstractAction::getToggleState() const
{
    return m_toggleState;
}

void AbstractAction::setReceivedFromRemoteHost(bool value)
{
    m_receivedFromRemoteHost = value;
}

bool AbstractAction::isReceivedFromRemoteHost() const
{
    return m_receivedFromRemoteHost;
}

int AbstractAction::getAggregationCount() const
{
    return m_aggregationCount;
}

bool AbstractAction::isProlonged() const
{
    return isActionProlonged(m_actionType, m_params);
}

void AbstractAction::setAggregationCount(int value)
{
    m_aggregationCount = value;
}

QString AbstractAction::getExternalUniqKey() const
{
    return QLatin1String("action_") + QString::number(static_cast<int>(m_actionType)) + '_';
}

void AbstractAction::assign(const AbstractAction& other)
{
    (*this) = other;
}

} // namespace nx::vms::event
