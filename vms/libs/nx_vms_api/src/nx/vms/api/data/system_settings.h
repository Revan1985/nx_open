// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/serialization/qt_containers_reflect_json.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/proxy_connection_access_policy.h>

#include "backup_settings.h"
#include "client_update_settings.h"
#include "email_settings.h"
#include "persistent_update_storage.h"
#include "pixelation_settings.h"
#include "user_session_settings.h"
#include "watermark_settings.h"

namespace nx::vms::api {

struct SaveableSettingsBase
{
    std::optional<QString> defaultExportVideoCodec;
    std::optional<WatermarkSettings> watermarkSettings;
    std::optional<PixelationSettings> pixelationSettings;
    std::optional<bool> webSocketEnabled;

    std::optional<bool> autoDiscoveryEnabled;
    std::optional<bool> cameraSettingsOptimization;
    std::optional<bool> statisticsAllowed;
    std::optional<QString> cloudNotificationsLanguage;

    std::optional<bool> auditTrailEnabled;
    std::optional<bool> trafficEncryptionForced;
    std::optional<bool> useHttpsOnlyForCameras;
    std::optional<bool> videoTrafficEncryptionForced;
    std::optional<bool> storageEncryption;
    std::optional<bool> showServersInTreeForNonAdmins;

    std::optional<bool> updateNotificationsEnabled;

    std::optional<EmailSettings> emailSettings;

    std::optional<bool> timeSynchronizationEnabled;
    std::optional<nx::Uuid> primaryTimeServer;
    std::optional<nx::utils::Url> customReleaseListUrl;

    std::optional<ClientUpdateSettings> clientUpdateSettings;
    std::optional<BackupSettings> backupSettings;
    std::optional<MetadataStorageChangePolicy> metadataStorageChangePolicy;

    std::optional<bool> allowRegisteringIntegrations;

    std::optional<QString> additionalLocalFsTypes;
    std::optional<bool> arecontRtspEnabled;
    std::optional<int> auditTrailPeriodDays; //< TODO: Make std::chrono.
    std::optional<bool> autoDiscoveryResponseEnabled;
    std::optional<bool> autoUpdateThumbnails;
    std::optional<std::chrono::milliseconds> checkVideoStreamPeriodMs;
    std::optional<QString> clientStatisticsSettingsUrl;
    std::optional<bool> cloudConnectRelayingEnabled;
    std::optional<bool> cloudConnectRelayingOverSslForced;
    std::optional<bool> cloudConnectUdpHolePunchingEnabled;
    std::optional<std::chrono::seconds> cloudPollingIntervalS;
    std::optional<bool> crossdomainEnabled;
    std::optional<QByteArray> currentStorageEncryptionKey;
    std::optional<QString> defaultVideoCodec;
    std::optional<QString> disabledVendors;
    std::optional<QMap<QString, QList<nx::Uuid>>> downloaderPeers;
    std::optional<int> ec2AliveUpdateIntervalSec; //< TODO: Make std::chrono.
    std::optional<bool> enableEdgeRecording;
    std::optional<int> eventLogPeriodDays; //< TODO: Make std::chrono.
    std::optional<bool> exposeDeviceCredentials;
    std::optional<bool> exposeServerEndpoints;
    std::optional<bool> forceAnalyticsDbStoragePermissions;
    std::optional<QString> forceLiveCacheForPrimaryStream;
    std::optional<QString> frameOptionsHeader;
    std::optional<bool> insecureDeprecatedApiEnabled;
    std::optional<bool> insecureDeprecatedApiInUseEnabled;
    std::optional<PersistentUpdateStorage> installedPersistentUpdateStorage;
    std::optional<QString> installedUpdateInformation;
    std::optional<bool> keepIoPortStateIntactOnInitialization;
    std::optional<QString> licenseServer;
    std::optional<QString> lowQualityScreenVideoCodec;
    std::optional<QString> masterCloudSyncList;
    std::optional<int> maxDifferenceBetweenSynchronizedAndInternetTime; //< TODO: Make std::chrono.
    std::optional<std::chrono::milliseconds> maxDifferenceBetweenSynchronizedAndLocalTimeMs;
    std::optional<int> maxEventLogRecords;
    std::optional<int> maxHttpTranscodingSessions;
    std::optional<qint64> maxP2pAllClientsSizeBytes;
    std::optional<int> maxP2pQueueSizeBytes;
    std::optional<int> maxRecordQueueSizeBytes;
    std::optional<int> maxRecordQueueSizeElements;
    std::optional<int> maxRemoteArchiveSynchronizationThreads;
    std::optional<int> maxRtpRetryCount;
    std::optional<int> maxRtspConnectDurationSeconds; //< TODO: Make std::chrono.
    std::optional<int> maxSceneItems;
    std::optional<int> maxVirtualCameraArchiveSynchronizationThreads;
    std::optional<int> mediaBufferSizeForAudioOnlyDeviceKb;
    std::optional<int> mediaBufferSizeKb;
    std::optional<std::chrono::milliseconds> osTimeChangeCheckPeriodMs;
    std::optional<int> proxyConnectTimeoutSec;
    std::optional<ProxyConnectionAccessPolicy> proxyConnectionAccessPolicy;
    std::optional<nx::utils::Url> resourceFileUri;
    std::optional<std::chrono::milliseconds> rtpTimeoutMs;
    std::optional<bool> securityForPowerUsers;
    std::optional<bool> sequentialFlirOnvifSearcherEnabled;
    std::optional<QString> serverHeader;
    std::optional<bool> showMouseTimelinePreview;
    std::optional<QString> statisticsReportServerApi;
    std::optional<QString> statisticsReportTimeCycle;
    std::optional<QString> statisticsReportUpdateDelay;
    std::optional<QString> supportedOrigins;
    std::optional<int> syncTimeEpsilon; //< TODO: Make std::chrono.
    std::optional<int> syncTimeExchangePeriod; //< TODO: Make std::chrono.
    std::optional<PersistentUpdateStorage> targetPersistentUpdateStorage;
    std::optional<QString> targetUpdateInformation;
    std::optional<bool> upnpPortMappingEnabled;
    std::optional<bool> useTextEmailFormat;
    std::optional<bool> useWindowsEmailLineFeed;

    bool operator==(const SaveableSettingsBase&) const = default;
};
#define SaveableSettingsBase_Fields \
    (defaultExportVideoCodec) \
    (watermarkSettings)(pixelationSettings)(webSocketEnabled) \
    (autoDiscoveryEnabled)(cameraSettingsOptimization)(statisticsAllowed) \
    (cloudNotificationsLanguage)(auditTrailEnabled)(trafficEncryptionForced) \
    (useHttpsOnlyForCameras)(videoTrafficEncryptionForced) \
    (storageEncryption)(showServersInTreeForNonAdmins)(updateNotificationsEnabled)(emailSettings) \
    (timeSynchronizationEnabled)(primaryTimeServer)(customReleaseListUrl)(clientUpdateSettings) \
    (backupSettings)(metadataStorageChangePolicy)(allowRegisteringIntegrations) \
    (additionalLocalFsTypes) \
    (arecontRtspEnabled) \
    (auditTrailPeriodDays) \
    (autoDiscoveryResponseEnabled) \
    (autoUpdateThumbnails) \
    (checkVideoStreamPeriodMs) \
    (clientStatisticsSettingsUrl) \
    (cloudConnectRelayingEnabled) \
    (cloudConnectRelayingOverSslForced) \
    (cloudConnectUdpHolePunchingEnabled) \
    (cloudPollingIntervalS) \
    (crossdomainEnabled) \
    (currentStorageEncryptionKey) \
    (defaultVideoCodec) \
    (disabledVendors) \
    (downloaderPeers) \
    (ec2AliveUpdateIntervalSec) \
    (enableEdgeRecording) \
    (eventLogPeriodDays) \
    (exposeDeviceCredentials) \
    (exposeServerEndpoints) \
    (forceAnalyticsDbStoragePermissions) \
    (forceLiveCacheForPrimaryStream) \
    (frameOptionsHeader) \
    (insecureDeprecatedApiEnabled) \
    (insecureDeprecatedApiInUseEnabled) \
    (installedPersistentUpdateStorage) \
    (installedUpdateInformation) \
    (keepIoPortStateIntactOnInitialization) \
    (licenseServer) \
    (lowQualityScreenVideoCodec) \
    (masterCloudSyncList) \
    (maxDifferenceBetweenSynchronizedAndInternetTime) \
    (maxDifferenceBetweenSynchronizedAndLocalTimeMs) \
    (maxEventLogRecords) \
    (maxHttpTranscodingSessions) \
    (maxP2pAllClientsSizeBytes) \
    (maxP2pQueueSizeBytes) \
    (maxRecordQueueSizeBytes) \
    (maxRecordQueueSizeElements) \
    (maxRemoteArchiveSynchronizationThreads) \
    (maxRtpRetryCount) \
    (maxRtspConnectDurationSeconds) \
    (maxSceneItems) \
    (maxVirtualCameraArchiveSynchronizationThreads) \
    (mediaBufferSizeForAudioOnlyDeviceKb) \
    (mediaBufferSizeKb) \
    (osTimeChangeCheckPeriodMs) \
    (proxyConnectTimeoutSec) \
    (proxyConnectionAccessPolicy) \
    (resourceFileUri) \
    (rtpTimeoutMs) \
    (securityForPowerUsers) \
    (sequentialFlirOnvifSearcherEnabled) \
    (serverHeader) \
    (showMouseTimelinePreview) \
    (statisticsReportServerApi) \
    (statisticsReportTimeCycle) \
    (statisticsReportUpdateDelay) \
    (supportedOrigins) \
    (syncTimeEpsilon) \
    (syncTimeExchangePeriod) \
    (targetPersistentUpdateStorage) \
    (targetUpdateInformation) \
    (upnpPortMappingEnabled) \
    (useTextEmailFormat) \
    (useWindowsEmailLineFeed)

struct NX_VMS_API SettingsBase
{
    QString cloudAccountName;
    QString cloudHost;
    QString lastMergeMasterId;
    QString lastMergeSlaveId;
    nx::Uuid organizationId;
    std::map<QString, int> specificFeatures;
    int statisticsReportLastNumber = 0;
    QString statisticsReportLastTime;
    QString statisticsReportLastVersion;

    bool operator==(const SettingsBase&) const = default;
};
#define SettingsBase_Fields \
    (cloudAccountName) \
    (cloudHost) \
    (lastMergeMasterId) \
    (lastMergeSlaveId) \
    (organizationId) \
    (specificFeatures) \
    (statisticsReportLastNumber) \
    (statisticsReportLastTime) \
    (statisticsReportLastVersion)

struct NX_VMS_API SaveableSystemSettings: SaveableSettingsBase, UserSessionSettings
{
    std::optional<QString> systemName;

    bool operator==(const SaveableSystemSettings&) const = default;
};
#define SaveableSystemSettings_Fields \
    SaveableSettingsBase_Fields \
    UserSessionSettings_Fields \
    (systemName)
QN_FUSION_DECLARE_FUNCTIONS(SaveableSystemSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(SaveableSystemSettings, SaveableSystemSettings_Fields)
NX_REFLECTION_TAG_TYPE(SaveableSystemSettings, jsonSerializeChronoDurationAsNumber)
NX_REFLECTION_TAG_TYPE(SaveableSystemSettings, jsonSerializeInt64AsString)

struct NX_VMS_API SystemSettings: SaveableSystemSettings, SettingsBase
{
    QString cloudSystemID;
    nx::Uuid localSystemId;
    bool system2faEnabled = false;

    bool operator==(const SystemSettings&) const = default;
};
#define SystemSettings_Fields SaveableSystemSettings_Fields SettingsBase_Fields \
    (cloudSystemID)(localSystemId)(system2faEnabled)
QN_FUSION_DECLARE_FUNCTIONS(SystemSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(SystemSettings, SystemSettings_Fields)
NX_REFLECTION_TAG_TYPE(SystemSettings, jsonSerializeChronoDurationAsNumber)
NX_REFLECTION_TAG_TYPE(SystemSettings, jsonSerializeInt64AsString)

struct NX_VMS_API SaveableSiteSettings: SaveableSettingsBase
{
    std::optional<UserSessionSettings> userSessionSettings;
    std::optional<QString> siteName;

    bool operator==(const SaveableSiteSettings&) const = default;
};
#define SaveableSiteSettings_Fields \
    SaveableSettingsBase_Fields \
    (userSessionSettings) \
    (siteName)
QN_FUSION_DECLARE_FUNCTIONS(SaveableSiteSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(SaveableSiteSettings, SaveableSiteSettings_Fields)
NX_REFLECTION_TAG_TYPE(SaveableSiteSettings, jsonSerializeChronoDurationAsNumber)
NX_REFLECTION_TAG_TYPE(SaveableSiteSettings, jsonSerializeInt64AsString)

struct SiteSettings: SaveableSiteSettings, SettingsBase
{
    QString cloudId;
    nx::Uuid localId;
    bool enabled2fa = false;

    bool operator==(const SiteSettings&) const = default;
};
#define SiteSettings_Fields \
    SaveableSiteSettings_Fields SettingsBase_Fields (cloudId)(localId)(enabled2fa)
QN_FUSION_DECLARE_FUNCTIONS(SiteSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(SiteSettings, SiteSettings_Fields)
NX_REFLECTION_TAG_TYPE(SiteSettings, jsonSerializeChronoDurationAsNumber)
NX_REFLECTION_TAG_TYPE(SiteSettings, jsonSerializeInt64AsString)

} // namespace nx::vms::api
