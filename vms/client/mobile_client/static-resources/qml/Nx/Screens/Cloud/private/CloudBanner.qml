// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import Nx.Core 1.0

Column
{
    id: column

    property alias text: label.text

    width: parent ? parent.width : 0

    Image
    {
        source: lp("/images/cloud_big.png")
        anchors.horizontalCenter: parent.horizontalCenter
    }

    Text
    {
        id: label
        text: appContext.appInfo.cloudName()
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: ColorTheme.colors.light1
        height: 32
        font.pixelSize: 20
        font.weight: Font.Light
        elide: Text.ElideRight
    }
}
