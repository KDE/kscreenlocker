/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>

class QDBusServiceWatcher;

struct SolidInhibition {
    uint cookie;
    QString appName;
    QString reason;
};

class PowerManagementInhibition : public QObject
{
    Q_OBJECT
public:
    explicit PowerManagementInhibition(QObject *parent = nullptr);
    ~PowerManagementInhibition() override;

    bool isInhibited() const
    {
        return m_inhibited;
    }

private Q_SLOTS:
    void inhibitionsChanged(const QList<SolidInhibition> &added, const QList<uint> &removed);

private:
    void checkInhibition();
    void update();

    QDBusServiceWatcher *m_solidPowerServiceWatcher;
    bool m_serviceRegistered = false;
    bool m_inhibited = false;
};
