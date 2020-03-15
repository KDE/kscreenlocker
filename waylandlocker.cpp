/********************************************************************
 KSld - the KDE Screenlocker Daemon
 This file is part of the KDE project.

 Copyright (C) 2015 Bhushan Shah <bhush94@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "waylandlocker.h"

#include <QGuiApplication>
#include <QScreen>

namespace ScreenLocker
{

WaylandLocker::WaylandLocker(QObject *parent)
    : AbstractLocker(parent)
{
    if (m_background) {
        updateGeometryOfBackground();
        const auto screens = qApp->screens();
        for (auto s : screens) {
            connect(s, &QScreen::geometryChanged, this, &WaylandLocker::updateGeometryOfBackground);
        }
        connect(qApp, &QGuiApplication::screenAdded, this,
            [this] (QScreen *s) {
                connect(s, &QScreen::geometryChanged, this, &WaylandLocker::updateGeometryOfBackground);
                updateGeometryOfBackground();
            }
        );
        connect(qApp, &QGuiApplication::screenRemoved, this, &WaylandLocker::updateGeometryOfBackground);
    }
}

WaylandLocker::~WaylandLocker()
{
}

void WaylandLocker::updateGeometryOfBackground()
{
    QRect combined;
    const auto screens = qApp->screens();
    for (auto s : screens) {
        combined = combined.united(s->geometry());
    }
    m_background->setGeometry(combined);
    m_background->update();
}

void WaylandLocker::showLockWindow()
{
}

void WaylandLocker::hideLockWindow()
{
}

void WaylandLocker::addAllowedWindow(quint32 window)
{
}

void WaylandLocker::stayOnTop()
{
}

}
