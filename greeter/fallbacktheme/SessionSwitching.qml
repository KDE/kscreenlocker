/*
    SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2023 Nate Graham <nate@kde.org>
    SPDX-FileCopyrightText: 2023 ivan tkachenko <me@ratijas.tk>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.extras as PlasmaExtras
import org.kde.plasma.private.sessions

Item {
    id: root

    required property SessionsModel sessionsModel

    signal sessionActivated()
    signal newSessionStarted()
    signal switchingCanceled()

    implicitWidth: layoutItem.implicitWidth + Kirigami.Units.largeSpacing * 2
    implicitHeight: layoutItem.implicitHeight + Kirigami.Units.largeSpacing * 2

    QQC2.StackView.onActivated: {
        const target = (root.sessionsModel.count > 0) ? userSessionsView : newSession;
        target.forceActiveFocus(Qt.TabFocusReason);
    }

    ColumnLayout {
        id: layoutItem

        anchors.centerIn: parent
        spacing: Kirigami.Units.largeSpacing

        PlasmaComponents3.Label {
            id: explainText

            Layout.fillWidth: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 25
            Layout.bottomMargin: Kirigami.Units.largeSpacing * 2
            text: i18nd("kscreenlocker_greet",
                "The current session will be hidden " +
                "and a new login screen or an existing session will be displayed.\n" +
                "An F-key is assigned to each session; " +
                "F%1 is usually assigned to the first session, " +
                "F%2 to the second session and so on. " +
                "You can switch between sessions by pressing " +
                "Ctrl, Alt and the appropriate F-key at the same time. " +
                "Additionally, the KDE Panel and Desktop menus have " +
                "actions for switching between sessions.",
                7, 8,
            )
            wrapMode: Text.Wrap
            font: Kirigami.Theme.smallFont
        }

        PlasmaComponents3.Label {
            Layout.fillWidth: true
            text: i18nd("kscreenlocker_greet", "Active sessions:")
            visible: root.sessionsModel.count > 0
        }

        PlasmaComponents3.ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: root.sessionsModel.count * userSessionsView.delegateHeight

            visible: root.sessionsModel.count > 0

            contentItem: ListView {
                id: userSessionsView

                property int delegateHeight: Kirigami.Units.gridUnit * 2

                model: root.sessionsModel

                delegate: PlasmaComponents3.Label {
                    required property int index
                    required property var model

                    function activate() {
                        root.sessionsModel.switchUser(model.vtNumber);
                        root.sessionActivated();
                    }

                    width: ListView.view.width
                    height: userSessionsView.delegateHeight

                    leftPadding: Kirigami.Units.largeSpacing
                    rightPadding: Kirigami.Units.largeSpacing

                    text: {
                        const username = model.realName || model.name || i18ndc("kscreenlocker_greet", "Nobody logged in", "Unused");
                        const display = model.isTty ? i18ndc("kscreenlocker_greet", "User logged in on console", "TTY") : (model.displayNumber || "");
                        const where = display
                            ? i18ndc("kscreenlocker_greet", "vt, display", "%1, %2", model.vtNumber, display)
                            : model.vtNumber;

                        return i18ndc("kscreenlocker_greet", "username (terminal, display)", "%1 (%2)", username, where);
                    }
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideMiddle

                    KeyNavigation.tab: activateSession
                    KeyNavigation.backtab: cancelSession
                    KeyNavigation.down: index === root.sessionsModel.count - 1 ? KeyNavigation.tab : null

                    Keys.onSpacePressed: activate()
                    Keys.onEnterPressed: activate()
                    Keys.onReturnPressed: activate()
                }

                highlight: PlasmaExtras.Highlight { }
                highlightMoveDuration: 0
                highlightResizeDuration: 0

                keyNavigationEnabled: true
                keyNavigationWraps: false

                MouseArea {
                    anchors.fill: parent
                    onClicked: mouse => {
                        const index = userSessionsView.indexAt(mouse.x, mouse.y);
                        if (index !== -1) {
                            userSessionsView.currentIndex = index;
                        }
                    }
                    onDoubleClicked: mouse => {
                        const delegate = userSessionsView.itemAt(mouse.x, mouse.y);
                        if (delegate) {
                            delegate.activate();
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Kirigami.Units.largeSpacing

            KeyNavigation.up: userSessionsView

            PlasmaComponents3.Button {
                id: activateSession
                text: i18nd("kscreenlocker_greet", "Activate Session")
                icon.name: "system-switch-user"
                enabled: userSessionsView.currentItem !== null

                // Override implicit reverse binding from delegates
                KeyNavigation.up: userSessionsView

                onClicked: {
                    const delegate = userSessionsView.currentItem;
                    if (delegate) {
                        delegate.activate();
                    }
                }
            }
            PlasmaComponents3.Button {
                id: newSession
                text: i18nd("kscreenlocker_greet", "Start New Session")
                icon.name: "list-add"
                visible: root.sessionsModel.canStartNewSession

                // Setting only on this button will auto-set reverse navigation for the other two buttons
                KeyNavigation.right: mirrored ? activateSession : cancelSession
                KeyNavigation.left: mirrored ? cancelSession : activateSession

                onClicked: {
                    root.sessionsModel.startNewSession();
                    root.newSessionStarted();
                }
            }
            PlasmaComponents3.Button {
                id: cancelSession
                text: i18nd("kscreenlocker_greet", "Cancel")
                icon.name: "dialog-cancel"
                onClicked: {
                    root.switchingCanceled();
                }
            }
        }
    }

    TapHandler {
        acceptedButtons: Qt.BackButton
        onTapped: {
            root.switchingCanceled();
        }
    }

    Shortcut {
        enabled: root.visible && root.enabled
        sequences: [StandardKey.Cancel, StandardKey.Back]
        onActivated: {
            root.switchingCanceled();
        }
    }
}
