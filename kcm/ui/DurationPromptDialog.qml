/*
    SPDX-FileCopyrightText: 2024 Kristen McWilliam <kmcwilliampublic@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

/**
 * A dialog that prompts the user to enter a duration.
 *
 * The dialog contains a spin box for the user to enter a duration, and a radio button to select the unit of the duration.
 *
 * The dialog emits the `accepted` signal when the user clicks the confirm button, and the `rejected` signal when the user clicks the cancel button.
 *
 * The dialog also has a `value` property that contains the value entered by the user.
 *
 * The dialog also has a `valueType` property that contains the unit of the value entered by the user.
 */
Kirigami.Dialog {
    id: root

    /**
     * @brief Determines whether the dialog accepts minutes.
     *
     * Defaults to `true`.
     */
    property bool acceptsMinutes: true

    /**
     * @brief Determines whether the dialog accepts seconds.
     *
     * Defaults to `false`.
     */
    property bool acceptsSeconds: false

    /**
     * @brief Text displayed above the input field.
     */
    property string subtitle

    /**
     * @brief The value of the input field.
     *
     * The value is in minutes if `valueType` is `DurationPromptDialog.ValueType.Minutes`, 
     * and in seconds if `valueType` is `DurationPromptDialog.ValueType.Seconds`.
     *
     * Assign before opening the dialog to set the initial value.
     *
     * The value is updated when the user submits the dialog.
     */
    property int value

    /**
     * @brief The unit of the value of the input field.
     *
     * Can be either `DurationPromptDialog.ValueType.Minutes` or `DurationPromptDialog.ValueType.Seconds`.
     */
    property var valueType

    /**
     * @brief The possible units of the value of the input field.
     */
    enum ValueType {
        Minutes,
        Seconds
    }

    padding: Kirigami.Units.largeSpacing
    standardButtons: Kirigami.Dialog.NoButton

    customFooterActions: [
        Kirigami.Action {
            text: i18nc("@action:button", "Confirm")
            icon.name: "dialog-ok-symbolic"
            onTriggered: {
                value = customDurationInput.value;
                accept();
            }
        },
        Kirigami.Action {
            text: i18nc("@action:button", "Cancel")
            icon.name: "dialog-cancel-symbolic"
            onTriggered: {
                reject();
            }
        }
    ]

    contentItem: 
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                text: subtitle
            }

            QQC2.SpinBox {
                id: customDurationInput
                from: 0
                to: 9999

                Binding on value {
                    when: root.visible
                    value: root.value
                }
            }

            QQC2.Label {
                visible: acceptsMinutes && !acceptsSeconds
                text: i18ncp("The unit of the time input field", "minute", "minutes", customDurationInput.value)
            }

            QQC2.Label {
                visible: acceptsSeconds && !acceptsMinutes
                text: i18ncp("The unit of the time input field", "second", "seconds", customDurationInput.value)
            }

            ColumnLayout {
                spacing: Kirigami.Units.smallSpacing
                visible: acceptsMinutes && acceptsSeconds

                QQC2.RadioButton {
                    id: minutesRadioButton
                    text: i18nc("The unit of the time input field", "minutes")
                    checked: valueType === DurationPromptDialog.ValueType.Minutes
                    onClicked: valueType = DurationPromptDialog.ValueType.Minutes
                }

                QQC2.RadioButton {
                    id: secondsRadioButton
                    text: i18nc("The unit of the time input field", "seconds")
                    checked: valueType === DurationPromptDialog.ValueType.Seconds
                    onClicked: valueType = DurationPromptDialog.ValueType.Seconds
                }
            }
        }
}
