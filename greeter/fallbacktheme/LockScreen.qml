/*
    SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2023 Nate Graham <nate@kde.org>
    SPDX-FileCopyrightText: 2023 ivan tkachenko <me@ratijas.tk>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2

import org.kde.kirigami as Kirigami
import org.kde.plasma.core as PlasmaCore
import org.kde.kquickcontrolsaddons
import org.kde.ksvg as KSvg
import org.kde.plasma.private.sessions

Item {
    id: root

    // Note: This property is set from C++
    property bool locked: false

    readonly property bool capsLockOn: keystateSourceLoader.item?.data?.["Caps Lock"]?.["Locked"] ?? false

    Loader {
        id: keystateSourceLoader
        source: Qt.resolvedUrl("./KeyState.qml")
    }

    // if there's no image, have a pure black background
    Rectangle {
        z: -1
        width: parent.width
        height: parent.height
        color: "black"
    }

    SessionsModel {
        id: sessionsModel
    }

    Image {
        anchors.fill: parent
        source: "file:" + PlasmaCore.Theme.wallpaperPathForSize(parent.width, parent.height)
        smooth: true
    }

    KSvg.FrameSvgItem {
        id: dialog

        visible: root.locked
        anchors.centerIn: parent
        width: (mainStack.currentItem?.implicitWidth ?? 0) + margins.left + margins.right
        height: (mainStack.currentItem?.implicitHeight ?? 0) + margins.top + margins.bottom
        imagePath: "widgets/background"

        Behavior on height {
            enabled: mainStack.currentItem !== null
            NumberAnimation {
                duration: Kirigami.Units.longDuration
            }
        }
        Behavior on width {
            enabled: mainStack.currentItem !== null
            NumberAnimation {
                duration: Kirigami.Units.longDuration
            }
        }

        QQC2.StackView {
            id: mainStack

            clip: true
            anchors {
                fill: parent
                leftMargin: dialog.margins.left
                topMargin: dialog.margins.top
                rightMargin: dialog.margins.right
                bottomMargin: dialog.margins.bottom
            }
            initialItem: unlockUI
        }
    }

    Greeter {
        id: unlockUI

        switchUserEnabled: sessionsModel.canSwitchUser
        capsLockOn: root.capsLockOn

        visible: opacity > 0
        opacity: mainStack.currentItem === unlockUI ? 1 : 0

        Behavior on opacity {
            NumberAnimation {
                duration: Kirigami.Units.longDuration
            }
        }

        onSwitchUserClicked: {
            mainStack.push(userSessionsUIComponent, { sessionsModel });
            mainStack.currentItem.forceActiveFocus();
        }
    }

    function returnToLogin() {
        mainStack.pop();
        unlockUI.resetFocus();
    }

    Component {
        id: userSessionsUIComponent

        SessionSwitching {
            id: userSessionsUI

            visible: false

            onSwitchingCanceled: {
                root.returnToLogin();
            }
            onSessionActivated: {
                root.returnToLogin();
            }
            onNewSessionStarted: {
                root.returnToLogin();
            }
        }
    }
}
