// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Controls
import Nx.Items
import Nx.Dialogs
import Nx.Models
import Nx.Ui

import nx.vms.client.core
import nx.vms.client.mobile

Page
{
    id: organizationScreen
    objectName: "organizationScreen"

    title: subtreeModel.sourceRoot && !searchField.visible
        ? accessor.getData(subtreeModel.sourceRoot, "display")
        : ""
    titleUnderlineVisible: true

    rightButtonIcon.source: searchField.visible
        ? "image://skin/24x24/Outline/close.svg?primary=light10"
        : "image://skin/24x24/Outline/search.svg?primary=light10"
    rightButtonIcon.width: 24
    rightButtonIcon.height: 24

    onLeftButtonClicked: goBack()
    onRightButtonClicked: searchField.visible ? goBack() : startSearch()

    customBackHandler: () => goBack()

    property var model
    property alias rootIndex: subtreeModel.sourceRoot

    property bool searching: !!siteList.searchText

    LinearizationListModel
    {
        id: subtreeModel

        sourceModel: model
        autoExpandAll: searching
        onAutoExpandAllChanged:
        {
            if (!autoExpandAll)
                collapseAll()
        }
    }

    ModelDataAccessor
    {
        id: accessor
        model: organizationScreen.model
    }

    titleControls:
    [
        MouseArea
        {
            width: parent.width
            height: 40
            visible: !searchField.visible && !breadcrumb.visible
            onClicked: breadcrumb.openWith(subtreeModel.sourceRoot)
        },

        SearchEdit
        {
            id: searchField

            visible: false
            width: parent.width
            height: 36
            anchors.verticalCenter: parent.verticalCenter
            placeholderText: qsTr("Search")

            onDisplayTextChanged: siteList.searchText = displayText
        }
    ]

    property var prevRootId: null

    function startSearch()
    {
        if (searchField.visible)
            return

        prevRootId = accessor.getData(subtreeModel.sourceRoot, "nodeId")
        subtreeModel.sourceRoot = currentOrgIndex()
        searchField.visible = true
        searchField.forceActiveFocus()
    }

    function endSearch()
    {
        if (!searchField.visible)
            return

        let index = model.indexFromId(prevRootId)
        if (index.row != -1)
            subtreeModel.sourceRoot = index
        prevRootId = null
        searchField.visible = false
        searchField.clear()
        siteList.positionViewAtBeginning()
    }

    function currentOrgIndex()
    {
        let org = null

        for (let node = rootIndex; node && node.row != -1; node = node.parent)
            org = node

        return org
    }

    function goBack()
    {
        if (searchField.visible)
        {
            endSearch()
            return
        }

        const newIndex = subtreeModel.sourceRoot && subtreeModel.sourceRoot.parent

        if (newIndex.row == -1)
            Workflow.popCurrentScreen()
        else
            subtreeModel.sourceRoot = newIndex
    }

    function goInto(current)
    {
        endSearch()

        if (accessor.getData(current, "type") != OrganizationsModel.System)
        {
            subtreeModel.sourceRoot = current
            return
        }

        organizationScreen.model.systemOpened(current)
    }

    MouseArea
    {
        id: searchCanceller

        z: 1
        anchors.fill: parent

        enabled: searchField.visible
        onPressed: (mouse) =>
        {
            mouse.accepted = false
            searchField.resetFocus()
        }
    }

    Breadcrumb
    {
        id: breadcrumb

        x: siteList.x
        y: siteList.y + 8
        width: siteList.width

        onItemClicked: (nodeId) =>
        {
            goInto(organizationScreen.model.indexFromId(nodeId))
        }

        function openWith(root)
        {
            let data = []

            for (let node = root; node && node.row != -1; node = node.parent)
            {
                data.push({
                    name: accessor.getData(node, "display"),
                    nodeId: accessor.getData(node, "nodeId")
                })
            }

            model = data.reverse()
            open()
        }
    }

    Placeholder
    {
        id: noSitesInOrgPlaceholder

        readonly property bool inOrg: rootIndex && rootIndex.parent.row == -1

        visible: !searching && siteList.count == 0 && !loadingIndicator.visible

        z: 1

        anchors.centerIn: parent
        anchors.verticalCenterOffset: -header.height / 2

        imageSource: "image://skin/64x64/Outline/nosite.svg?primary=light10"
        text: qsTr("No Sites")
        description: inOrg
            ? qsTr("We did not find any sites in this organization")
            : qsTr("We did not find any sites in this folder")
        buttonText: inOrg ? qsTr("How to connect sites?") : ""
        buttonIcon.source: "image://skin/24x24/Outline/cloud.svg?primary=dark1"
        onButtonClicked: { console.log("how to connect sites?"); }
    }

    Placeholder
    {
        id: searchNotFoundPlaceholder

        visible: searching && siteList.count == 0

        anchors.centerIn: parent
        anchors.verticalCenterOffset: -header.height / 2
        imageSource: "image://skin/64x64/Outline/notfound.svg?primary=light10"
        text: qsTr("Nothing found")
        description: qsTr("Try changing the search parameters")
    }

    Skeleton
    {
        id: loadingIndicator

        anchors.fill: parent

        visible: accessor.getData(rootIndex, "isLoading")
        Connections
        {
            target: accessor
            function updateVisibility()
            {
                loadingIndicator.visible = accessor.getData(rootIndex, "isLoading")
            }
            function onDataChanged() { updateVisibility() }
            function onRowsInserted() { updateVisibility() }
            function onRowsRemoved() { updateVisibility() }
        }

        Flow
        {
            anchors.fill: parent
            topPadding: 16
            leftPadding: 20
            spacing: siteList.spacing

            Repeater
            {
                model: 4

                delegate: Rectangle
                {
                    radius: 8
                    color: "white"
                    antialiasing: false
                    width: siteList.cellWidth
                    height: 116
                }
            }
        }
    }

    SiteList
    {
        id: siteList

        anchors.fill: parent

        siteModel: subtreeModel

        onItemClicked: (nodeId) =>
        {
            goInto(organizationScreen.model.indexFromId(nodeId))
        }
    }
}
