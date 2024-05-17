// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "initializer.h"

#include <nx/vms/common/system_context.h>
#include <nx/vms/rules/action_builder_fields/builtin_fields.h>
#include <nx/vms/rules/actions/builtin_actions.h>
#include <nx/vms/rules/event_filter_fields/builtin_fields.h>
#include <nx/vms/rules/events/builtin_events.h>

#include "action_builder_field_validators/optional_time_field_validator.h"
#include "action_builder_field_validators/target_user_field_validator.h"
#include "event_filter_field_validators/source_user_field_validator.h"

namespace nx::vms::rules {

Initializer::Initializer(nx::vms::common::SystemContext* context):
    SystemContextAware(context)
{
}

Initializer::~Initializer()
{
}

void Initializer::registerEvents() const
{
    auto context = systemContext();

    // Register built-in events.
    registerEvent<AnalyticsEvent>();
    registerEvent<AnalyticsObjectEvent>();
    registerEvent<BackupFinishedEvent>();
    registerEvent<CameraInputEvent>(context);
    registerEvent<DeviceDisconnectedEvent>(context);
    registerEvent<DeviceIpConflictEvent>(context);
    registerEvent<FanErrorEvent>();
    registerEvent<GenericEvent>();
    registerEvent<LdapSyncIssueEvent>();
    registerEvent<LicenseIssueEvent>();
    registerEvent<SaasIssueEvent>();
    registerEvent<MotionEvent>();
    registerEvent<NetworkIssueEvent>();
    registerEvent<PluginDiagnosticEvent>();
    registerEvent<PoeOverBudgetEvent>();
    registerEvent<ServerCertificateErrorEvent>();
    registerEvent<ServerConflictEvent>();
    registerEvent<ServerFailureEvent>();
    registerEvent<ServerStartedEvent>();
    registerEvent<SoftTriggerEvent>();
    registerEvent<StorageIssueEvent>();
}

void Initializer::registerActions() const
{
    // Register built-in actions.
    registerAction<AcknowledgeAction>();
    registerAction<BuzzerAction>();
    registerAction<BookmarkAction>();
    registerAction<DeviceOutputAction>();
    registerAction<DeviceRecordingAction>();
    registerAction<EnterFullscreenAction>();
    registerAction<ExitFullscreenAction>();
    registerAction<HttpAction>();
    registerAction<NotificationAction>();
    registerAction<OpenLayoutAction>();
    registerAction<PanicRecordingAction>();
    registerAction<PlaySoundAction>();
    registerAction<PtzPresetAction>();
    registerAction<PushNotificationAction>();
    registerAction<RepeatSoundAction>();
    registerAction<SendEmailAction>();
    registerAction<ShowOnAlarmLayoutAction>();
    registerAction<SpeakAction>();
    registerAction<TextOverlayAction>();
    registerAction<WriteToLogAction>();
}

void Initializer::registerFields() const
{
    registerEventField<AnalyticsEngineField>();
    registerEventField<AnalyticsEventLevelField>();
    m_engine->registerEventField(
        fieldMetatype<AnalyticsEventTypeField>(),
        [this](const FieldDescriptor* descriptor)
        {
            return new AnalyticsEventTypeField(this->m_context, descriptor);
        });
    registerEventField<AnalyticsObjectAttributesField>();
    m_engine->registerEventField(
        fieldMetatype<AnalyticsObjectTypeField>(),
        [this](const FieldDescriptor* descriptor)
        {
            return new AnalyticsObjectTypeField(this->m_context, descriptor);
        });
    registerEventField<CustomizableFlagField>();
    registerEventField<CustomizableIconField>();
    registerEventField<CustomizableTextField>();
    registerEventField<DummyField>();
    registerEventField<EventFlagField>();
    registerEventField<EventTextField>();
    registerEventField<ExpectedUuidField>();
    registerEventField<InputPortField>();
    registerEventField<IntField>();
    registerEventField<KeywordsField>();
    m_engine->registerEventField(
        fieldMetatype<ObjectLookupField>(),
        [this](const FieldDescriptor* descriptor)
        {
            return new ObjectLookupField(this->m_context, descriptor);
        });
    registerEventField<SourceCameraField>();
    registerEventField<SourceServerField>();
    m_engine->registerEventField(
        fieldMetatype<SourceUserField>(),
        [this](const FieldDescriptor* descriptor)
        {
            return new SourceUserField(systemContext(), descriptor);
        });
    registerEventField<StateField>();
    m_engine->registerEventField(
        fieldMetatype<TextLookupField>(),
        [this](const FieldDescriptor* descriptor)
        {
            return new TextLookupField(this->m_context, descriptor);
        });
    registerEventField<UniqueIdField>();

    registerActionField<ActionIntField>();
    registerActionField<ActionTextField>();
    registerActionField<ActionFlagField>();
    registerActionField<FpsField>();
    registerActionField<ContentTypeField>();
    m_engine->registerActionField(
        fieldMetatype<EmailMessageField>(),
        [this](const FieldDescriptor* descriptor)
        {
            return new EmailMessageField(this->m_context, descriptor);
        });
    m_engine->registerActionField(
        fieldMetatype<ExtractDetailField>(),
        [this](const FieldDescriptor* descriptor)
        {
            return new ExtractDetailField(this->m_context, descriptor);
        });
    registerActionField<EventIdField>();
    registerActionField<EventDevicesField>();
    registerActionField<HttpAuthTypeField>();
    registerActionField<HttpMethodField>();
    registerActionField<LayoutField>();
    registerActionField<OptionalTimeField>();
    registerActionField<OutputPortField>();
    registerActionField<PasswordField>();
    registerActionField<PtzPresetField>();
    registerActionField<SoundField>();
    registerActionField<StreamQualityField>();
    registerActionField<TargetDeviceField>();
    registerActionField<TargetServerField>();
    m_engine->registerActionField(
        fieldMetatype<TargetUserField>(),
        [this](const FieldDescriptor* descriptor)
        {
            return new TargetUserField(this->m_context, descriptor);
        });
    m_engine->registerActionField(
        fieldMetatype<TextFormatter>(),
        [this](const FieldDescriptor* descriptor)
        {
            return new TextFormatter(this->m_context, descriptor);
        });
    m_engine->registerActionField(
        fieldMetatype<TextWithFields>(),
        [this](const FieldDescriptor* descriptor)
        {
            return new TextWithFields(this->m_context, descriptor);
        });
    registerActionField<Substitution>();
    registerActionField<TargetLayoutField>();
    registerActionField<TargetSingleDeviceField>();
    registerActionField<TimeField>();
    registerActionField<VolumeField>();
    registerActionField<HttpAuthField>();
}

void Initializer::registerFieldValidators() const
{
    // Event field validators.
    registerFieldValidator<SourceUserField>(new SourceUserFieldValidator);

    // Action field validators.
    registerFieldValidator<OptionalTimeField>(new OptionalTimeFieldValidator);
    registerFieldValidator<TargetUserField>(new TargetUserFieldValidator);
}

} // namespace nx::vms::rules
