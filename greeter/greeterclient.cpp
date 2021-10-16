/********************************************************************
 KSld - the KDE Screenlocker Daemon
 This file is part of the KDE project.

 Copyright (C) 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

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

#include "greeterclient.h"

#include <QDataStream>

class SocketWriter
{
public:
    explicit SocketWriter(QLocalSocket *socket)
        : m_socket(socket)
        , m_stream(&m_buffer, QIODevice::WriteOnly)
    {
    }

    ~SocketWriter()
    {
        m_socket->write(m_buffer);
        m_socket->flush();
    }

    SocketWriter &operator<<(quint32 value)
    {
        m_stream << value;
        return *this;
    }

private:
    QLocalSocket *m_socket;
    QByteArray m_buffer;
    QDataStream m_stream;
};

GreeterClient::GreeterClient(int fd, QObject *parent)
    : QObject(parent)
    , m_socket(new QLocalSocket(this))
{
    m_socket->setSocketDescriptor(fd);
}

void GreeterClient::sendRegister(quint32 windowId)
{
    SocketWriter(m_socket) << quint32(MessageId::RegisterWindow) << windowId;
}
