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

class GlobalAccel;

namespace ScreenLocker
{
class Locker;

class BackgroundWindow : public QRasterWindow
{
    Q_OBJECT
public:
    explicit BackgroundWindow(Locker *lock);
    ~BackgroundWindow() override;

    void emergencyShow();

protected:
    void paintEvent(QPaintEvent *) override;

private:
    Locker *m_lock;
    bool m_greeterFailure = false;
};

class Locker : public QObject
{
    Q_OBJECT
public:
    explicit Locker(QObject *parent);
    ~Locker() override;

    void addAllowedWindow(quint32 window);

    void setGlobalAccel(GlobalAccel *ga)
    {
        m_globalAccel = ga;
    }

    void emergencyShow();

Q_SIGNALS:
    void userActivity();
    void lockWindowShown();

protected:
    GlobalAccel *globalAccel()
    {
        return m_globalAccel;
    }
    QScopedPointer<BackgroundWindow> m_background;

private:
    void updateGeometryOfBackground();

    GlobalAccel *m_globalAccel = nullptr;

    friend class BackgroundWindow;
};

}
