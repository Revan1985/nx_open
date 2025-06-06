// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Controls

import "../../UserManagement/Components"
import ".."
import "../ldap_helper.mjs" as LdapHelper

import nx.vms.client.desktop

Rectangle
{
    id: control

    color: ColorTheme.colors.dark7

    property string uri
    property string adminDn
    property string password
    property bool startTls: false
    property bool ignoreCertErrors: false
    property alias filters: filtersModel.filters
    property bool continuousSyncEnabled: true
    property var continuousSync
    property bool isValid
    property string loginAttribute
    property string groupObjectClass
    property string memberAttribute
    property int syncIntervalS: kDefaultSyncIntervalS
    property int searchTimeoutS: kDefaultSearchTimeoutS
    property bool syncIsRunning
    property int syncMode: LdapSettings.Sync.usersAndGroups
    property bool syncRequested
    property var preferredSyncServer
    property bool isHttpDigestEnabledOnImport
    property bool isHttpDigestEnabledOnImportInitial
    property bool insecureAuthEnabledBySiteSettings: true

    property string defaultUserLocale

    property bool hideEmptyLdapWarning: false
    property bool hideEditingWarning: false

    property bool modified

    readonly property bool hasConfig:
        uri != "" || adminDn != "" || filters.length != 0

    readonly property bool showSpinners: (syncIsRunning || syncRequested) && !modified
    readonly property bool showEmptyLdapWarning: userCount <= 0 && groupCount <= 0 && lastSync
    readonly property bool showEditingWarning: modified && !showEmptyLdapWarning &&
        (lastSync || continuousSync === LdapSettings.Sync.disabled)
    readonly property int kDefaultSyncIntervalS: 3600 //< 1 hour.
    readonly property int kDefaultSearchTimeoutS: 60 //< 1 minute.
    readonly property int kShortSyncIntervalS: 60 //< 1 minute.

    property var self
    property var testState: LdapSettings.TestState.initial
    property string testMessage: ""

    property string lastSync: ""
    property int userCount: -1
    property int groupCount: -1

    property bool checkingOnlineStatus: false

    property bool online: false

    Component
    {
        id: advancedSettingsComponent

        AdvancedSettingsDialog
        {
            transientParent: control.Window.window

            continuousSyncEnabled: control.continuousSyncEnabled

            WindowContextAware.onBeforeSystemChanged: reject()

            onAccepted:
            {
                control.loginAttribute = loginAttribute
                control.groupObjectClass = groupObjectClass
                control.memberAttribute = memberAttribute
                control.continuousSync = continuousSync
                control.syncIntervalS = syncIntervalS
                control.searchTimeoutS = searchTimeoutS
                control.preferredSyncServer = preferredSyncServer
                control.isHttpDigestEnabledOnImport = isHttpDigestEnabledOnImport
                control.defaultUserLocale = defaultUserLocale
            }
        }
    }

    Component
    {
        id: connectionSettingsComponent

        ConnectionSettingsDialog
        {
            id: connectionSettingsDialog
            transientParent: control.Window.window

            self: control.self
            testState: control.testState
            testMessage: control.testMessage

            property string initUri
            property string initAdminDn
            property string initPassword
            property bool initStartTls: false
            property bool initIgnoreCertErrors: false

            WindowContextAware.onBeforeSystemChanged: reject()

            onAccepted:
            {
                if (!control.hasConfig)
                {
                    control.continuousSyncEnabled = true
                    control.continuousSync = LdapSettings.Sync.usersAndGroups
                    control.lastSync = ""

                    control.syncIntervalS = kDefaultSyncIntervalS
                    control.searchTimeoutS = kDefaultSearchTimeoutS

                    control.loginAttribute = "" //< Auto.
                    control.groupObjectClass = "" //< Auto.
                }

                control.uri = ldapScheme + hostAndPort
                control.adminDn = adminDn
                control.password = password
                control.startTls = startTls
                control.ignoreCertErrors = ignoreCertErrors

                control.testMessage = ""

                if (control.uri != initUri
                    || control.adminDn != initAdminDn
                    || control.password != initPassword
                    || control.startTls != initStartTls
                    || control.ignoreCertErrors != initIgnoreCertErrors)
                {
                    self.testOnline(
                        control.uri,
                        control.adminDn,
                        control.password,
                        control.startTls,
                        control.ignoreCertErrors)
                }
            }
        }
    }

    Component
    {
        id: filterComponent

        FilterDialog
        {
            id: filterDialog
            transientParent: control.Window.window

            self: control.self
            testState: control.testState
            testMessage: control.testMessage

            WindowContextAware.onBeforeSystemChanged: reject()

            onAccepted:
            {
                if (list.currentIndex == -1)
                {
                    filtersModel.addFilter(
                        filterDialog.name,
                        filterDialog.baseDn,
                        filterDialog.filter)
                    list.currentIndex = 0
                }
                else
                {
                    filtersModel.setFilter(
                        list.currentIndex,
                        filterDialog.name,
                        filterDialog.baseDn,
                        filterDialog.filter)
                }
            }
        }
    }

    Placeholder
    {
        visible: !control.hasConfig
        anchors.verticalCenterOffset: -32
        imageSource: "image://skin/64x64/Outline/nosettings.svg"
        text: qsTr("Not configured yet")
        additionalText: qsTr("Connect your Site to an LDAP server for easier user management")

        maxWidth: 250

        Button
        {
            Layout.alignment: Qt.AlignHCenter

            text: qsTr("Configure")

            onClicked:
            {
                const connectionSettingsDialog = connectionSettingsComponent.createObject(control)
                control.testState = LdapSettings.TestState.initial
                control.testMessage = ""
                connectionSettingsDialog.uri = control.uri
                connectionSettingsDialog.adminDn = control.adminDn
                connectionSettingsDialog.password = control.password
                connectionSettingsDialog.startTls = control.startTls
                connectionSettingsDialog.ignoreCertErrors = control.ignoreCertErrors

                // For checking ONLINE state without Apply.
                connectionSettingsDialog.initUri = control.uri
                connectionSettingsDialog.initAdminDn = control.adminDn
                connectionSettingsDialog.initPassword = control.password
                connectionSettingsDialog.initStartTls = control.startTls
                connectionSettingsDialog.initIgnoreCertErrors = control.ignoreCertErrors

                connectionSettingsDialog.show()
            }
        }
    }

    ColumnLayout
    {
        anchors.fill: parent
        spacing: 8

        visible: control.hasConfig

        Rectangle
        {
            color: ColorTheme.colors.dark8

            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.topMargin: 12

            height: childrenRect.height + 24

            Spinner
            {
                anchors
                {
                    top: parent.top
                    right: parent.right
                    topMargin: 12
                    rightMargin: 12
                }

                running: control.checkingOnlineStatus
            }

            Rectangle
            {
                id: statusTag

                visible: !control.checkingOnlineStatus

                radius: 2
                height: 20
                width: statusTagText.width + 8

                property bool online: control.online

                color: online
                    ? ColorTheme.colors.green_core
                    : ColorTheme.colors.red_core

                anchors
                {
                    top: parent.top
                    right: parent.right
                    topMargin: 12
                    rightMargin: 12
                }

                Text
                {
                    id: statusTagText

                    anchors.centerIn: parent
                    color: "#FFFFFF"
                    font: Qt.font({pixelSize: 10, weight: Font.Medium})
                    text: statusTag.online ? qsTr("ONLINE") : qsTr("OFFLINE")
                }
            }

            Column
            {
                x: 12
                y: 12
                width: parent.width - 24

                spacing: 16

                Text
                {
                    text: LdapHelper.splitUrl(control.uri).hostAndPort

                    width: parent.width - statusTag.width - 12
                    elide: Text.ElideRight

                    font: Qt.font({pixelSize: 16, weight: Font.Medium})
                    color: ColorTheme.colors.light4
                }

                GridLayout
                {
                    columns: 2
                    columnSpacing: 50
                    rowSpacing: 0

                    Text
                    {
                        text: qsTr("Users")

                        font: Qt.font({pixelSize: 14, weight: Font.Normal})
                        color: ColorTheme.colors.light16

                        verticalAlignment: Text.AlignVCenter
                        Layout.minimumHeight: 20
                    }

                    RowLayout
                    {
                        spacing: 4

                        Text
                        {
                            text: control.userCount > 0 && statusTag.online
                                ? control.userCount
                                : (control.showSpinners ? "" : "-")

                            visible: !!text
                            font: Qt.font({pixelSize: 14, weight: Font.Normal})
                            color: ColorTheme.colors.light16
                            verticalAlignment: Text.AlignVCenter
                            Layout.minimumHeight: 20
                        }

                        Spinner
                        {
                            running: control.showSpinners
                                && control.syncMode == LdapSettings.Sync.usersAndGroups
                        }
                    }

                    Text
                    {
                        text: qsTr("Groups")

                        font: Qt.font({pixelSize: 14, weight: Font.Normal})
                        color: ColorTheme.colors.light16

                        verticalAlignment: Text.AlignVCenter
                        Layout.minimumHeight: 20
                    }

                    RowLayout
                    {
                        spacing: 4

                        Text
                        {
                            text: control.groupCount > 0 && statusTag.online
                                ? control.groupCount
                                : (control.showSpinners ? "" : "-")

                            visible: !!text
                            font: Qt.font({pixelSize: 14, weight: Font.Normal})
                            color: ColorTheme.colors.light16
                            verticalAlignment: Text.AlignVCenter
                            Layout.minimumHeight: 20
                        }

                        Spinner
                        {
                            running: control.showSpinners
                                && (control.syncMode == LdapSettings.Sync.groupsOnly
                                    || control.syncMode == LdapSettings.Sync.usersAndGroups)
                        }
                    }

                    Item
                    {
                        height: 12
                        Layout.columnSpan: 2
                    }

                    Text
                    {
                        text: qsTr("Last Sync")

                        font: Qt.font({pixelSize: 14, weight: Font.Normal})
                        color: ColorTheme.colors.light16

                        verticalAlignment: Text.AlignVCenter
                        Layout.minimumHeight: 20
                    }

                    RowLayout
                    {
                        spacing: 4

                        Text
                        {
                            text: statusTag.online && control.lastSync
                                ? control.lastSync
                                : (control.showSpinners ? "" : "-")

                            visible: !!text

                            font: Qt.font({pixelSize: 14, weight: Font.Normal})
                            color: ColorTheme.colors.light16
                            verticalAlignment: Text.AlignVCenter
                            Layout.minimumHeight: 20
                        }

                        Spinner
                        {
                            running: control.showSpinners
                        }

                        ImageButton
                        {
                            id: syncButton

                            visible: !modified
                                && control.online
                                && !control.syncRequested
                                && !control.syncIsRunning
                                && control.syncIntervalS > control.kShortSyncIntervalS

                            icon.source: "image://skin/20x20/Outline/rotate_cw.svg"

                            icon.color: hovered
                                ? ColorTheme.colors.light13
                                : ColorTheme.colors.light16

                            background: null

                            onClicked: self.requestSync()
                        }
                    }
                }

                RowLayout
                {
                    spacing: 12

                    Button
                    {
                        id: editConnectionSettings

                        text: qsTr("Edit")

                        onClicked:
                        {
                            const connectionSettingsDialog =
                                connectionSettingsComponent.createObject(control)
                            control.testState = LdapSettings.TestState.initial
                            connectionSettingsDialog.uri = control.uri
                            connectionSettingsDialog.adminDn = control.adminDn
                            connectionSettingsDialog.originalAdminDn = control.adminDn
                            connectionSettingsDialog.password = control.password
                            connectionSettingsDialog.startTls = control.startTls
                            connectionSettingsDialog.ignoreCertErrors = control.ignoreCertErrors
                            connectionSettingsDialog.hasConfig = control.hasConfig

                            connectionSettingsDialog.initUri = control.uri
                            connectionSettingsDialog.initAdminDn = control.adminDn
                            connectionSettingsDialog.initPassword = control.password
                            connectionSettingsDialog.initStartTls = control.startTls
                            connectionSettingsDialog.initIgnoreCertErrors = control.ignoreCertErrors
                            connectionSettingsDialog.show()
                        }
                    }

                    TextButton
                    {
                        Layout.alignment: Qt.AlignVCenter

                        text: qsTr("Advanced Settings")
                        font: Qt.font({pixelSize: 14, weight: Font.Normal})
                        color: ColorTheme.colors.light16
                        icon.source: "image://skin/20x20/Outline/settings.svg"

                        onClicked:
                        {
                            const advancedSettingsDialog = advancedSettingsComponent.createObject(
                                control)
                            advancedSettingsDialog.loginAttribute = control.loginAttribute
                            advancedSettingsDialog.loginAttributeAuto =
                                control.loginAttribute == ""
                            advancedSettingsDialog.groupObjectClass = control.groupObjectClass
                            advancedSettingsDialog.groupObjectClassAuto =
                                control.groupObjectClass == ""
                            advancedSettingsDialog.memberAttribute = control.memberAttribute
                            advancedSettingsDialog.memberAttributeAuto =
                                control.memberAttribute == ""

                            advancedSettingsDialog.continuousSync = control.continuousSync
                            advancedSettingsDialog.syncIntervalS = control.syncIntervalS
                            advancedSettingsDialog.searchTimeoutS = control.searchTimeoutS
                            advancedSettingsDialog.preferredSyncServer = control.preferredSyncServer
                            advancedSettingsDialog.isHttpDigestEnabledOnImport =
                                control.isHttpDigestEnabledOnImport
                            advancedSettingsDialog.isHttpDigestEnabledOnImportInitial =
                                control.isHttpDigestEnabledOnImportInitial
                            advancedSettingsDialog.defaultUserLocale = control.defaultUserLocale
                            advancedSettingsDialog.insecureAuthEnabledBySiteSettings =
                                control.insecureAuthEnabledBySiteSettings

                            advancedSettingsDialog.show()
                        }
                    }

                    TextButton
                    {
                        id: resetConnectionSettings
                        Layout.alignment: Qt.AlignVCenter

                        text: qsTr("Disconnect")
                        font: Qt.font({pixelSize: 14, weight: Font.Normal})
                        color: ColorTheme.colors.light16
                        icon.source: "image://skin/20x20/Outline/disconnect.svg"

                        onClicked: self.requestLdapReset()
                    }
                }
            }
        }

        SectionHeader
        {
            text: qsTr("Search Bases")

            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16

            TextButton
            {
                anchors.right: parent.right
                anchors.rightMargin: 2
                anchors.top: parent.top
                anchors.topMargin: parent.baselineOffset - baselineOffset - 1

                text: '+ ' + qsTr("Add")

                font: Qt.font({pixelSize: 14, weight: Font.Normal})
                color: ColorTheme.colors.light16

                onClicked:
                {
                    list.currentIndex = -1
                    const filterDialog = filterComponent.createObject(
                        control, {"title": qsTr("Add Search Base")})
                    filterDialog.name = ""
                    filterDialog.baseDn = ""
                    filterDialog.filter = ""
                    filterDialog.show()
                }
            }
        }

        Item
        {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16

            ListView
            {
                id: list

                anchors.fill: parent
                clip: true

                readonly property real scrollBarWidth: scrollBar.visible ? scrollBar.width : 0

                model: LdapFiltersModel
                {
                    id: filtersModel
                }

                hoverHighlightColor: "transparent"

                delegate: Item
                {
                    id: filterItem

                    readonly property bool containsMouse: rowMouseArea.containsMouse

                    readonly property color textColor: list.currentIndex == index
                        ? ColorTheme.colors.light4
                        : ColorTheme.colors.light10

                    height: 22
                    width: parent ? parent.width : 0

                    Rectangle
                    {
                        anchors.fill: parent
                        color:
                        {
                            if (list.currentIndex == index)
                                return ColorTheme.colors.brand_core

                            if (filterItem.containsMouse)
                                return ColorTheme.colors.dark12

                            return "transparent"
                        }

                        opacity: rowMouseArea.containsMouse ? 0.4 : 0.3
                    }

                    RowLayout
                    {
                        anchors.left: parent.left
                        anchors.leftMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: parent.right
                        anchors.rightMargin: 28

                        spacing: 8

                        Text
                        {
                            text: model.name
                            color: filterItem.textColor
                            Layout.minimumWidth: parent.width * 0.3
                            Layout.maximumWidth: parent.width * 0.3
                            font: Qt.font({pixelSize: 12, weight: Font.Normal})
                            elide: Text.ElideRight
                        }

                        Text
                        {
                            text: model.base
                            color: filterItem.textColor
                            Layout.minimumWidth: parent.width * 0.3
                            Layout.maximumWidth: parent.width * 0.3
                            font: Qt.font({pixelSize: 12, weight: Font.Normal})
                            elide: Text.ElideRight
                        }

                        Text
                        {
                            text: model.filter
                            color: filterItem.textColor
                            Layout.fillWidth: true
                            font: Qt.font({pixelSize: 12, weight: Font.Normal})
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea
                    {
                        id: rowMouseArea
                        anchors.fill: parent
                        onClicked:
                        {
                            list.currentIndex = index
                            const filterDialog = filterComponent.createObject(control)
                            filterDialog.name = model.name
                            filterDialog.baseDn = model.base
                            filterDialog.filter = model.filter
                            filterDialog.show()
                        }

                        cursorShape: containsMouse ? Qt.PointingHandCursor : Qt.ArrowCursor
                        hoverEnabled: true
                        CursorOverride.shape: cursorShape
                        CursorOverride.active: containsMouse


                        ImageButton
                        {
                            id: removeFilterButton
                            visible: rowMouseArea.containsMouse
                            anchors.right: parent.right
                            anchors.rightMargin: 4 + list.scrollBarWidth
                            anchors.verticalCenter: parent.verticalCenter

                            icon.source: "image://skin/20x20/Outline/delete.svg"

                            icon.color: hovered
                                ? ColorTheme.colors.light15
                                : ColorTheme.colors.light16

                            background: null

                            onClicked: filtersModel.removeFilter(index)
                        }
                    }
                }
            }

            RowLayout
            {
                visible: list.count == 0

                ColoredImage
                {
                    Layout.alignment: Qt.AlignTop
                    sourcePath: "image://skin/20x20/Solid/alert2.svg"
                    primaryColor: "yellow"
                    sourceSize: Qt.size(20, 20)
                }

                Text
                {
                    text: qsTr("Specify at least one search base to synchronize users and groups")
                    font: Qt.font({pixelSize: 14, weight: Font.Bold})
                    color: ColorTheme.colors.yellow_core
                }
            }
        }

        ColumnLayout
        {
            spacing: 2

            Layout.fillHeight: false

            DialogBanner
            {
                visible: !control.continuousSyncEnabled

                style: DialogBanner.Style.Info

                text: qsTr("Continuous import from LDAP server is disabled for this site. Updates "
                    + "to groups and user and groups membership will occur solely through manual "
                    + "synchronization.")

                Layout.fillWidth: true
            }

            DialogBanner
            {
                visible: control.showEmptyLdapWarning && !control.hideEmptyLdapWarning && !closed

                style: DialogBanner.Style.Warning

                text: qsTr("No users or groups match synchronization settings and are added to the "
                    + "site DB. Make sure LDAP server parameters and filters are configured "
                    + "correctly.")

                Layout.fillWidth: true

                onCloseClicked: control.hideEmptyLdapWarning = true
            }

            DialogBanner
            {
                style: DialogBanner.Style.Warning

                text: qsTr("Please use care when altering LDAP settings. Incorrect configuration "
                    + "could disrupt site availability for a large number of users simultaneously.")

                visible: control.showEditingWarning && !control.hideEditingWarning && !closed
                closeable: true

                Layout.fillWidth: true

                onCloseClicked: control.hideEditingWarning = true
            }
        }
    }
}
