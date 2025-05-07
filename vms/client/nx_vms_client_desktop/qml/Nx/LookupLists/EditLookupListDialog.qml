// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml

import Nx.Controls
import Nx.Core
import Nx.Dialogs

import nx.vms.client.desktop
import nx.vms.client.desktop.analytics as Analytics
import nx.vms.client.core.analytics as Analytics

ModalDialog
{
    id: dialog

    required property SystemContext systemContext
    property Analytics.StateView taxonomy : systemContext.taxonomyManager.createStateView(dialog)

    // Source model which is used when editing an exising list.
    property LookupListModel sourceModel
    property bool editMode: !!sourceModel

    // Model working as backend of the dialog.
    property alias viewModel: model

    property bool typeIsSelected: false
    property bool canAccept: model.isValid && typeIsSelected
    property int minimumLabelColumnWidth: 0

    property bool columnWithDataWasRemoved: false
    property bool sourceModelIsChanged: false

    property bool deletionIsAllowed: editMode

    // Attributes of sourceModel, before any edit was applied to LookupListModel.
    // Can't be alias, since sourceModel.attributeNames changes during the editing.
    readonly property var previousAttrbutes: editMode ? sourceModel.attributeNames : []
    readonly property var previousListName: editMode ? sourceModel.name : ""

    signal deleteRequested()

    title: editMode ? qsTr("List Settings") : qsTr("New List")

    function iconPathForObjectType(objectType)
    {
        return Analytics.IconManager.iconUrl(objectType ? objectType.iconSource: "")
    }

    function _addObjectTypesToModelRecursive(result, objectTypes, currentLevel)
    {
        for(let i = 0; i < objectTypes.length; ++i)
        {
            if (objectTypes[i].attributes.length === 0)
                continue

            // Add spaces in beginning of text, to indicate the level in hierarchy.
            result.push(
                {
                    value: objectTypes[i],
                    decorationPath: iconPathForObjectType(objectTypes[i]),
                    text: ("  ").repeat(currentLevel) + objectTypes[i].name
                })
            _addObjectTypesToModelRecursive(result, objectTypes[i].derivedObjectTypes, currentLevel + 1)
        }
    }

    function createListTypeModel()
    {
        if (!taxonomy)
            return;

        let result = [{ value: null, decorationPath: iconPathForObjectType(null), text: qsTr("Generic") }]
        _addObjectTypesToModelRecursive(result, taxonomy.objectTypes, 0)
        return result
    }

    function addAttributesRecursive(objectType, prefix)
    {
        if (!objectType)
        {
            console.log("WARNING: Model integrity failure")
            return;
        }

        for (let i = 0; i < objectType.attributes.length; ++i)
        {
            const attribute = objectType.attributes[i]
            const attributeName = prefix ? prefix + "." + attribute.name : attribute.name
            attributesModel.append({
                text: attributeName,
                checked: model.attributeNames.indexOf(attributeName) >= 0 && editMode
            })
            if (attribute.type === Analytics.Attribute.Type.attributeSet)
            {
                addAttributesRecursive(attribute.attributeSet, attributeName)
            }
        }
    }

    function populateAttributesModel()
    {
        attributesModel.clear()
        attributesSelector.selectionState = AnalyticsObjectAttributesSelector.SelectionState.Partial
        if (model.isGeneric)
            return

        addAttributesRecursive(model.listType, null)

        // Remove from the list attributes which are not available anymore.
        updateSelectedAttributes()
    }

    function updateSelectedAttributes()
    {
        if (model.isGeneric)
            return

        let attributeNames = []
        for (let i = 0; i < attributesModel.count; ++i)
        {
            if (attributesModel.get(i).checked)
                attributeNames.push(attributesModel.get(i).text)
        }

        dialog.columnWithDataWasRemoved = editMode && previousAttrbutes.some((attr) => !attributeNames.includes(attr))
        model.attributeNames = attributeNames
    }

    function areArraysEqual(arr1, arr2): boolean {
        return arr1.length === arr2.length && [...arr1].sort().join() === [...arr2].sort().join();
    }

    LookupListModel
    {
        id: model

        data.id: sourceModel ? sourceModel.data.id : NxGlobals.generateUuid()
        data.entries: sourceModel ? sourceModel.data.entries : []
        // objectTypeId must be initialized before attributes names,
        // since attributeNames changes processing requires correct objectTypeId state.
        objectTypeId: sourceModel ? sourceModel.objectTypeId : ""
        attributeNames: sourceModel ? sourceModel.attributeNames : ["Value"]
        name: sourceModel ? sourceModel.name : qsTr("New List")

        property Analytics.ObjectType listType: taxonomy ? taxonomy.objectTypeById(objectTypeId) : null
        property bool isValid: !!name && (isGeneric ? !!columnName : attributeNames.length)
        property string columnName: (isGeneric && attributeNames.length) ? attributeNames[0] : ""

        function setColumnName(text)
        {
            if (!isGeneric)
            {
                console.log("Cannot set column name for non-generic list")
                return
            }

            if (attributeNames)
                attributeNames[0] = text
            else
                attributeNames.push(text)
        }
    }

    ListModel
    {
        id: attributesModel
    }

    contentItem: ColumnLayout
    {
        GridLayout
        {
            columns: 2

            component AlignedLabel: Label
            {
                Layout.alignment: Qt.AlignBaseline
                Layout.minimumWidth: minimumLabelColumnWidth
                horizontalAlignment: Text.AlignRight
                Component.onCompleted:
                {
                    minimumLabelColumnWidth = Math.max(minimumLabelColumnWidth, implicitWidth)
                }
            }

            AlignedLabel
            {
                text: qsTr("Name")
            }

            TextField
            {
                id: nameField

                Layout.fillWidth: true
                text: model.name
                onTextEdited: model.name = text
                Component.onCompleted:
                {
                    if (!editMode)
                    {
                        forceActiveFocus()
                        selectAll()
                    }
                }
            }

            AlignedLabel
            {
                text: qsTr("Type")
            }

            ComboBox
            {
                id: typeField

                Layout.fillWidth: true
                textRole: "text"
                valueRole: "value"
                enabled: !editMode //< Changing list type while editing is forbidden.
                model: createListTypeModel()
                placeholderText: qsTr("Select type")
                withIconSection: true

                onActivated:
                {
                    viewModel.objectTypeId = currentValue ? currentValue.mainTypeId : ""
                    populateAttributesModel()
                }

                Component.onCompleted:
                {
                    populateAttributesModel()
                    currentIndex = editMode ? indexOfValue(model.listType) : -1
                }

                onCurrentTextChanged:
                {
                    // Remove spaces in beginning of text, used for displaying hierarchy in popup.
                    displayText = currentText.replace(/^\s+/, '')
                }

                onCurrentIndexChanged: dialog.typeIsSelected = currentIndex !== -1
            }

            AlignedLabel
            {
                text: qsTr("Column Name")
                visible: model.isGeneric && dialog.typeIsSelected
            }

            TextField
            {
                id: columnNameField

                Layout.fillWidth: true
                visible: model.isGeneric && dialog.typeIsSelected
                text: model.columnName
                onTextEdited: model.setColumnName(text)
            }

            AlignedLabel
            {
                text: qsTr("Attributes")
                // Ensure the item occupies layout space even when hidden
                opacity: !model.isGeneric && attributesModel.count
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                Layout.topMargin: 14
            }

            AnalyticsObjectAttributesSelector
            {
                id: attributesSelector

                Layout.fillWidth: true
                // Ensure the item occupies layout space even when hidden
                opacity: !model.isGeneric && attributesModel.count
                model: attributesModel
                onSelectionChanged: updateSelectedAttributes()
            }
        }

        DialogBanner
        {
            id: removingAttributeWarning

            Layout.fillWidth: true
            Layout.leftMargin: -16
            Layout.rightMargin: -16
            visible: columnWithDataWasRemoved && !closed
            watchToReopen: columnWithDataWasRemoved
            style: DialogBanner.Style.Warning
            closeable: true
            text: qsTr("Removing attributes will delete all associated data")
        }
    }

    function accept()
    {
        if (canAccept)
        {
            dialog.accepted()
            dialog.close()
        }
    }

    buttonBox: DialogButtonBox
    {
        id: buttonBox
        buttonLayout: DialogButtonBox.KdeLayout
        standardButtons: DialogButtonBox.Cancel

        Button
        {
            text: editMode ? qsTr("OK") : qsTr("Create")
            width: Math.max(buttonBox.standardButton(DialogButtonBox.Cancel).width, implicitWidth)
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            enabled: canAccept
            isAccentButton: true
            onClicked:
            {
                if (!dialog.editMode)
                    return

                dialog.sourceModelIsChanged = viewModel.name !== dialog.previousListName
                    || !areArraysEqual(viewModel.attributeNames, dialog.previousAttrbutes)
            }
        }
    }

    TextButton
    {
        x: 16
        anchors.verticalCenter: buttonBox.verticalCenter
        visible: deletionIsAllowed
        text: qsTr("Delete")
        DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        icon.source: "image://skin/20x20/Outline/delete.svg"

        function confirmDelete(countOfRules)
        {
            const descriptionText = countOfRules > 0
                ? qsTr("This list is associated with %n Event Rules. Are you sure you want to delete it?", "", countOfRules)
                : qsTr("Deleting the list will erase all the data inside it.");

            const result = MessageBox.exec(
                MessageBox.Icon.Question,
                qsTr("Delete List?"),
                descriptionText,
                MessageBox.Cancel,
                {
                    text: qsTr("Delete"),
                    role: MessageBox.AcceptRole
                }
            );

            return result !== MessageBox.Cancel;
        }

        onClicked:
        {
            const countOfRules = dialog.sourceModel.countOfAssociatedVmsRules(dialog.systemContext);

            if (dialog.sourceModel.isEmpty() || confirmDelete(countOfRules))
            {
                dialog.deleteRequested()
                dialog.reject()
            }
        }
    }
}
