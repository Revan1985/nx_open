// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls as QuickControls

import Nx.Controls
import Nx.Core
import Nx.Dialogs
import Nx.Screens
import Nx.Settings
import Nx.Ui

import nx.vms.client.core
import nx.vms.client.mobile

BaseSettingsScreen
{
    id: settingsScreen
    objectName: "developerSettingsScreen"

    title: qsTr("Developer Settings")
    onLeftButtonClicked: Workflow.popCurrentScreen()
    padding: 16

    DeveloperSettingsHelper
    {
        id: helper
    }

    Column
    {
        spacing: 16

        width: parent.width

        Button
        {
            text: "Change Log Level: " + helper.logLevel
            onClicked:
            {
                logLevelDialog.open()
            }
        }

        Row
        {
            visible: !d.logSessionId
            spacing: 8
            Button
            {
                text: "Upload logs next"
                onClicked:
                {
                    d.logSessionId = windowContext.logManager.startRemoteLogSession(
                        parseInt(minutesComboBox.currentValue))
                }
            }

            QuickControls.ComboBox
            {
                id: minutesComboBox

                anchors.verticalCenter: parent.verticalCenter
                model: [1, 3, 5, 10, 15, 30]
                width: 80
            }

            Text
            {
                anchors.verticalCenter: parent.verticalCenter
                text: "minute(s)"
                font.pixelSize: 16
                color: ColorTheme.colors.light16
            }
        }

        Row
        {
            spacing: 8

            Text
            {
                text: "Language"
                font.pixelSize: 16
                color: ColorTheme.colors.light16
                anchors.verticalCenter: parent.verticalCenter
            }

            QuickControls.ComboBox
            {
                id: languageComboBox

                model: TranslationListModel {}
                currentIndex: !!appContext.settings.locale
                    ? model.localeIndex(appContext.settings.locale)
                    : -1
                textRole: "display"
                valueRole: "localeCode"
                width: 200

                onCurrentValueChanged: appContext.settings.locale = currentValue
            }
        }

        Row
        {
            id: customCloudHostRow

            spacing: 8
            visible: !supportMetaOrganizations.visible
                || !appContext.settings.supportMetaOrganizations

            TextField
            {
                id: customCloudHostTextField

                text: appContext.settings.customCloudHost
                width: settingsScreen.width / 2
            }

            Button
            {
                text: "Set cloud host"

                onClicked:
                {
                    const value = customCloudHostTextField.text.trim()
                    if (value === appContext.settings.customCloudHost)
                        return

                    appContext.settings.customCloudHost = value
                    d.openRestartDialog()
                }
            }
        }

        LabeledSwitch
        {
            id: forceTrafficLoggingSwitch

            width: parent.width
            text: "Force traffic logging"
            checkState: appContext.settings.forceTrafficLogging ? Qt.Checked : Qt.Unchecked
            onCheckStateChanged:
            {
                const value = checkState != Qt.Unchecked
                if (value == appContext.settings.forceTrafficLogging)
                    return

                appContext.settings.forceTrafficLogging = value
            }
        }

        LabeledSwitch
        {
            id: ignoreCustomizationSwitch

            width: parent.width
            text: "Ignore customization"
            checkState: appContext.settings.ignoreCustomization ? Qt.Checked : Qt.Unchecked
            onCheckStateChanged:
            {
                const value = checkState != Qt.Unchecked
                if (value == appContext.settings.ignoreCustomization)
                    return

                appContext.settings.ignoreCustomization = value
                d.openRestartDialog()
            }
        }

        LabeledSwitch
        {
            id: supportMetaOrganizations

            width: parent.width
            text: "Show Meta Organizations"
            visible: Branding.customization() == "default"
            checkState: appContext.settings.supportMetaOrganizations ? Qt.Checked : Qt.Unchecked
            onCheckStateChanged:
            {
                const value = checkState != Qt.Unchecked
                if (value == appContext.settings.supportMetaOrganizations)
                    return

                appContext.settings.supportMetaOrganizations = value
                appContext.settings.customCloudHost = value
                    ? "meta.nxvms.com"
                    : ""
                d.openRestartDialog()
            }
        }

        Column
        {
            visible: !!d.logSessionId

            Text
            {
                text: "Copied to clipboard:"
                font.pixelSize: 16
                color: ColorTheme.colors.light16
            }

            Text
            {
                font.pixelSize: 16
                text: d.logSessionId
                color: ColorTheme.colors.light1
            }

            Button
            {
                text: qsTr("Copy log ID")
                onClicked: d.copySessionIdToClipboard()
            }
        }
    }

    ItemSelectionDialog
    {
        id: logLevelDialog
        title: qsTr("Log Level")
        currentItem: helper.logLevel
        onCurrentItemChanged: helper.logLevel = currentItem
        model: [
            "NONE",
            "ALWAYS",
            "ERROR",
            "WARNING",
            "INFO",
            "DEBUG1",
            "DEBUG2"]
    }

    NxObject
    {
        id: d

        function copySessionIdToClipboard()
        {
            if (logSessionId)
                windowContext.ui.windowHelpers.copyToClipboard(d.logSessionId)
        }

        function openRestartDialog()
        {
            Workflow.openStandardDialog("Please restart the app to apply the changes")
        }

        property string logSessionId: windowContext.logManager.remoteLogSessionId()

        onLogSessionIdChanged: copySessionIdToClipboard()
        Component.onCompleted: copySessionIdToClipboard()
    }
}
