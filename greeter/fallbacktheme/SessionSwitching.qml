/*
SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.15

import org.kde.plasma.components 2.0 as PlasmaComponents
import org.kde.plasma.extras 2.0 as PlasmaExtras

Item {
    readonly property bool switchUserSupported: sessionsModel.canSwitchUser

    signal sessionActivated()
    signal newSessionStarted()
    signal switchingCanceled()

    implicitWidth: theme.mSize(theme.defaultFont).width * 55
    implicitHeight: theme.mSize(theme.defaultFont).height * 25

    anchors {
        fill: parent
        margins: 6
    }

    PlasmaExtras.ScrollArea {
        anchors {
            left: parent.left
            right: parent.right
            bottom: buttonRow.top
            bottomMargin: 5
        }
        height: parent.height - explainText.implicitHeight - buttonRow.height - 10

        ListView {
            id: userSessionsView

            model: sessionsModel
            anchors.fill: parent

            delegate: PlasmaComponents.ListItem {
                readonly property int userVt: model.vtNumber

                content: PlasmaComponents.Label {
                    text: {
                        var display = model.isTty ? i18ndc("kscreenlocker_greet", "User logged in on console", "TTY") : model.displayNumber || ""

                        return i18ndc("kscreenlocker_greet", "username (terminal, display)", "%1 (%2)",
                                      (model.realName || model.name || i18ndc("kscreenlocker_greet", "Nobody logged in", "Unused")),
                                      display ? i18ndc("kscreenlocker_greet", "vt, display", "%1, %2", model.vtNumber, display)
                                              : model.vtNumber
                               )
                    }
                }
            }
            highlight: PlasmaComponents.Highlight {
                hover: true
                width: parent.width
            }
            focus: true

            MouseArea {
                anchors.fill: parent
                onClicked: userSessionsView.currentIndex = userSessionsView.indexAt(mouse.x, mouse.y)
                onDoubleClicked: {
                    sessionsModel.switchUser(userSessionsView.indexAt(mouse.x, mouse.y).userVt)
                    sessionActivated();
                }
            }
        }
    }

    PlasmaComponents.Label {
        id: explainText
        text: i18nd("kscreenlocker_greet", "The current session will be hidden " +
                    "and a new login screen or an existing session will be displayed.\n" +
                    "An F-key is assigned to each session; " +
                    "F%1 is usually assigned to the first session, " +
                    "F%2 to the second session and so on. " +
                    "You can switch between sessions by pressing " +
                    "Ctrl, Alt and the appropriate F-key at the same time. " +
                    "Additionally, the KDE Panel and Desktop menus have " +
                    "actions for switching between sessions.",
                    7, 8)
        wrapMode: Text.Wrap
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
    }
    PlasmaComponents.ButtonRow {
        id: buttonRow
        exclusive: false
        spacing: theme.mSize(theme.defaultFont).width / 2
        property bool showAccel: false

        AccelButton {
            id: activateSession
            label: i18nd("kscreenlocker_greet", "Activate")
            iconSource: "fork"
            onClicked: {
                sessionsModel.switchUser(userSessionsView.currentItem.userVt)
                sessionActivated();
            }
        }
        AccelButton {
            id: newSession
            label: i18nd("kscreenlocker_greet", "Start New Session")
            iconSource: "fork"
            visible: sessionsModel.canStartNewSession
            onClicked: {
                sessionsModel.startNewSession()
                newSessionStarted();
            }
        }
        AccelButton {
            id: cancelSession
            label: i18nd("kscreenlocker_greet", "Cancel")
            iconSource: "dialog-cancel"
            onClicked: switchingCanceled()
        }
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: userSessionsUI.horizontalCenter
    }

    Keys.onPressed: {
        const alt = event.modifiers & Qt.AltModifier;
        buttonRow.showAccel = alt;

        if (alt) {
            const buttons = [activateSession, newSession, cancelSession];
            for (let b = 0; b < buttons.length; ++b) {
                if (event.key == buttons[b].accelKey) {
                    buttonRow.showAccel = false;
                    buttons[b].clicked();
                    break;
                }
            }
        }
    }

    Keys.onReleased: {
        buttonRow.showAccel = event.modifiers & Qt.AltModifier;
    }
}
