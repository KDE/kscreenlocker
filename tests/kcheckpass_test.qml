/*
SPDX-FileCopyrightText: 2017 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick 2.1
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.3

ApplicationWindow {
    visible: true

    Component.onCompleted: authenticator.startAuthenticating()



    ColumnLayout {
        anchors.fill: parent
        Label {
            id: message
            Connections {
                target: authenticator
                function onSucceeded() {
                    message.text = "Authentication succeeded";
                }
                function onFailed() {
                    message.text = "Failed"
                }
            }
        }
        TextField {
            id: password
            enabled: true
            echoMode: TextInput.Password
        }
        Button {
            text: "Respond"
            onClicked: {
                console.log("respond")
                authenticator.respond(password.text)
            }
        }
        Button {
            text: "Stop"
            onClicked: {
                console.log("cancel")
                authenticator.stopAuthenticating()
            }
        }
        Item {
            Layout.fillHeight: true
        }
    }
}
