// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls as QC2

import Nx.Core
import Nx.Controls
import Nx.Core.Controls

TextField
{
    id: searchField

    property var menu: null

    property color hoveredButtonColor: activeFocus
        ? ColorTheme.shadow
        : ColorTheme.lighter(ColorTheme.shadow, 1)

    property real iconPadding: 1
    property int spacing: 2

    property alias iconSource: actionButton.icon.source

    placeholderText: qsTr("Search")
    placeholderTextColor: ColorTheme.windowText

    leftPadding: actionButton.visible ? (actionButton.x + actionButton.width + spacing) : 8
    rightPadding: enabled ? (width - clearButton.x) : 8

    QC2.Button
    {
        id: actionButton

        enabled: !!menu
        visible: !!icon.source
        width: image.implicitWidth
        height: parent.height - 2
        anchors.left: parent.left
        anchors.leftMargin: searchField.iconPadding
        anchors.verticalCenter: parent.verticalCenter
        focusPolicy: Qt.NoFocus
        padding: 0

        icon.source: searchField.menu
            ? "image://skin/24x20/Outline/search_drop.svg"
            : "image://skin/20x20/Outline/search.svg"

        icon.color: searchField.text ? searchField.color : searchField.placeholderTextColor

        icon.width: implicitWidth
        icon.height: implicitHeight

        contentItem: Item
        {
            ColoredImage
            {
                id: image

                readonly property real factor:
                    Math.min(implicitWidth / parent.width, implicitHeight / parent.height)

                anchors.centerIn: parent

                width: implicitWidth * factor
                height: implicitHeight * factor

                sourcePath: actionButton.icon.source
                sourceSize: searchField.menu ? Qt.size(24, 24) : Qt.size(20, 20)

                primaryColor: actionButton.icon.color
            }
        }

        background: Rectangle
        {
            radius: 1
            color: (actionButton.hovered && !actionButton.down) ? hoveredButtonColor : "transparent"
        }

        onClicked:
        {
            if (menu instanceof Menu)
                menu.popup(actionButton, 0, actionButton.height)
            else if (menu instanceof PlatformMenu)
                menu.open(actionButton)
        }
    }

    Button
    {
        id: clearButton

        width: height
        height: parent.height - 2
        anchors.right: parent.right
        anchors.rightMargin: 1
        anchors.verticalCenter: parent.verticalCenter
        padding: 0
        focusPolicy: Qt.NoFocus

        icon.source: "image://skin/20x20/Outline/cross_close.svg"
        icon.color: searchField.color

        enabled: searchField.text
        opacity: searchField.text ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 100 }}

        background: Rectangle
        {
            radius: 1
            color: (clearButton.hovered && !clearButton.down) ? hoveredButtonColor : "transparent"
        }

        onClicked:
            searchField.clear()
    }

    Connections
    {
        target: menu

        function onAboutToShow() { searchField.cursorVisible = false }
        function onAboutToHide() { searchField.cursorVisible = searchField.activeFocus }
    }
}
