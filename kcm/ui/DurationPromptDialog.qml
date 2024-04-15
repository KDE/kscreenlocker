/*
    SPDX-FileCopyrightText: 2024 Kristen McWilliam <kmcwilliampublic@gmail.com>
    SPDX-FileCopyrightText: 2024 Jakob Petsovits <jpetso@petsovits.com>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

/**
 * A dialog that prompts the user to enter a duration.
 *
 * The dialog contains a spin box for the user to enter a duration. If more than one accepted unit
 * is specified, the dialog allows the user to select the unit of the duration.
 *
 * At least one accepted unit property (e.g. `acceptsSeconds`, `acceptsMinutes`) must be set to `true`.
 * They are all `false` by default.
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
     * @brief Determines whether the dialog accepts milliseconds.
     */
    property bool acceptsMilliseconds: false

    /**
     * @brief Determines whether the dialog accepts seconds.
     */
    property bool acceptsSeconds: false

    /**
     * @brief Determines whether the dialog accepts minutes.
     */
    property bool acceptsMinutes: false

    /**
     * @brief Determines whether the dialog accepts hours.
     */
    property bool acceptsHours: false

    /**
     * @brief Determines whether the dialog accepts days.
     */
    property bool acceptsDays: false

    /**
     * @brief Determines whether the dialog accepts weeks.
     */
    property bool acceptsWeeks: false

    /**
     * @brief Determines whether the dialog accepts months.
     */
    property bool acceptsMonths: false

    /**
     * @brief Determines whether the dialog accepts years.
     */
    property bool acceptsYears: false

    /**
     * @brief A text label in front of the input field.
     */
    property string label

    /**
     * @brief The value of the input field.
     *
     * The value is in seconds if `valueType` is `DurationPromptDialog.ValueType.Seconds`,
     * in minutes if `valueType` is `DurationPromptDialog.ValueType.Minutes`, and so on.
     *
     * Assign before opening the dialog to set the initial value.
     *
     * The value will have been updated when the user submits the dialog.
     */
    property int value

    /**
     * @brief The unit of the value of the input field.
     *
     * Can be either `DurationPromptDialog.ValueType.Minutes` or `DurationPromptDialog.ValueType.Seconds`.
     */
    property int valueType

    /**
     * @brief The possible units of the value of the input field.
     */
    enum ValueType {
        Milliseconds,
        Seconds,
        Minutes,
        Hours,
        Days,
        Weeks,
        Months,
        Years
    }

    /**
     * @brief An array of all accepted value types.
     */
    readonly property var acceptedUnits: {
        let units = [];
        if (acceptsMilliseconds) {
            units.push(DurationPromptDialog.ValueType.Milliseconds);
        }
        if (acceptsSeconds) {
            units.push(DurationPromptDialog.ValueType.Seconds);
        }
        if (acceptsMinutes) {
            units.push(DurationPromptDialog.ValueType.Minutes);
        }
        if (acceptsHours) {
            units.push(DurationPromptDialog.ValueType.Hours);
        }
        if (acceptsDays) {
            units.push(DurationPromptDialog.ValueType.Days);
        }
        if (acceptsWeeks) {
            units.push(DurationPromptDialog.ValueType.Weeks);
        }
        if (acceptsMonths) {
            units.push(DurationPromptDialog.ValueType.Months);
        }
        if (acceptsYears) {
            units.push(DurationPromptDialog.ValueType.Years);
        }
        return units;
    }

    padding: Kirigami.Units.largeSpacing
    standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel

    implicitWidth: Math.max(contentItem.implicitWidth + leftPadding + rightPadding, root.implicitHeaderWidth, root.implicitFooterWidth)

    contentItem: RowLayout {
        spacing: 0

        Item {
            Layout.fillWidth: true
        }

        RowLayout {
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                text: label
            }

            QQC2.SpinBox {
                id: customDurationInput
                from: 0
                to: 9999
                onValueModified: root.value = value;

                Connections {
                    target: root
                    function onVisibleChanged() {
                        if (root.visible) {
                            customDurationInput.value = root.value;
                        }
                    }
                }
            }

            function unitSuffixForValue(val, unit) {
                switch (unit) {
                case DurationPromptDialog.ValueType.Milliseconds:
                    return i18ncp("The unit of the time input field", "millisecond", "milliseconds", val);
                case DurationPromptDialog.ValueType.Seconds:
                    return i18ncp("The unit of the time input field", "second", "seconds", val);
                case DurationPromptDialog.ValueType.Minutes:
                    return i18ncp("The unit of the time input field", "minute", "minutes", val);
                case DurationPromptDialog.ValueType.Hours:
                    return i18ncp("The unit of the time input field", "hour", "hours", val);
                case DurationPromptDialog.ValueType.Days:
                    return i18ncp("The unit of the time input field", "day", "days", val);
                case DurationPromptDialog.ValueType.Weeks:
                    return i18ncp("The unit of the time input field", "week", "weeks", val);
                case DurationPromptDialog.ValueType.Months:
                    return i18ncp("The unit of the time input field", "month", "months", val);
                case DurationPromptDialog.ValueType.Years:
                    return i18ncp("The unit of the time input field", "year", "years", val);
                }
            }

            QQC2.Label {
                visible: acceptedUnits.length == 1
                text: parent.unitSuffixForValue(value, acceptedUnits[0])

                // Try not to shrink. The +1 is there because I couldn't figure out why actual
                // plural metrics were 49 but implicitWidth for the plural string 49.78125.
                // An extra pixel shouldn't hurt either way, and hopefully metrics are close enough.
                Layout.preferredWidth: Math.max(implicitWidth, pluralUnitLabelMetrics.width + leftPadding + rightPadding + 1)
                TextMetrics {
                    id: pluralUnitLabelMetrics
                    text: {
                        const radioIndex = acceptedUnits[0] - DurationPromptDialog.ValueType.Milliseconds;
                        return unitSelection.children[radioIndex].text;
                    }
                }
            }

            ColumnLayout {
                id: unitSelection
                spacing: Kirigami.Units.smallSpacing
                visible: acceptedUnits.length > 1

                QQC2.RadioButton {
                    visible: acceptsMilliseconds
                    text: i18nc("@text:radiobutton Unit of the time input field", "milliseconds")
                    checked: valueType === DurationPromptDialog.ValueType.Milliseconds
                    onClicked: valueType = DurationPromptDialog.ValueType.Milliseconds
                }

                QQC2.RadioButton {
                    visible: acceptsSeconds
                    text: i18nc("@text:radiobutton Unit of the time input field", "seconds")
                    checked: valueType === DurationPromptDialog.ValueType.Seconds
                    onClicked: valueType = DurationPromptDialog.ValueType.Seconds
                }

                QQC2.RadioButton {
                    visible: acceptsMinutes
                    text: i18nc("@text:radiobutton Unit of the time input field", "minutes")
                    checked: valueType === DurationPromptDialog.ValueType.Minutes
                    onClicked: valueType = DurationPromptDialog.ValueType.Minutes
                }

                QQC2.RadioButton {
                    visible: acceptsHours
                    text: i18nc("@text:radiobutton Unit of the time input field", "hours")
                    checked: valueType === DurationPromptDialog.ValueType.Hours
                    onClicked: valueType = DurationPromptDialog.ValueType.Hours
                }

                QQC2.RadioButton {
                    text: i18nc("@text:radiobutton Unit of the time input field", "days")
                    visible: acceptsDays
                    checked: valueType === DurationPromptDialog.ValueType.Days
                    onClicked: valueType = DurationPromptDialog.ValueType.Days
                }

                QQC2.RadioButton {
                    text: i18nc("@text:radiobutton Unit of the time input field", "weeks")
                    visible: acceptsWeeks
                    checked: valueType === DurationPromptDialog.ValueType.Weeks
                    onClicked: valueType = DurationPromptDialog.ValueType.Weeks
                }

                QQC2.RadioButton {
                    text: i18nc("@text:radiobutton Unit of the time input field", "months")
                    visible: acceptsMonths
                    checked: valueType === DurationPromptDialog.ValueType.Months
                    onClicked: valueType = DurationPromptDialog.ValueType.Months
                }

                QQC2.RadioButton {
                    text: i18nc("@text:radiobutton Unit of the time input field", "years")
                    visible: acceptsDays
                    checked: valueType === DurationPromptDialog.ValueType.Years
                    onClicked: valueType = DurationPromptDialog.ValueType.Years
                }
            }
        }

        Item {
            Layout.fillWidth: true
        }
    }
}
