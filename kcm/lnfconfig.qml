/********************************************************************
 KSld - the KDE Screenlocker Daemon
 This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
import QtQuick 2.0
import QtQuick.Controls 1.0 as QtControls
import QtQuick.Layouts 1.1

ColumnLayout {
    id: root
    property int formAlignment: 0
    property alias sourceFile: main.sourceFile
    signal configurationChanged

//BEGIN functions
    function saveConfig() {
        if (main.currentItem.saveConfig) {
            main.currentItem.saveConfig()
        }
        for (var key in configDialog.lnfConfiguration) {
            if (main.currentItem["cfg_"+key] !== undefined) {
                configDialog.lnfConfiguration[key] = main.currentItem["cfg_"+key]
            }
        }
    }
//END functions

    Item {
        id: emptyConfig
    }

    QtControls.StackView {
        id: main
        Layout.fillHeight: true
        anchors {
            left: parent.left;
            right: parent.right;
        }

        implicitHeight: currentItem && currentItem.implicitHeight || 0

        property string sourceFile
        onSourceFileChanged: {
            if (sourceFile) {
                var props = {}
                var lnfConfiguration = configDialog.lnfConfiguration
                for (var key in lnfConfiguration) {
                    props["cfg_" + key] = lnfConfiguration[key]
                }

                var newItem = push({
                    item: sourceFile,
                    replace: true,
                    properties: props
                })

                for (var key in lnfConfiguration) {
                    var changedSignal = newItem["cfg_" + key + "Changed"]
                    if (changedSignal) {
                        changedSignal.connect(root.configurationChanged)
                    }
                }
            } else {
                replace(emptyConfig)
            }
        }
    }
}
