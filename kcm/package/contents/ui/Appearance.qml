/********************************************************************
 KSld - the KDE Screenlocker Daemon
 This file is part of the KDE project.

Copyright (C) 2020 David Redondo <kde@david-redondo.de>

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

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15

import org.kde.kcm 1.6 as KCM
import org.kde.kirigami 2.19 as Kirigami

Kirigami.Page {
    // Plugins expect these two properties
    property var wallpaper: kcm.wallpaperIntegration
    property var configDialog: kcm

    title: i18nc("@title", "Appearance")

    ColumnLayout {
        anchors.fill: parent

        LnfConfig {
            sourceFile: kcm.lnfConfigFile
            onConfigurationChanged: kcm.updateState()
        }

        Kirigami.FormLayout {
            id: parentLayout // Don't change needed for correct alignment with lnf and wallpaper config

            QQC2.ComboBox {
                Kirigami.FormData.label: i18n("Wallpaper type:")
                model: kcm.availableWallpaperPlugins()
                textRole: "name"
                valueRole: "id"
                currentIndex: model.findIndex(wallpaper => wallpaper["id"] === kcm.settings.wallpaperPluginId)
                displayText: model[currentIndex]["name"]

                onActivated: {
                    kcm.settings.wallpaperPluginId = model[index]["id"]
                }

                KCM.SettingStateBinding {
                    configObject: kcm.settings
                    settingName: "wallpaperPluginId"
                }
            }
        }

        WallpaperConfig {
            sourceFile: kcm.wallpaperConfigFile
            onConfigurationChanged: kcm.updateState()
        }
    }
}
