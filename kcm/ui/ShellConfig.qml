/*
SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
SPDX-FileCopyrightText: 2019 Kevin Ottens <kevin.ottens@enioka.com>

SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15

QQC2.StackView {
    id: main

    signal configurationChanged

    property string sourceFile

    implicitHeight: currentItem && currentItem.implicitHeight || 0
    Layout.fillWidth: true

    onSourceFileChanged: {
        pop()
        if (sourceFile) {
            const props = {}
            const shellConfiguration = configDialog.shellConfiguration
            for (const key in shellConfiguration) {
                props["cfg_" + key] = shellConfiguration[key]
            }

            const newItem = push(sourceFile, props, QQC2.StackView.ReplaceTransition)

            shellConfiguration.valueChanged.connect((key, value) => {
                if (newItem["cfg_" + key] !== undefined) {
                    newItem["cfg_" + key] = value
                }
            })

            const createSignalHandler = key => {
                return () => {
                    configDialog.shellConfiguration[key] = newItem["cfg_" + key]
                    configurationChanged()
                }
            }

            for (const key in shellConfiguration) {
                const changedSignal = newItem["cfg_" + key + "Changed"]
                if (changedSignal) {
                    changedSignal.connect(createSignalHandler(key))
                }
            }
        } else {
            push(empty)
        }
    }

    Component {
        id: empty
        Item {}
    }
}
