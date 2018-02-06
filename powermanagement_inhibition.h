/********************************************************************
 KSld - the KDE Screenlocker Daemon
 This file is part of the KDE project.

 Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef POWERMANAGEMENT_INHIBITION_H
#define POWERMANAGEMENT_INHIBITION_H

#include <QObject>

class QDBusServiceWatcher;

using InhibitionInfo = QPair<QString, QString>;

class PowerManagementInhibition : public QObject
{
    Q_OBJECT
public:
    explicit PowerManagementInhibition(QObject *parent = nullptr);
    ~PowerManagementInhibition() override;

    bool isInhibited() const {
        return m_inhibited;
    }

private Q_SLOTS:
    void inhibitionsChanged(const QList<InhibitionInfo> &added, const QStringList &removed);

private:
    void checkInhibition();
    void update();

    QDBusServiceWatcher *m_solidPowerServiceWatcher;
    bool m_serviceRegistered = false;
    bool m_inhibited = false;
};

#endif
