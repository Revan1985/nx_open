// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Shapes

import Qt5Compat.GraphicalEffects

import Nx.Core
import Nx.Controls

import nx.vms.client.core
import nx.vms.client.desktop

import "../Components"

Item
{
    id: control

    property alias userEnabled: enabledUserSwitch.checked
    property alias login: loginTextField.text
    property alias fullName: fullNameTextField.text
    property alias email: emailTextField.text
    property alias password: passwordTextField.text
    property alias locale: languageComboBox.value
    property alias allowInsecure: allowInsecureCheckBox.checked
    property alias linkValidUntil: temporaryLinkSettings.linkValidUntil
    property alias expiresAfterLoginS: temporaryLinkSettings.expiresAfterLoginS
    property alias revokeAccessEnabled: temporaryLinkSettings.revokeAccessEnabled
    property bool insecureAuthEnabledBySiteSettings: true

    property int userType: UserSettingsGlobal.LocalUser
    readonly property bool isLocalUser: userType != UserSettingsGlobal.CloudUser

    property string savedEmail

    onIsLocalUserChanged:
    {
        if (!savedEmail)
            savedEmail = email
        else
            [email, savedEmail] = [savedEmail, email]

        if (isLocalUser)
        {
            if (!login)
                loginTextField.textField.warningState = false
            if (!email)
                emailTextField.textField.warningState = false
        }
        setDefaultFocus()
    }

    property var self
    property alias model: groupsComboBox.groupsModel

    function validate()
    {
        switch (userType)
        {
            case UserSettingsGlobal.LocalUser:
                return loginTextField.validate()
                    && emailTextField.validate()
                    && passwordTextField.validate()
                    && passwordTextField.strengthAccepted
                    && confirmPasswordTextField.validate()

            case UserSettingsGlobal.TemporaryUser:
                return loginTextField.validate()
                    && emailTextField.validate()

            case UserSettingsGlobal.CloudUser:
                return emailTextField.validate()
        }

        return true
    }

    function setDefaultFocus()
    {
        const textField = isLocalUser
            ? loginTextField
            : emailTextField

        if (!textField.text)
            textField.forceActiveFocus()
    }

    Scrollable
    {
        id: scroll
        anchors.fill: parent
        anchors.bottom: insecureBanner.top

        contentItem: Column
        {
            spacing: 8

            x: 16
            width: scroll.width - 16 * 2

            component Spacer: Item
            {
                width: 1
                height: 8
            }

            SectionHeader
            {
                text: qsTr("New User")
            }

            CenteredField
            {
                UserEnabledSwitch
                {
                    height: 28
                    id: enabledUserSwitch
                }
            }

            CenteredField
            {
                id: typeField

                text: qsTr("Type")

                visible: control.self ? control.self.isConnectedToCloud() : false

                ButtonGroup
                {
                    buttons: typeButtonsRow.children
                    exclusive: true
                    onCheckedButtonChanged:
                    {
                        control.userType = localUserButton.checked
                            ? (accessComboBox.currentIndex == 0
                                ? UserSettingsGlobal.LocalUser
                                : UserSettingsGlobal.TemporaryUser)
                            : UserSettingsGlobal.CloudUser
                        setDefaultFocus()
                    }
                }

                Row
                {
                    id: typeButtonsRow

                    spacing: 0

                    ToggleButton
                    {
                        id: localUserButton
                        text: qsTr("Local")
                        icon.source: "image://skin/20x20/Solid/user.svg"
                        flatSide: -1 //< Rigth.
                        checked: control.userType == UserSettingsGlobal.LocalUser
                            || control.userType == UserSettingsGlobal.TemporaryUser

                        onCheckedChanged:
                        {
                            if (checked && visualFocus)
                                loginTextField.textField.forceActiveFocus()
                        }
                    }

                    ToggleButton
                    {
                        text: qsTr("Cloud")
                        icon.source: "image://skin/20x20/Outline/cloud.svg"
                        flatSide: 1 //< Left.
                        checked: control.userType == UserSettingsGlobal.CloudUser

                        onCheckedChanged:
                        {
                            if (checked && visualFocus)
                                emailTextField.textField.forceActiveFocus()
                        }
                    }
                }
            }

            Spacer
            {
                visible: typeField.visible
            }

            CenteredField
            {
                visible: control.userType != UserSettingsGlobal.CloudUser

                text: qsTr("Login")

                TextFieldWithValidator
                {
                    id: loginTextField
                    width: parent.width
                    fixupFunc: (text) => (text.trim())
                    validateFunc: control.self ? (text) => control.self.validateLogin(text) : null
                }
            }

            CenteredField
            {
                visible: control.userType != UserSettingsGlobal.CloudUser

                text: qsTr("Full Name")

                TextField
                {
                    id: fullNameTextField
                    width: parent.width
                }
            }

            CenteredField
            {
                text: qsTr("Email")

                TextFieldWithValidator
                {
                    id: emailTextField
                    width: parent.width

                    onTextChanged:
                        text = fixupFunc(text)

                    fixupFunc: (text) => (control.self ? control.self.extractEmail(text) : text)

                    validateFunc: (text) =>
                    {
                        return control.self
                            ? control.self.validateEmail(
                                text, control.userType == UserSettingsGlobal.CloudUser)
                            : ""
                    }
                }
            }

            Spacer {}

            CenteredField
            {
                visible: control.userType == UserSettingsGlobal.LocalUser
                    || control.userType == UserSettingsGlobal.TemporaryUser

                text: qsTr("Access")

                ComboBox
                {
                    id: accessComboBox
                    width: parent.width
                    textRole: "text"
                    valueRole: "value"

                    model:
                    [
                        {
                            text: qsTr("Regular user with credentials"),
                            value: UserSettingsGlobal.LocalUser
                        },
                        {
                            text: qsTr("Temporary with link"),
                            value: UserSettingsGlobal.TemporaryUser
                        },
                    ]

                    onCurrentValueChanged:
                    {
                        if (control.userType == UserSettingsGlobal.LocalUser
                            || control.userType == UserSettingsGlobal.TemporaryUser)
                        {
                            control.userType = accessComboBox.currentValue
                        }
                    }
                }
            }

            TemporaryLinkSettings
            {
                id: temporaryLinkSettings

                visible: control.userType == UserSettingsGlobal.TemporaryUser
                self: control.self
            }

            CenteredField
            {
                visible: control.userType != UserSettingsGlobal.CloudUser
                    && control.userType != UserSettingsGlobal.TemporaryUser

                text: qsTr("Password")

                PasswordFieldWithWarning
                {
                    id: passwordTextField
                    width: parent.width
                    validateFunc: (text) => UserSettingsGlobal.validatePassword(text)
                    onTextChanged: confirmPasswordTextField.warningState = false
                }
            }

            CenteredField
            {
                visible: control.userType != UserSettingsGlobal.CloudUser
                    && control.userType != UserSettingsGlobal.TemporaryUser

                text: qsTr("Confirm Password")

                PasswordFieldWithWarning
                {
                    id: confirmPasswordTextField
                    width: parent.width
                    showStrength: false
                    validateFunc: (text) =>
                    {
                        return passwordTextField.text == confirmPasswordTextField.text
                            ? ""
                            : qsTr("Passwords do not match")
                    }
                }
            }

            CenteredField
            {
                visible: control.userType != UserSettingsGlobal.CloudUser
                    && control.userType != UserSettingsGlobal.TemporaryUser
                    && control.insecureAuthEnabledBySiteSettings

                CheckBox
                {
                    id: allowInsecureCheckBox
                    text: qsTr("Allow insecure (digest) authentication")

                    font.pixelSize: 14
                    wrapMode: Text.WordWrap

                    anchors
                    {
                        left: parent.left
                        right: parent.right
                        leftMargin: -3
                    }
                }
            }

            CenteredField
            {
                visible: control.userType == UserSettingsGlobal.CloudUser

                Text
                {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    font: Qt.font({pixelSize: 14, weight: Font.Normal})
                    color: ColorTheme.colors.light16
                    text: qsTr("You need to specify only user's email address.")
                        + "\n\n"
                        + qsTr("The added site will quickly become visible to users with an"
                        + " existing cloud account, while users without an existing cloud account"
                        + " will receive instructions by Email.")
                }
            }

            Spacer {}

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
                visible: control.userType != UserSettingsGlobal.CloudUser

                text: qsTr("Notification Language")

                LanguageComboBox
                {
                    id: languageComboBox

                    width: parent.width
                }
            }
        }
    }

    InsecureBanner
    {
        id: insecureBanner
        anchors.left: control.left
        anchors.right: control.right
        anchors.bottom: control.bottom

        login: control.login
        visible: allowInsecureCheckBox.checked
            && allowInsecureCheckBox.visible
    }
}
