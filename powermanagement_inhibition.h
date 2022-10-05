/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
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

    bool isInhibited() const
    {
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
