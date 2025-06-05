/*
 *  SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com
 *
 *  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.ksvg as KSvg
import org.kde.plasma.components as PlasmaComponents

Item {
    id: root

    // Kwin::Effect
    required property QtObject effect

    // TODO: Invalid alias reference. Unable to find id "effect"
    //property alias zoomFactor: effect.zoomFactor
    //property alias targetZoom: effect.targetZoom

    // Ideally we want a gridUnit around the background, but we must account
    // for the shadow we draw providing some of that desired margin internally
    readonly property real margins: Math.max(Kirigami.Units.gridUnit - (background.x - shadow.x), 0)

    implicitWidth: shadow.width
    implicitHeight: shadow.height

    // Manage opacity for the overlay, matching the behaviour of the zoom effect
    layer.enabled: true
    opacity: (effect.targetZoom === 1.0) ? 0 : 1
    Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.Linear } }

    KSvg.FrameSvgItem {
        id: shadow
        anchors.fill: background
        anchors.topMargin: -margins.top
        anchors.leftMargin: -margins.left
        anchors.rightMargin: -margins.right
        anchors.bottomMargin: -margins.bottom

        imagePath: "widgets/tooltip"
        prefix: "shadow"
    }

    KSvg.FrameSvgItem {
        id: background
        anchors.fill: container
        anchors.topMargin: -margins.top
        anchors.leftMargin: -margins.left
        anchors.rightMargin: -margins.right
        anchors.bottomMargin: -margins.bottom

        imagePath: "widgets/tooltip"
    }

    Item {
        id: container

        anchors.centerIn: parent

        width: content.implicitWidth + (content.anchors.margins * 2)
        height: content.implicitHeight + (content.anchors.margins * 2)

        ColumnLayout {
            id: content
            anchors.centerIn: parent
            anchors.margins: Kirigami.Units.largeSpacing

            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                id: zoomIcon
                implicitHeight: Kirigami.Units.iconSizes.medium
                implicitWidth: Kirigami.Units.iconSizes.medium

                property real previousTargetZoom: 1.0
                property bool isZoomingIn: true

                Connections {
                    target: root.effect
                    function onTargetZoomChanged() {
                        zoomIcon.isZoomingIn = root.effect.targetZoom > zoomIcon.previousTargetZoom;
                        zoomIcon.previousTargetZoom = root.effect.targetZoom;
                    }
                }

                source: {
                    if (root.effect.targetZoom >= 15.0) { // TODO: 'pixelGridZoom'
                        return "zoom-pixels";
                    } else if (zoomIcon.isZoomingIn) {
                        return "zoom-in-symbolic";
                    } else {
                        return "zoom-out-symbolic"
                    }
                }
            }

            PlasmaComponents.Label {
                text: "Hello, world!"
            }
        }
    }
}
