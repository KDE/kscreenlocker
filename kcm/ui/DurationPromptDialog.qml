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
 * The dialog contains a spin box for the user to enter a duration, and its corresponding time unit.
 *
 * The dialog emits the `accepted` signal when the user clicks the confirm button, and the `rejected` signal when the user clicks the cancel button.
 *
 * After the dialog has been accepted, the `value` property will contain the duration value entered
 * by the user. The `unit` property will contain the time unit of `value`.
 */
Kirigami.Dialog {
    id: root

    /**
     * @brief The possible time units of the value of the input field.
     */
    enum Unit {
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
     * @brief An array of time units that can be associated with the duration value.
     *
     * All elements must be `DurationPromptDialog.Unit` enum values, with no duplicates.
     *
     * At least one element is required. If more than one accepted unit is specified, the dialog
     * allows the user to select the unit of the duration.
     */
    required property var acceptsUnits

    /**
     * @brief A text label in front of the input field.
     */
    property string label

    /**
     * @brief The value of the input field.
     *
     * The value is in seconds if `unit` is `DurationPromptDialog.ValueType.Seconds`,
     * in minutes if `valueType` is `DurationPromptDialog.ValueType.Minutes`, and so on.
     *
     * Assign before opening the dialog to set the initial value.
     *
     * The value will have been updated when the user accepts the dialog.
     */
    property int value

    /**
     * @brief The unit of the value of the input field, of type `DurationPromptDialog.Unit`.
     *
     * This must be one of the elements from the specified `acceptsUnits` property.
     * The first element of that array is used as default unit.
     */
    property int unit: acceptsUnits[0]

    padding: Kirigami.Units.largeSpacing
    standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel

    // FIXME: KF 6.3 and up doesn't need content.implicitWidth as part of preferredWidth, it will
    //        automatically expand to implicit content size. Remove this once we can rely on KF 6.3.
    preferredWidth: Math.max(content.implicitWidth,
                             Kirigami.Units.gridUnit * 10, implicitHeaderWidth, implicitFooterWidth)

    RowLayout {
        id: content
        spacing: 0

        Item {
            Layout.fillWidth: true
        }

        RowLayout {
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                text: label
                visible: (label?.length ?? 0) > 0
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
                case DurationPromptDialog.Unit.Milliseconds:
                    return i18ncp("The unit of the time input field", "millisecond", "milliseconds", val);
                case DurationPromptDialog.Unit.Seconds:
                    return i18ncp("The unit of the time input field", "second", "seconds", val);
                case DurationPromptDialog.Unit.Minutes:
                    return i18ncp("The unit of the time input field", "minute", "minutes", val);
                case DurationPromptDialog.Unit.Hours:
                    return i18ncp("The unit of the time input field", "hour", "hours", val);
                case DurationPromptDialog.Unit.Days:
                    return i18ncp("The unit of the time input field", "day", "days", val);
                case DurationPromptDialog.Unit.Weeks:
                    return i18ncp("The unit of the time input field", "week", "weeks", val);
                case DurationPromptDialog.Unit.Months:
                    return i18ncp("The unit of the time input field", "month", "months", val);
                case DurationPromptDialog.Unit.Years:
                    return i18ncp("The unit of the time input field", "year", "years", val);
                }
                console.log("invalid unit in unitSuffixForValue()");
            }

            QQC2.Label {
                visible: acceptsUnits.length == 1
                text: parent.unitSuffixForValue(value, acceptsUnits[0])

                // Try not to shrink. The +1 is there because I couldn't figure out why actual
                // plural metrics were 49 but implicitWidth for the plural string 49.78125.
                // An extra pixel shouldn't hurt either way, and hopefully metrics are close enough.
                Layout.preferredWidth: Math.max(implicitWidth, pluralUnitLabelMetrics.width + leftPadding + rightPadding + 1)
                TextMetrics {
                    id: pluralUnitLabelMetrics
                    text: unitSelectionRadios.labelForUnit(acceptsUnits[0])
                }
            }

            ColumnLayout {
                id: unitSelectionRadios
                spacing: Kirigami.Units.smallSpacing
                visible: acceptsUnits.length > 1

                function labelForUnit(unit) {
                    switch (unit) {
                    case DurationPromptDialog.Unit.Milliseconds:
                        return i18nc("@text:radiobutton Unit of the time input field", "milliseconds");
                    case DurationPromptDialog.Unit.Seconds:
                        return i18nc("@text:radiobutton Unit of the time input field", "seconds");
                    case DurationPromptDialog.Unit.Minutes:
                        return i18nc("@text:radiobutton Unit of the time input field", "minutes");
                    case DurationPromptDialog.Unit.Hours:
                        return i18nc("@text:radiobutton Unit of the time input field", "hours");
                    case DurationPromptDialog.Unit.Days:
                        return i18nc("@text:radiobutton Unit of the time input field", "days");
                    case DurationPromptDialog.Unit.Weeks:
                        return i18nc("@text:radiobutton Unit of the time input field", "weeks");
                    case DurationPromptDialog.Unit.Months:
                        return i18nc("@text:radiobutton Unit of the time input field", "months");
                    case DurationPromptDialog.Unit.Years:
                        return i18nc("@text:radiobutton Unit of the time input field", "years");
                    }
                    console.log("invalid unit in radioButtonLabelForValue()");
                }

                Repeater {
                    model: acceptsUnits

                    QQC2.RadioButton {
                        required property int modelData
                        text: parent.labelForUnit(modelData)
                        checked: unit === modelData
                        onClicked: unit = modelData
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
        }
    }
}
