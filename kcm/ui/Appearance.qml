/*
SPDX-FileCopyrightText: 2020 David Redondo <kde@david-redondo.de>

SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15

import org.kde.kcmutils as KCM
import org.kde.kirigami 2.20 as Kirigami

Kirigami.Page {
    // The following two must be set for correct alignment with wallpaper config
    id: appearanceRoot
    property alias parentLayout: parentLayout
    // Plugins expect these two properties
    property var wallpaper: kcm.wallpaperIntegration
    property var configDialog: kcm

    title: i18nc("@title", "Appearance")

    padding: 6  // Layout_ChildMarginWidth from Breeze

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.smallSpacing

        ShellConfig {
            sourceFile: kcm.shellConfigFile
            onConfigurationChanged: kcm.updateState()
        }

        Kirigami.FormLayout {
            id: parentLayout // Don't change needed for correct alignment with shell and wallpaper config

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
            onConfigurationForceChanged: kcm.forceUpdateState()
            // Cancel out page margins so it touches the edges
            Layout.margins: -appearanceRoot.padding
        }
    }
}
