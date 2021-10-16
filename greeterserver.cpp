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
#include "greeterserver.h"

#include <QDataStream>

// ksld
#include <config-kscreenlocker.h>
// system
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace ScreenLocker
{
GreeterServer::GreeterServer(QObject *parent)
    : QObject(parent)
{
}

GreeterServer::~GreeterServer()
{
    stop();
}

int GreeterServer::start()
{
    stop();

    int socketPair[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, socketPair) == -1) {
        // failed creating socket
        return -1;
    }
    fcntl(socketPair[0], F_SETFD, FD_CLOEXEC);

    m_socket = new QLocalSocket(this);
    if (!m_socket->setSocketDescriptor(socketPair[0])) {
        // failed creating the Wayland client
        stop();
        close(socketPair[0]);
        close(socketPair[1]);
        return -1;
    }

    connect(m_socket, &QLocalSocket::readyRead, this, &GreeterServer::dispatch);

    return socketPair[1];
}

void GreeterServer::stop()
{
    delete m_socket;
    m_socket = nullptr;
}

void GreeterServer::dispatch()
{
    QDataStream stream(m_socket);

    quint32 message;
    stream >> message;

    switch (MessageId(message)) {
    case MessageId::RegisterWindow: {
        quint32 windowId;
        stream >> windowId;
        Q_EMIT x11WindowAdded(windowId);
        break;
    }
    default: {
        qDebug() << "Unknown message id:" << message;
        break;
    }
    }
}

}
