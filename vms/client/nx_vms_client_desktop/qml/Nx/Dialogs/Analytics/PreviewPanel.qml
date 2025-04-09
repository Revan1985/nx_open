// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Core.Items
import Nx.Controls
import Nx.Items
import Nx.RightPanel

import nx.vms.client.core
import nx.vms.client.desktop
import nx.vms.client.desktop.analytics as Analytics

Rectangle
{
    id: previewPanel

    property var selectedItem: null
    property alias slideAnimationEnabled: panelAnimation.enabled

    property alias nextEnabled: intervalPreviewControls.nextEnabled
    property alias prevEnabled: intervalPreviewControls.prevEnabled

    property int loadingIndicatorTimeoutMs: 5000

    signal prevClicked()
    signal nextClicked()
    signal showOnLayoutClicked()
    signal close()

    signal searchRequested(var attribute)

    color: ColorTheme.colors.dark8

    Behavior on x
    {
        id: panelAnimation
        enabled: false

        NumberAnimation
        {
            id: slideAnimation
            duration: 200
            easing.type: Easing.Linear

            onRunningChanged:
            {
                // Disable the animation right away to avoid running it on window resize.
                if (!running)
                    panelAnimation.enabled = false
            }
        }
    }

    onSelectedItemChanged:
    {
        if (selectedItem?.trackId !== intervalPreview.trackId)
        {
            intervalPreview.resource = null
            intervalPreview.startTimeMs = 0
            intervalPreview.trackId = selectedItem?.trackId
        }

        if (selectedItem?.previewResource)
        {
            intervalPreview.resource = selectedItem.previewResource
            intervalPreview.startTimeMs = selectedItem.previewTimestampMs
            intervalPreview.durationMs = selectedItem.previewDurationMs
            intervalPreview.aspectRatio = selectedItem.previewAspectRatio
        }
    }

    RowLayout
    {
        id: header

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 8
        anchors.leftMargin: 12
        anchors.rightMargin: 12

        Text
        {
            id: displayText

            Layout.fillWidth: true
            color: ColorTheme.colors.light10
            font.pixelSize: 0
            font.weight: FontConfig.normal.weight

            elide: Text.ElideRight

            text: previewPanel.selectedItem ? previewPanel.selectedItem.display : ""
        }

        ImageButton
        {
            id: closeButton

            icon.source: "image://skin/16x16/Outline/close.svg"
            onClicked:
            {
                previewPanel.slideAnimationEnabled = true
                previewPanel.close()
            }
        }
    }

    Rectangle
    {
        id: playerContainer
        color: ColorTheme.colors.dark7
        border.color: ColorTheme.colors.dark5
        radius: 1

        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 8
        anchors.leftMargin: 12
        anchors.rightMargin: 12

        height: Math.min(width * (9 / 16) + intervalPreviewControls.height, parent.height / 2)

        IntervalPreview
        {
            id: intervalPreview

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: parent.height - intervalPreviewControls.height

            color: ColorTheme.colors.dark3

            active: !!previewPanel.selectedItem
            loopDelayMs: 0 //< No need to pause because the user has manual controls.
            speedFactor: 1.0 //< This is a regular player (not tile preview) so use normal speed.

            property var trackId: null

            Rectangle
            {
                id: previewUnavailablePlaceholder

                anchors.fill: parent
                color: ColorTheme.colors.dark3
                radius: 1

                NxDotPreloader
                {
                    id: preloader

                    anchors.centerIn: parent
                    color: ColorTheme.colors.dark11

                    opacity: running ? 1.0 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 200 }}
                }

                Text
                {
                    id: previewUnavailableText

                    anchors.centerIn: parent
                    width: Math.min(parent.width, 130)

                    color: ColorTheme.colors.dark11
                    font: Qt.font({pixelSize: 12, weight: Font.Normal})

                    text: qsTr("Preview is not available for the selected object")
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap

                    opacity: preloader.running ? 0.0 : 1.0
                    visible: !preloader.running

                    Behavior on opacity { NumberAnimation { duration: 100 }}

                    Timer
                    {
                        interval: previewPanel.loadingIndicatorTimeoutMs
                        running: preloader.running
                        repeat: false

                        onTriggered:
                            preloader.running = false
                    }
                }

                function updateState()
                {
                    const loadingOrMissing = previewPanel.selectedItem
                        && intervalPreview.previewState !== EventSearch.PreviewState.ready

                    preloader.running = loadingOrMissing
                        && intervalPreview.previewState !== EventSearch.PreviewState.missing

                    visible = loadingOrMissing
                }
            }

            onResourceChanged:
                previewUnavailablePlaceholder.updateState()

            onPreviewStateChanged:
                previewUnavailablePlaceholder.updateState()
        }

        IntervalPreviewControls
        {
            id: intervalPreviewControls

            anchors.top: intervalPreview.bottom
            anchors.left: parent.left
            anchors.right: parent.right

            preview: intervalPreview
            selectedItem: previewPanel.selectedItem

            onPrevClicked: previewPanel.prevClicked()
            onNextClicked: previewPanel.nextClicked()
        }
    }

    Item
    {
        anchors.left: playerContainer.left
        anchors.top: playerContainer.bottom
        anchors.right: playerContainer.right
        anchors.bottom: buttonBox.top

        Column
        {
            anchors.centerIn: parent

            spacing: 6

            visible: !previewPanel.selectedItem

            Text
            {
                anchors.horizontalCenter: parent.horizontalCenter

                color: ColorTheme.colors.light16
                horizontalAlignment: Text.AlignHCenter

                font.pixelSize: FontConfig.large.pixelSize

                text: qsTr("No Preview")
            }

            Text
            {
                anchors.horizontalCenter: parent.horizontalCenter

                color: ColorTheme.colors.light16
                horizontalAlignment: Text.AlignHCenter

                wrapMode: Text.WordWrap
                width: 150
                font.pixelSize: 12

                text: qsTr("Select the object to display the preview")
            }
        }

        Column
        {
            id: dateTimeColumn

            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: 16

            spacing: 4

            Text
            {
                id: timeText
                color: ColorTheme.colors.light16
                anchors.right: parent.right
                font.pixelSize: FontConfig.xLarge.pixelSize
                text: previewPanel.selectedItem
                    ? previewPanel.selectedItem.timestamp.split(" ", 2).pop()
                    : ""
            }

            Text
            {
                id: dateText
                color: ColorTheme.colors.light16
                anchors.right: parent.right
                font.pixelSize: FontConfig.small.pixelSize
                text:
                {
                    if (!previewPanel.selectedItem)
                        return ""

                    const parts = previewPanel.selectedItem.timestamp.split(" ", 2)
                    return parts.length > 1 ? parts[0] : ""
                }
            }
        }

        Column
        {
            id: displayColumn

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: dateTimeColumn.left
            anchors.topMargin: 16

            spacing: 4

            ResourceList
            {
                id: resourceList

                width: parent.width

                color: ColorTheme.colors.light10
                remainderColor: ColorTheme.colors.light16

                resourceNames: previewPanel.selectedItem
                    ? previewPanel.selectedItem.resourceList
                    : []
            }
        }

        RowLayout
        {
            id: rowLayout

            anchors.top: displayColumn.bottom
            anchors.topMargin: 16
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 12
            anchors.left: parent.left
            anchors.leftMargin: -12
            anchors.right: parent.right
            anchors.rightMargin: -12

            spacing: 24

            Column
            {
                id: titleColumn

                Layout.alignment: Qt.AlignTop
                Layout.maximumHeight: rowLayout.height
                Layout.maximumWidth: 146

                leftPadding: 20
                rightPadding: 8
                spacing: 16

                visible: !!(titleText.text || titleImage.imageTrackId)

                Text
                {
                    id: titleText

                    width: titleColumn.width

                    font.pixelSize: 18
                    font.weight: Font.Medium

                    color: ColorTheme.colors.light4
                    textFormat: Text.PlainText

                    text: previewPanel.selectedItem ? previewPanel.selectedItem.objectTitle : ""
                    GlobalToolTip.text: text
                    GlobalToolTip.enabled: truncated
                    visible: !!text

                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }

                Image
                {
                    id: titleImage

                    readonly property string imageTrackId:
                        previewPanel.selectedItem && previewPanel.selectedItem.hasTitleImage
                            ? previewPanel.selectedItem.trackId.toSimpleString()
                            : ""

                    readonly property real availableHeight:
                        rowLayout.height - titleImage.y - titleColumn.bottomPadding

                    width: Math.min(implicitWidth, titleColumn.width)
                    height: Math.min(implicitHeight, availableHeight)
                    horizontalAlignment: Image.AlignHCenter
                    verticalAlignment: Image.AlignTop
                    fillMode: Image.PreserveAspectFit

                    visible: !!imageTrackId

                    source:
                    {
                        if (!selectedItem || !selectedItem.previewResource || !imageTrackId)
                            return ""

                        const path = `rest/v4/analytics/objectTracks/${imageTrackId}/titleImage.jpg`
                        const deviceId = selectedItem.previewResource.id.toSimpleString()
                        return `image://remote/${path}?deviceId=${deviceId}`
                    }

                    NxDotPreloader
                    {
                        color: ColorTheme.colors.light10
                        running: titleImage.status === Image.Loading
                    }
                }
            }

            ScrollView
            {
                id: nameValueScrollView

                Layout.fillHeight: true
                Layout.fillWidth: true

                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                contentWidth: availableWidth - 8 //< Mind vertical scrollbar width.
                contentHeight: attributeTable.height
                clip: true

                Flickable
                {
                    anchors.fill: parent
                    boundsBehavior: Flickable.StopAtBounds

                    AnalyticsAttributeTable
                    {
                        id: attributeTable

                        attributes: previewPanel.selectedItem
                            ? previewPanel.selectedItem.attributes
                            : []

                        visible: items.length
                        width: nameValueScrollView.contentWidth

                        nameColor: ColorTheme.colors.light16
                        valueColor: ColorTheme.colors.light10
                        nameFont { pixelSize: FontConfig.normal.pixelSize; weight: Font.Normal }
                        valueFont { pixelSize: FontConfig.normal.pixelSize; weight: Font.Normal }

                        interactive: true

                        contextMenu: Menu
                        {
                            id: menu

                            property var attribute

                            onAboutToShow: attribute = attributeTable.hoveredItem
                            onAboutToHide: attribute = undefined

                            MenuItem
                            {
                                text: qsTr("Copy")
                                onTriggered:
                                {
                                    if (menu.attribute)
                                    {
                                        NxGlobals.copyToClipboard(
                                            menu.attribute.displayedValues.join(", "))
                                    }
                                }
                            }

                            MenuItem
                            {
                                text: qsTr("Filter by")
                                onTriggered:
                                {
                                    if (menu.attribute)
                                        previewPanel.searchRequested(menu.attribute)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    DialogPanel
    {
        id: buttonBox

        color: ColorTheme.colors.dark8
        width: parent.width
        height: 52

        Button
        {
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter

            isAccentButton: true

            enabled: !!previewPanel.selectedItem

            text: qsTr("Show on Layout")

            onClicked:
                previewPanel.showOnLayoutClicked()
        }
    }

    Rectangle
    {
        x: 0
        y: 0
        width: 1
        height: parent.height
        color: ColorTheme.colors.dark9
    }
}
