// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import Nx.Core 1.0
import Nx.Items 1.0

DummyMessage
{
    id: dummyMessage

    state: ""

    states: [
        State
        {
            name: ""
            PropertyChanges
            {
                target: dummyMessage
                title: ""
                image: ""
            }
        },
        State
        {
            name: "serverOffline"
            PropertyChanges
            {
                target: dummyMessage
                title: qsTr("Server offline")
                image: lp("/images/alert_server_offline.png")
            }
        },
        State
        {
            name: "cameraUnauthorized"
            PropertyChanges
            {
                target: dummyMessage
                title: qsTr("Authentication required")
                image: lp("/images/alert_locked.png")
            }
        },
        State
        {
            name: "cameraOffline"
            PropertyChanges
            {
                target: dummyMessage
                title: qsTr("Camera offline")
                image: lp("/images/alert_camera_offline.png")
            }
        },
        State
        {
            name: "videoLoadingFailed"
            PropertyChanges
            {
                target: dummyMessage
                title: qsTr("Cannot load video")
                image: lp("/images/alert_alert.png")
            }
        },
        State
        {
            name: "noVideoStreams"
            PropertyChanges
            {
                target: dummyMessage
                title: qsTr("Cannot play any video stream")
                image: lp("/images/alert_alert.png")
            }
        },
        State
        {
            name: "noLicenses"
            PropertyChanges
            {
                target: dummyMessage
                title: qsTr("Not enough licenses")
                image: lp("/images/alert_license.png")
            }
        },
        State
        {
            name: "defaultPasswordAlert"
            PropertyChanges
            {
                target: dummyMessage
                title: qsTr("Password required")
                image: lp("/images/alert_alert.png")
                description: qsTr(
                    "Set password on this camera using %1 client or camera web-page to view video",
                    "%1 is the short desktop client name (like 'Nx Witness')")
                    .arg(appContext.appInfo.productName());
            }
        },
        State
        {
            name: "audioOnlyMode"
            PropertyChanges
            {
                target: dummyMessage
                image: lp("/images/alert_sound.png")
            }
        },
        State
        {
            name: "oldFirmwareAlert"
            PropertyChanges
            {
                target: dummyMessage
                title: qsTr("Unsupported firmware version")
                image: lp("/images/alert_alert.png")
                description: qsTr("Please update firmware")
            }
        },
        State
        {
            name: "tooManyConnections"
            PropertyChanges
            {
                target: dummyMessage
                title: qsTr("Too many connections")
                image: lp("/images/alert_alert.png")
            }
        },
        State
        {
            name: "cannotDecryptMedia"
            PropertyChanges
            {
                target: dummyMessage
                title: qsTr("Archive encrypted")
                description: qsTr("Enter the encryption password on the Desktop Client "
                    + "to decrypt this archive, or ask your site administrator for help.")
                image: lp("/images/alert_archive_encrypted.svg")
            }
        },
        State
        {
            name: "ioModuleWarning"
            PropertyChanges
            {
                target: dummyMessage
                title: qsTr("I/O interface not supported yet")
                description: qsTr(
                    "In this app I/O module can be used as a sound input device only."
                    + " To use it as a control module, please use desktop version of %1",
                    "%1 is the short desktop client name (like 'Nx Witness')")
                    .arg(appContext.appInfo.productName())
                image: lp("/images/alert_io.png")
            }
        },
        State
        {
            name: "ioModuleAudioPlaying"
            PropertyChanges
            {
                target: dummyMessage
                image: lp("/images/alert_sound.png")
            }
        },
        State
        {
            name: "noLiveStream"
            PropertyChanges
            {
                target: dummyMessage
                title: qsTr("No live stream")
            }
        }
    ]
}
