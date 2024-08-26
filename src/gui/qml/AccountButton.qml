/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ToolButton {
    id: control

    readonly property real goldenRatio: 1.618
    readonly property real widthHint: height * goldenRatio

    property string altText: ""
    property int gradient: Gradient.AfricanField
    property color solidColor: null
    property color solidColorChecked: null

    clip: true
    icon.height: 32
    icon.width: 32
    implicitWidth: Math.min(implicitContentWidth + leftPadding + rightPadding, widthHint)

    // make the current button pop
    // don't get highlight from our own palette else we create a binding loop
    palette.button: parent.palette.highlight

    Component {
        id: imageComponent

        Image {
            fillMode: Image.PreserveAspectFit
            source: control.icon.source
            sourceSize.height: control.icon.height
            sourceSize.width: control.icon.width
            cache: control.icon.cache
        }
    }

    Component {
        id: placeholderComponent

        Rectangle {
            radius: 180
            gradient: control.solidColor.valid ? undefined : control.gradient
            color: control.solidColor.valid ? (control.checked ? control.solidColorChecked : control.solidColor) : null
            Label {
                anchors.fill: parent
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: control.altText
                font.bold: true
                elide: Text.ElideMiddle
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: control.spacing

        Loader {
            id: loader
            Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
            Layout.preferredHeight: control.icon.height
            Layout.preferredWidth: control.icon.width
            sourceComponent: control.icon.source.toString() === "" ? placeholderComponent : imageComponent
        }

        Label {
            Layout.fillHeight: true
            Layout.fillWidth: true
            color: control.visualFocus ? control.palette.highlight : control.palette.buttonText
            // elide middle would look better but doesn't work with wrapping
            elide: Text.ElideRight
            font: control.font
            horizontalAlignment: Text.AlignHCenter
            maximumLineCount: 2
            opacity: enabled ? 1.0 : 0.3
            text: control.text
            verticalAlignment: Text.AlignTop
            wrapMode: Text.WordWrap
        }
    }
}
