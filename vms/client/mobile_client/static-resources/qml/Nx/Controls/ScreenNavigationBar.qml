// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Controls
import Nx.Core
import Nx.Core.Controls
import Nx.Mobile
import Nx.Ui

import nx.vms.client.mobile

Rectangle
{
    id: control

    color: ColorTheme.colors.dark4

    readonly property real heightOffset: visible
        ? height
        : 0

    visible: d.currentIndex !== -1
        && d.buttonModel[d.currentIndex].screenId === uiController.currentScreen
        && sessionManager.hasActiveSession

    x: 0
    y: mainWindow.availableHeight - height
    width: parent.width
    height:d.kBarSize

    Row
    {
        x: spacing / 2
        y: (control.height - d.buttonSize.height) / 2
        width: parent.width - spacing + 1
        height: d.buttonSize.height

        spacing:
        {
            const buttonsCount = repeater.model.length
            return (parent.width - buttonsCount * d.buttonSize.width) / buttonsCount
        }

        Repeater
        {
            id: repeater

            model: d.buttonModel

            MouseArea
            {
                id: mouseArea

                objectName: modelData.objectName

                readonly property bool isSelected: d.currentIndex === index

                width: d.buttonSize.width
                height: d.buttonSize.height

                Rectangle
                {
                    anchors.fill: parent
                    color: isSelected
                        ? ColorTheme.colors.brand_core
                        : "transparent"
                    radius: 6

                    MaterialEffect
                    {
                        anchors.fill: parent
                        clip: true
                        radius: parent.radius
                        mouseArea: mouseArea
                        rippleSize: 160
                        highlightColor: "#30ffffff"
                    }
                }

                ColoredImage
                {
                    anchors.centerIn: parent
                    sourceSize: Qt.size(24, 24)
                    sourcePath: modelData.iconSource
                    primaryColor: isSelected
                        ? ColorTheme.colors.dark1
                        : ColorTheme.colors.light1
                }

                onClicked: d.switchToPage(index)
            }
        }
    }

    Connections
    {
        target: uiController

        function onCurrentScreenChanged()
        {
            for (let index = 0; index !== d.buttonModel.length; ++index)
            {
                if (d.buttonModel[index].screenId === uiController.currentScreen)
                {
                    d.currentIndex = index
                    return;
                }
            }

            d.currentIndex = -1
        }
    }

    QtObject
    {
        id: d

        property int currentIndex: 0
        readonly property real kBarSize: 70
        readonly property size buttonSize: Qt.size(64, 46)
        readonly property var buttonModel: [
            {
                "objectName": "switchToResourcesScreenButton",
                "iconSource": "image://skin/24x24/Outline/camera.svg",
                "screenId": Controller.ResourcesScreen,
                "openScreen": () => Workflow.openResourcesScreen(uiController.currentSystemName)
            },
            // Icon paths below will be fixed as we get them in core icons pack.
            {
                "objectName": "switchToEventSearchMenuScreenButton",
                "iconSource": "image://skin/navigation/event_search_button.svg",
                "screenId": Controller.EventSearchMenuScreen,
                "openScreen": () => Workflow.openEventSearchMenuScreen()
            },
            {
                "objectName": "switchToMenuScreenButton",
                "iconSource": "image://skin/navigation/menu_button.svg",
                "screenId": Controller.MenuScreen,
                "openScreen": () => Workflow.openMenuScreen()
            }
        ]

        function switchToPage(index)
        {
            if (index < 0)
                return

            Workflow.popCurrentScreen()
            d.buttonModel[index].openScreen()
        }
    }
}
