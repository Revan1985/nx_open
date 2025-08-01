// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QVariant>

#include <nx/kit/ini_config.h>

namespace nx::vms::client::desktop {

struct NX_VMS_CLIENT_DESKTOP_API Ini: nx::kit::IniConfig
{
    Ini(): IniConfig("desktop_client.ini") { reload(); }

    bool isAutoCloudHostDeductionMode() const;

    // ---------------------------------------------------------------------------------------------
    // Developers' section
    // Flags here can be removed by developers' decision.

    #if defined(NX_DEVELOPER_BUILD)
        static constexpr bool kEnableDeveloperModeByDefault = true;
    #else
        static constexpr bool kEnableDeveloperModeByDefault = false;
    #endif

    NX_INI_FLAG(kEnableDeveloperModeByDefault, developerMode,
        "[Dev] Enable developer mode.");

    NX_INI_FLAG(false, demoMode,
        "[Dev] Allow client to connect to servers of all customizations. Enables automatic \n"
        "cloud host deduction if it is not passed explicitly.");

    NX_INI_FLAG(false, profilerMode,
        "[Dev] Enables client profiling mode. Includes only statistics panel display as of now.");

    NX_INI_STRING("", cloudHost,
        "[Dev] Overrides the current Client's Cloud Host. Allows to connect to the Server that \n"
        "uses the specified Cloud Host. Use 'auto' to allow client switch cloud host on the fly.");

    NX_INI_FLAG(true, limitFrameRate,
        "[Dev] Limits client frame rate to the maximum of 60 fps. Forces VSync on the video \n"
        "drivers.");

    NX_INI_FLAG(false, enableVSyncWorkaround,
        "[Dev] Always limit frame rate to approximately 60 fps, even if VSync is disabled.");

    NX_INI_FLAG(false, developerGuiPanel,
        "[Dev] Enables on-screen Qt debug panel (WARNING: can be very slow).");

    NX_INI_FLAG(false, showVideoQualityOverlay,
        "[Dev] Display green overlay over video if HQ or red overlay if LQ.");

    NX_INI_FLAG(false, debugThumbnailProviders,
        "[Dev] Enables debug mode for the thumbnail providers.");

    NX_INI_FLAG(false, calibratePtzActions,
        "[Dev] Enables calibrate PTZ actions in the camera context menu on scene.");

    NX_INI_STRING("", displayAnalyticsObjectsDebugInfo,
        "[Dev] Whether to add a debug info label to Analytics Object description.\n"
        "Allows to see the Analytics Objects details in the Desktop Client. Use \"all\" to\n"
        "display all available metadata. More precise filtering is available by combining the\n"
        "following options:\n"
        " * \"id\" - Track id,\n"
        " * \"delay\" - Delay between actual timestamp and object timestamp in milliseconds,\n"
        " * \"actual_ts\" - Current timestamp,\n"
        " * \"actual_rect\" - Interpolated rect if \"enableObjectMetadataInterpolation\" is on,\n"
        " * \"object_ts\" - Original object timestamp,\n"
        " * \"object_rect\" - Original object rect,\n"
        " * \"future_ts\" - Future object timestamp,\n"
        " * \"future_rect\" - Future object rect.\n"
        "Fields can be combined using space, comma or any other separator."
    );

    NX_INI_STRING("", debugAnalyticsVideoOverlayFromLogFile,
        "[Dev] Display analytics debug overlay, based on the log file parsing. The absolute path\n"
        "to an Analytics Log file recorded via analytics_logging.ini must be specified here.");

    NX_INI_FLOAT(1.0f, elementHighlighterOverlayOpacity,
        "[Dev] Opacity for element highlighter overlay (default is 1.0).");

    NX_INI_FLAG(false, showPreciseItemTimestamps,
        "[Dev] Whether to show precise timestamps in the camera window. If enabled, the current\n"
        "time in the camera window is shown in microseconds since epoch together with the\n"
        "default human-readable date-time representation.");

    NX_INI_FLAG(false, massSystemUpdateDebugInfo,
        "[Dev] Whether to show additional debug information for experimental update system.");

    NX_INI_FLAG(false, massSystemUpdateClearDownloads,
        "[Dev] Forces the Client to remove downloaded data for system updates.");

    NX_INI_FLAG(false, skipUpdateFilesVerification, "[Dev] Skip update files signature check.");

    NX_INI_FLAG(true, modalServerSetupWizard,
        "[Dev] Whether to show Server's setup wizard dialog in a modal window (default).\n"
        "Developers can change to non-modal for debugging of web-based components.");

    NX_INI_STRING("", autoUpdateCheckVersionOverride,
        "[Dev] Background updates check will use this version instead of the latest version.");

    NX_INI_INT(0, massSystemUpdateWaitForServerOnlineSecOverride,
        "[Dev] Time to wait until Server goes online in seconds.\n"
        "Default value is used when set to 0.");

    NX_INI_INT(0, backgroupdUpdateCheckPeriodOverrideSec,
        "[Dev] Period to check for new updates, in seconds. Set to zero to use built-in value.");

    NX_INI_FLAG(false, startClientOnlyUpdateImmediately,
        "[Dev] Forces client-only update to start update of the current Client immediately when\n"
        "an update is available. Any update date planning is not performed in this case.");

    NX_INI_FLAG(false, offlineClientUpdateEnabled,
        "[Dev] Enables storing update packages on servers persistently to make offline updates\n"
        "possible.");

    NX_INI_FLAG(false, alwaysShowGetUpdateFileButton,
        "[Dev] Always show Get Update File button.");

    NX_INI_STRING("", currentOsVariantOverride,
        "[Dev] Override detected OS variant value (e.g. \"ubuntu\").");

    NX_INI_STRING("", currentOsVariantVersionOverride,
        "[Dev] Override detected OS variant version value (e.g. \"16.04\").");

    NX_INI_FLAG(false, overrideDialogFramesWIN,
        "[Dev] Replace system dialog frames with application defined ones (Windows-only).");

    NX_INI_FLAG(false, grayscaleDecoding,
        "[Dev] Use grayscale video decoding.");

    NX_INI_FLAG(0, disableVideoRendering,
        "[Dev] Completely disable video rendering to simplify memory leaks detection.");

    NX_INI_FLAG(false, doubleGeometrySet, "[Dev] Restore client geometry twice.");

    NX_INI_FLAG(false, crossSystemLayoutsExtendedDebug, "[Dev] Cross-system layouts debug info.");

    NX_INI_FLAG(false, allowOwnCloudNotifications,
        "[Dev] Allow receiving cloud notifications from the current cloud system");

    NX_INI_FLAG(false, joystickInvestigationWizard,
        "[Dev] Enables joystick investigation wizard (works only on MacOS).");

    NX_INI_FLAG(false, virtualJoystick,
        "[Dev] Enables joystick emulator (works only on MacOS).");

    NX_INI_FLAG(true, joystickPollingInSeparateThread,
        "[Dev] Joystick manager on Windows with polling in the separate thread.");

    NX_INI_INT(200, maxSeverRequestCountPerMinunte,
        "[Dev] The maximum number of requests to the server per minute, after which it is\n"
        "considered that there are too many requests.");

    NX_INI_FLAG(false, emulateCloudBackupSettingsOnNonCloudStorage,
        "[Dev] Treat plain backup storage as cloud one on the systems with SaaS enabled.");

    NX_INI_FLAG(false, allowToPutAnyAccessibleDeviceOnScene,
        "[Dev] Allow to put devices on the scene without view live or view archive permission.");

    NX_INI_FLAG(false, simplifySceneItemMetric,
        "[Dev] Use simpler metric for the 'closest free slot' item location on the scene.");

    NX_INI_STRING("autoselect", graphicsApi,
        "[Dev] Select graphics API.\n"
        "Possible values:\n"
        " * \"legacyopengl\" - legacy OpenGL code\n"
        " * \"autoselect\" - selection depends on the OS, default\n"
        " * \"opengl\" - OpenGL or OpenGL ES\n"
        " * \"metal\" - Metal (macOS only)\n"
        " * \"direct3d11\" - Direct3D 11 (Windows only)\n"
        " * \"direct3d12\" - Direct3D 12 (Windows only)\n"
        " * \"vulkan\" - Vulkan\n"
        " * \"software\" - Software\n"
        );

    NX_INI_STRING("nvidia", gpuName,
        "[Dev] Select GPU to use by a part of its name. Only for Direct3D and Vulkan.\n"
        "Use \"intel\" for Intel, \"nvidia\" for Nvidia etc.\n"
        "By default the first available Nvidia GPU is selected.\n");

    // VMS-42523
    NX_INI_INT(400, analyticsSearchRequestDelayMs,
        "[Dev] Analytics search request delay in milliseconds.");

    // VMS-47602, VMS-36266
    NX_INI_INT(true, enableDesktopCameraLazyInitialization,
        "[Dev] Initialize desktop camera only on Push My Screen action.");

    NX_INI_FLAG(true, cacheAnalyticsTooltips,
        "[Dev] Cache analytics tooltips into textures.");

    // VMS-55770
    NX_INI_FLAG(true, useFileBasedSharedMemory,
        "[Dev] Use shared memory implementation based on the memory mapped file.");

    NX_INI_STRING(
        "typ_header alg kid sid typ_payload client_id iss cloud_access_token",
        debugInfoFilter,
        "[Dev] Disables output of fields in the information widget. Allows to disable output of\n"
        "fields you don't need. Use \"all\" to disable all output of fields. More precise\n"
        "filtering is available by combining the following options:\n"
        " * \"all\",\n"
        " * \"fps\",\n"
        " * \"process_cpu\",\n"
        " * \"total_cpu\",\n"
        " * \"memory\",\n"
        " * \"gpu\",\n"
        " * \"threads\",\n"
        " * \"digest\",\n"
        " * \"token\",\n"
        " * \"cloud_access_token\",\n"
        " * \"typ_header\",\n"
        " * \"alg\",\n"
        " * \"kid\",\n"
        " * \"exp\",\n"
        " * \"pwd_time\",\n"
        " * \"sid\",\n"
        " * \"typ_payload\",\n"
        " * \"aud\",\n"
        " * \"iat\",\n"
        " * \"sub\",\n"
        " * \"client_id\",\n"
        " * \"iss\",\n"
        " * \"requests_per_min\",\n"
        " * \"developer_build\".\n"
        "Fields can be combined using space, comma or semicolon."
    );

    NX_INI_STRING("", harFile,
        "[Dev] Path to HAR file. If empty, HAR logging is disabled. "
        "If path is relative then the file will be created in the same directory as log files.\n"
        "Placeholders %T, %P, %N and %V in the file name will be replaced with the following values:\n"
        " * %T - current date/time in format yyyy-MM-dd_HH-mm-ss-zzz\n"
        " * %P - process id\n"
        " * %N - client name\n"
        " * %V - client version\n");

    // VMS-57873
    NX_INI_FLAG(false, logFailedRecordingFinishedStacktrace,
        "[Dev] Log stacktrace needed for debugging the layout export bug.");

    // ---------------------------------------------------------------------------------------------
    // Design section
    // Flags here can be removed when designers approve the resulting approach.

    NX_INI_FLAG(false, startPlaybackOnTileNavigation,
        "[Design] Whether to start the playback if the timeline navigation occurred after Right\n"
        "Panel tile click or double click.");

    NX_INI_FLAG(false, enableSyncedChunksForExtraContent,
        "[Design] Whether to show merged bookmarks and analytics chunks in the synced area of\n"
        "the timeline.");

    // DESIGN-750
    NX_INI_FLAG(true, enableAnalyticsPlaybackMask,
        "[Design] Enable playback mode of analytics chunks only.");

    NX_INI_INT(-1, simpleModeTilesNumber,
        "[Design] Maximum number of Welcome screen tiles, while it is in simple mode.\n"
        "Set it to -1 for default value.");

    NX_INI_INT(500, autoExpandDelayMs,
        "[Design] The delay time in milliseconds before items in a tree view are opened\n"
        "during a drag and drop operation.");

    NX_INI_FLAG(false, compactSearchFilterEditors,
        "[Design] Tag and combo box attribute filter editors instead of radio button groups.");

    NX_INI_FLAG(false, roundDpiScaling,
        "[Design] Whether a DPI scaling factor should be rounded to an integer value.");

    NX_INI_STRING(nullptr, fontConfigPath,
        "[Design] Path to a file with custom font config.");

    NX_INI_FLAG(false, newAddDevicesDialog,
        "[Design] Use the new redesigned Add Device dialog.");

    // ---------------------------------------------------------------------------------------------
    // Features section
    // Flags here can be removed when QA approves the feature to be definitely present in the
    // current release.

    // VMS-10483
    NX_INI_FLAG(true, enableTimelinePreview,
        "[Feature] Allows to show the screenshot above the timeline when mouse pointer hovers\n"
        "over the timeline.");

    // VMS-10483
    NX_INI_FLOAT(0.3f, timelineThumbnailBehindPreviewOpacity,
        "[Feature] Opacity of Thumbnail Panel when Timeline Preview is shown in front of it.");

    // VMS-20240
    NX_INI_FLAG(true, enableTimelineSelectionAtTickmarkArea,
        "[Feature] Allows to create and change timeline selection while mouse pointer is at\n"
        "tickmark bar. While pointer is at chunk bar, those actions are enabled unconditionally.");

    // VMS-16691
    NX_INI_FLAG(false, allowDeleteLocalFiles,
        "[Feature] Allows to delete local files from the context menu.");

    // VMS-11893
    NX_INI_FLAG(false, systemUpdateProgressInformers,
        "[Feature] Whether to show Right Panel progress informers during System Update");

    // VMS-11543
    NX_INI_FLAG(false, pluginInformationInServerSettings,
        "[Feature] Show information about installed plugin libraries in Server Settings.");

    // VMS-21806
    NX_INI_STRING("auto", defaultResolution,
        "[Feature] Default behavior of the layout resolution in client.\n"
        "Possible values:\n"
        " * \"auto\" - default\n"
        " * \"high\" - all new layouts are opened with forced high resolution\n"
        " * \"low\" - all new layouts are opened with forced low resolution");

    // VMS-30701
    NX_INI_FLAG(true, enableCameraReplacementFeature,
        "[Feature] Makes Camera Replacement feature available in the client.");

    // VMS-32543
    NX_INI_FLAG(true, allowConfigureCloudServiceToSendEmail,
        "[Feature] Makes the option to send emails via cloud service available in the\n"
        "outgoing email settings dialog.");

    NX_INI_FLAG(false, enableMultiSystemTabBar,
        "[Feature] Enable double layer tab bar.");

    // VMS-34514
    NX_INI_FLAG(true, enableCameraHotspotsFeature,
        "[Feature] Makes Camera Hotspots feature available in the client.");

    // VMS-32420
    NX_INI_FLAG(true, enableMetadataApi,
        "[Feature] Enables MetadataApi feature.");

    // VMS-36874
    NX_INI_FLAG(true, webPagesAndIntegrations, "[Feature] Separates Web Pages and Integrations.");

    // VMS-37021
    NX_INI_FLAG(true, lookupLists, "[Feature] Enable Lookup Lists support.");

    // VMS-41144
    NX_INI_FLAG(false, nativeLinkForTemporaryUsers,
        "[Feature] Use native link type for temporary user");

    // VMS-42600
    NX_INI_FLAG(true, openDefaultWebPageOnConnect,
        "[Feature] Open web page, marked as default, when client connects to a system.");

    // VMS-41884
    NX_INI_FLAG(true, integrationsManagement,
        "[Feature] Enables Integrations Management feature.");

    // VMS-39166
    NX_INI_FLAG(true, javaScriptApiDocumentation,
        "[Feature] Enables the JavaScript API documentation.");

    // VMS-51677
    NX_INI_FLAG(false, objectPixelation,
        "[Feature] Enables Object Pixelation feature.");

    // VMS-41660
    NX_INI_FLAG(true, foldersForLayoutsInResourceTree,
        "[Feature] Enables folders for layouts in the Resource Tree.");

    NX_INI_FLAG(true, perItemMute,
        "[Feature] Show per-item mute buttons in 'play audio from all cameras' mode.");

    NX_INI_FLAG(true, audioVisualization,
        "[Feature] Show audio visualizers on audio-only items.");

    // ---------------------------------------------------------------------------------------------
    // Support section.
    // Flags here can be removed when support engineers decide whether to discard the flag or move
    // it to the client settings permanently.

    NX_INI_INT(30, rightPanelPreviewReloadDelay,
        "[Support] Right Panel preview reload delay in seconds after receiving \"NO DATA\"\n"
        "(0 to disable).");

    NX_INI_INT(0, tilePreviewLoadIntervalMs,
        "[Support] Right Panel tiles will not request previews more often than this period.\n"
        "Value in milliseconds.");

    NX_INI_INT(6, maxSimultaneousTilePreviewLoads,
        "[Support] Right Panel tab will not request simultaneously more previews than this number\n"
        "from standard arch (non-ARM) server. Valid range: [1, 15].");

    NX_INI_INT(3, maxSimultaneousTilePreviewLoadsArm,
        "[Support] Right Panel tab will not request simultaneously more previews than this number\n"
        "from ARM server. Valid range: [1, 5].");

    NX_INI_INT(1024, rightPanelMaxThumbnailWidth,
        "[Support] Maximum potential image width in pixels, that Right Panel can request from a\n"
        "server. Idea here that we need to request big image, so cropped analytics object looks\n"
        "sharp on the right panel preview.");

    NX_INI_INT(180000, connectTimeoutMs,
        "[Support] Timeout for waiting for the initial resource message from the Server.\n"
        "If exceeded, then the connection is dropped to avoid infinite UI \"Loading...\" state.\n"
        "Value in milliseconds, 0 means infinite timeout.");

    NX_INI_FLAG(false, debugDisableCameraThumbnails,
        "[Support] Disable camera thumbnail server requests for debugging and profiling purposes.\n"
        "Also disables Timeline Thumbnail Pane and Preview Search.");

    NX_INI_FLAG(false, showCameraCrosshair,
        "[Support] Show crosshair over the camera center for PTZ debugging purposes.");

    // VMS-28095.
    NX_INI_FLAG(true, ignoreTimelineGaps,
        "[Support] Ignores timeline gaps when exporting media data");

    // VMS-36585.
    NX_INI_INT(60, resourcePreviewRefreshInterval,
        "[Support] How often Resource Tree thumbnails request updates from non-ARM servers,\n"
        "in seconds. Set 0 to disable automatic updates.");
    NX_INI_INT(300, resourcePreviewRefreshIntervalArm,
        "[Support] How often Resource Tree thumbnails request updates from ARM servers,\n"
        "in seconds. Set 0 to disable automatic updates.");
    NX_INI_INT(1, resourcePreviewPreloadDelay,
        "[Support] A delay before a thumbnail is first time loaded from non-ARM servers,\n"
        "in seconds. Set 0 to remove the delay.");
    NX_INI_INT(10, resourcePreviewPreloadDelayArm,
        "[Support] A delay before a thumbnail is first time loaded from ARM servers,\n"
        "in seconds. Set 0 to remove the delay.");
    NX_INI_FLOAT(0.5, resourcePreviewRequestScatter,
        "[Support] Additional scatter to Resource Tree thumbnails requests,\n"
        "as a fraction of absolute interval value, to prevent requesting all thumbnails\n"
        "at the same time. Applied to both preload delay and refresh interval.\n"
        "Actual interval is a random value in the range\n"
        "[interval * (1 - scatter/2) ... interval * (1 + scatter/2)]");

    // VMS-37530, VMS-37550.
    NX_INI_FLAG(false, debugDisableAttributeTables,
        "[Support] Disable analytics attribute tables for graphics debugging purposes");
    NX_INI_FLAG(false, debugDisableQmlTooltips,
        "[Support] Disable tooltips rendered to an offscreen buffer for graphics debugging purposes");

    // VMS-37975.
    NX_INI_STRING("auto", considerOverallCpuUsageInRadass,
        "[Support] Consider overall CPU usage in the Radass quality control mechanism. Value "
        "\"true\" enables this mode by default. In \"auto\" mode mechanism will work only in "
        "multi-window environment and also in VideoWall mode.");

    NX_INI_FLOAT(0.7f, highSystemCpuUsageInRadass,
        "[Support] Radass should not raise item quality if system CPU usage is higher than this "
        "value. Used only when `considerOverallCpuUsageInRadass` is enabled. Range: [0, 1].");

    NX_INI_FLOAT(0.9f, criticalSystemCpuUsageInRadass,
        "[Support] Radass should lower one item quality if system CPU usage is higher than this "
        "value. Used only when `considerOverallCpuUsageInRadass` is enabled. Range: [0, 1].");

    NX_INI_FLOAT(0.6f, highProcessCpuUsageInRadass,
        "[Support] Radass should not raise item quality if process CPU usage is higher than this "
        "value. Used only when `considerOverallCpuUsageInRadass` is enabled. Range: [0, 1].");

    NX_INI_FLOAT(0.8f, criticalProcessCpuUsageInRadass,
        "[Support] Radass should lower one item quality if process CPU usage is higher than this "
        "value. Used only when `considerOverallCpuUsageInRadass` is enabled. Range: [0, 1].");

    NX_INI_INT(5000, criticalRadassCpuUsageTimeMs,
        "[Support] Radass should lower item quality if CPU usage is critically high for this "
        "period of time (in milliseconds).");

    NX_INI_FLAG(false, showBorderInVideoWallMode,
        "[Support] Show window border in Video Wall mode to workaround graphics drivers issues.");

    // ---------------------------------------------------------------------------------------------
    // CI section.
    // Flags here are used for the client functional unit tests.

    NX_INI_INT(0, clientWebServerPort,
        "[CI] Enables web server to remotely control the Nx Client operation; port should be in\n"
        "range 1..65535 (typically 7012) to enable; 0 means disabled.");

    NX_INI_STRING("", clientWebServerHost,
        "[CI] Listen address for local web server. It should contain a valid ip address.");

    NX_INI_INT(1000, storeFrameTimePoints,
        "[CI] Number of frame timestamps stored by the Client. Used in Functional Tests for fps\n"
        "measurement.");

    // ---------------------------------------------------------------------------------------------
    // Flags needed for Squish tests.

    NX_INI_STRING("", clientExecutableName,
        "[SQ] Special executable name to launch 'New Window' or Video Wall.");

    NX_INI_FLAG(false, hideOtherSystemsFromResourceTree,
        "[SQ] Do not display 'Other Systems' node in the Resource Tree.");
};

NX_VMS_CLIENT_DESKTOP_API Ini& ini();

} // namespace nx::vms::client::desktop
