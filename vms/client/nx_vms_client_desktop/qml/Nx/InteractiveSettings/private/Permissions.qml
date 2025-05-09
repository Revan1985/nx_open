// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core
import Nx.Controls

Control
{
    id: control

    property string permission: ""
    property bool highlighted: false
    property bool collapsed: !highlighted
    property bool collapsible: false

    function reset()
    {
        collapsed = !highlighted
    }

    contentItem: Column
    {
        spacing: 16

        Column
        {
            spacing: 8
            visible: !collapsed || !collapsible

            Text
            {
                text: qsTr("Required permission group")
                font.pixelSize: 14
                color: ColorTheme.colors.light10
            }

            Text
            {
                text: `  \u2022  ${permission}`
                font.pixelSize: 14
                color: highlighted ? ColorTheme.colors.yellow_d : ColorTheme.colors.light16
            }
        }

        TextButton
        {
            text: control.collapsed ? qsTr("View Permissions") : qsTr("Hide Permissions")
            visible: collapsible

            font.pixelSize: 16
            icon.source: control.collapsed
                ? "image://skin/20x20/Outline/arrow_down.svg"
                : "image://skin/20x20/Outline/arrow_up.svg"

            onClicked: control.collapsed = !control.collapsed
        }
    }
}
