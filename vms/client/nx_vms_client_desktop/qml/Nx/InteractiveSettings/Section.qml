// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core
import Nx.Controls

Column
{
    id: root
    property alias header: header
    property alias description: description
    property bool checkable: false
    property alias checked: switchButton.checked

    signal switchClicked()

    spacing: 8

    RowLayout
    {
        width: parent.width

        Text
        {
            id: header

            color: ColorTheme.colors.light4
            font.pixelSize: 16
            font.weight: Font.Medium

            Layout.fillWidth: true
            Layout.alignment: Qt.AlignCenter

        }

        SwitchButton
        {
            id: switchButton

            background: null
            Layout.alignment: Qt.AlignCenter
            visible: root.checkable
            onClicked: root.switchClicked()
        }
    }

    Rectangle
    {
        id: separator

        width: parent.width
        height: 1
        color: ColorTheme.colors.dark12
    }

    Text
    {
        id: description

        visible: !!text

        font.pixelSize: 14
        color: ColorTheme.colors.light16
        wrapMode: Text.Wrap
        width: parent.width
    }
}
