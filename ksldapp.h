/********************************************************************
 KSld - the KDE Screenlocker Daemon
 This file is part of the KDE project.

 Copyright 1999 Martin R. Jones <mjones@kde.org>
 Copyright 2003 Oswald Buddenhagen <ossi@kde.org>
 Copyright 2008 Chani Armitage <chanika@gmail.com>
 Copyright (C) 2011 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef SCREENLOCKER_KSLDAPP_H
#define SCREENLOCKER_KSLDAPP_H

#include <QElapsedTimer>
#include <QProcessEnvironment>

#include <KScreenLocker/kscreenlocker_export.h>

// forward declarations
class GlobalAccel;
class LogindIntegration;
class QTimer;
class KSldTest;
class PowerManagementInhibition;

namespace KWayland
{
namespace Server {
class Display;
class ClientConnection;
}
}

namespace ScreenLocker
{

enum class EstablishLock {
    Immediate, ///Require password from the start. Use if invoked explicitly by the user
    Delayed, ///Allow the user to log back in without a password for a configured grace time.
    DefaultToSwitchUser ///UI should default to showing the "switch user dialog"
};

class AbstractLocker;
class WaylandServer;

class KSCREENLOCKER_EXPORT KSldApp : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.ksld.App")
    Q_ENUMS(LockState)

public:
    enum LockState {
        Unlocked,
        AcquiringLock,
        Locked
    };

    static KSldApp* self();

    explicit KSldApp(QObject * parent = nullptr);
    ~KSldApp() override;

    LockState lockState() const {
        return m_lockState;
    }

    /**
     * @returns the number of milliseconds passed since the screen has been locked.
     **/
    uint activeTime() const;

    void configure();

    bool isGraceTime() const;

    void setWaylandDisplay(KWayland::Server::Display *display);

    KWayland::Server::ClientConnection *greeterClientConnection() const {
        return m_greeterClientConnection;
    }
    void setGreeterEnvironment(const QProcessEnvironment &env);

    /**
     * Can be used by the lock window to remove the lock during grace time.
     **/
    void unlock();
    void inhibit();
    void uninhibit();

    void lock(EstablishLock establishLock, int attemptCount = 0);
    void initialize();

    bool event(QEvent *event) override;

    /**
     * For testing
     * @internal
     **/
    int idleId() const {
        return m_idleId;
    }
    /**
     * For testing
     * @internal
     **/
    void setIdleId(int idleId) {
        m_idleId = idleId;
    }
    /**
     * For testing
     * @internal
     **/
    void setGraceTime(int msec) {
        m_lockGrace = msec;
    }

    bool forceSoftwareRendering() const {
        return m_forceSoftwareRendering;
    }

    void setForceSoftwareRendering(bool force) {
        m_forceSoftwareRendering = force;
    }

Q_SIGNALS:
    void aboutToLock();
    void locked();
    void unlocked();
    void greeterClientConnectionChanged();
    void lockStateChanged();

private Q_SLOTS:
    void cleanUp();
    void endGraceTime();
    void solidSuspend();

public Q_SLOTS:
    void lockScreenShown();

private:
    void initializeX11();
    bool establishGrab();
    void startLockProcess(EstablishLock establishLock);
    void showLockWindow();
    void hideLockWindow();
    void doUnlock();
    bool isFdoPowerInhibited() const;

    LockState m_lockState;
    QProcess *m_lockProcess;
    AbstractLocker *m_lockWindow;
    WaylandServer *m_waylandServer;
    KWayland::Server::Display *m_waylandDisplay;
    KWayland::Server::ClientConnection *m_greeterClientConnection;
    /**
     * Timer to measure how long the screen is locked.
     * This information is required by DBus Interface.
     **/
    QElapsedTimer m_lockedTimer;
    int m_idleId;
    /**
     * Number of milliseconds after locking in which user activity will result in screen being
     * unlocked without requiring a password.
     **/
    int m_lockGrace;
    /**
     * Controls whether user activity may remove the lock. Only enabled after idle timeout.
     **/
    bool m_inGraceTime;
    /**
     * Grace time ends when timer expires.
     **/
    QTimer *m_graceTimer;
    int m_inhibitCounter;
    LogindIntegration *m_logind;
    GlobalAccel *m_globalAccel = nullptr;
    bool m_hasXInput2 = false;
    bool m_forceSoftwareRendering = false;

    bool m_isX11;
    bool m_isWayland;
    int m_greeterCrashedCounter = 0;
    QProcessEnvironment m_greeterEnv;
    PowerManagementInhibition *m_powerManagementInhibition;

    // for auto tests
    friend KSldTest;
};
} // namespace

#endif // SCREENLOCKER_KSLDAPP_H
