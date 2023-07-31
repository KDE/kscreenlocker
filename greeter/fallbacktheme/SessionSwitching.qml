/*
SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>
SPDX-FileCopyrightText: 2023 Nate Graham <nate@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.15
import QtQuick.Layouts 1.15

import org.kde.kirigami 2.20 as Kirigami
import org.kde.plasma.components 3.0 as PlasmaComponents3
import org.kde.plasma.extras 2.0 as PlasmaExtras

Item {
    readonly property bool switchUserSupported: sessionsModel.canSwitchUser

    signal sessionActivated()
    signal newSessionStarted()
    signal switchingCanceled()

    implicitWidth: layoutItem.width + Kirigami.Units.largeSpacing * 2
    implicitHeight: layoutItem.height + Kirigami.Units.largeSpacing * 2

    ColumnLayout {
        id: layoutItem
        anchors.centerIn: parent
        spacing: Kirigami.Units.largeSpacing

        PlasmaComponents3.Label {
            id: explainText
            Layout.fillWidth: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 25
            Layout.bottomMargin: Kirigami.Units.largeSpacing * 2
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
            font: Kirigami.Theme.smallFont
        }

        PlasmaComponents3.Label {
            Layout.fillWidth: true
            text: i18nd("kscreenlocker_greet", "Active sessions:")
        }
        PlasmaComponents3.ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: userSessionsView.count * userSessionsView.delegateHeight

            contentItem: ListView {
                id: userSessionsView
                property int delegateHeight: Kirigami.Units.gridUnit * 2

                model: sessionsModel

                delegate: PlasmaComponents3.Label {
                    readonly property int userVt: model.vtNumber
                    width: userSessionsView.width
                    height: userSessionsView.delegateHeight
                    leftPadding: Kirigami.Units.largeSpacing

                    text: {
                        var display = model.isTty ? i18ndc("kscreenlocker_greet", "User logged in on console", "TTY") : model.displayNumber || ""

                        return i18ndc("kscreenlocker_greet", "username (terminal, display)", "%1 (%2)",
                                    (model.realName || model.name || i18ndc("kscreenlocker_greet", "Nobody logged in", "Unused")),
                                    display ? i18ndc("kscreenlocker_greet", "vt, display", "%1, %2", model.vtNumber, display)
                                            : model.vtNumber
                            )
                    }
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                }

                highlight: PlasmaExtras.Highlight { }
                highlightMoveDuration: 0
                highlightResizeDuration: 0

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

        RowLayout {
            id: buttonRow
            Layout.alignment: Qt.AlignHCenter
            spacing: Kirigami.Units.largeSpacing

            PlasmaComponents3.Button {
                id: activateSession
                text: i18nd("kscreenlocker_greet", "Activate Session")
                icon.name: "system-switch-user"
                onClicked: {
                    sessionsModel.switchUser(userSessionsView.currentItem.userVt)
                    sessionActivated();
                }
            }
            PlasmaComponents3.Button {
                id: newSession
                text: i18nd("kscreenlocker_greet", "Start New Session")
                icon.name: "list-add"
                visible: sessionsModel.canStartNewSession
                onClicked: {
                    sessionsModel.startNewSession()
                    newSessionStarted();
                }
            }
            PlasmaComponents3.Button {
                id: cancelSession
                text: i18nd("kscreenlocker_greet", "Cancel")
                icon.name: "dialog-cancel"
                onClicked: switchingCanceled()
            }
        }
    }
}
