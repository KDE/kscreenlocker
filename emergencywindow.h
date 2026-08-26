/*
    SPDX-FileCopyrightText: 1999 Martin R. Jones <mjones@kde.org>
    SPDX-FileCopyrightText: 2002 Luboš Luňák <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2003 Oswald Buddenhagen <ossi@kde.org>
    SPDX-FileCopyrightText: 2008 Chani Armitage <chanika@gmail.com>
    SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2015 Bhushan Shah <bhush94@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QRasterWindow>

namespace ScreenLocker
{

class EmergencyWindow : public QRasterWindow
{
    Q_OBJECT
public:
    explicit EmergencyWindow();
    ~EmergencyWindow() override;

protected:
    void paintEvent(QPaintEvent *) override;
    void updateGeometry();
};

}
