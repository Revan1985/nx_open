// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx
import Nx.Core

AbstractButton
{
    id: button

    property alias radius: rectangle.radius

    property color normalBackground: "transparent"
    property color normalForeground: ColorTheme.windowText
    property color hoveredBackground: ColorTheme.transparent(ColorTheme.windowText, 0.1)
    property color hoveredForeground: normalForeground
    property color pressedBackground: ColorTheme.transparent(ColorTheme.colors.dark1, 0.1)
    property color pressedForeground: normalForeground

    property bool useInheritedPalette: false //< Use inherited palette instead of color properties.

    // The most common defaults.
    icon.width: 20
    icon.height: 20

    palette
    {
        button: useInheritedPalette ? undefined : normalBackground
        buttonText: useInheritedPalette ? undefined : normalForeground
        light: useInheritedPalette ? undefined : hoveredBackground
        brightText: useInheritedPalette ? undefined : hoveredForeground
        window: useInheritedPalette ? undefined : pressedBackground
        windowText: useInheritedPalette ? undefined : pressedForeground
    }

    icon.color: down || checked
        ? palette.windowText
        : (hovered ? palette.brightText : palette.buttonText)

    background: Rectangle
    {
        id: rectangle
        color: down || checked ? palette.window : (hovered ? palette.light : palette.button)
    }

    contentItem: IconImage
    {
        id: image

        name: button.icon.name
        color: button.icon.color
        source: button.icon.source
        sourceSize: Qt.size(button.icon.width, button.icon.height)
    }
}
