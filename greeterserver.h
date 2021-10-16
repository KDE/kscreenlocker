/********************************************************************
 KSld - the KDE Screenlocker Daemon
 This file is part of the KDE project.

 Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef SCREENLOCKER_WAYLANDSERVER_H
#define SCREENLOCKER_WAYLANDSERVER_H

#include <QLocalSocket>
#include <QObject>

namespace ScreenLocker
{
class GreeterServer : public QObject
{
    Q_OBJECT
public:
    enum class MessageId : quint32 {
        RegisterWindow = 1,
    };

    explicit GreeterServer(QObject *parent = nullptr);
    ~GreeterServer() override;
    int start();
    void stop();

Q_SIGNALS:
    void x11WindowAdded(quint32 window);

private:
    void dispatch();

    QLocalSocket *m_socket = nullptr;
};

}

#endif
