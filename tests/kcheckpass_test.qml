/********************************************************************
 KSld - the KDE Screenlocker Daemon
 This file is part of the KDE project.

Copyright (C) 2017 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
import QtQuick 2.1
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.3

ApplicationWindow {
    visible: true
    ColumnLayout {
        anchors.fill: parent
        Label {
            id: message
            Connections {
                target: authenticator
                onFailed: {
                    message.text = "Authentication failed";
                }
                onSucceeded: {
                    message.text = "Authentication succeeded";
                }
                onGraceLockedChanged: {
                    message.text = ""
                }
            }
        }
        TextField {
            id: password
            enabled: !authenticator.graceLocked
            echoMode: TextInput.Password
        }
        Button {
            text: "Authenticate"
            enabled: !authenticator.graceLocked
            onClicked: {
                authenticator.tryUnlock(password.text)
            }
        }
    }
}
