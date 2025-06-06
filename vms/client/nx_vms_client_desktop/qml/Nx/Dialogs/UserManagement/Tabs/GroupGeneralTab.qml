// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import QtQuick.Window

import Qt5Compat.GraphicalEffects

import Nx.Core
import Nx.Core.Controls
import Nx.Controls

import nx.vms.client.desktop

import "../Components"

Item
{
    id: control

    property var groupId
    property alias name: groupNameTextField.text
    property bool nameEditable: true
    property bool descriptionEditable: true
    property bool isLdap: false
    property bool isPredefined: false
    property alias description: descriptionTextArea.text
    property var groups: []
    property int userCount: 0
    property int groupCount: 0
    property bool deleteAvailable: true
    property int continuousSync: LdapSettings.Sync.usersAndGroups
    property bool cycledGroup: false
    property var editingContext
    property bool nameIsUnique: true

    property alias model: groupsComboBox.groupsModel
    property alias parentGroupsEditable: groupsComboBox.enabled

    property var self

    property bool ldapError: false

    function validate()
    {
        return groupNameTextField.validate()
    }

    signal deleteRequested()
    signal moreGroupsClicked()

    ColumnLayout
    {
        anchors.fill: parent
        spacing: 0

        Rectangle
        {
            color: ColorTheme.colors.dark8
            Layout.fillWidth: true
            height: 103

            ColoredImage
            {
                id: groupTypeIcon
                x: 24
                y: 24
                width: 64
                height: 64
                sourcePath: control.isLdap
                    ? "image://skin/64x64/Solid/ldap_group.svg"
                    : (control.isPredefined
                        ? "image://skin/64x64/Solid/group_default.svg"
                        : "image://skin/64x64/Solid/group.svg")
                sourceSize: Qt.size(width, height)
                primaryColor: "dark7"
                secondaryColor: "light10"
                tertiaryColor: "light4"
            }

            EditableLabel
            {
                id: groupNameTextField

                enabled: control.nameEditable && control.enabled
                validateFunc: self ? (text) => self.validateName(text) : null

                anchors.left: groupTypeIcon.right
                anchors.leftMargin: 24
                anchors.verticalCenter: groupTypeIcon.verticalCenter

                anchors.right: parent.right
                anchors.rightMargin: 16

                Connections
                {
                    target: control.model

                    function onUserIdChanged() { groupNameTextField.forceFinishEdit() }
                }
            }

            Row
            {
                anchors.bottom: groupTypeIcon.bottom
                anchors.right: parent.right
                anchors.rightMargin: 20
                spacing: 12

                TextButton
                {
                    icon.source: "image://skin/20x20/Outline/delete.svg"
                    color: ColorTheme.colors.light16
                    icon.width: 12
                    icon.height: 14
                    spacing: 8
                    visible: control.deleteAvailable
                    text: qsTr("Delete")
                    onClicked: control.deleteRequested()
                }
            }
        }

        Rectangle
        {
            color: ColorTheme.colors.dark6

            Layout.fillWidth: true
            height: 1
        }

        Scrollable
        {
            id: scroll

            Layout.fillWidth: true
            Layout.fillHeight: true

            contentItem: Column
            {
                x: 16
                width: scroll.width - 16 * 2

                spacing: 8

                SectionHeader
                {
                    text: qsTr("Info")
                }

                CenteredField
                {
                    text: qsTr("Description")

                    TextAreaWithScroll
                    {
                        id: descriptionTextArea

                        width: parent.width
                        height: 64
                        readOnly: !control.descriptionEditable
                        wrapMode: TextEdit.Wrap

                        textArea.KeyNavigation.priority: KeyNavigation.BeforeItem
                        textArea.KeyNavigation.tab: groupsComboBox
                        textArea.KeyNavigation.backtab: groupsComboBox
                    }
                }

                CenteredField
                {
                    text: qsTr("Permission Groups")

                    visible: !control.isPredefined
                    toolTipText: dialog.self ? dialog.self.toolTipText() : ""

                    GroupsComboBox
                    {
                        id: groupsComboBox

                        width: parent.width
                    }
                }

                SectionHeader
                {
                    Layout.fillWidth: true
                    text: qsTr("Members")
                }

                SummaryTable
                {
                    Layout.topMargin: 8
                    model: {
                        return [
                            { "label": qsTr("Users"), "text": `${control.userCount}` },
                            { "label": qsTr("Groups"), "text": `${control.groupCount}` }
                        ]
                    }
                }
            }
        }

        ColumnLayout
        {
            spacing: 2

            Layout.fillHeight: false

            DialogBanner
            {
                id: bannerGroupNotFound

                style: DialogBanner.Style.Error
                closeable: true
                watchToReopen: control.groupId
                visible: control.ldapError && !closed
                Layout.fillWidth: true

                text: qsTr("This group is not found in the LDAP database.")

                buttonText: control.deleteAvailable && control.enabled ? qsTr("Delete") : ""
                buttonIcon: "image://skin/20x20/Outline/delete.svg?primary=light16"

                onButtonClicked: control.deleteRequested()
            }

            DialogBanner
            {
                id: bannerLdapContinousSyncDisabled

                style: DialogBanner.Style.Warning
                closeable: true
                watchToReopen: control.groupId
                visible: control.isLdap
                    && control.continuousSync === LdapSettings.Sync.disabled
                    && !closed
                Layout.fillWidth: true

                text: qsTr("When continuous sync with LDAP server is disabled, groups do not " +
                    "synchronize automatically. To update this group, initiate a manual sync.")
            }

            DialogBanner
            {
                style: DialogBanner.Style.Info
                visible: !control.nameIsUnique && !closed
                closeable: true
                watchToReopen: control.groupId
                Layout.fillWidth: true

                text: qsTr("Another group with the same name exists. "
                    + "It is recommended to assign unique names to the groups.")
            }

            DialogBanner
            {
                style: DialogBanner.Style.Error
                visible: control.cycledGroup && !closed
                closeable: true
                watchToReopen: control.groupId
                Layout.fillWidth: true

                text: qsTr("The group has another group as both its parent, and as a child member, or "
                    + "is a part of such a circular reference chain. Resolve this chain to prevent "
                    + "incorrect calculation of permissions.")
            }
        }
    }
}
