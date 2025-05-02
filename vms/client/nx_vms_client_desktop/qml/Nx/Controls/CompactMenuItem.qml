// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls
import Nx.Core

import nx.vms.client.core
import nx.vms.client.desktop

MenuItem
{
    id: menuItem

    property color textColor: highlighted ? ColorTheme.colors.brand_contrast : ColorTheme.colors.light4

    height: 24

    leftPadding: 8
    rightPadding: 8 + ((subMenu && arrow) ? arrow.width : 0)

    contentItem: Text
    {
        id: label

        color: textColor
        opacity: menuItem.enabled ? 1.0 : 0.3
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: FontConfig.normal.pixelSize
        text: menuItem.text
    }
}
