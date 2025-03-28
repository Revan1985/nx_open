// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import QtQuick.Controls 2.4

import Nx.Core 1.0
import Nx.Controls 1.0

Popup
{
    id: popup

    width: mainWindow.width
    height: mainWindow.height
    background: null
    modal: false
    focus: true

    signal clicked(point pos)

    contentItem: MouseArea
    {
        id: moveOnTapItem

        anchors.fill: parent

        visible: opacity > 0

        Rectangle
        {
            x: 16
            y: 16
            width: parent.width - x * 2
            height: message.height + 32
            radius: 2
            color: ColorTheme.transparent(ColorTheme.colors.dark8, 0.8)

            Text
            {
                id: message

                anchors.centerIn: parent
                color: ColorTheme.colors.light1
                width: parent.width - 32
                font: Qt.font({pixelSize:14})
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter

                text: qsTr("Tap on the image to position your camera")
            }
        }

        Button
        {
            id: cancelButton

            anchors.left: parent.left
            anchors.leftMargin: 4
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 4

            height: 48
            flat: true
            labelPadding: 16
            leftPadding: 0
            rightPadding: 0
            topPadding: 0
            bottomPadding: 0
            padding: 0

            text: qsTr("CANCEL")
            onClicked: close()
        }

        onClicked:
        {
            popup.clicked(Qt.point(mouse.x, mouse.y))
            close()
        }
    }

    onVisibleChanged:
    {
        if (visible)
            content.forceActiveFocus()
    }
}
