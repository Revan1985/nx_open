// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Window

import Nx.Core
import Nx.Controls

import nx.vms.client.core
import nx.vms.client.desktop

import "private"

GridView
{
    id: view

    property bool brief: false //< Only resource list and preview.

    property alias controller: controller
    property alias placeholder: controller.placeholder
    property alias tileController: controller.tileController
    property alias standardTileInteraction: controller.standardTileInteraction

    property real columnWidth: 240
    property real columnSpacing: 2
    property real rowHeight: d.automaticRowHeight
    property real rowSpacing: 2

    property real scrollStepSize: 20

    property var hoveredItem: null

    cellWidth: columnWidth + columnSpacing
    cellHeight: rowHeight + rowSpacing

    topMargin: 14
    leftMargin: 8
    rightMargin: 8 + scrollBar.width - columnSpacing
    bottomMargin: 8 - rowSpacing

    boundsBehavior: Flickable.StopAtBounds
    interactive: false
    clip: true

    function currentCentralPointUs()
    {
        if (!count)
            return d.currentTimeUs()

        // Calculate the currently visible rows
        const firstVisibleRow = Math.floor((contentY + topMargin - originY ) / cellHeight);
        const lastVisibleRow = Math.ceil((contentY + topMargin - originY + height) / cellHeight) - 1;

        // Looks for the avarage time of first items in the rows.
        const columnCount = Math.floor(width / (columnWidth + columnSpacing))
        const firstItem = itemAtIndex(firstVisibleRow * columnCount)
        const lastItem = itemAtIndex(lastVisibleRow * columnCount)

        const items = [firstItem, lastItem]
        let validCount = 0
        let timestampSum = 0
        items.forEach(
            (item) =>
            {
                if (!item || !item.videoPreviewStartTimeMs)
                    return

                timestampSum += item.videoPreviewStartTimeMs
                validCount++
            })

        return validCount
            ? timestampSum / count * 1000
            : d.currentTimeUs()
    }

    ScrollBar.vertical: ScrollBar
    {
        id: scrollBar
        stepSize: view.scrollStepSize / view.contentHeight
    }

    Rectangle
    {
        id: scrollbarPlaceholder

        parent: scrollBar.parent
        anchors.fill: scrollBar
        color: ColorTheme.colors.dark5
        visible: view.visibleArea.heightRatio >= 1.0
    }

    delegate: TileLoader
    {
        id: loader

        width: view.cellWidth - columnSpacing
        height: view.cellHeight - rowSpacing
        tileController: controller.tileController

        Connections
        {
            target: loader.item

            function onHoveredChanged()
            {
                if (target.hovered)
                {
                    view.hoveredItem = loader
                    return
                }

                if (view.hoveredItem === loader)
                    view.hoveredItem = null
            }
        }
    }

    objectName: "EventGrid"

    LoggingCategory
    {
        id: loggingCategory
        name: "Nx.RightPanel.EventGrid"
    }

    ModelViewController
    {
        id: controller

        view: view
        paused: !view?.visible //< Default binding. Replace if invisible view can receive data.
        loggingCategory: loggingCategory
    }

    header: Component { Column { width: parent.width }}
    footer: Component { Column { width: parent.width }}

    MouseArea
    {
        id: mouseArea

        acceptedButtons: Qt.NoButton
        anchors.fill: parent
        hoverEnabled: false
        z: -1

        AdaptiveMouseWheelTransmission { id: gearbox }

        onWheel: (wheel) =>
        {
            if (!view.contentHeight)
                return;

            scrollBar.scrollBy(-gearbox.pixelDelta(wheel, /*pixelsPer15DegreeStep*/ 20)
                / view.contentHeight)
        }
    }

    NxObject
    {
        id: d

        property real automaticRowHeight: 0
        property real maxImplicitRowHeight: 0

        function currentTimeUs()
        {
            return new Date().getTime() * 1000
        }

        function resetAutomaticRowHeight()
        {
            d.maxImplicitRowHeight = 0
            automaticRowHeightAnimation.enabled = false
        }

        Connections
        {
            target: view.Window.window

            function onAfterAnimating()
            {
                const implicitRowHeight = Array.prototype.reduce.call(view.contentItem.children,
                    function(maximumHeight, item)
                    {
                        return item instanceof TileLoader
                            ? Math.max(maximumHeight, item.implicitHeight)
                            : maximumHeight
                    },
                    /*initial value*/ 0)

                const kDefaultRowHeight = 400
                d.maxImplicitRowHeight = Math.max(d.maxImplicitRowHeight, implicitRowHeight)
                d.automaticRowHeight = d.maxImplicitRowHeight || kDefaultRowHeight
                automaticRowHeightAnimation.enabled = d.maxImplicitRowHeight > 0
            }
        }

        Connections
        {
            target: controller.tileController

            function onShowThumbnailsChanged() { d.resetAutomaticRowHeight() }
            function onShowInformationChanged() { d.resetAutomaticRowHeight() }
        }

        Connections
        {
            target: controller.tileController?.attributeManager

            function onVisibleAttributesChanged() { d.resetAutomaticRowHeight() }
        }

        Behavior on automaticRowHeight
        {
            id: automaticRowHeightAnimation

            enabled: false
            animation: NumberAnimation { duration: 250 }
        }
    }

    onCountChanged:
    {
        if (count === 0)
            d.resetAutomaticRowHeight()
    }

    onCellWidthChanged:
        d.resetAutomaticRowHeight()
}
