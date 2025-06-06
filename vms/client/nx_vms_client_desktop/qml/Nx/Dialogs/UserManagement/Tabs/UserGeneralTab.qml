// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Shapes

import Nx.Core
import Nx.Core.Controls
import Nx.Controls

import nx.vms.client.core
import nx.vms.client.desktop

import ".."
import "../Components"

Item
{
    id: control

    property bool isSelf: false
    property bool isOrgUser: false
    property var userId

    property bool deleteAvailable: true
    property bool auditAvailable: true

    property alias login: userLoginText.text
    property bool loginEditable: false
    property alias fullName: userFullNameTextField.text
    property bool fullNameEditable: true
    property alias email: userEmailTextField.text
    property bool emailEditable: true
    property string password: ""
    property bool passwordEditable: true

    property alias locale: languageComboBox.value
    property bool localeEditable: true

    property alias userEnabled: enabledUserSwitch.checked
    property bool userEnabledEditable: true

    property alias allowInsecure: allowInsecureCheckBox.checked
    property bool allowInsecureEditable: true
    property bool insecureAuthEnabledBySiteSettings: true

    property alias model: groupsComboBox.groupsModel
    property alias parentGroupsEditable: groupsComboBox.enabled

    property bool enabled: true
    property int userType: UserSettingsGlobal.LocalUser

    property bool linkEditable: true
    property alias linkValidFrom: linkDates.linkValidFrom
    property alias linkValidUntil: linkDates.linkValidUntil
    property alias expiresAfterLoginS: linkDates.expiresAfterLoginS
    property alias revokeAccessEnabled: linkDates.revokeAccessEnabled
    property alias firstLoginTime: linkDates.firstLoginTime
    property alias displayOffsetMs: linkDates.displayOffsetMs

    property bool ldapError: false
    property bool ldapOffline: true
    property bool linkReady: true
    property int continuousSync: LdapSettings.Sync.usersAndGroups
    property bool nameIsUnique: true

    property var self

    property var editingContext

    signal deleteRequested()
    signal auditTrailRequested()
    signal moreGroupsClicked()

    function isCloudUser()
    {
        return control.userType == UserSettingsGlobal.CloudUser
            || control.userType == UserSettingsGlobal.OrganizationUser
            || control.userType == UserSettingsGlobal.ChannelPartnerUser
    }

    function validate()
    {
        let result = true

        if (userLoginText.enabled)
            result = userLoginText.validate()

        if (!isCloudUser())
            return userEmailTextField.validate() && result

        return result
    }

    ColumnLayout
    {
        anchors.fill: parent

        spacing: 0

        Rectangle
        {
            id: loginPanel

            color: ColorTheme.colors.dark8
            Layout.fillWidth: true

            Layout.preferredHeight: Math.max(
                103,
                enabledUserSwitch.y + enabledUserSwitch.height + 22)

            ColoredImage
            {
                id: userTypeIcon

                x: 24
                y: 24
                width: 64
                height: 64

                sourcePath:
                {
                    switch (control.userType)
                    {
                        case UserSettingsGlobal.LocalUser:
                            return "image://skin/64x64/Solid/user.svg"
                        case UserSettingsGlobal.TemporaryUser:
                            return "image://skin/64x64/Solid/user_temp.svg"
                        case UserSettingsGlobal.CloudUser:
                            return "image://skin/64x64/Solid/user_cloud.svg"
                        case UserSettingsGlobal.OrganizationUser:
                            return "image://skin/64x64/Solid/user_organization.svg"
                        case UserSettingsGlobal.ChannelPartnerUser:
                            return "image://skin/64x64/Solid/user_cp.svg"
                        case UserSettingsGlobal.LdapUser:
                            return "image://skin/64x64/Solid/user_ldap.svg"
                    }
                }

                sourceSize: Qt.size(width, height)
                primaryColor: ColorTheme.colors.dark7
                secondaryColor: ColorTheme.colors.light10
                tertiaryColor: ColorTheme.colors.light4
            }

            EditableLabel
            {
                id: userLoginText

                enabled: control.loginEditable && control.enabled

                anchors.left: userTypeIcon.right
                anchors.leftMargin: 24
                anchors.top: userTypeIcon.top

                anchors.right: parent.right
                anchors.rightMargin: 16

                validateFunc: control.self
                    ? (text) => (isCloudUser()
                        ? control.self.validateEmail(text) : control.self.validateLogin(text))
                    : null

                Connections
                {
                    target: control.model

                    function onUserIdChanged() { userLoginText.forceFinishEdit() }
                }
            }

            UserEnabledSwitch
            {
                id: enabledUserSwitch

                enabled: control.userEnabledEditable && control.enabled

                anchors.top: userLoginText.bottom
                anchors.topMargin: 14
                anchors.left: userLoginText.left
            }

            Row
            {
                anchors.top: userLoginText.bottom
                anchors.topMargin: 14
                anchors.right: parent.right
                anchors.rightMargin: 20
                spacing: 8

                TextButton
                {
                    id: auditTrailButton

                    icon.source: "image://skin/20x20/Outline/audit_trail.svg"
                    text: qsTr("Audit Trail")
                    visible: control.auditAvailable
                    onClicked: control.auditTrailRequested()
                }

                TextButton
                {
                    id: deleteButton

                    visible: control.deleteAvailable
                    enabled: control.enabled
                    icon.source: "image://skin/20x20/Outline/delete.svg"
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

            contentItem: Rectangle
            {
                color: ColorTheme.colors.dark7

                width: scroll.width
                height: childrenRect.height

                Column
                {
                    id: contentColumn

                    spacing: 8
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.right: parent.right
                    anchors.rightMargin: 16

                    SectionHeader
                    {
                        text: qsTr("Info")
                    }

                    CenteredField
                    {
                        text: qsTr("Full Name")

                        TextField
                        {
                            id: userFullNameTextField
                            width: parent.width
                            readOnly: !control.fullNameEditable
                                || !control.enabled
                                || isCloudUser()
                        }
                    }

                    CenteredField
                    {
                        visible: isCloudUser() && control.isSelf

                        Item
                        {
                            id: externalLink

                            width: childrenRect.width
                            height: childrenRect.height
                            property string link: "?"

                            Text
                            {
                                id: linkText
                                text: qsTr("Account Settings")
                                color: ColorTheme.colors.brand_core
                                font: Qt.font({pixelSize: 14, weight: Font.Normal, underline: true})
                            }

                            Shape
                            {
                                id: linkArrow

                                width: 5
                                height: 5

                                anchors.left: linkText.right
                                anchors.verticalCenter: linkText.verticalCenter
                                anchors.verticalCenterOffset: -1
                                anchors.leftMargin: 6

                                ShapePath
                                {
                                    strokeWidth: 1
                                    strokeColor: linkText.color
                                    fillColor: "transparent"

                                    startX: 1; startY: 0
                                    PathLine { x: linkArrow.width; y: 0 }
                                    PathLine { x: linkArrow.width; y: linkArrow.height - 1 }
                                    PathMove { x: linkArrow.width; y: 0 }
                                    PathLine { x: 0; y: linkArrow.height }
                                }
                            }

                            MouseArea
                            {
                                id: mouseArea
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked:
                                {
                                    Qt.openUrlExternally(UserSettingsGlobal.accountManagementUrl())
                                }
                            }
                        }
                    }

                    CenteredField
                    {
                        text: qsTr("Email")
                        visible: !isCloudUser()

                        TextFieldWithValidator
                        {
                            id: userEmailTextField
                            width: parent.width
                            readOnly: !(control.emailEditable && control.enabled)

                            onTextChanged:
                                text = fixupFunc(text)

                            fixupFunc:
                                (text) => (control.self ? control.self.extractEmail(text) : text)

                            validateFunc: (text) =>
                            {
                                return control.self && enabled
                                    ? control.self.validateEmail(text, isCloudUser())
                                    : ""
                            }
                        }
                    }

                    CenteredField
                    {
                        // Change password.

                        visible: control.passwordEditable
                            && control.userType == UserSettingsGlobal.LocalUser

                        Item
                        {
                            height: changePasswordButton.height + 8

                            Component
                            {
                                id: changePasswordDialog

                                PasswordChangeDialog
                                {
                                    id: dialog

                                    transientParent: control.Window.window
                                    visible: false

                                    login: control.login
                                    askCurrentPassword: control.isSelf
                                    currentPasswordValidator: control.isSelf
                                        ? (text) => control.self.validateCurrentPassword(text)
                                        : null

                                    onAccepted:
                                    {
                                        control.password = newPassword
                                    }

                                    Connections
                                    {
                                        target: control.Window.window
                                        function onClosing() { dialog.reject() }
                                    }
                                }
                            }

                            Button
                            {
                                y: 4
                                id: changePasswordButton
                                text: qsTr("Change password")
                                enabled: control.enabled

                                onClicked:
                                {
                                    changePasswordDialog.createObject(control).openNew()
                                }
                            }
                        }
                    }

                    CenteredField
                    {
                        // Allow digest authentication.

                        visible: !isCloudUser()
                            && control.userType != UserSettingsGlobal.TemporaryUser
                            && control.insecureAuthEnabledBySiteSettings

                        Component
                        {
                            id: changePasswordDigestDialog

                            PasswordChangeDialog
                            {
                                id: dialog

                                transientParent: control.Window.window
                                visible: false

                                text: qsTr("Set password to enable insecure authentication")
                                login: control.login
                                askCurrentPassword: control.isSelf
                                showLogin: true

                                currentPasswordValidator: control.isSelf
                                    ? (text) => control.self.validateCurrentPassword(text)
                                    : null

                                onAccepted:
                                {
                                    allowInsecureCheckBox.checked = true
                                    control.password = newPassword
                                }

                                Connections
                                {
                                    target: control.Window.window
                                    function onClosing() { dialog.reject() }
                                }
                            }
                        }

                        CheckBox
                        {
                            id: allowInsecureCheckBox

                            text: qsTr("Allow insecure (digest) authentication")
                            font.pixelSize: 14

                            enabled: control.allowInsecureEditable && control.enabled

                            wrapMode: Text.WordWrap

                            anchors
                            {
                                left: parent.left
                                right: parent.right
                                leftMargin: -3
                            }

                            nextCheckState: () =>
                            {
                                // Enabling digest for LDAP user does not require password reset.
                                if (control.userType == UserSettingsGlobal.LdapUser)
                                    return checkState === Qt.Unchecked ? Qt.Checked : Qt.Unchecked

                                if (checkState === Qt.Unchecked)
                                    changePasswordDigestDialog.createObject(control).openNew()

                                return Qt.Unchecked
                            }
                        }
                    }

                    Item
                    {
                        height: 64 - contentColumn.spacing * 2
                        width: parent.width

                        visible: control.userType == UserSettingsGlobal.TemporaryUser
                                && !control.linkEditable
                                && linkDates.expiresInMs > 0

                        CenteredField
                        {
                            id: expiresTextField

                            y: contentColumn.spacing

                            text: qsTr("Access expires")

                            Text
                            {
                                anchors.baseline: parent.baseline

                                width: parent.width

                                text: (control.self || "") && "%1%2"
                                    .arg(linkDates.expirationDateText)
                                    .arg(control.self.durationFormat(linkDates.expiresInMs))

                                color: ColorTheme.colors.light4
                                font: Qt.font({pixelSize: 14, weight: Font.Normal})
                            }
                        }

                    }

                    CenteredField
                    {
                        text: qsTr("Permission Groups")

                        GroupsComboBox
                        {
                            id: groupsComboBox

                            width: parent.width
                        }
                    }

                    CenteredField
                    {
                        text: qsTr("Notification Language")

                        LanguageComboBox
                        {
                            id: languageComboBox

                            width: parent.width
                            enabled: control.localeEditable && control.enabled
                        }
                    }

                    CenteredField
                    {
                        // Visible only when the user is editing his own profile.
                        visible: control.isSelf

                        TextButton
                        {
                            id: interfaceLanguageButton
                            width: parent.width
                            text: qsTr("Interface Language")
                            icon.source: "image://skin/20x20/Outline/earth.svg"
                            icon.width: 20
                            icon.height: 20

                            onClicked:
                            {
                                control.self.openLookAndFeelSettings()
                            }
                        }
                    }

                    SectionHeader
                    {
                        id: accessLinkSection

                        text: qsTr("Access Link")
                        visible: control.userType == UserSettingsGlobal.TemporaryUser
                            && control.linkEditable
                    }

                    ColumnLayout
                    {
                        id: obtainLinkLayout
                        visible: accessLinkSection.visible && !control.linkReady

                        onVisibleChanged:
                        {
                            if (!visible)
                                allertAboutBadInternet.visible = false;
                        }

                        Timer
                        {
                            interval: 10000
                            repeat: false
                            running: obtainLinkLayout.visible
                            onTriggered:
                            {
                                allertAboutBadInternet.visible = obtainLinkLayout.visible
                            }
                        }

                        RowLayout
                        {
                            spacing: 8

                            TextButton
                            {
                                id: obtainLinkButton

                                icon.source: "image://skin/20x20/Outline/loading.svg?primary=light16"
                                icon.width: 20
                                icon.height: 20
                                spacing: 4
                                text: qsTr("Obtaining Link...")

                                RotationAnimation on iconRotation
                                {
                                    from: 0;
                                    to: 360;
                                    duration: 1200
                                    running: obtainLinkButton.visible
                                    loops: Animation.Infinite
                                }
                            }
                        }

                        RowLayout
                        {
                            id: allertAboutBadInternet
                            spacing: 8

                            Image
                            {
                                source: "image://skin/24x24/Solid/warning.svg?primary=yellow"
                                sourceSize.width: 24
                                sourceSize.height: 24
                            }

                            Text
                            {
                                text: qsTr("Ensure that this computer is able to connect to the %1",
                                    "%1 is the cloud name").arg(Branding.cloudName())
                                color: ColorTheme.colors.yellow_d1
                                font.pixelSize: 14
                            }
                        }
                    }

                    ColumnLayout
                    {
                        id: accessLinkLayout

                        visible: accessLinkSection.visible && control.linkReady

                        width: parent.width
                        spacing: 16

                        function openNewLinkDialog(showWarning)
                        {
                            const newLinkDialog = newLinkDialogComponent.createObject(control)

                            newLinkDialog.linkValidUntil = self.newValidUntilDate()
                            newLinkDialog.showWarning = false

                            newLinkDialog.openNew()
                        }

                        function openResetLinkDialog()
                        {
                            const newLinkDialog = newLinkDialogComponent.createObject(control)

                            newLinkDialog.linkValidUntil = control.linkValidUntil
                            if (control.revokeAccessEnabled)
                            {
                                newLinkDialog.revokeAccessEnabled = true
                                newLinkDialog.expiresAfterLoginS = control.expiresAfterLoginS
                            }

                            newLinkDialog.showWarning = true
                            newLinkDialog.openNew()
                        }

                        Component
                        {
                            id: newLinkDialogComponent

                            NewLinkDialog
                            {
                                id: dialog
                                login: control.login
                                self: control.self
                                isSaving: !control.enabled
                                transientParent: control.Window.window

                                onAccepted:
                                {
                                    control.self.onResetLink(
                                        linkValidUntil,
                                        revokeAccessEnabled
                                            ? expiresAfterLoginS
                                            : -1,
                                        ok => { if (ok) close() })
                                }

                                onRejected:
                                {
                                    if (isSaving)
                                        control.self.cancelRequest()
                                }

                                Connections
                                {
                                    target: control.Window.window
                                    function onClosing() { dialog.reject() }
                                }
                            }
                        }

                        TemporaryLinkDates
                        {
                            id: linkDates

                            property var currentServerTimePointMs:
                                NxGlobals.syncTimeCurrentTimePointMs()
                            readonly property bool expired: expiresInMs <= 0
                            readonly property var expiresInMs:
                                expirationDate.getTime() - currentServerTimePointMs

                            timeOffsetProvider: control.self

                            Timer
                            {
                                interval: 1000
                                running: control.userType == UserSettingsGlobal.TemporaryUser
                                repeat: true
                                onTriggered:
                                {
                                    linkDates.currentServerTimePointMs =
                                        NxGlobals.syncTimeCurrentTimePointMs()
                                }
                            }
                        }

                        Text
                        {
                            visible: !linkDates.expired

                            wrapMode: Text.WordWrap
                            textFormat: Text.StyledText

                            color: ColorTheme.colors.light16
                            font: Qt.font({pixelSize: 14, weight: Font.Normal})
                            width: parent.width

                            text: linkDates.validityDatesText
                        }

                        RowLayout
                        {
                            visible: !linkDates.expired

                            spacing: 8

                            Button
                            {
                                id: copyLinkButton

                                property bool copied: false
                                readonly property string copyLinkText: qsTr("Copy Link")
                                readonly property string copiedText: qsTr("Copied",
                                    "Copied here means that a link is copied")
                                enabled: control.enabled
                                text: copied ? copiedText : copyLinkText
                                icon.source:
                                    copied ? "image://skin/20x20/Outline/check.svg?primary=light10" : ""
                                icon.width: 20
                                icon.height: 20
                                Layout.preferredWidth:
                                {
                                    const copyLinkWidth = leftPaddingOnlyText
                                        + fontMetrics.advanceWidth(copyLinkText) + rightPadding
                                    const copiedWidth = leftPaddingWithIcon + icon.width + 4
                                        + fontMetrics.advanceWidth(copiedText) + rightPadding
                                    return Math.max(copyLinkWidth, copiedWidth)
                                }

                                FontMetrics
                                {
                                    id: fontMetrics
                                    font: copyLinkButton.font
                                }

                                Timer
                                {
                                    interval: 3000
                                    repeat: false
                                    running: copyLinkButton.copied
                                    onTriggered: copyLinkButton.copied = false
                                }

                                onClicked:
                                {
                                    control.self.onCopyLink()
                                    copied = true
                                }
                            }

                            TextButton
                            {
                                icon.source: "image://skin/20x20/Outline/terminate.svg"
                                icon.width: 20
                                icon.height: 20
                                spacing: 4
                                text: qsTr("Terminate")
                                enabled: control.enabled

                                onClicked: control.self.onTerminateLink()
                            }

                            TextButton
                            {
                                icon.source: "image://skin/20x20/Outline/rollback.svg"
                                icon.width: 20
                                icon.height: 20
                                spacing: 4
                                text: qsTr("New Link...")
                                enabled: control.enabled

                                onClicked: accessLinkLayout.openResetLinkDialog()
                            }
                        }

                        Text
                        {
                            Layout.alignment: Qt.AlignHCenter
                            visible: linkDates.expired

                            text: qsTr("No valid link for this user")
                            color: ColorTheme.colors.light16
                            font: Qt.font({pixelSize: 14, weight: Font.Normal})
                        }

                        Button
                        {
                            Layout.alignment: Qt.AlignHCenter
                            visible: linkDates.expired
                            enabled: control.enabled

                            text: qsTr("New Link...")
                            onClicked: accessLinkLayout.openNewLinkDialog()
                        }
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
                id: bannerUserNotFound

                style: DialogBanner.Style.Error
                closeable: true
                watchToReopen: control.userId
                visible: control.ldapError && !closed && !control.isSelf
                Layout.fillWidth: true

                text: qsTr("This user is not found in LDAP database and is not able to log in.")

                buttonText: control.deleteAvailable && control.enabled ? qsTr("Delete") : ""
                buttonIcon: "image://skin/20x20/Outline/delete.svg?primary=light4"

                onButtonClicked: control.deleteRequested()
            }

            DialogBanner
            {
                id: ldapServerIsOffline

                style: DialogBanner.Style.Error
                closeable: true
                watchToReopen: control.userId
                visible: control.ldapOffline && !closed && !control.isSelf
                Layout.fillWidth: true

                text: qsTr("LDAP server is offline. User is not able to log in.")
            }

            DialogBanner
            {
                style: DialogBanner.Style.Error
                visible: !control.nameIsUnique && control.userEnabled && !closed && !control.isSelf
                closeable: true
                watchToReopen: control.userId
                Layout.fillWidth: true

                text: qsTr("This user’s login duplicates the login of another user. None of them is "
                    + "able to log in. To resolve this issue you can change user’s login or disable "
                    + "or delete users with duplicating logins.")
            }

            DialogBanner
            {
                style: DialogBanner.Style.Warning
                closeable: true
                watchToReopen: control.userId
                Layout.fillWidth: true

                visible: control.userType === UserSettingsGlobal.LdapUser
                    && control.continuousSync === LdapSettings.Sync.disabled
                    && !closed

                text: qsTr("When continuous sync with LDAP server is disabled, user membership in " +
                    "groups does not synchronize automatically. To update this information, " +
                    "initiate a manual sync.")
            }

            InsecureBanner
            {
                Layout.fillWidth: true

                login: control.login
                watchToReopen: control.userId
                visible: allowInsecureCheckBox.checked
                    && allowInsecureCheckBox.visible
            }
        }
    }
}
