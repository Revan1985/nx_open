#!/bin/bash

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set -e #< Stop on first error.
set -u #< Require variable definitions.

source "$(dirname $0)/../../build_distribution_utils.sh"

distrib_loadConfig "build_distribution.conf"

WORK_DIR="client_build_distribution_tmp"
LOG_FILE="$LOGS_DIR/client_build_distribution.log"

STAGE="$WORK_DIR/$DISTRIBUTION_NAME"
BIN_INSTALL_PATH="opt/${CUSTOMIZATION}/client/${VERSION}/bin"
STAGE_MODULE="$STAGE/opt/$CUSTOMIZATION/client/$VERSION"
STAGE_BIN="${STAGE}/${BIN_INSTALL_PATH}"
STAGE_LIBEXEC="$STAGE_MODULE/libexec"
STAGE_LIB="$STAGE_MODULE/lib"
STAGE_ICONS="$STAGE/usr/share/icons"

#--------------------------------------------------------------------------------------------------

# [in] STAGE_BIN
copyBins()
{
    echo "Copying client binaries"

    mkdir -p "$STAGE_BIN"

    install -m 755 "$BUILD_DIR/bin/$CLIENT_BINARY_NAME" "$STAGE_BIN/"
    install -m 755 "$BUILD_DIR/bin/$APPLAUNCHER_BINARY_NAME" "$STAGE_BIN/"
    install -m 755 "bin/client" "$STAGE_BIN/client"
    # Create a symlink for backwards compatibility to old applaunchers.
    ln -s "client" "$STAGE_BIN/client-bin"
    install -m 755 "$OPEN_SOURCE_DIR/build_utils/linux/choose_newer_stdcpp.sh" "$STAGE_BIN/"
    install -m 644 "$BUILD_DIR/bin/$LAUNCHER_VERSION_FILE" "$STAGE_BIN/"
    install -m 755 "bin/applauncher" "$STAGE_BIN/"
    install -m 644 "$OPEN_SOURCE_DIR/nx_log_viewer.html" "$STAGE_BIN/"
}

# [in] STAGE
# [in] STAGE_ICONS
copyIcons()
{
    echo "Copying icons"
    mkdir -p "$STAGE_ICONS"
    cp -r usr "$STAGE/"
    mv -f "$STAGE/usr/share/applications/icon.desktop" \
        "$STAGE/usr/share/applications/$INSTALLER_NAME.desktop"
    mv -f "$STAGE/usr/share/applications/protocol.desktop" \
        "$STAGE/usr/share/applications/$URI_PROTOCOL.desktop"
    cp -r "$CUSTOMIZATION_DIR/icons/linux/hicolor" "$STAGE_ICONS/"
    for FILE in $(find "$STAGE_ICONS" -name "*.png")
    do
        mv "$FILE" "$(dirname "$FILE")/$(basename "$FILE" .png)-$CUSTOMIZATION_NAME.png"
    done
}

# [in] STAGE_MODULE
copyHelpFiles()
{
    echo ""

    echo "Copying Desktop Client help"
    local -r STAGE_HELP="$STAGE_BIN/help"
    mkdir -p "$STAGE_HELP"
    cp -r "$CLIENT_HELP_PATH"/* "$STAGE_HELP/"

    echo "Copying Quick Start Guide"
    cp "$QUICK_START_GUIDE_FILE" "$STAGE_MODULE"

    if [[ "${CUSTOMIZATION_MOBILE_CLIENT_ENABLED,,}" == "true" ]]
    then
        echo "Copying Mobile Help"
        cp "$MOBILE_HELP_FILE" "$STAGE_MODULE"
    fi
}

# [in] STAGE_MODULE
copyBackgrounds()
{
    echo "Copying backgrounds"
    local -r STAGE_BACKGROUNDS="$STAGE_MODULE/share/pictures/sample-backgrounds"
    mkdir -p "$STAGE_BACKGROUNDS"
    cp -r "$BUILD_DIR/backgrounds"/* "$STAGE_BACKGROUNDS/"
}

# [in] STAGE_BIN
copyResources()
{
    echo "Copying resources"
    cp "$BUILD_DIR/bin/client_external.dat" "$STAGE_BIN/"
    cp "$BUILD_DIR/bin/client_core_external.dat" "$STAGE_BIN/"
    cp "$BUILD_DIR/bin/bytedance_iconpark.dat" "$STAGE_BIN/"
}

# [in] STAGE_BIN
copyTranslations()
{
    echo "Copying translations"

    local -r STAGE_TRANSLATIONS="$STAGE_BIN/translations"
    mkdir -p "$STAGE_TRANSLATIONS"

    for FILE in "${TRANSLATION_FILES[@]}"
    do
        echo "  Copying bin/translations/$FILE"
        cp "$BUILD_DIR/bin/translations/$FILE" "$STAGE_TRANSLATIONS"
    done
}

# [in] STAGE_LIB
copyLibs()
{
    echo ""
    echo "Copying libs"

    mkdir -p "$STAGE_LIB"

    local -r NX_LIBS=(
        appserver2
        cloud_db_client
        nx_audio
        nx_fusion
        nx_reflect
        nx_kit
        nx_branding
        nx_build_info
        nx_codec
        nx_json_rpc
        nx_monitoring
        nx_media
        nx_media_core
        nx_network
        nx_network_rest
        nx_telemetry
        nx_pathkit
        nx_rtp
        nx_vms_api
        nx_vms_applauncher_api
        nx_vms_client_core
        nx_vms_client_desktop
        nx_vms_common
        nx_vms_rules
        nx_vms_update
        nx_vms_utils
        nx_vms_update
        nx_zip
        nx_utils
        nx_speech_synthesizer
        udt
        vms_gateway_core
    )

    local LIB
    local FILE

    for LIB in "${NX_LIBS[@]}"
    do
        FILE="lib$LIB.so"
        echo "  Copying $FILE"
        install -m 755 "$BUILD_DIR/lib/$FILE" "$STAGE_LIB/"
    done

    echo ""
    echo "  Copying 3rd-party libs"

    local -r THIRD_PARTY_LIBS=(
        libavcodec.so.61
        libavfilter.so.10
        libavformat.so.61
        libavutil.so.59
        libswscale.so.8
        libswresample.so.5

        libcrypto.so.1.1
        libssl.so.1.1

        libicuuc.so.74
        libicudata.so.74
        libicui18n.so.74

        libopenal.so.1
        libqtkeychain.so.0.9.0
        libquazip.so
    )
    distrib_copyLibs "${BUILD_DIR}/lib" "$STAGE_LIB" "${THIRD_PARTY_LIBS[@]}"

    if [[ ${#CPP_RUNTIME_LIBS[@]} != 0 ]]
    then
        echo "  Copying system libs: ${CPP_RUNTIME_LIBS[@]}"
        local -r STDCPP_LIBS_PATH="$STAGE_LIB/stdcpp"
        mkdir "$STDCPP_LIBS_PATH"
        distrib_copySystemLibs "$STDCPP_LIBS_PATH" "${CPP_RUNTIME_LIBS[@]}"
    fi

    if [ "$ARCH" != "arm" ]
    then
        echo "  Copying additional system libs"
        distrib_copySystemLibs "$STAGE_LIB" \
            libXss.so.1 \
            libxcb-cursor.so.0 \
            libxcb-xinerama.so.0
        distrib_copySystemLibs "$STAGE_LIB" libpng16.so.16 \
            || distrib_copySystemLibs "$STAGE_LIB" libpng.so
        distrib_copySystemLibs "$STAGE_LIB" \
            libgstallocators-1.0.so.0 \
            libgstapp-1.0.so.0 \
            libgstaudio-1.0.so.0 \
            libgstbase-1.0.so.0 \
            libgstfft-1.0.so.0 \
            libgstgl-1.0.so.0 \
            libgstpbutils-1.0.so.0 \
            libgstreamer-1.0.so.0 \
            libgsttag-1.0.so.0 \
            libgstvideo-1.0.so.0 \
            libxkbcommon.so.0 \
            libxkbcommon-x11.so.0

        local -r OPENGL_LIBS_PATH="$STAGE_LIB/opengl"
        mkdir "$OPENGL_LIBS_PATH"
        distrib_copySystemLibs "$OPENGL_LIBS_PATH" libOpenGL.so.0
    fi

    if [ "$ARCH" != "arm64" ] && [ "$ARCH" != "arm" ];
    then
        distrib_copySystemLibs "$STAGE_LIB" \
            libva.so.2 \
            libva-x11.so.2 \
            libva-drm.so.2 \
            libvpl.so.2 \
            libmfxhw64.so.1 \
            libmfx.so.1 \
            libmfx-gen.so.1.2 \
            libcudart.so.12 \
            libxcb-shape.so.0

        mkdir "${STAGE_LIB}/libva-drivers"
        cp "${BUILD_DIR}/lib/libva-drivers/iHD_drv_video.so" "${STAGE_LIB}/libva-drivers"
    fi
}

# [in] STAGE_MODULE
copyQtPlugins()
{
    echo ""
    echo "Copying Qt plugins"

    local -r PLUGINS_DIR="$STAGE_MODULE/plugins"
    mkdir "$PLUGINS_DIR"

    local QT_PLUGINS=(
        platforminputcontexts
        imageformats
        xcbglintegrations
        platforms
        tls
        multimedia
    )

    local QT_PLUGIN
    for QT_PLUGIN in "${QT_PLUGINS[@]}"
    do
        echo "  Copying (Qt plugin) $QT_PLUGIN"
        cp -r "$QT_DIR/plugins/$QT_PLUGIN" "$PLUGINS_DIR/"
    done
}

# [in] STAGE_LIB
copyQtLibs()
{
    echo ""
    echo "Copying Qt libs"

    local QT_LIBS=(
        Concurrent
        Core
        Core5Compat
        DBus
        Gui
        LabsPlatform
        LabsQmlModels
        Multimedia
        MultimediaQuick
        Network
        OpenGL
        OpenGLWidgets
        Positioning
        Qml
        QmlMeta
        QmlModels
        QmlWorkerScript
        Quick
        QuickControls2
        QuickControls2Basic
        QuickControls2BasicStyleImpl
        QuickControls2Impl
        QuickEffects
        QuickLayouts
        QuickShapes
        QuickTemplates2
        QuickWidgets
        ShaderTools
        Sql
        Svg
        WebChannel
        WebChannelQuick
        WebEngineCore
        WebEngineQuick
        WebEngineWidgets
        WebView
        Widgets
        XcbQpa
        Xml
    )
    if [ "$ARCH" == "arm" ]
    then
        QT_LIBS+=(
            Sensors
        )
    fi

    local QT_LIB
    local FILE
    for QT_LIB in "${QT_LIBS[@]}"
    do
        FILE="libQt6$QT_LIB.so"
        echo "  Copying (Qt) $FILE"
        cp -P "$QT_DIR/lib/$FILE"* "$STAGE_LIB/"
    done
}

# [in] STAGE_MODULE
copyQml()
{
    cp -r "$QT_DIR/qml" "$STAGE_MODULE/qml"
}

# [in] STAGE_LIBEXEC
# [in] STAGE_MODULE
copyAdditionalQtFiles()
{
    echo ""
    echo "Copying additional Qt files"

    echo "  Copying qt.conf"
    cp "qt.conf" "$STAGE_BIN/"

    echo "  Copying (Qt) libexec"
    mkdir "$STAGE_LIBEXEC"
    cp "$QT_DIR/libexec/QtWebEngineProcess" "$STAGE_LIBEXEC"

    echo "  Copying (Qt) resources"
    cp -r "$QT_DIR/resources" "$STAGE_MODULE/resources"
    mkdir "$STAGE_MODULE/translations"
    cp -r "$QT_DIR/translations/qtwebengine_locales" "$STAGE_MODULE/translations/qtwebengine_locales"
}

# [in] STAGE
createDebianDir()
{
    echo "Preparing DEBIAN dir"
    mkdir -p "$STAGE/DEBIAN"
    chmod g-s,go+rx "$STAGE/DEBIAN"

    local -r INSTALLED_SIZE=$(du -s "$STAGE" |awk '{print $1;}')

    echo "Generating DEBIAN/control"
    cat "debian/control.template" \
        |sed "s/INSTALLED_SIZE/$INSTALLED_SIZE/g" \
        |sed "s/VERSION/$RELEASE_VERSION/g" \
        >"$STAGE/DEBIAN/control"

    echo "Copying DEBIAN dir"
    for FILE in $(ls debian)
    do
        if [ "$FILE" != "control.template" ]
        then
            install -m 755 "debian/$FILE" "$STAGE/DEBIAN/"
        fi
    done

    echo "Generating DEBIAN/md5sums"
    (cd "$STAGE"
        find * -type f -not -regex '^DEBIAN/.*' -print0 |xargs -0 md5sum >"DEBIAN/md5sums"
        chmod 644 "DEBIAN/md5sums"
    )
}

# [in] STAGE_MODULE
# [in] STAGE_ICONS
createUpdateZip()
{
    echo ""
    echo "Creating \"update\" .zip"

    echo "  Copying icon(s) for \"update\" .zip"
    local -r STAGE_UPDATE_ICONS="$STAGE_MODULE/share/icons"
    mkdir -p "$STAGE_UPDATE_ICONS"
    cp "$STAGE_ICONS/hicolor/scalable/apps"/* "$STAGE_UPDATE_ICONS/"

    local -r FREE_SPACE_REQUIRED=$(du -bs "$STAGE_MODULE" |awk '{print $1}')

    echo "  Copying update.json and package.json for \"update\" .zip"
    cp "update/update.json" "$STAGE_MODULE/"

    sed "s/\"freeSpaceRequired\": 0/\"freeSpaceRequired\": $FREE_SPACE_REQUIRED/" \
        "package.json" >"$STAGE_MODULE/package.json"

    distrib_createArchive "$DISTRIBUTION_OUTPUT_DIR/$UPDATE_ZIP" "$STAGE_MODULE" \
        zip -r `# Preserve symlinks #`-y -$ZIP_COMPRESSION_LEVEL
}

buildDistribution()
{
    local FILE

    echo "Prepare stage directory"
    mkdir -p "${STAGE_MODULE}/"

    distrib_copyMetadata
    copyBins
    copyIcons
    copyHelpFiles
    distrib_copyFonts
    copyBackgrounds
    copyResources
    copyTranslations
    copyLibs
    copyQtPlugins
    copyQml
    copyQtLibs
    copyAdditionalQtFiles

    echo "Setting permissions"
    find "$STAGE" -type d -print0 |xargs -0 chmod 755
    find "$STAGE" -type f -print0 |xargs -0 chmod 644

    # Restore permissions for executables.
    chmod 755 "$STAGE_BIN"/{applauncher*,*client*,*.sh}
    chmod 755 "$STAGE_LIBEXEC/QtWebEngineProcess"

    createDebianDir

    local -r DEB="$DISTRIBUTION_OUTPUT_DIR/$DISTRIBUTION_NAME.deb"

    echo "Creating $DEB"
    fakeroot dpkg-deb -z $DEB_COMPRESSION_LEVEL -b "$STAGE" "$DEB"

    createUpdateZip

    # TODO: This file seems to go nowhere. Is it needed? If yes, add a comment.
    echo "Generating finalname-client.properties"
    echo "client.finalName=$DISTRIBUTION_NAME" >>finalname-client.properties
}

#--------------------------------------------------------------------------------------------------

main()
{
    distrib_prepareToBuildDistribution "$WORK_DIR" "$LOG_FILE" "$@"

    buildDistribution
}

main "$@"
