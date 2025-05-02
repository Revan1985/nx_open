// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core
import Nx.Controls

PanelBase
{
    id: panel

    required property var engineInfo
    required property var licenseSummary

    property bool checkable: true
    property bool checked: false
    property bool refreshing: false
    property bool refreshable: false
    property bool removable: false
    property bool offline: false

    property alias streamSelectorVisible: panel.contentVisible
    property alias streamSelectorEnabled: streamSelection.enabled
    property alias currentStreamIndex: streamComboBox.currentIndex

    readonly property real kLabelWidthFraction: 0.3

    signal enableSwitchClicked()
    signal refreshButtonClicked()
    signal removeClicked()

    rejectButtonVisible: false

    headerItem: IntegrationHeader
    {
        engineInfo: panel.engineInfo
        licenseSummary: panel.licenseSummary
        checkable: panel.checkable
        checked: panel.checked
        refreshing: panel.refreshing
        refreshable: panel.refreshable
        offline: panel.offline
        removable: panel.removable
        onEnableSwitchClicked: panel.enableSwitchClicked()
        onRefreshButtonClicked: panel.refreshButtonClicked()
        onRemoveClicked: panel.removeClicked()
    }

    description: engineInfo ? engineInfo.description : ""

    sourceItem: Grid
    {
        columns: 2
        columnSpacing: 8
        rowSpacing: 8
        flow: GridLayout.LeftToRight

        Text
        {
            text: qsTr("Version")
            color: ColorTheme.colors.light16
            visible: version.visible
        }

        Text
        {
            id: version
            text: engineInfo ? engineInfo.version : ""
            color: ColorTheme.colors.light10
            visible: !!text
        }

        Text
        {
            text: qsTr("Vendor")
            color: ColorTheme.colors.light16
            visible: vendor.visible
        }

        Text
        {
            id: vendor
            text: engineInfo ? engineInfo.vendor : ""
            color: ColorTheme.colors.light10
            visible: !!text
        }

        Text
        {
            text: qsTr("Usage")
            color: ColorTheme.colors.light16
            visible: usage.visible
        }

        Text
        {
            id: usage
            text: licenseSummary ? licenseSummary.inUse + "/" + licenseSummary.available : ""
            color: !!licenseSummary && licenseSummary.inUse > licenseSummary.available
                ? ColorTheme.colors.red_core
                : ColorTheme.colors.light10
            visible: !panel.checkable && !!text
        }
    }

    permissions
    {
        highlighted: !!request && !!request.permission
        collapsible: !request
        permission: (engineInfo && engineInfo.permission) || (request && request.permission) || ""
    }

    content: Row
    {
        id: streamSelection

        spacing: 8

        ContextHintLabel
        {
            id: streamLabel

            text: qsTr("Camera stream")
            horizontalAlignment: Text.AlignRight
            contextHintText: qsTr("Select video stream from the camera for analysis")
            color: ColorTheme.colors.light16
            width: mapFromItem(panel, panel.width * kLabelWidthFraction, 0).x
            y: streamComboBox.baselineOffset - baselineOffset
        }

        ComboBox
        {
            id: streamComboBox

            width: parent.width - x
            model: ["Primary", "Secondary"]
        }
    }
}
