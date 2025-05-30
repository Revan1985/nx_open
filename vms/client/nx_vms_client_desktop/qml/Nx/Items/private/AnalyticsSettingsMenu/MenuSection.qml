// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core
import Nx.Core.Controls
import Nx.Controls

import nx.vms.client.desktop

import "."

Column
{
    id: control

    property alias navigationMenu: menuItem.navigationMenu
    property var itemId: this

    property alias text: menuItem.text
    property alias active: menuItem.active
    property alias font: menuItem.font
    property alias selected: menuItem.selected
    property alias indicatorText: indicator.text
    property alias content: menuContent.contentItem
    property alias collapsed: menuContent.collapsed
    property int level: 0
    property bool collapsible: content.implicitHeight > 0
    property bool mainItemVisible: true
    property string iconSource: ""

    signal clicked()

    MenuItem
    {
        id: menuItem

        itemId: control.itemId
        width: control.width
        level: control.level
        visible: mainItemVisible

        onClicked:
        {
            if (collapsible)
                menuContent.collapsed = false

            control.clicked()
        }

        iconComponent: LocalSettings.iniConfigValue("integrationsManagement") ? icon : arrow

        Component
        {
            id: icon

            ColoredImage
            {
                sourceSize: Qt.size(20, 20)
                sourcePath: level === 0
                    ? iconSource
                    : collapsed
                        ? "image://skin/20x20/Outline/arrow_right.svg"
                        : "image://skin/20x20/Outline/arrow_down.svg"

                primaryColor: menuItem.color
                visible: level === 0 || collapsible

                MouseArea
                {
                    enabled: level !== 0
                    anchors.fill: parent

                    onClicked:
                    {
                        if (selected && !menuItem.current)
                            menuItem.click()

                        menuContent.collapsed = !menuContent.collapsed
                    }
                }
            }
        }

        Component
        {
            id: arrow
            ArrowButton
            {
                width: 10
                height: 10
                visible: collapsible
                icon.lineWidth: control.level === 0 ? 1.5 : 1
                state: menuContent.collapsed ? "right" : "down"

                onClicked:
                {
                    if (selected && !menuItem.current)
                        menuItem.click()

                    menuContent.collapsed = !menuContent.collapsed
                }
            }
        }

        Text
        {
            id: indicator

            font: parent.font
            color: menuContent.collapsed ? ColorTheme.colors.brand_core : parent.color

            anchors.right: parent.right
            anchors.baseline: parent.baseline
            anchors.margins: 8
        }
    }

    Control
    {
        id: menuContent

        property bool collapsed: false

        width: parent.width
        height: collapsed ? 0 : implicitHeight
        clip: true
        visible: active
    }
}
