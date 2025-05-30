// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Window

import Nx.Controls

import nx.vms.client.core
import nx.vms.client.desktop

SelectableTextButton
{
    id: cameraSelector

    property CommonObjectSearchSetup setup
    property bool limitToCurrentCamera: false

    selectable: false
    deactivatable: !limitToCurrentCamera
    accented: setup && setup.cameraSelection === EventSearch.CameraSelection.current
    icon.source: "image://skin/20x20/Solid/camera.svg"

    readonly property var actionNames:
    {
        const mixedDevices = setup && setup.mixedDevices
        let result = {}

        result[EventSearch.CameraSelection.all] =
            mixedDevices ? qsTr("Any device") : qsTr("Any camera")

        result[EventSearch.CameraSelection.layout] =
            mixedDevices ? qsTr("Devices on layout") : qsTr("Cameras on layout")

        result[EventSearch.CameraSelection.current] =
            mixedDevices ? qsTr("Selected device") : qsTr("Selected camera")

        result[EventSearch.CameraSelection.custom] =
            mixedDevices ? qsTr("Choose devices...") : qsTr("Choose cameras...")

        return result
    }

    text:
    {
        if (!setup)
            return ""

        function singleCameraText(base, camera)
        {
            const kNdash = "\u2013"
            const name = camera ? camera.name : qsTr("none")
            return `${base} ${kNdash} ${name}`
        }

        switch (setup.cameraSelection)
        {
            case EventSearch.CameraSelection.custom:
            {
                if (setup.cameraCount > 1)
                {
                    return setup.mixedDevices
                        ? qsTr("%n chosen devices", "", setup.cameraCount)
                        : qsTr("%n chosen cameras", "", setup.cameraCount)
                }

                const base = setup.mixedDevices ? qsTr("Chosen device") : qsTr("Chosen camera")
                return singleCameraText(base, setup.singleCamera)
            }

            case EventSearch.CameraSelection.current:
                return singleCameraText(actionNames[setup.cameraSelection], setup.singleCamera)

            default:
                return actionNames[setup.cameraSelection]
        }
    }

    function updateState()
    {
        cameraSelector.setState(
            !setup || setup.cameraSelection === EventSearch.CameraSelection.all
                ? SelectableTextButton.State.Deactivated
                : SelectableTextButton.State.Unselected)
    }

    onDeactivated:
        defaultAction.trigger()

    Component.onCompleted:
        updateState()

    Connections
    {
        target: setup

        function onCameraSelectionChanged()
        {
            cameraSelector.updateState()
        }
    }

    menu: limitToCurrentCamera ? null : menuControl

    CompactMenu
    {
        id: menuControl

        component MenuAction: Action { text: cameraSelector.actionNames[data] }

        MenuAction { data: EventSearch.CameraSelection.layout }
        MenuAction { data: EventSearch.CameraSelection.current }
        CompactMenuSeparator {}
        MenuAction { data: EventSearch.CameraSelection.custom }
        CompactMenuSeparator {}
        MenuAction { id: defaultAction; data: EventSearch.CameraSelection.all }

        onTriggered: (action) =>
        {
            if (!setup)
                return

            if (action.data === EventSearch.CameraSelection.custom)
            {
                Qt.callLater(() =>
                {
                    setup.chooseCustomCameras()

                    // Ensure the current window has focus after cameras are selected.
                    cameraSelector.Window.window.raise()
                    cameraSelector.Window.window.requestActivate()
                })
            }
            else
            {
                setup.cameraSelection = action.data
            }
        }
    }
}
