/********************************************************************
 KSld - the KDE Screenlocker Daemon
 This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2019 Kevin Ottens <kevin.ottens@enioka.com>

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
import QtQuick.Controls 2.15 as QtControls
import QtQuick.Layouts 1.1
QtControls.StackView {
    id: main
    Layout.fillHeight: true
    implicitHeight: 490
    Layout.fillWidth: true
    property string sourceFile
    signal configurationChanged
    onSourceFileChanged: {
        if (sourceFile) {
            var props = {}
            var wallpaperConfig = configDialog.wallpaperConfiguration
            for (var key in wallpaperConfig) {
                props["cfg_" + key] = wallpaperConfig[key]
            }

            var newItem = replace(sourceFile, props, QtControls.StackView.ReplaceTransition)

            wallpaperConfig.valueChanged.connect(function(key, value) {
                if (newItem["cfg_" + key] !== undefined) {
                    newItem["cfg_" + key] = value
                }
            })

            var createSignalHandler = function(key) {
                return function() {
                    configDialog.wallpaperConfiguration[key] = newItem["cfg_" + key]
                    configurationChanged()
                }
            }

            for (var key in wallpaperConfig) {
                var changedSignal = newItem["cfg_" + key + "Changed"]
                if (changedSignal) {
                    changedSignal.connect(createSignalHandler(key))
                }
            }
        }
        else {
            replace(empty)
        }
    }
    Component {
        id: empty
        Item {}
    }
}
