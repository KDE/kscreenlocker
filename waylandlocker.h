/*
    SPDX-FileCopyrightText: 2015 Bhushan Shah <bhush94@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef WAYLANDLOCKER_H
#define WAYLANDLOCKER_H

#include "abstractlocker.h"

namespace KWayland
{
namespace Server
{
class Display;
}
}

namespace ScreenLocker
{
class WaylandLocker : public AbstractLocker
{
    Q_OBJECT

public:
    WaylandLocker(QObject *parent);
    ~WaylandLocker() override;

    void showLockWindow() override;
    void hideLockWindow() override;

    void addAllowedWindow(quint32 window) override;

private:
    void stayOnTop() override;
    void updateGeometryOfBackground();
};

}

#endif // WAYLANDLOCKER_H
