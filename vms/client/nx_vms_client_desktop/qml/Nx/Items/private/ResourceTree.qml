// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx.Core 1.0
import Nx.Common 1.0
import Nx.Controls 1.0
import Nx.Layout 1.0
import Nx.Models 1.0
import Nx.Utils 1.0

import nx.vms.api
import nx.vms.client.core 1.0
import nx.vms.client.desktop 1.0

TreeView
{
    id: resourceTree

    readonly property real kRowHeight: 24
    readonly property real kSeparatorRowHeight: 16

    property var scene: null

    property alias filterText: resourceTreeModel.filterText
    property alias filterType: resourceTreeModel.filterType
    property alias isFiltering: resourceTreeModel.isFiltering
    property alias localFilesMode: resourceTreeModel.localFilesMode

    property alias toolTipVideoDelayMs: toolTip.videoDelayMs //< Disabled if zero or negative.
    property alias toolTipVideoQuality: toolTip.videoQuality

    property rect toolTipEnclosingRect: Qt.rect(width, 0, Number.MAX_VALUE / 2, height)

    property bool shortcutHintsEnabled: true

    readonly property bool isEditing: currentItem ? currentItem.isEditing : false

    property var stateStorage: null

    function isFilterRelevant(type)
    {
        return model.isFilterRelevant(type)
    }

    topMargin: 8
    leftMargin: rootIsDecorated ? -2 : 2
    indentation: 20
    scrollStepSize: kRowHeight
    hoverHighlightColor: ColorTheme.transparent(ColorTheme.colors.dark8, 0.4)
    selectionHighlightColor: ColorTheme.transparent(ColorTheme.colors.brand_core, 0.3)
    dropHighlightColor: ColorTheme.transparent(ColorTheme.colors.brand_core, 0.6)
    rootIndex: resourceTreeModel.rootIndex
    autoExpandRoleName: "autoExpand"
    selectAllSiblingsRoleName: "nodeType"

    expandsOnDoubleClick: (modelIndex => resourceTreeModel.expandsOnDoubleClick(modelIndex))
    activateOnSingleClick: (modelIndex => resourceTreeModel.activateOnSingleClick(modelIndex))

    rootIsDecorated:
    {
        return filterType == ResourceTree.FilterType.noFilter
            || filterType == ResourceTree.FilterType.everything
            || filterType == ResourceTree.FilterType.cameras
            || filterType == ResourceTree.FilterType.localFiles
    }

    onContextMenuRequested: (globalPos, modelIndex, selection) =>
        resourceTreeModel.showContextMenu(globalPos, modelIndex, selection)

    onActivated: (modelIndex, selection, activationType, modifiers) =>
        resourceTreeModel.activateItem(modelIndex, selection, activationType, modifiers)

    model: ResourceTreeModel
    {
        id: resourceTreeModel

        context: windowContext

        onEditRequested:
            resourceTree.startEditing()

        onFilterAboutToBeChanged:
        {
            if (!isFiltering)
                pushState(resourceTree.expandedState())
        }

        onFilterChanged:
        {
            if (isFiltering)
                resourceTree.expandAll()
            else
                resourceTree.setExpandedState(popState())
        }

        function acquireExpandedState(expandedIndexes, prevState)
        {
            const state = {
                // Maintain expanded servers state when they are hidden.
                // See releaseExpandedState() below.
                "expandedResources": prevState ? prevState.expandedResources : new Set(),
                "expandedGroups": new Set(),
                "expandedServers": new Set(),
                "expandedCameraGroups": new Set(),
                "expandedTopLevelRows": new Set(),
                "topLevelRows": []
            }

            const kTopLevelTypes = [
                ResourceTree.NodeType.servers,
                ResourceTree.NodeType.camerasAndDevices]

            for (let index of expandedIndexes)
            {
                // Only select indexes under top level "Servers" or "Cameras & Devices".
                let topLevel = index
                while (topLevel.parent != resourceTreeModel.rootIndex)
                    topLevel = topLevel.parent

                const nodeType = Number(modelDataAccessor.getData(topLevel, "nodeType"))
                if (!kTopLevelTypes.includes(nodeType))
                    continue

                // "Servers" and "Cameras & Devices" share the same row number.
                state.topLevelRows[0] = topLevel.row
                if (index.parent == resourceTreeModel.rootIndex)
                    state.expandedTopLevelRows.add(index.row)

                const customId = modelDataAccessor.getData(index, "customGroupId")
                if (customId)
                {
                    state.expandedGroups.add(customId)
                    continue
                }

                const cameraGroupId = modelDataAccessor.getData(index, "cameraGroupId")
                if (cameraGroupId)
                {
                    state.expandedCameraGroups.add(cameraGroupId)
                    continue
                }

                const resource = modelDataAccessor.getData(index, "resource")
                if (resource)
                {
                    state.expandedResources.add(resource.id)

                    if (resource.flags & NxGlobals.ServerResourceFlag)
                        state.expandedServers.add(resource.id)
                }
            }

            return state
        }

        function expandedStateCallback(state)
        {
            return (index) =>
                {
                    if (index.parent == resourceTreeModel.rootIndex)
                        return state.expandedTopLevelRows.has(index.row)

                    const customId = modelDataAccessor.getData(index, "customGroupId")
                    if (state.expandedGroups.has(customId))
                        return true

                    const cameraGroupId = modelDataAccessor.getData(index, "cameraGroupId")
                    if (state.expandedCameraGroups.has(cameraGroupId))
                        return true

                    const resource = modelDataAccessor.getData(index, "resource")
                    if (resource && state.expandedResources.has(resource.id))
                        return true

                    return false
                }
        }

        function releaseExpandedState(prevState)
        {
            // Maintain expanded servers state when they are hidden.
            // Servers state is going to be restored because expandedResources
            // is carried on from the previous state.
            // Remember that we toggle only between two states:
            //   Servers
            //   Cameras & Devices
            return {
                "expandedResources": prevState.expandedServers
            }
        }

        onSaveExpandedState:
        {
            // Convert selectedIndexes to QVariantList to avoid huge performance loss.
            const expandedIndexes = NxGlobals.toQVariantList(resourceTree.expandedState())
            stateStorage = acquireExpandedState(expandedIndexes, stateStorage)
        }

        onRestoreExpandedState:
        {
            resourceTree.expand(expandedStateCallback(stateStorage), stateStorage.topLevelRows)
            stateStorage = releaseExpandedState(stateStorage)
        }
    }

    delegate: Component
    {
        ResourceTreeDelegate
        {
            id: delegateItem

            readonly property alias thumbnailSource: thumbnail
            readonly property bool isSelected: currentIndex === modelIndex

            showExtraInfo: resourceTreeModel.extraInfoRequired
                || resourceTreeModel.isExtraInfoForced(resource)
                || (!!model && !!model.forceExtraInfo)

            implicitHeight: isSeparator ? kSeparatorRowHeight : kRowHeight

            function defaultState()
            {
                return isSelected ? ResourceTree.ItemState.selected : ResourceTree.ItemState.normal
            }

            itemState:
            {
                if (!resourceTree.scene || !model)
                    return defaultState()

                switch (Number(model.nodeType))
                {
                    case ResourceTree.NodeType.currentSystem:
                    case ResourceTree.NodeType.currentUser:
                        return ResourceTree.ItemState.selected

                    case ResourceTree.NodeType.layoutItem:
                    {
                        const itemLayout = modelDataAccessor.getData(
                            resourceTreeModel.parent(modelIndex), "resource")

                        if (!itemLayout || scene.currentLayout !== itemLayout)
                            return defaultState()

                        return scene.currentResource && scene.currentResource === model.resource
                            ? ResourceTree.ItemState.accented
                            : ResourceTree.ItemState.selected
                    }

                    case ResourceTree.NodeType.resource:
                    case ResourceTree.NodeType.sharedLayout:
                    case ResourceTree.NodeType.edge:
                    case ResourceTree.NodeType.sharedResource:
                    {
                        const resource = model.resource
                        if (!resource)
                            return defaultState()

                        if (scene.reviewedVideoWall === resource
                            || scene.controlledVideoWall === resource)
                        {
                            return ResourceTree.ItemState.selected
                        }

                        if (scene.currentResource === resource)
                            return ResourceTree.ItemState.accented

                        if (scene.currentLayout === resource
                            || scene.resources.containsResource(resource))
                        {
                            return ResourceTree.ItemState.selected
                        }

                        return defaultState()
                    }

                    case ResourceTree.NodeType.recorder:
                    {
                        const childCount = resourceTreeModel.rowCount(modelIndex)
                        let hasSelectedChildren = false

                        for (let i = 0; i < childCount; ++i)
                        {
                            const childResource = modelDataAccessor.getData(
                                resourceTreeModel.index(i, 0, modelIndex), "resource")

                            if (!childResource)
                                continue

                            if (scene.currentResource === childResource)
                                return ResourceTree.ItemState.accented

                            hasSelectedChildren |= scene.resources.containsResource(childResource)
                        }

                        return hasSelectedChildren || isSelected
                            ? ResourceTree.ItemState.selected
                            : ResourceTree.ItemState.normal
                    }

                    case ResourceTree.NodeType.videoWallItem:
                    {
                        if (!scene.controlledVideoWallItemId.isNull())
                        {
                            return scene.controlledVideoWallItemId == model.uuid || isSelected
                                ? ResourceTree.ItemState.selected
                                : ResourceTree.ItemState.normal
                        }

                        const videoWall = modelDataAccessor.getData(modelIndex.parent, "resource")
                        return videoWall && videoWall === scene.reviewedVideoWall || isSelected
                            ? ResourceTree.ItemState.selected
                            : ResourceTree.ItemState.normal
                    }

                    case ResourceTree.NodeType.layoutTour:
                    {
                        return scene.reviewedShowreelId == model.uuid || isSelected
                            ? ResourceTree.ItemState.selected
                            : ResourceTree.ItemState.normal
                    }
                }

                return defaultState()
            }

            isDisabledResource:
            {
                if (delegateItem.resource)
                    return delegateItem.resource.status === API.ResourceStatus.offline;

                if (delegateItem.nodeType === ResourceTree.NodeType.recorder)
                {
                    const childCount = resourceTreeModel.rowCount(modelIndex);
                    if (!childCount)
                        return false

                    for (let i = 0; i < childCount; ++i)
                    {
                        const childResource = modelDataAccessor.getData(
                            resourceTreeModel.index(i, 0, modelIndex), "resource");

                        if (!childResource)
                            continue;

                        if (childResource.status === API.ResourceStatus.online
                            || childResource.status === API.ResourceStatus.recording)
                        {
                            return false; //< At least one child resource is online.
                        }
                    }

                    return true; //< All child resources offline or uninitialized.
                }

                return !enabled //< There is no resource to check status.
            }

            ResourceIdentificationThumbnail
            {
                id: thumbnail

                resource: delegateItem.resource
                maximumSize: 400
                obsolescenceMinutes: 15
                enforced: delegateItem === resourceTree.hoveredItem

                refreshIntervalSeconds: isArmServer
                    ? LocalSettings.iniConfigValue("resourcePreviewRefreshIntervalArm")
                    : LocalSettings.iniConfigValue("resourcePreviewRefreshInterval")

                preloadDelayMs: 1000 * (isArmServer
                    ? LocalSettings.iniConfigValue("resourcePreviewPreloadDelayArm")
                    : LocalSettings.iniConfigValue("resourcePreviewPreloadDelay"))

                requestScatter: LocalSettings.iniConfigValue("resourcePreviewRequestScatter")
            }
        }
    }

    footer: Component
    {
        Column
        {
            id: shortcutHintColumn

            width: parent.width
            topPadding: 8
            bottomPadding: 8
            leftPadding: 2
            spacing: 8
            visible: resourceTree.shortcutHintsEnabled

            Repeater
            {
                model: resourceTreeModel.shortcutHints

                ShortcutHint
                {
                    keys: modelData.keys
                    description: modelData.description
                }
            }
        }
    }

    ModelDataAccessor
    {
        id: modelDataAccessor
        model: resourceTreeModel
    }

    ResourceToolTip
    {
        id: toolTip

        Connections
        {
            target: resourceTree

            function onActivated() { toolTip.suppress() }
            function onSelectionChanged() { toolTip.suppress() }
            function onIsEditingChanged() { toolTip.suppress() }
            function onContentYChanged() { toolTip.suppress(/*immediately*/ true) }
        }

        readonly property var hoverData: {
            "item": resourceTree.hoveredItem,
            "modelIndex": resourceTree.hoveredIndex}

        onHoverDataChanged:
            updateToolTip()

        function updateToolTip()
        {
            if (!hoverData.item || hoverData.item.isEditing || hoverData.item.isSeparator)
            {
                hide()
                return
            }

            target = hoverData.item
            thumbnailSource = hoverData.item.thumbnailSource

            enclosingRect = hoverData.item.mapFromItem(resourceTree,
                resourceTree.toolTipEnclosingRect)

            toolTip.text = toolTipText(hoverData.modelIndex)

            if (state !== BubbleToolTip.Suppressed)
                show()
        }

        function toolTipText(modelIndex)
        {
            const customGroupId = modelDataAccessor.getData(modelIndex, "customGroupId")
            if (customGroupId)
            {
                const namePos = customGroupId.lastIndexOf("\n") + 1
                if (namePos <= 0)
                    return NxGlobals.toHtmlEscaped(customGroupId)

                const name = NxGlobals.toHtmlEscaped(customGroupId.substring(namePos))
                const path = NxGlobals.toHtmlEscaped(
                    customGroupId.substring(0, namePos).replace(/\n/g, "/"))

                return `<span style="color: ${ColorTheme.colors.light10};">${path}</span>${name}`
            }

            const text = modelDataAccessor.getHtmlEscapedData(modelIndex, "display")
            if (!text)
                return ""

            const extra = modelDataAccessor.getData(modelIndex, "extraInfo")

            return extra
                ? `<b>${text}</b> ${extra}`
                : text
        }
    }
}
