// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Common
import Nx.Controls
import Nx.Core
import Nx.Core.Controls
import Nx.Items

import nx.vms.api
import nx.vms.client.core
import nx.vms.client.desktop

FocusScope
{
    id: delegateItem

    required property bool showExtraInfo
    required property int itemState //< ResourceTree.ItemState enumeration

    readonly property Resource resource: (model && model.resource) || null
    readonly property int nodeType: (model && model.nodeType) || -1

    readonly property bool isSeparator: nodeType == ResourceTree.NodeType.separator
        || nodeType == ResourceTree.NodeType.localSeparator

    property real availableWidth: width
    property bool isEditing: false
    property bool isDisabledResource: resource && resource.status == API.ResourceStatus.offline
    property string logText: (model && model.logInfo) || ""

    focus: isEditing

    implicitHeight: 20 //< Some sensible default.
    implicitWidth: isSeparator ? 0 : contentRow.implicitWidth

    Row
    {
        id: contentRow

        height: delegateItem.height
        width: parent.width
        spacing: 4

        // Resource name and extra information need to be elided proportionally to their sizes.
        readonly property real availableTextWidth: width - icon.width
            - (name.visible ? spacing : 0) - (extraInfo.visible ? spacing : 0)

        readonly property real actualTextWidth: name.implicitWidth + extraInfo.implicitWidth
        readonly property bool isElideRequired: actualTextWidth > availableTextWidth

        readonly property real nameWidth: isElideRequired
            ? (actualTextWidth > 0 ? availableTextWidth * name.implicitWidth / actualTextWidth : 0)
            : name.implicitWidth

        readonly property real extraInfoWidth: isElideRequired
            ? (actualTextWidth > 0 ? availableTextWidth * extraInfo.implicitWidth / actualTextWidth : 0)
            : extraInfo.implicitWidth

        Image
        {
            id: icon

            anchors.verticalCenter: contentRow.verticalCenter

            source: iconSource
            sourceSize: Qt.size(20, 20)

            width: 20
            height: 20

            Row
            {
                id: extras

                spacing: 0
                height: parent.height
                x: -(width + (NxGlobals.hasChildren(modelIndex) ? 20 : 0))

                readonly property int flags: (model && model.resourceExtraStatus) || 0

                ColoredImage
                {
                    visible: extras.flags & ResourceTree.ResourceExtraStatusFlag.locked
                    sourcePath: "image://skin/20x20/Solid/locked.svg"
                    sourceSize: Qt.size(20, 20)
                    primaryColor: "light4"
                }

                RecordingStatusIndicator
                {
                    id: recordingIcon
                    resource: delegateItem.resource
                    color: (extras.flags & ResourceTree.ResourceExtraStatusFlag.recording
                        || extras.flags & ResourceTree.ResourceExtraStatusFlag.scheduled)
                        ? "red_l"
                        : "dark17"
                }
            }
        }

        Text
        {
            id: name

            text: (model && model.display) || ""
            textFormat: Text.PlainText
            font: FontConfig.resourceTree
            height: parent.height
            verticalAlignment: Text.AlignVCenter
            visible: !delegateItem.isEditing && text.length !== 0
            width: parent.nameWidth
            elide: Text.ElideRight
            color: delegateItem.mainTextColor
        }

        Text
        {
            id: extraInfo

            text: ((delegateItem.showExtraInfo
               || (model && model.nodeType == ResourceTree.NodeType.cloudSystem))
                    && model && model.extraInfo) || ""
            textFormat: Text.PlainText
            font.pixelSize: FontConfig.resourceTree.pixelSize
            font.weight: Font.Normal
            height: parent.height
            width: parent.extraInfoWidth
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            visible: !delegateItem.isEditing && text.length !== 0
            leftPadding: 1

            color:
            {
                switch (itemState)
                {
                    case ResourceTree.ItemState.accented:
                        return ColorTheme.colors.brand_d3
                    case ResourceTree.ItemState.selected:
                        return ColorTheme.colors.light10
                    default:
                        return isSelected ? ColorTheme.colors.light14 : ColorTheme.colors.dark17
                }
            }
        }

        TextInput
        {
            id: nameEditor

            font: FontConfig.resourceTree
            height: parent.height
            width: delegateItem.width - x - 1
            verticalAlignment: Text.AlignVCenter
            selectionColor: selectionHighlightColor
            selectByMouse: true
            selectedTextColor: delegateItem.mainTextColor
            color: delegateItem.mainTextColor
            visible: delegateItem.isEditing
            focus: true
            clip: true

            onVisibleChanged:
            {
                if (!visible)
                    return

                forceActiveFocus()
                selectAll()
            }

            Keys.onPressed: (event) =>
            {
                event.accepted = true
                switch (event.key)
                {
                    case Qt.Key_Enter:
                    case Qt.Key_Return:
                        nameEditor.commit()
                        break

                    case Qt.Key_Escape:
                        nameEditor.revert()
                        break

                    default:
                        event.accepted = false
                        break
                }
            }

            Keys.onShortcutOverride: (event) =>
            {
                switch (event.key)
                {
                    case Qt.Key_Enter:
                    case Qt.Key_Return:
                    case Qt.Key_Escape:
                        event.accepted = true
                        break
                }
            }

            onEditingFinished:
                commit()

            function edit()
            {
                if (!model)
                    return

                text = model.display || ""
                delegateItem.isEditing = true
            }

            function commit()
            {
                if (!delegateItem.isEditing)
                    return

                delegateItem.isEditing = false

                if (!model)
                    return

                const trimmedName = text.trim().replace('\n','')
                if (trimmedName)
                    model.edit = trimmedName
            }

            function revert()
            {
                delegateItem.isEditing = false
            }

            Connections
            {
                target: delegateItem.parent

                function onStartEditing() { nameEditor.edit() }
                function onFinishEditing() { nameEditor.commit() }
            }
        }
    }

    Rectangle
    {
        id: separatorLine

        height: 1
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: -itemIndent

        color: ColorTheme.transparent(ColorTheme.colors.dark8, 0.4)
        visible: delegateItem.isSeparator
    }

    // Never pass key presses to parents while editing.
    Keys.onPressed: (event) =>
        event.accepted = isEditing

    readonly property string iconSource:
    {
        if (!model)
            return ""

        const path = model.decorationPath
        if (path)
            return path.startsWith("file:") ? path : ("qrc:/skin/" + path)

        return (model.iconKey && model.iconKey !== 0
            && ("image://resource/" + model.iconKey + "/" + itemState + "?primary=" + iconColor)) || ""
    }

    property color mainTextColor:
    {
        switch (itemState)
        {
            case ResourceTree.ItemState.accented:
                return ColorTheme.colors.brand_core
            case ResourceTree.ItemState.selected:
                return ColorTheme.colors.light4
            default:
                return delegateItem.isDisabledResource ? ColorTheme.colors.dark17 : ColorTheme.colors.light10
        }
    }

    property string iconColor:
    {
        switch (itemState)
        {
            case ResourceTree.ItemState.accented:
                return "brand_core"
            case ResourceTree.ItemState.selected:
                return "light4"
            default:
                return delegateItem.isDisabledResource ? "dark17" : "light10"
        }
    }
}
