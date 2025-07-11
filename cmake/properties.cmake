## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# This file is loaded after the customization. It contains logic to set complex variables, combined
# from other customizable variables. Also it contains defaults for the rarely overridden variables.
# Finally, we can declare here some variables that are not overridden, but are semantically related
# to the customizable product options.

# Transform the variables for the migration simplicity.
# TODO: #sivanov Replace old variables with new ones.
set(build_paxton OFF)
if(customization.paxton.enabled)
    set(build_paxton ON)
endif()

set(build_mobile OFF)
if(customization.mobile.enabled)
    set(build_mobile ON)
endif()

# shortCloudName can be overridden with full name in the customization package.
set(shortCloudName "Cloud")
if(customization.advanced.useFullCloudNameEverywhere)
    set(shortCloudName "${customization.cloudName}")
endif()

# windowsInstallPath can be overridden in the customization package.
set(windowsInstallPath "${customization.companyName}")
if(customization.advanced.customWindowsInstallPath)
    set(windowsInstallPath "${customization.advanced.customWindowsInstallPath}")
endif()


# Internal names must not be changed in all rebrandings.
set(mobileClientInternalName "${customization.companyName} ${customization.vmsId} Mobile Client")
set(desktopClientInternalName "${customization.companyName} ${customization.vmsId} Client")
set(mediaFolderName "${customization.vmsId} Media")

# Component display names.
set(clientDisplayName "${customization.vmsName} Client")
if(customization.advanced.customClientDisplayName)
    # Overridden in metavms.
    set(clientDisplayName "${customization.advanced.customClientDisplayName}")
endif()

set(mediaserver_service_name "${customization.companyName} Server")
if(customization.advanced.customMediaserverServiceName)
    # Overridden in metavms.
    set(mediaserver_service_name "${customization.advanced.customMediaserverServiceName}")
endif()

set(mediaserver_application_name "${customization.companyName} Media Server")
if(customization.advanced.customMediaserverApplicationName)
    # Overridden in metavms.
    set(mediaserver_application_name "${customization.advanced.customMediaserverApplicationName}")
endif()

set(applauncher.name "${customization.companyName} ${customization.vmsName} Launcher")
set(traytool.name "${customization.vmsName} Tray Assistant")
set(mediaserver.display.name "${customization.vmsName} Media Server")
set(testcamera.name "${customization.companyName} ${customization.vmsName} Test Camera")

set(quick_start_guide_file_name "quick-start-guide.pdf")
set(quick_start_guide_document_name "Quick Start Guide")
set(quick_start_guide_shortcut_name "${customization.vmsName} - ${quick_start_guide_document_name}")

# Binary names and paths.
if(WINDOWS)
    set(client.binary.name "${customization.vmsId}.exe")
    set(client_launcher_name ${client.binary.name})
    set(applauncher.binary.name "applauncher.exe")
    set(minilauncher.binary.name "${customization.vmsId} Launcher.exe")
    set(testcamera.binary.name "testcamera.exe")
    set(installation_root "C:/Program Files/${windowsInstallPath}/${customization.vmsName}")
elseif(MACOSX)
    set(client.binary.name "${customization.vmsName}")
    set(client_launcher_name ${client.binary.name})
    set(applauncher.binary.name "applauncher-bin")
    set(minilauncher.binary.name "applauncher")
    set(testcamera.binary.name "testcamera")
    set(installation_root "/Applications")
else()
    set(client.binary.name "${customization.installerName}_client")
    set(client_launcher_name "client")
    set(applauncher.binary.name "applauncher-bin")
    set(minilauncher.binary.name "applauncher")
    set(testcamera.binary.name "testcamera")
    set(installation_root "/opt/${customization.companyId}")
endif()

# Localization.  See https://developer.crowdin.com/language-codes/ for the list of codes.
set(translations
    en_US
    en_GB
    fr_FR
    cs_CZ
    de_DE
    fi_FI
    ru_RU
    es_ES
    ca_ES
    gl_ES
    id_ID
    it_IT
    ja_JP
    ko_KR
    tr_TR
    zh_CN
    zh_TW
    he_IL
    hu_HU
    nl_BE
    nl_NL
    no_NO
    pl_PL
    pt_BR
    pt_PT
    uk_UA
    vi_VN
    th_TH
    sv_SE
)

# Mobile platforms should have all languages enabled.
if(NOT (${platform} MATCHES "android|ios"))
    # Temporary workaround cms issue.
    if(customization.customLanguages AND NOT customization.customLanguages STREQUAL "[]")
        set(translations ${customization.customLanguages})
    endif()
endif()

# Other variables.

set(launcher.version.file "launcher.version")

string(TIMESTAMP current_year %Y UTC)
set(nx_copyright_owner "Network Optix")
set(nx_copyright "Copyright (c) 2011-${current_year} ${nx_copyright_owner}")

if(eulaVersionOverride)
    set(customization.eulaVersion ${eulaVersionOverride})
endif()

# Packages (.deb, .zip and .tar.gz) compression levels.
if(publicationType STREQUAL "local" OR publicationType STREQUAL "private")
    set(deb_compression_level 0)
    set(zip_compression_level 1)
    set(gzip_compression_level 1)
else()
    set(deb_compression_level 6)
    set(zip_compression_level 6)
    set(gzip_compression_level 6)
endif()

set(appleTeamId "" CACHE STRING "Custom team id for signing a developer build.")

if(IOS OR MACOSX)
    # If appleTeamId is not passed via command line:

    # 1. Figure out sign identity from customization.
    if(NOT appleTeamId)
        if(IOS)
            set(identityFromCustomization "${customization.mobile.ios.signIdentity}")
        else()
            set(identityFromCustomization "${customization.desktop.macos.signIdentity}")
        endif()

        if(identityFromCustomization)
            string(REGEX REPLACE "^.*\\(([^\\)]+)\\)$" "\\1" appleTeamId "${identityFromCustomization}")
        endif()
    endif()

    # 2. For a developer build try autoselect the identity from Xcode settings - select the first
    # non-free team id. If no non-free team id is found, select the first encountered free team id.
    # This is similar to how Qt does the auto selection.
    if(NOT appleTeamId)
        if(developerBuild)
            nx_execute_process_or_fail(
                COMMAND ${PYTHON_EXECUTABLE} ${open_source_root}/build_utils/macos/team.py
                    select
                OUTPUT_VARIABLE appleTeamId
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_MESSAGE
                    "Cannot determine the developer team id automatically. "
                    "Please specify it manually via -DappleTeamId"
            )
        else()
            # For non-developer builds avoid autoselection and just fail with an informative
            # message about invalid customization file.
            if(IOS)
                set(missingProperty "customization.mobile.ios.signIdentity")
            else()
                set(missingProperty "customization.desktop.macos.signIdentity")
            endif()
            message(FATAL_ERROR "Customization file is missing ${missingProperty} property")
        endif()
    endif()

    if(developerBuild AND IOS)
        nx_execute_process_or_fail(
            COMMAND ${PYTHON_EXECUTABLE} ${open_source_root}/build_utils/macos/team.py
                isfree ${appleTeamId}
            OUTPUT_VARIABLE appleTeamIdIsFreeString
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_MESSAGE
                "Cannot determine the developer team ${appleTeamId} capabilities."
        )
    endif()
endif()

if(appleTeamIdIsFreeString STREQUAL "True")
    set(appleTeamIdIsFree TRUE)
    message(WARNING "Push notifications capability is disabled because free team id ${appleTeamId}"
        " is used for signing")
else()
    set(appleTeamIdIsFree FALSE)
endif()
