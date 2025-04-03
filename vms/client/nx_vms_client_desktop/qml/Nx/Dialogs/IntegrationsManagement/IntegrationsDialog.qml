// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Window

import Nx.Core
import Nx.Controls
import Nx.Dialogs

import nx.vms.client.desktop

Dialog
{
    id: dialog
    objectName: "IntegrationsDialog"

    modality: Qt.ApplicationModal

    width: minimumWidth
    height: minimumHeight

    minimumWidth: 768
    minimumHeight: 700

    title: qsTr("Manage Integrations")
    color: ColorTheme.colors.dark7

    property var store: null
    property var requestsModel: store ? store.makeApiIntegrationRequestsModel() : null

    function setCurrentTab(type)
    {
        const index = tabs.visibleTabs.findIndex((tab) => tab.type === type)
        tabs.currentTabIndex = index >= 0 ? index : 0
    }

    WindowContextAware.onBeforeSystemChanged: reject()

    DialogTabControl
    {
        id: tabs

        anchors.fill: parent
        anchors.bottomMargin: buttonBox.height

        dialogLeftPadding: dialog.leftPadding
        dialogRightPadding: dialog.rightPadding
        tabBar.spacing: 0

        Tab
        {
            readonly property int type: IntegrationsDialog.Tab.integrations

            button: PrimaryTabButton
            {
                text: qsTr("Integrations")
            }

            page: IntegrationsTab
            {
                store: dialog.store
                requestsModel: dialog.requestsModel
                visible: dialog.visible
            }
        }

        Tab
        {
            readonly property int type: IntegrationsDialog.Tab.settings

            button: PrimaryTabButton
            {
                text: qsTr("Settings")
            }

            page: SettingsTab
            {
                store: dialog.store
            }
        }
    }

    buttonBox: DialogButtonBox
    {
        buttonLayout: DialogButtonBox.KdeLayout
        standardButtons: DialogButtonBox.Ok | DialogButtonBox.Apply | DialogButtonBox.Cancel

        Component.onCompleted:
        {
            let applyButton = buttonBox.standardButton(DialogButtonBox.Apply)
            applyButton.enabled = Qt.binding(function() { return !!store && store.hasChanges })
        }
    }

    onAccepted:
    {
        if (store)
        {
            store.applySettingsValues()
            store.applySystemSettings()
        }
    }

    onApplied:
    {
        if (store)
        {
            store.applySettingsValues()
            store.applySystemSettings()
        }
    }

    onRejected:
    {
        if (store)
            store.discardChanges()
    }

    onVisibleChanged:
    {
        if (visible)
            store.refreshSettings()
    }
}
