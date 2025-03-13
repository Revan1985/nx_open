// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core
import Nx.Core.Controls
import Nx.Controls as Nx

Item
{
    id: closedTileItem

    property Item tile: null

    property alias addressText: address.text
    property alias versionText: versionTag.text

    readonly property alias menuHovered: menuButton.hovered
    readonly property int tileNameRightPadding: 32

    function closeMenu()
    {
        menuButton.closeMenu()
    }

    Label
    {
        id: address

        x: 16
        y: 39
        width: parent.width - x - 32
        height: 14

        textFormat: Text.PlainText
        elide: Text.ElideRight
        font.pixelSize: 12
        color: closedTileItem.tile.connectable ? ColorTheme.colors.light16 : ColorTheme.colors.dark14
    }

    MenuButton
    {
        id: menuButton

        anchors.right: parent.right
        anchors.rightMargin: 8
        y: 12

        enabled: !tile.isConnecting
        visible: tile.isFactorySystem
            ? false
            : (closedTileItem.tile.connectable || tile.cloud || !tile.showDeleteSystemButton)

        menuComponent: tileMenuComponent

        onMenuClosed: tile.releaseFocus()

        Component
        {
            id: tileMenuComponent

            TileMenu
            {
                id: tileMenu

                cloud: tile.cloud
                connectable: closedTileItem.tile.connectable

                model: tile.visibilityMenuModel
                loggedIn: tile.loggedIn
                hideActionEnabled: tile.hideActionEnabled

                onEditClicked: tile.expand()
                onDeleteClicked: tile.deleteSystem()
                onTileHidden: tile.forgetAllCredentials()
            }
        }
    }

    Nx.Button
    {
        id: deleteButton

        visible: !tile.connectable && !tile.isFactorySystem
            && !tile.cloud && tile.showDeleteSystemButton

        anchors.right: parent.right
        anchors.rightMargin: 8
        y: 12
        width: 20
        height: 20

        flat: true
        backgroundColor: "transparent"
        pressedColor: ColorTheme.colors.dark6
        hoveredColor: ColorTheme.transparent(ColorTheme.colors.light10, 0.05)

        onClicked: tile.deleteSystem()

        ColoredImage
        {
            anchors.centerIn: parent
            width: 20
            height: 20

            sourcePath: "image://skin/20x20/Outline/delete.svg"
            primaryColor: "light16"
            sourceSize: Qt.size(width, height)
        }
    }

    ColoredImage
    {
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        width: 39
        height: 78

        sourcePath: "image://skin/Illustrations/39x78/settings.svg"
        primaryColor: ColorTheme.colors.dark13
        sourceSize: Qt.size(width, height)

        visible: isFactorySystem
    }

    Row
    {
        x: 16
        y: 70
        width: parent.width - 2 * x

        spacing: 8

        ColoredImage
        {
            width: 22
            height: 16

            sourcePath: closedTileItem.tile.connectable && !context.hasCloudConnectionIssue
                ? "image://skin/22x16/Solid/cloud.svg"
                : "image://skin/22x16/Outline/cloud_offline.svg"
            sourceSize: Qt.size(width, height)
            primaryColor: closedTileItem.tile.connectable && !context.hasCloudConnectionIssue
                ? "brand_core"
                : "dark12"

            visible: tile.cloud
        }

        Nx.Tag
        {
            color: ColorTheme.colors.dark12

            text: qsTr("Offline")

            visible: !tile.online && !tile.isPending
        }

        Nx.Tag
        {
            color: ColorTheme.colors.yellow_core

            text: qsTr("Suspended")

            visible: tile.saasSuspended && tile.online && !tile.incompatible
        }

        Nx.Tag
        {
            color: ColorTheme.colors.red_core

            text: qsTr("Shut Down")

            visible: tile.saasShutDown && tile.online && !tile.incompatible
        }

        Nx.Tag
        {
            color: ColorTheme.colors.dark12

            text: qsTr("Unreachable")

            visible: tile.unreachable && tile.online
        }

        Nx.Tag
        {
            color: ColorTheme.colors.red_core

            text: qsTr("Incompatible")

            visible: tile.incompatible && tile.online
        }

        Nx.Tag
        {
            id: versionTag

            color: ColorTheme.colors.yellow_core

            visible: text && !tile.incompatible && tile.online
        }

        Nx.Tag
        {
            id: anchorTag

            color: ColorTheme.colors.green_attention

            text: qsTr("New")

            visible: tile.isFactorySystem && tile.online
        }

        Nx.Tag
        {
            id: newTag

            color: ColorTheme.colors.dark12

            text: qsTr("Pending")

            visible: tile.isPending
        }

        ColoredImage
        {
            anchors.verticalCenter: anchorTag.verticalCenter
            width: 10
            height: 14

            sourcePath: "image://skin/10x14/Solid/lock.svg"
            sourceSize: Qt.size(width, height)

            primaryColor: "dark12"

            visible: tile.systemRequires2FaEnabledForUser
                || (!tile.cloud && !tile.isFactorySystem && !tile.loggedIn)
        }
    }
}
