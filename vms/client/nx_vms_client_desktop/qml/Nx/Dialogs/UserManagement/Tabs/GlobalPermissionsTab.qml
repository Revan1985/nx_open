// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Controls
import Nx.Items

import nx.vms.client.desktop

import "../Components"

Item
{
    id: control

    required property var model
    property bool editable: true

    ColumnLayout
    {
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.top: parent.top
        anchors.topMargin: 16

        spacing: 0

        Text
        {
            text: control.model.context
                && control.model.context.currentSubjectType() ===
                    AccessSubjectEditingContext.SubjectType.group
                ? qsTr("At the site level, group members have permissions to:")
                : qsTr("At the site level, the user has permissions to:")

            font: Qt.font({pixelSize: 14, weight: Font.Normal})
            color: ColorTheme.colors.light16
            bottomPadding: 16 - parent.spacing
        }

        Column
        {
            spacing: 2
            Layout.leftMargin: -3

            Repeater
            {
                model: control.model

                delegate: RowLayout
                {
                    CheckBox
                    {
                        id: viewLogsCheckBox

                        textLeftPadding: 9
                        enabled: control.editable
                        text: model.display

                        checkState: model.isChecked ? Qt.Checked : Qt.Unchecked
                        onToggled: model.isChecked = (checkState == Qt.Checked)

                        Layout.alignment: Qt.AlignTop
                    }

                    ColoredImage
                    {
                        id: inheritedFromIcon

                        property string toolTip: model.toolTip || ""
                        visible: !!toolTip

                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: viewLogsCheckBox.baselineOffset - baselineOffset
                        baselineOffset: height * 0.7 //< This is a property of this icon type.
                        sourceSize: Qt.size(20, 20)
                        sourcePath: "image://skin/20x20/Solid/group.svg"
                        primaryColor: groupMouseArea.containsMouse
                            ? ColorTheme.colors.light16
                            : ColorTheme.colors.dark13

                        MouseArea
                        {
                            id: groupMouseArea
                            hoverEnabled: true
                            anchors.fill: parent

                            GlobalToolTip.text: inheritedFromIcon.toolTip
                            GlobalToolTip.delay: 0
                        }
                    }
                }
            }
        }
    }
}
