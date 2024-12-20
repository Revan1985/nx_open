// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx.Dialogs 1.0
import Nx.Core 1.0

DialogBase
{
    id: itemSelectionDialog

    property string title: ""
    property string currentItem: ""
    property int currentIndex: -1
    property alias model: repeater.model

    signal itemActivated()

    Column
    {
        width: parent.width

        DialogTitle
        {
            text: itemSelectionDialog.title
        }

        Repeater
        {
            id: repeater

            DialogListItem
            {
                id: dialogItem

                readonly property int row: index
                readonly property string value: model.display || modelData

                active: value == currentItem
                text: value

                onClicked:
                {
                    itemSelectionDialog.currentItem = value
                    itemSelectionDialog.currentIndex = row
                    itemSelectionDialog.itemActivated()
                    itemSelectionDialog.close()
                }
            }
        }
    }
}
