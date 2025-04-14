// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml

import Nx.Core
import Nx.Core.Controls

import "private"

ColoredImage
{
    property int checkState: Qt.Unchecked
    property bool hovered: false
    property bool pressed: false
    property bool enabled: true

    readonly property ButtonColors colors: ButtonColors { normal: ColorTheme.colors.light10 }
    readonly property ButtonColors checkedColors: ButtonColors { normal: ColorTheme.colors.light4 }

    opacity: enabled ? 1.0 : 0.3
    baselineOffset: 14

    sourceSize: Qt.size(12, 12)
    implicitWidth: 20
    implicitHeight: 20

    primaryColor:
    {
        const baseColors = (checkState === Qt.Unchecked) ? colors : checkedColors
        if (!enabled)
            return baseColors.normal

        return pressed
            ? baseColors.pressed
            : (hovered ? baseColors.hovered : baseColors.normal)
    }

    sourcePath:
    {
        switch (checkState)
        {
            case Qt.Checked:
                return "image://skin/12x12/Outline/checkbox_checked.svg"

            case Qt.Unchecked:
                return "image://skin/12x12/Outline/checkbox_empty.svg"

            case Qt.PartiallyChecked:
                return "image://skin/12x12/Outline/checkbox_half_checked.svg"

            default:
                console.assert(false, `Invalid check state ${checkState}`)
                return null
        }
    }
}
