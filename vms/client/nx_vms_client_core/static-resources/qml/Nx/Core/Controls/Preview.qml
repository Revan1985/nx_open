// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Core.Items 1.0
import Nx.Core.Controls 1.0
import Nx.Items 1.0

import nx.vms.client.core 1.0

Control
{
    id: preview

    property string previewId
    property alias previewSource: image.source
    property int previewState: { return EventSearch.PreviewState.initial }
    property real previewAspectRatio: 1
    property rect highlightRect: Qt.rect(0, 0, 0, 0)
    property bool showHighlightBorder: false

    property color backgroundColor: ColorTheme.colors.dark7
    property color borderColor: ColorTheme.colors.dark10
    property color highlightColor: ColorTheme.colors.brand_core
    property color foregroundColor: ColorTheme.colors.light16

    // Preview item will not have aspect ratio lesser than this value.
    // Actual preview image will be fit into these bounds keeping its own aspect ratio.
    property real minimumAspectRatio: 16.0 / 9.0
    property bool videoPreviewEnabled: true
    property bool videoPreviewForced: false //< Always play video preview, not only on hover.
    property alias videoPreviewResource: intervalPreview.resource
    property alias videoPreviewStartTimeMs: intervalPreview.startTimeMs
    property alias videoPreviewDurationMs: intervalPreview.durationMs
    property alias snippedPreview: intervalPreview.snippedPreview

    padding: backgroundRect.border.width

    background: Rectangle
    {
        id: backgroundRect

        border.color: preview.borderColor
        color: preview.backgroundColor
    }

    contentItem: Item
    {
        implicitHeight: width / Math.max(previewAspectRatio, minimumAspectRatio)

        Image
        {
            id: image

            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            source: previewId ? `image://previews/${previewId}` : ""
            visible: !!source.toString()

            RectangularMask
            {
                id: mask

                anchors.centerIn: image
                color: ColorTheme.transparent(ColorTheme.colors.dark7, 0.3)

                width: (image.paintedWidth + 1) || image.width
                height: (image.paintedHeight + 1) || image.height

                rectangle: preview.highlightRect
                visible: mask.rectangle.width && mask.rectangle.height

                Rectangle
                {
                    color: "transparent"
                    border.color: preview.highlightColor

                    x: preview.highlightRect.x * mask.width
                    y: preview.highlightRect.y * mask.height
                    width: preview.highlightRect.width * mask.width
                    height: preview.highlightRect.height * mask.height

                    visible: showHighlightBorder
                }
            }

            Watermark
            {
                anchors.fill: parent
                resource: intervalPreview.resource
                sourceSize: image.sourceSize
            }

            IntervalPreview
            {
                id: intervalPreview

                delayMs: CoreSettings.iniConfigValue("intervalPreviewDelayMs")
                loopDelayMs: CoreSettings.iniConfigValue("intervalPreviewLoopDelayMs")
                speedFactor: CoreSettings.iniConfigValue("intervalPreviewSpeedFactor")

                anchors.fill: parent
                aspectRatio: preview.previewAspectRatio

                active: preview.videoPreviewEnabled
                    && (preview.hovered || preview.videoPreviewForced)
            }
        }

        Text
        {
            id: noData

            anchors.fill: parent
            verticalAlignment: Qt.AlignVCenter
            horizontalAlignment: Qt.AlignHCenter
            visible: previewState == EventSearch.PreviewState.missing
            color: preview.foregroundColor
            text: qsTr("NO DATA")
        }

        NxDotPreloader
        {
            id: preloader

            anchors.centerIn: parent
            color: preview.foregroundColor

            running: previewState === EventSearch.PreviewState.busy
                || previewState === EventSearch.PreviewState.initial
        }
    }
}
