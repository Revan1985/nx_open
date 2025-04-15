// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls

import "private"
import "private/utils.js" as Utils
import "../settings.js" as Settings

/**
 * Interactive Settings type.
 */
Item
{
    id: control

    property string name: ""
    property string addButtonCaption: ""
    property string deleteButtonCaption: ""

    readonly property Item childrenItem: column

    readonly property real buttonSpacing: 24

    property bool fillWidth: true

    implicitHeight: column.implicitHeight + addButton.implicitHeight + buttonSpacing

    property int visibleItemsCount: 0

    property bool filled: false

    function updateFilled()
    {
        const lastFilled = lastFilledItemIndex()
        visibleItemsCount = Math.max(1, lastFilled + 1)
        filled = (lastFilled >= 0)
    }

    LabeledColumnLayout
    {
        id: column
        width: parent.width
        layoutControl: control
    }

    Row
    {
        x: column.labelWidth + 8
        y: column.y + column.height + buttonSpacing
        spacing: 12

        Button
        {
            id: addButton

            text: addButtonCaption || qsTr("Add")
            visible: visibleItemsCount < column.layoutItems.length
            icon.source: "image://skin/20x20/Outline/add.svg"

            onClicked:
            {
                if (visibleItemsCount < column.layoutItems.length)
                    ++visibleItemsCount
            }
        }

        Button
        {
            id: removeButton

            text: deleteButtonCaption || qsTr("Delete")
            visible: visibleItemsCount > 1
            icon.source: "image://skin/20x20/Outline/minus.svg"

            onClicked:
            {
                if (!visibleItemsCount)
                    return

                --visibleItemsCount
                let itemToReset = column.layoutItems[visibleItemsCount]
                Settings.resetValues(itemToReset)
            }
        }
    }

    onVisibleItemsCountChanged:
    {
        updateItemsVisibility()
    }

    function lastFilledItemIndex()
    {
        for (let i = column.layoutItems.length - 1; i >= 0; --i)
        {
            if (Utils.isItemFilled(column.layoutItems[i]))
                return i
        }
        return -1
    }

    function updateItemsVisibility()
    {
        for (let i = 0; i < column.layoutItems.length; ++i)
            column.layoutItems[i].opacity = i < visibleItemsCount ? 1.0 : 0.0
    }
}
