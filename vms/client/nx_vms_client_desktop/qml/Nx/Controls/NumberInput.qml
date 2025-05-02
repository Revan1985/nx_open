// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Controls
import Nx.Core
import Nx.Core.Controls

FocusScope
{
    id: control

    property var minimum: undefined
    property var maximum: undefined

    signal editingFinished()

    readonly property alias value: textField.value

    property alias editorCursorPosition: textField.cursorPosition

    function valueFromText(text)
    {
        return text ? Number(text) : undefined
    }

    function textFromValue(value)
    {
        if (isNaN(value))
            return ""

        return control.mode === NumberInput.Integer
            ? Math.trunc(value).toString()
            : Number(value).toString()
    }

    function setValue(text)
    {
        const parsedValue = valueFromText(text)

        if (textField.value === parsedValue)
            return

        if (parsedValue === undefined)
        {
            // 'undefined' is valid value, returned on empty text.
            textField.value = parsedValue
            return
        }

        if (isNaN(parsedValue))
            return //< NaN is returned during casting of incorrect string to number. (for example symbol '-').

        // Value is invalid, when it is outside [min,max] range.
        const isInvalidValue = textField.validator && (textField.validator.validate(parsedValue) !== Qt.Acceptable)
        textField.value = isInvalidValue
            ? MathUtils.bound(textField.validator.bottom, parsedValue, textField.validator.top)
            : parsedValue
    }

    function clear()
    {
        setValue(undefined)
    }

    function isValidated()
    {
        return textField.value === control.value
    }

    property alias color: textField.color
    property alias placeholderText: textField.placeholderText //< Displayed when value is undefined.
    property alias placeholderTextColor: textField.placeholderTextColor
    property alias prefix: prefixText.text
    property alias background: textField.background
    property alias cursorPosition: textField.cursorPosition
    property alias contextMenuOpening: textField.contextMenuOpening

    enum Mode
    {
        Integer,
        Real
    }

    property int mode: NumberInput.Integer

    implicitWidth: 200
    implicitHeight: textField.implicitHeight
    baselineOffset: textField.baselineOffset

    TextField
    {
        id: textField

        focus: true

        width: control.width

        topPadding: 0
        bottomPadding: 0
        verticalAlignment: Qt.AlignVCenter

        leftPadding: LayoutMirroring.enabled
            ? (clearButton.width + 4)
            : (prefix ? (prefixText.width + 16) : 8)

        rightPadding: LayoutMirroring.enabled
            ? (prefix ? (prefixText.width + 16) : 8)
            : (clearButton.width + 4)

        property var value: undefined

        Text
        {
            id: prefixText

            anchors.left: textField.left
            anchors.leftMargin: 8

            height: textField.height
            verticalAlignment: Qt.AlignVCenter

            font: textField.font
            color: ColorTheme.colors.light16
        }

        AbstractButton
        {
            id: clearButton

            width: height
            height: textField.height - 4
            padding: 0

            anchors.right: textField.right
            anchors.rightMargin: 2
            anchors.verticalCenter: textField.verticalCenter

            focusPolicy: Qt.NoFocus

            opacity: textField.text ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: 100 }}

            visible: opacity > 0

            contentItem: ColoredImage
            {
                sourcePath: "image://skin/20x20/Outline/cross_close.svg"
                sourceSize: Qt.size(20, 20)
                primaryColor: textField.color
                fillMode: Image.Pad
            }

            background: Rectangle
            {
                radius: 1
                color: clearButton.down
                    ? ColorTheme.colors.dark5
                    : (clearButton.hovered ? ColorTheme.colors.dark6 : "transparent")
            }

            onClicked:
                textField.clear()
        }

        onTextChanged:
        {
            if (acceptableInput || !text) //< Empty text is an undefined value.
                setValue(text)
        }

        onEditingFinished: control.editingFinished()

        onValueChanged:
        {
            if (valueFromText(text) !== value)
                text = textFromValue(value)
        }

        onActiveFocusChanged:
        {
            if (!activeFocus)
            {
                value = control.value
                control.editingFinished()
            }
        }

        Keys.onEnterPressed:
        {
            value = control.value
            control.editingFinished()
        }

        Keys.onReturnPressed:
        {
            value = control.value
            control.editingFinished()
        }

        function updateValidator()
        {
            if (validator)
                validator.destroy()

            if (control.mode === NumberInput.Integer)
            {
                validator = Qt.createQmlObject("import nx.vms.client.core 1.0; IntValidator {}",
                    textField)
            }
            else
            {
                validator = Qt.createQmlObject("import nx.vms.client.core 1.0; DoubleValidator {}",
                    textField)

                validator.notation = DoubleValidator.StandardNotation
                validator.decimals = 5
            }

            validator.locale = NxGlobals.numericInputLocale()

            validator.bottom = Qt.binding(() => CoreUtils.getValue(control.minimum, validator.lowest))

            validator.top = Qt.binding(() => CoreUtils.getValue(control.maximum, validator.highest))
        }
    }

    Component.onCompleted: textField.updateValidator()
    onModeChanged: textField.updateValidator()

    onActiveFocusChanged:
    {
        if (textField.activeFocus !== activeFocus)
            textField.focus = activeFocus
        if (!activeFocus)
            editingFinished()
    }
}
