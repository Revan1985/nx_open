// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts
import QtQuick.Window

import Nx.Analytics
import Nx.Controls
import Nx.Core
import Nx.Core.Controls
import Nx.Models
import Nx.RightPanel

import nx.vms.client.core
import nx.vms.client.core.analytics as Analytics
import nx.vms.client.desktop
import nx.vms.client.desktop.analytics

import "metrics.js" as Metrics
import "../../RightPanel/private" as RightPanel

Window
{
    id: dialog

    property WindowContext windowContext: null
    property bool showPreview: false
    property alias tileView: tileViewButton.checked

    property bool showDevControls: eventModel.analyticsSetup
        && eventModel.analyticsSetup.vectorizationSearchEnabled

    modality: Qt.NonModal

    width: Metrics.kDefaultDialogWidth
    height: Metrics.kDefaultDialogHeight
    minimumHeight: Metrics.kMinimumDialogHeight

    minimumWidth: Math.max(Metrics.kMinimumDialogWidth,
        filtersPanel.width + previewPanel.width + Metrics.kSearchResultsWidth + 2)

    onWidthChanged:
    {
        previewPanel.setWidth(previewPanel.width)
    }

    color: ColorTheme.colors.dark2

    title: qsTr("Advanced Object Search")

    ContextHelp.topicId: HelpTopic.ObjectSearch

    onVisibleChanged:
    {
        // Always load new tiles when the dialog is reopened.
        if (dialog.visible)
            counterBlock.commitAvailableNewTracks()
    }

    onTileViewChanged:
    {
        // This has to be done procedurally, in this exact order:
        //  - disable old tab's controller
        //  - switch tabs
        //  - enable new tab's controller

        layout.children[tileView ? 1 : 0].controller.enabled = false
        if (!eventModel.placeholderRequired)
            layout.currentIndex = tileView ? 0 : 1
        layout.children[tileView ? 0 : 1].controller.enabled = true
    }

    signal accepted()

    TabBar
    {
        id: tabBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top

        height: d.pluginTabsShown ? 36 : 0

        color: ColorTheme.colors.dark9
        visible: d.pluginTabsShown

        leftPadding: 16
        rightPadding: 16
        topPadding: 1
        spacing: 16

        function selectEngine(engineId)
        {
            for (let i = 0; i < tabBar.count; ++i)
            {
                if (tabBar.itemAt(i).engineId === engineId)
                {
                    tabBar.currentIndex = i
                    return
                }
            }
        }

        component EngineButton: PrimaryTabButton
        {
            property Analytics.Engine engine: null
            readonly property var engineId: NxGlobals.uuid(engine ? engine.id : "")

            text: engine ? engine.name : qsTr("Any plugin")
            backgroundColors.pressed: ColorTheme.colors.dark8
        }

        EngineButton {}

        Repeater
        {
            model: eventModel.analyticsFilterModel?.engines

            onModelChanged:
            {
                if (eventModel.analyticsFilterModel.engines.length > 0 && eventModel.analyticsSetup)
                    tabBar.selectEngine(eventModel.analyticsSetup.engine)
            }

            EngineButton
            {
                engine: modelData
            }
        }
    }

    Item
    {
        id: content

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.top: tabBar.bottom

        Rectangle
        {
            id: filtersPanel

            width: Metrics.kMinimumFilterColumnWidth
            height: content.height
            color: ColorTheme.colors.dark4

            Scrollable
            {
                id: filtersContainer

                width: parent.width - scrollBar.width - 2
                height: parent.height

                verticalScrollBar: scrollBar

                contentItem: SearchPanelHeader
                {
                    id: header

                    showDevControls: dialog.showDevControls

                    model: eventModel
                    searchDelay: LocalSettings.iniConfigValue("analyticsSearchRequestDelayMs")
                    limitToCurrentCamera: model.crossSiteMode

                    y: 12
                    width: filtersContainer.width
                    height: implicitHeight

                    SelectableTextButton
                    {
                        id: referenceTrackIdButton

                        visible: showDevControls
                            && !eventModel.analyticsSetup.referenceTrackId.isNull()

                        parent: header.filtersColumn
                        Layout.maximumWidth: header.filtersColumn.width

                        selectable: false
                        deactivatable: false
                        accented: true

                        icon.source: "image://skin/20x20/Outline/image.svg"
                        text: "Similar to: " + eventModel.analyticsSetup.referenceTrackId.toString()

                        onClicked:
                            eventModel.analyticsSetup.referenceTrackId = NxGlobals.uuid("")
                    }

                    SelectableTextButton
                    {
                        id: areaSelectionButton

                        visible: eventModel.analyticsSetup
                            && eventModel.analyticsSetup.referenceTrackId.isNull()

                        parent: header.filtersColumn
                        Layout.maximumWidth: header.filtersColumn.width

                        selectable: false
                        icon.source: "image://skin/20x20/Outline/frame.svg"
                        accented: eventModel.analyticsSetup
                            && eventModel.analyticsSetup.areaSelectionActive

                        desiredState:
                            eventModel.analyticsSetup && (eventModel.analyticsSetup.isAreaSelected
                                || eventModel.analyticsSetup.areaSelectionActive)
                                    ? SelectableTextButton.Unselected
                                    : SelectableTextButton.Deactivated

                        text:
                        {
                            if (state === SelectableTextButton.Deactivated)
                                return qsTr("Select area")

                            return accented
                                ? qsTr("Select some area on the video...")
                                : qsTr("In selected area")
                        }

                        onDeactivated:
                        {
                            if (eventModel.analyticsSetup)
                                eventModel.analyticsSetup.clearAreaSelection()
                        }

                        onClicked:
                        {
                            if (eventModel.analyticsSetup)
                               eventModel.analyticsSetup.areaSelectionActive = true
                        }
                    }

                    AnalyticsFilters
                    {
                        id: analyticsFilters

                        visible: eventModel.analyticsSetup
                            && eventModel.analyticsSetup.referenceTrackId.isNull()

                        width: parent.width
                        bottomPadding: 16

                        model: eventModel.analyticsFilterModel

                        onModelChanged:
                        {
                            if (!model)
                                return

                            setSelectedObjectTypeIds(
                                eventModel.analyticsSetup.objectTypes)

                            setSelectedAttributeFilters(
                                eventModel.analyticsSetup.attributeFilters)
                        }
                    }
                }
            }

            ScrollBar
            {
                id: scrollBar

                height: content.height
                anchors.left: filtersContainer.right
                policy: ScrollBar.AlwaysOn
                enabled: size < 1
                opacity: enabled ? 1.0 : 0.1
            }

            Rectangle
            {
                anchors.right: parent.right

                width: 1
                height: parent.height
                color: ColorTheme.colors.dark9
            }
        }

        Resizer
        {
            id: filtersPanelResizer

            z: 1
            edge: Qt.RightEdge
            target: filtersPanel
            handleWidth: Metrics.kResizerWidth
            drag.minimumX: Metrics.kMinimumFilterColumnWidth

            drag.onActiveChanged:
            {
                if (drag.active)
                {
                    drag.maximumX = Math.min(Metrics.kMaximumFilterColumnWidth,
                        filtersPanel.width + resultsPanel.width - Metrics.kMinimumSearchPanelWidth)
                }
            }

            onDragPositionChanged: (pos) =>
                filtersPanel.width = pos
        }

        ColumnLayout
        {
            id: resultsPanel

            anchors.left: filtersPanel.right
            spacing: 0
            width:
            {
                let resultsWidth = content.width - filtersPanel.width - 1

                // Reduce resultsPanel width only when previewPanel is fully visible.
                // Avoid resource-consuming grid resize animation.
                if (eventGrid.count > 0
                    && showPreview
                    && !previewPanel.slideAnimationEnabled)
                {
                    resultsWidth -= previewPanel.width
                }

                return Math.max(resultsWidth, Metrics.kMinimumSearchPanelWidth)
            }

            height: content.height

            RowLayout
            {
                Layout.minimumHeight: Metrics.kTopBarHeight
                Layout.leftMargin: 8
                Layout.rightMargin: 8
                spacing: 8

                CounterBlock
                {
                    id: counterBlock

                    Layout.alignment: Qt.AlignVCenter

                    property int availableNewTracks: eventModel.analyticsSetup
                        ? eventModel.analyticsSetup.availableNewTracks : 0

                    displayedItemsText: eventModel.itemCountText
                    visible: !!displayedItemsText && !eventModel.placeholderRequired

                    availableItemsText:
                    {
                        if (!availableNewTracks)
                            return ""

                        return availableNewTracks > 0
                            ? `+ ${qsTr("%n new results", "", availableNewTracks)}`
                            : `\u2191 ${qsTr("To the top")}`
                    }

                    onCommitNewItemsRequested:
                        commitAvailableNewTracks()

                    onAvailableNewTracksChanged:
                    {
                        // Automatically commit available new tracks once if the results grid is empty.
                        if (!eventGrid.count && availableNewTracks)
                            Qt.callLater(commitAvailableNewTracks)
                    }

                    function commitAvailableNewTracks()
                    {
                        eventGrid.positionViewAtBeginning()
                        if (eventModel.analyticsSetup)
                            eventModel.analyticsSetup.commitAvailableNewTracks()
                        if (eventModel.itemCount > 0)
                            selection.index = eventModel.index(0, 0)
                    }
                }

                TextButton
                {
                    id: settingsButton

                    Layout.alignment: Qt.AlignVCenter
                    text: qsTr("Settings")
                    visible: d.objectTypeSelected
                    icon.source: "image://skin/20x20/Outline/settings.svg"

                    onClicked:
                    {
                        if (tileView)
                            tileFilterSettingsDialog.show()
                        else
                            tableFilterSettingsDialog.show()
                    }
                }

                Item
                {
                    Layout.fillWidth: true
                }

                ImageButton
                {
                    id: tileViewButton

                    Layout.alignment: Qt.AlignVCenter
                    padding: 6
                    radius: 4

                    checkable: true
                    checked: true

                    icon.source: checked
                        ? "image://skin/20x20/Outline/table_view.svg"
                        : "image://skin/20x20/Outline/card_view.svg"
                    borderColor: ColorTheme.colors.dark11

                    GlobalToolTip.text: checked
                        ? qsTr("Switch to table view")
                        : qsTr("Switch to card view")
                }
            }

            Rectangle
            {
                height: 1
                color: ColorTheme.colors.dark6
            }

            StackLayout
            {
                id: layout

                Layout.fillHeight: true

                EventGrid
                {
                    id: eventGrid

                    objectName: "AnalyticsSearchDialog.EventGrid"
                    standardTileInteraction: false
                    keyNavigationEnabled: false //< We implement our own.
                    focus: true
                    controller.paused: !dialog.visible
                    controller.allowToFetchNewer: counterBlock.availableNewTracks <= 0
                    currentIndex: selection.index.row

                    tileController
                    {
                        showInformation: d.objectTypeSelected
                        showThumbnails: true
                        selectedRow: dialog.showPreview ? selection.index.row : -1
                        attributeManager: d.tileViewAttributeManager

                        videoPreviewMode: showPreview
                            ? RightPanelGlobals.VideoPreviewMode.none
                            : RightPanelGlobals.VideoPreviewMode.hover

                        onClicked: (row) =>
                        {
                            if (showOnLayoutButton.hovered)
                            {
                                eventModel.showOnLayout(
                                    eventGrid.tileController.hoveredTile.tileIndex)
                            }
                            else if (!showPreview)
                            {
                                previewPanel.slideAnimationEnabled = true
                                showPreview = true
                            }
                            eventGrid.forceActiveFocus()
                            if (row !== selection.index.row)
                                selection.index = eventModel.index(row, 0)
                        }

                        onDoubleClicked:
                            d.showSelectionOnLayout()
                    }

                    Item
                    {
                        // Single item which is re-parented to the hovered tile overlay.
                        id: tilePreviewOverlay

                        parent: eventGrid.tileController.hoveredTile
                            && eventGrid.tileController.hoveredTile.overlayContainer

                        anchors.fill: parent || undefined
                        visible: parent
                            && eventGrid.tileController.hoveredTile
                            && eventGrid.tileController.hoveredTile.hovered
                            && !showPreview

                        Column
                        {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: 4
                            spacing: 4

                            TileOverlayButton
                            {
                                id: showOnLayoutButton

                                icon.source: "image://skin/20x20/Solid/show_on_layout.svg"
                                accent: true
                            }
                        }
                    }

                    PersistentIndexWatcher
                    {
                        id: selection
                    }

                    Connections
                    {
                        target: selection.index.model

                        function onDataChanged(topLeft, bottomRight)
                        {
                            const selectedRow = selection.index.row
                            if (selectedRow < topLeft.row || selectedRow > bottomRight.row)
                                return

                            previewPanel.update()
                        }
                    }

                    ModelDataAccessor
                    {
                        id: accessor
                        model: eventGrid.model
                    }

                    readonly property real availableWidth: width - leftMargin - rightMargin

                    readonly property int numColumns: Math.floor(
                        availableWidth / (Metrics.kMinimumTileWidth + columnSpacing))

                    readonly property int rowsPerPage:
                        Math.floor((height - topMargin - bottomMargin) / cellHeight)

                    columnWidth: (availableWidth / numColumns) - columnSpacing

                    model: eventModel

                    Shortcut
                    {
                        sequence: "Home"
                        enabled: eventGrid.count > 0 && showPreview

                        onActivated:
                            selection.index = eventModel.index(0, 0)
                    }

                    Shortcut
                    {
                        sequence: "End"
                        enabled: eventGrid.count > 0 && showPreview

                        onActivated:
                            selection.index = eventModel.index(eventModel.rowCount() - 1, 0)
                    }

                    Shortcut
                    {
                        sequence: "Left"
                        enabled: eventGrid.count > 0 && selection.index.valid && showPreview

                        onActivated:
                        {
                            const newRow = Math.max(selection.index.row - 1, 0)
                            selection.index = eventModel.index(newRow, 0)
                        }
                    }

                    Shortcut
                    {
                        sequence: "Right"
                        enabled: eventGrid.count > 0 && selection.index.valid && showPreview

                        onActivated:
                        {
                            const newRow = Math.min(selection.index.row + 1, eventModel.rowCount() - 1)
                            selection.index = eventModel.index(newRow, 0)
                        }
                    }

                    Shortcut
                    {
                        sequence: "Up"
                        enabled: eventGrid.count > 0 && selection.index.valid && showPreview

                        onActivated:
                        {
                            const newRow = selection.index.row -
                                (tileView ? eventGrid.numColumns : 1)
                            if (newRow >= 0)
                                selection.index = eventModel.index(newRow, 0)
                        }
                    }

                    Shortcut
                    {
                        sequence: "Down"
                        enabled: eventGrid.count > 0 && selection.index.valid && showPreview

                        onActivated:
                        {
                            const newRow = selection.index.row +
                                (tileView ? eventGrid.numColumns : 1)
                            if (newRow < eventModel.rowCount())
                                selection.index = eventModel.index(newRow, 0)
                        }
                    }

                    Shortcut
                    {
                        sequence: "PgUp"
                        enabled: eventGrid.count > 0 && selection.index.valid && showPreview

                        onActivated:
                        {
                            const rowsToSkip = tileView
                                ? eventGrid.rowsPerPage * eventGrid.numColumns
                                : tableView.rowsPerPage
                            const newRow = Math.max(0, selection.index.row - rowsToSkip)
                            selection.index = eventModel.index(newRow, 0)
                        }
                    }

                    Shortcut
                    {
                        sequence: "PgDown"
                        enabled: eventGrid.count > 0 && selection.index.valid && showPreview

                        onActivated:
                        {
                            const rowsToSkip = tileView
                                ? eventGrid.rowsPerPage * eventGrid.numColumns
                                : tableView.rowsPerPage
                            const newRow = Math.min(eventModel.rowCount() - 1,
                                selection.index.row + rowsToSkip)
                            selection.index = eventModel.index(newRow, 0)
                        }
                    }

                    Shortcut
                    {
                        sequences: ["Up", "Down", "Left", "Right", "PgUp", "PgDown"]
                        enabled: eventGrid.count > 0 && !selection.index.valid

                        onActivated:
                            selection.index = eventModel.index(0, 0)
                    }
                }

                ScrollView
                {
                    id: scrollView

                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    contentWidth: tableView.width - 8
                    clip: true
                    implicitWidth: contentWidth

                    readonly property alias controller: controller

                    TableView
                    {
                        id: tableView

                        objectName: "AnalyticsSearchDialog.TableView"
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 16
                        horizontalHeaderVisible: true
                        readonly property int cellHeight: 28
                        readonly property int rowsPerPage:
                            Math.floor((height - topMargin - bottomMargin) / cellHeight)

                        clip: true
                        selectionBehavior: TableView.SelectRows
                        headerBackgroundColor: ColorTheme.colors.dark2
                        sortEnabled: false

                        sourceModel: AnalyticsDialogTableModel
                        {
                            id: tableModel
                            sourceModel: eventModel
                            attributeManager: d.tableViewAttributeManager
                            objectTypeIds: analyticsFilters.selectedObjectTypeIds ?? []
                        }

                        columnWidthProvider: function(index)
                        {
                            return tableViewColumnsCalculator.columnWidth(index)
                        }

                        TableViewColumnsCalculator
                        {
                            id: tableViewColumnsCalculator
                        }

                        Connections
                        {
                            target: selection

                            function onIndexChanged()
                            {
                                if (tableView.selectionModel.currentIndex.row === selection.index.row)
                                    return

                                console.assert(tableModel === tableView.sourceModel)
                                const tableModelIndex = tableView.model.mapFromSource(
                                    tableModel.mapFromSource(selection.index))
                                tableView.selectionModel.setCurrentIndex(tableModelIndex,
                                    ItemSelectionModel.Current)

                                if (tableView.currentRow >= 0)
                                {
                                    tableView.positionViewAtRow(tableView.currentRow,
                                        TableView.Contain, -tableView.topMargin)
                                }
                            }
                        }

                        onCurrentRowChanged:
                        {
                            if (currentRow >= 0)
                                selection.index = eventModel.index(tableView.currentRow, 0)
                        }

                        delegate: Rectangle
                        {
                            id: tableDelegate

                            readonly property bool isCurrentRow: tableView.currentRow === row
                            readonly property bool isHoveredRow: tableView.hoveredRow === row
                            readonly property var modelData: model
                            property int cellHeight: tableView.cellHeight

                            function getData(name)
                            {
                                return accessor.getData(eventModel.index(row, 0), name)
                            }

                            readonly property var tooltipData: ({
                                "analyticsEngineName": getData("analyticsEngineName"),
                                "analyticsAttributes": getData("analyticsAttributes")})

                            color: isCurrentRow && showPreview
                                ? ColorTheme.colors.dark9
                                : isHoveredRow ? ColorTheme.colors.dark4 : ColorTheme.colors.dark2
                            implicitWidth: Math.max(
                                contentRowLayout.implicitWidth +
                                contentRowLayout.anchors.leftMargin +
                                contentRowLayout.anchors.rightMargin,
                                28)
                            implicitHeight: Math.max(contentRowLayout.implicitHeight, cellHeight)

                            RowLayout
                            {
                                id: contentRowLayout

                                anchors.fill: parent
                                anchors.leftMargin: 8
                                spacing: 8

                                ColoredImage
                                {
                                    sourceSize: Qt.size(20, 20)
                                    sourcePath: modelData.iconKey
                                        ? "image://skin/" + modelData.iconKey
                                        : ""
                                    primaryColor: isHoveredRow
                                        ? ColorTheme.colors.light7
                                        : ColorTheme.colors.light10
                                    visible: !!sourcePath
                                }

                                Repeater
                                {
                                    model: tableDelegate.modelData.colors

                                    Rectangle
                                    {
                                        width: 16
                                        height: 16
                                        color: modelData ?? "transparent"
                                        border.color: ColorTheme.colors.dark5
                                        border.width: 1
                                        radius: 2
                                    }
                                }

                                Text
                                {
                                    Layout.fillWidth: true

                                    elide: Text.ElideRight
                                    font.pixelSize: 14

                                    color: isHoveredRow
                                        ? ColorTheme.colors.light7
                                        : ColorTheme.colors.light10
                                    text: modelData.display || ""
                                }

                                Item
                                {
                                    id: spacer
                                    Layout.fillWidth: true
                                }

                                ImageButton
                                {
                                    id: previewIcon

                                    rightPadding: 4

                                    icon.source: "image://skin/20x20/Solid/show_on_layout.svg"
                                    visible: isHoveredRow
                                        && column === tableView.model.columnCount() - 1

                                    onClicked:
                                        eventModel.showOnLayout(row)
                                }
                            }

                            MouseArea
                            {
                                anchors.fill: parent
                                enabled: !previewIcon.hovered
                                acceptedButtons: Qt.LeftButton | Qt.RightButton

                                onClicked: (mouse) =>
                                {
                                    if (mouse.button === Qt.RightButton)
                                    {
                                        eventModel.showContextMenu(
                                            row, parent.mapToGlobal(mouse.x, mouse.y), false)
                                        return
                                    }

                                    tableView.selectionModel.setCurrentIndex(
                                        tableView.model.index(row, column),
                                        ItemSelectionModel.Current)

                                    if (!showPreview)
                                    {
                                        previewPanel.slideAnimationEnabled = true
                                        showPreview = true
                                    }
                                }

                                onDoubleClicked: (mouse) =>
                                {
                                    if (mouse.button === Qt.LeftButton)
                                        eventModel.showOnLayout(row)
                                }
                            }
                        }

                        RightPanel.ModelViewController
                        {
                            id: controller

                            view: tableView
                            model: eventModel
                            enabled: false
                            paused: !dialog.visible
                            allowToFetchNewer: counterBlock.availableNewTracks <= 0
                            loggingCategory: LoggingCategory { name: "Nx.RightPanel.TableView" }
                        }
                    }
                }

                Rectangle
                {
                    id: placeholder

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: dialog.color

                    ResultsPlaceholder
                    {
                        id: placeholderItem

                        anchors.fill: parent
                        shown: true

                        icon: "image://skin/64x64/Outline/noobjects.svg?primary=dark17"
                        title: qsTr("No objects")
                        description: qsTr("Try changing the filters or configure object detection "
                            + "in the camera plugin settings")
                    }
                }
            }
        }

        Resizer
        {
            id: previewPanelResizer

            z: 1
            edge: Qt.LeftEdge
            target: previewPanel
            anchors.rightMargin: -width //< Place the resizer insize previewPanel.

            enabled: showPreview

            onDragPositionChanged: (pos) =>
            {
                const requestedWidth =
                    previewPanel.x + previewPanel.width - pos - anchors.rightMargin
                previewPanel.setWidth(requestedWidth)
            }
        }

        PreviewPanel
        {
            id: previewPanel

            showDevControls: dialog.showDevControls

            height: content.height
            width: 460
            x: showPreview ? content.width - width : content.width

            visible: eventGrid.count > 0

            prevEnabled: selection.index.row > 0
            nextEnabled: selection.index.row !== -1 && selection.index.row < accessor.count - 1

            onPrevClicked:
                selection.index = eventModel.index(selection.index.row - 1, 0)

            onNextClicked:
                selection.index = eventModel.index(selection.index.row + 1, 0)

            onFindSimilarClicked: (trackId) =>
                eventModel.analyticsSetup.referenceTrackId = NxGlobals.uuid(trackId)

            onShowOnLayoutClicked:
                d.showSelectionOnLayout()

            onSearchRequested: (attribute) =>
            {
                if (attribute)
                    header.searchText = createSearchRequestText(attribute.id, attribute.values)
            }

            onClose:
            {
                showPreview = false
            }

            property int selectedItemUpdateInstigator: 0

            selectedItem:
            {
                if (selection.index.row === -1 || !showPreview)
                    return null

                selectedItemUpdateInstigator; //< Binding to this property for updating on command.

                const paddingTimeMs = CoreSettings.iniConfigValue("previewPaddingTimeMs")
                const getData = name => accessor.getData(selection.index, name)

                return {
                    "trackId": getData("trackId"),
                    "previewResource": getData("previewResource"),
                    "previewTimestampMs": getData("timestampMs") - paddingTimeMs || 0,
                    "previewDurationMs": getData("durationMs") + paddingTimeMs * 2 || 0,
                    "previewAspectRatio": getData("previewAspectRatio") || 1.0,
                    "timestamp": getData("timestamp") || "",
                    "display": getData("display") || "",
                    "objectTitle": getData("objectTitle") || "",
                    "hasTitleImage": getData("hasTitleImage") || false,
                    "description": getData("description") || "",
                    "additionalText": getData("additionalText") || "",
                    "attributes": getData("analyticsAttributes") || [],
                    "resourceList": getData("resourceList") || []
                }
            }

            function update()
            {
                previewPanel.selectedItemUpdateInstigator++
            }

            function createSearchRequestText(key, values)
            {
                const escape =
                    (str) =>
                    {
                        str = str.replace(/([\"\\\:\$])/g, '\\$1');
                        return str.includes(" ") ? `"${str}"` : str
                    }

                const escapedKeyValuePairs =
                    Array.prototype.map.call(values, v => `${escape(key)}=${escape(v)}`)

                return escapedKeyValuePairs.join(" ")
            }

            function setWidth(requestedWidth)
            {
                const maxWidth = dialog.width - filtersPanel.width - Metrics.kSearchResultsWidth
                previewPanel.width = Math.max(Math.min(requestedWidth, maxWidth), 280)
            }
        }
    }

    NxObject
    {
        id: d

        property bool isModelEmpty: true

        property var analyticsFiltersByEngine: ({})

        property AttributeDisplayManager tileViewAttributeManager:
            windowContext && eventModel.analyticsSetup
                ? AttributeDisplayManagerFactory.create(
                    AttributeDisplayManager.Mode.tileView, eventModel.analyticsFilterModel)
                : null

        property AttributeDisplayManager tableViewAttributeManager:
            windowContext && eventModel.analyticsSetup
                ? AttributeDisplayManagerFactory.create(
                    AttributeDisplayManager.Mode.tableView, eventModel.analyticsFilterModel)
                : null

        readonly property Analytics.Engine selectedAnalyticsEngine:
            tabBar.currentItem ? tabBar.currentItem.engine : null

        property var delayedAttributesFilter: []

        readonly property bool pluginTabsShown:
            eventModel.analyticsFilterModel?.engines.length > 1 ?? false

        readonly property bool objectTypeSelected:
            analyticsFilters.selectedObjectTypeIds?.length > 0 ?? false

        PropertyUpdateFilter on delayedAttributesFilter
        {
            source: analyticsFilters.selectedAttributeFilters
            minimumIntervalMs: LocalSettings.iniConfigValue("analyticsSearchRequestDelayMs")
        }

        function showSelectionOnLayout()
        {
            if (selection.index.valid)
                eventModel.showOnLayout(selection.index.row)
        }

        onSelectedAnalyticsEngineChanged:
        {
            if (!eventModel.analyticsFilterModel || eventModel.analyticsFilterModel.engines.length === 0)
                return

            storeCurrentEngineFilterState()

            if (!eventModel.analyticsSetup)
                return

            eventModel.analyticsSetup.engine = selectedAnalyticsEngine
                ? NxGlobals.uuid(selectedAnalyticsEngine.id)
                : NxGlobals.uuid("")

            restoreEngineFilterState(selectedAnalyticsEngine)
        }

        onDelayedAttributesFilterChanged:
        {
            if (!eventModel.analyticsSetup)
                return

            eventModel.analyticsSetup.attributeFilters = delayedAttributesFilter
        }

        Connections
        {
            target: analyticsFilters
            enabled: eventModel.analyticsSetup

            function onSelectedObjectTypeIdsChanged()
            {
                eventModel.analyticsSetup.objectTypes =
                    analyticsFilters.selectedObjectTypeIds
            }
        }

        function storeCurrentEngineFilterState()
        {
            analyticsFiltersByEngine[analyticsFilters.engine] = {
                objectTypeIds: analyticsFilters.selectedObjectTypeIds,
                attributeFilters: analyticsFilters.selectedAttributeFilters
            }
        }

        function restoreEngineFilterState(engine)
        {
            const savedData = analyticsFiltersByEngine[engine]

            analyticsFilters.setSelected(
                engine,
                savedData ? savedData.objectTypeIds : null,
                savedData ? savedData.attributeFilters : {})
        }

        Connections
        {
            target: eventModel.analyticsSetup

            function onObjectTypesChanged()
            {
                analyticsFilters.setSelectedObjectTypeIds(
                    eventModel.analyticsSetup.objectTypes)
            }

            function onAttributeFiltersChanged()
            {
                analyticsFilters.setSelectedAttributeFilters(
                    eventModel.analyticsSetup.attributeFilters)
            }

            function onEngineChanged()
            {
                const engineId = eventModel.analyticsSetup.engine
                tabBar.selectEngine(engineId)
            }
        }

        Connections
        {
            target: eventModel

            function onItemCountChanged()
            {
                const hasItems = eventModel.itemCount > 0
                if (hasItems && d.isModelEmpty)
                    selection.index = eventModel.index(0, 0)

                d.isModelEmpty = !hasItems
            }
        }
    }

    RightPanelModel
    {
        id: eventModel

        context: windowContext
        type: { return EventSearch.SearchType.analytics }
        active: true

        onAnalyticsSetupChanged:
        {
            const engineId = analyticsSetup.engine
            tabBar.selectEngine(engineId)

            analyticsFilters.setSelectedObjectTypeIds(
                analyticsSetup.objectTypes)

            analyticsFilters.setSelectedAttributeFilters(
                analyticsSetup.attributeFilters)
        }

        onPlaceholderRequiredChanged:
        {
            layout.currentIndex = eventModel.placeholderRequired
                ? 2
                : tileView ? 0 : 1
        }
    }

    FilterSettingsDialog
    {
        id: tileFilterSettingsDialog

        title: qsTr("Tile Settings")
        attributeManager: d.tileViewAttributeManager
        objectTypeIds: analyticsFilters.selectedObjectTypeIds
    }

    FilterSettingsDialog
    {
        id: tableFilterSettingsDialog

        title: qsTr("Table Settings")
        attributeManager: d.tableViewAttributeManager
        objectTypeIds: analyticsFilters.selectedObjectTypeIds
    }

    InformationBubble
    {
        id: informationToolTip

        readonly property Item hoveredItem:
        {
            return dialog.tileView
                ? (eventGrid.hoveredItem?.item ?? null)
                : (tableView.hoveredItem ?? null)
        }

        onHoveredItemChanged:
        {
            if (dialog.tileView)
                open(eventGrid, hoveredItem, hoveredItem?.modelData)
            else
                open(tableView, hoveredItem, hoveredItem?.tooltipData)
        }

        z: 2
    }
}
