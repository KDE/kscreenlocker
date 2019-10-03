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
#include "ksldapp.h"
#include "interface.h"
#include "globalaccel.h"
#include "x11locker.h"
#include "waylandlocker.h"
#include "logind.h"
#include "kscreensaversettings.h"
#include "powermanagement_inhibition.h"
#include <config-kscreenlocker.h>
#include <config-X11.h>
#include "waylandserver.h"
// KDE
#include <KAuthorized>
#include <KIdleTime>
#include <KLocalizedString>
#include <KNotification>
#include <KGlobalAccel>

//kwayland
#include <KWayland/Server/display.h>
#include <KWayland/Server/clientconnection.h>

// Qt
#include <QAction>
#include <QKeyEvent>
#include <QTimer>
#include <QProcess>
#include <QX11Info>
// X11
#include <X11/Xlib.h>
#include <xcb/xcb.h>
#ifdef X11_Xinput_FOUND
#include <X11/extensions/XInput2.h>
#endif
// other
#include <unistd.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>

namespace ScreenLocker
{

static const QString s_qtQuickBackend = QStringLiteral("QT_QUICK_BACKEND");

static KSldApp * s_instance = nullptr;

KSldApp* KSldApp::self()
{
    if (!s_instance) {
        s_instance = new KSldApp();
    }

    return s_instance;
}

KSldApp::KSldApp(QObject * parent)
    : QObject(parent)
    , m_lockState(Unlocked)
    , m_lockProcess(nullptr)
    , m_lockWindow(nullptr)
    , m_waylandServer(new WaylandServer(this))
    , m_waylandDisplay(nullptr)
    , m_lockedTimer(QElapsedTimer())
    , m_idleId(0)
    , m_lockGrace(0)
    , m_inGraceTime(false)
    , m_graceTimer(new QTimer(this))
    , m_inhibitCounter(0)
    , m_logind(nullptr)
    , m_greeterEnv(QProcessEnvironment::systemEnvironment())
    , m_powerManagementInhibition(new PowerManagementInhibition(this))
{
    m_isX11 = QX11Info::isPlatformX11();
    m_isWayland = QCoreApplication::instance()->property("platformName").toString().startsWith( QLatin1String("wayland"), Qt::CaseInsensitive);
}

KSldApp::~KSldApp()
{
}

static int s_XTimeout;
static int s_XInterval;
static int s_XBlanking;
static int s_XExposures;

void KSldApp::cleanUp()
{
    if (m_lockProcess && m_lockProcess->state() != QProcess::NotRunning) {
        m_lockProcess->terminate();
    }
    delete m_lockProcess;
    delete m_lockWindow;

    // Restore X screensaver parameters
    XSetScreenSaver(QX11Info::display(), s_XTimeout, s_XInterval, s_XBlanking, s_XExposures);
}

static bool s_graceTimeKill = false;
static bool s_logindExit = false;

static bool hasXInput()
{
#ifdef X11_Xinput_FOUND
    Display *dpy = QX11Info::display();
    int xi_opcode, event, error;
    // init XInput extension
    if (!XQueryExtension(dpy, "XInputExtension", &xi_opcode, &event, &error)) {
        return false;
    }

    // verify that the XInput extension is at at least version 2.0
    int major = 2, minor = 0;
    int result = XIQueryVersion(dpy, &major, &minor);
    if (result == BadImplementation) {
        // Xinput 2.2 returns BadImplementation if checked against 2.0
        major = 2;
        minor = 2;
        return XIQueryVersion(dpy, &major, &minor) == Success;
    }
    return result == Success;
#else
    return false;
#endif
}

void KSldApp::initializeX11()
{
    m_hasXInput2 = hasXInput();
    // Save X screensaver parameters
    XGetScreenSaver(QX11Info::display(), &s_XTimeout, &s_XInterval, &s_XBlanking, &s_XExposures);
    // And disable it. The internal X screensaver is not used at all, but we use its
    // internal idle timer (and it is also used by DPMS support in X). This timer must not
    // be altered by this code, since e.g. resetting the counter after activating our
    // screensaver would prevent DPMS from activating. We use the timer merely to detect
    // user activity.
    XSetScreenSaver(QX11Info::display(), 0, s_XInterval, s_XBlanking, s_XExposures);
}

void KSldApp::initialize()
{
    if (m_isX11) {
        initializeX11();
    }

    // Global keys
    if (KAuthorized::authorizeAction(QStringLiteral("lock_screen"))) {
        qDebug() << "Configuring Lock Action";
        QAction *a = new QAction(this);
        a->setObjectName(QStringLiteral("Lock Session"));
        a->setProperty("componentName", QStringLiteral("ksmserver"));
        a->setText(i18n("Lock Session"));
        KGlobalAccel::self()->setGlobalShortcut(a, QList<QKeySequence>() << Qt::META+Qt::Key_L << Qt::ALT+Qt::CTRL+Qt::Key_L << Qt::Key_ScreenSaver );
        connect(a, &QAction::triggered, this,
            [this]() {
                lock(EstablishLock::Immediate);
            }
        );
    }

    // idle support
    auto idleTimeSignal = static_cast<void (KIdleTime:: *)(int)>(&KIdleTime::timeoutReached);
    connect(KIdleTime::instance(), idleTimeSignal, this,
        [this](int identifier) {
            if (identifier != m_idleId) {
                // not our identifier
                return;
            }
            if (lockState() != Unlocked) {
                return;
            }
            if (m_inhibitCounter // either we got a direct inhibit request thru our outdated o.f.Screensaver iface ...
                    || isFdoPowerInhibited()) { // ... or the newer one at o.f.PowerManagement.Inhibit
                // there is at least one process blocking the auto lock of screen locker
                return;
            }
            if (m_lockGrace) {  // short-circuit if grace time is zero
                m_inGraceTime = true;
            } else if (m_lockGrace == -1) {
                m_inGraceTime = true;  // if no timeout configured, grace time lasts forever
            }

            lock(EstablishLock::Delayed);
        }
    );

    m_lockProcess = new QProcess();
    m_lockProcess->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    m_lockProcess->setReadChannel(QProcess::StandardOutput);
    auto finishedSignal = static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished);
    connect(m_lockProcess, finishedSignal, this,
        [this](int exitCode, QProcess::ExitStatus exitStatus) {
            if (m_isWayland && m_waylandDisplay && m_greeterClientConnection) {
                m_greeterClientConnection->destroy();
            }
            if ((!exitCode && exitStatus == QProcess::NormalExit) || s_graceTimeKill || s_logindExit) {
                // unlock process finished successfully - we can remove the lock grab
                s_graceTimeKill = false;
                s_logindExit = false;
                doUnlock();
                return;
            }
            // failure, restart lock process
            m_greeterCrashedCounter++;
            if (m_greeterCrashedCounter < 4) {
                // Perhaps it crashed due to a graphics driver issue, force software rendering now
                setForceSoftwareRendering(true);
                startLockProcess(EstablishLock::Immediate);
            } else if (m_lockWindow) {
                m_lockWindow->emergencyShow();
            }
        }
    );
    connect(m_lockProcess, &QProcess::errorOccurred, this,
        [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                doUnlock();
                m_waylandServer->stop();
                qCritical() << "Greeter Process not available";
            }
        }
    );
    m_lockedTimer.invalidate();
    m_graceTimer->setSingleShot(true);
    connect(m_graceTimer, &QTimer::timeout, this, &KSldApp::endGraceTime);
    // create our D-Bus interface
    new Interface(this);

    // connect to logind
    m_logind = new LogindIntegration(this);
    connect(m_logind, &LogindIntegration::requestLock, this,
        [this]() {
            lock(EstablishLock::Immediate);
        }
    );
    connect(m_logind, &LogindIntegration::requestUnlock, this,
        [this]() {
            if (lockState() == Locked || lockState() == AcquiringLock) {
                if (m_lockProcess->state() != QProcess::NotRunning) {
                    s_logindExit = true;
                    m_lockProcess->terminate();
                } else {
                    doUnlock();
                }
            }
        }
    );
    connect(m_logind, &LogindIntegration::prepareForSleep, this,
        [this](bool goingToSleep) {
            if (!goingToSleep) {
                // not interested in doing anything on wakeup
                return;
            }
            if (KScreenSaverSettings::lockOnResume()) {
                lock(EstablishLock::Immediate);
            }
        }
    );
    connect(m_logind, &LogindIntegration::inhibited, this,
        [this]() {
            // if we are already locked, we immediatelly remove the inhibition lock
            if (m_lockState == KSldApp::Locked) {
                m_logind->uninhibit();
            }
        }
    );
    connect(m_logind, &LogindIntegration::connectedChanged, this,
        [this]() {
            if (m_logind->isConnected() && m_lockState == ScreenLocker::KSldApp::Unlocked && KScreenSaverSettings::lockOnResume()) {
                m_logind->inhibit();
            }
        }
    );
    connect(this, &KSldApp::locked, this,
        [this]() {
            m_logind->uninhibit();
            if (m_lockGrace > 0 && m_inGraceTime) {
                m_graceTimer->start(m_lockGrace);
            }
        }
    );
    connect(this, &KSldApp::unlocked, this,
        [this]() {
            if (KScreenSaverSettings::lockOnResume()) {
                m_logind->inhibit();
            }
        }
    );

    m_globalAccel = new GlobalAccel(this);
    connect(this, &KSldApp::locked, m_globalAccel, &GlobalAccel::prepare);
    connect(this, &KSldApp::unlocked, m_globalAccel, &GlobalAccel::release);

    // fallback for non-logind systems:
    // connect to signal emitted by Solid. This is emitted unconditionally also on logind enabled systems
    // ksld ignores it in case logind is used
    QDBusConnection::sessionBus().connect(QStringLiteral("org.kde.Solid.PowerManagement"),
                                          QStringLiteral("/org/kde/Solid/PowerManagement/Actions/SuspendSession"),
                                          QStringLiteral("org.kde.Solid.PowerManagement.Actions.SuspendSession"),
                                          QStringLiteral("aboutToSuspend"),
                                          this, SLOT(solidSuspend()));

    configure();
}

void KSldApp::configure()
{
    KScreenSaverSettings::self()->load();
    // idle support
    if (m_idleId) {
        KIdleTime::instance()->removeIdleTimeout(m_idleId);
        m_idleId = 0;
    }
    const int timeout = KScreenSaverSettings::timeout();
    // screen saver enabled means there is an auto lock timer
    // timeout > 0 is for backwards compatibility with old configs
    if (KScreenSaverSettings::autolock() && timeout > 0) {
        // timeout stored in minutes
        m_idleId = KIdleTime::instance()->addIdleTimeout(timeout*1000*60);
    }
    if (KScreenSaverSettings::lock()) {
        // lockGrace is stored in seconds
        m_lockGrace = KScreenSaverSettings::lockGrace() * 1000;
    } else {
        m_lockGrace = -1;
    }
    if (m_logind && m_logind->isConnected()) {
        if (KScreenSaverSettings::lockOnResume() && !m_logind->isInhibited()) {
            m_logind->inhibit();
        } else if (!KScreenSaverSettings::lockOnResume() && m_logind->isInhibited()) {
            m_logind->uninhibit();
        }
    }
}

void KSldApp::lock(EstablishLock establishLock, int attemptCount)
{
    if (lockState() != Unlocked) {
        // already locked or acquiring lock, no need to lock again
        // but make sure it's really locked
        endGraceTime();
        if (establishLock == EstablishLock::Immediate) {
            // signal the greeter to switch to immediateLock mode
            kill(m_lockProcess->pid(), SIGUSR1);
        }
        return;
    }

    if (attemptCount == 0) {
        emit aboutToLock();
    }

    qDebug() << "lock called";
    if (!establishGrab()) {
        if (attemptCount < 3) {
            qWarning() << "Could not establish screen lock. Trying again in 10ms";
            QTimer::singleShot(10, this, [=]() {
                lock(establishLock, attemptCount+1);
            });
        } else {
            qCritical() << "Could not establish screen lock";
        }
        return;
    }

    KNotification::event(QStringLiteral("locked"),
                         i18n("Screen locked"),
                         QPixmap(),
                         nullptr,
                         KNotification::CloseOnTimeout,
                         QStringLiteral("ksmserver"));

    // blank the screen
    showLockWindow();

    m_lockState = AcquiringLock;

    setForceSoftwareRendering(false);
    // start unlock screen process
    startLockProcess(establishLock);
    emit lockStateChanged();
}

/*
 * Forward declarations:
 * Only called from KSldApp::establishGrab(). Using from somewhere else is incorrect usage!
 **/
static bool grabKeyboard();
static bool grabMouse();

class XServerGrabber
{
public:
    XServerGrabber() {
        xcb_grab_server(QX11Info::connection());
    }
    ~XServerGrabber() {
        xcb_ungrab_server(QX11Info::connection());
        xcb_flush(QX11Info::connection());
    }
};

bool KSldApp::establishGrab()
{
    if (!m_isX11) {
        return true;
    }
    XSync(QX11Info::display(), False);
    XServerGrabber serverGrabber;
    if (!grabKeyboard()) {
        return false;
    }

    if (!grabMouse()) {
        XUngrabKeyboard(QX11Info::display(), CurrentTime);
        XFlush(QX11Info::display());
        return false;
    }

#ifdef X11_Xinput_FOUND
    if (m_hasXInput2) {
        // get all devices
        Display *dpy = QX11Info::display();
        int numMasters;
        XIDeviceInfo *masters = XIQueryDevice(dpy, XIAllMasterDevices, &numMasters);
        bool success = true;
        for (int i = 0; i < numMasters; ++i) {
            // ignoring core pointer and core keyboard as we already grabbed them
            if (qstrcmp(masters[i].name, "Virtual core pointer") == 0) {
                continue;
            }
            if (qstrcmp(masters[i].name, "Virtual core keyboard") == 0) {
                continue;
            }
            XIEventMask mask;
            uchar bitmask[] = { 0, 0 };
            mask.deviceid = masters[i].deviceid;
            mask.mask = bitmask;
            mask.mask_len = sizeof(bitmask);
            XISetMask(bitmask, XI_ButtonPress);
            XISetMask(bitmask, XI_ButtonRelease);
            XISetMask(bitmask, XI_Motion);
            XISetMask(bitmask, XI_Enter);
            XISetMask(bitmask, XI_Leave);
            const int result = XIGrabDevice(dpy, masters[i].deviceid, QX11Info::appRootWindow(),
                                            XCB_TIME_CURRENT_TIME, XCB_CURSOR_NONE, XIGrabModeAsync,
                                            XIGrabModeAsync, True, &mask);
            if (result != XIGrabSuccess) {
                success = false;
                break;
            }
        }
        if (!success) {
            // ungrab all devices again
            for (int i = 0; i < numMasters; ++i) {
                XIUngrabDevice(dpy, masters[i].deviceid, XCB_TIME_CURRENT_TIME);
            }
            xcb_connection_t *c = QX11Info::connection();
            xcb_ungrab_keyboard(c, XCB_CURRENT_TIME);
            xcb_ungrab_pointer(c, XCB_CURRENT_TIME);
        }
        XIFreeDeviceInfo(masters);
        XFlush(dpy);
        return success;
    }
#endif

    return true;
}

static bool grabKeyboard()
{
    int rv = XGrabKeyboard( QX11Info::display(), QX11Info::appRootWindow(),
        True, GrabModeAsync, GrabModeAsync, CurrentTime );

    return (rv == GrabSuccess);
}

static bool grabMouse()
{
#define GRABEVENTS ButtonPressMask | ButtonReleaseMask | PointerMotionMask | \
                   EnterWindowMask | LeaveWindowMask
    int rv = XGrabPointer( QX11Info::display(), QX11Info::appRootWindow(),
            True, GRABEVENTS, GrabModeAsync, GrabModeAsync, None,
            None, CurrentTime );
#undef GRABEVENTS

    return (rv == GrabSuccess);
}

void KSldApp::doUnlock()
{
    qDebug() << "Grab Released";
    if (m_isX11) {
        xcb_connection_t *c = QX11Info::connection();
        xcb_ungrab_keyboard(c, XCB_CURRENT_TIME);
        xcb_ungrab_pointer(c, XCB_CURRENT_TIME);
        xcb_flush(c);
#ifdef X11_Xinput_FOUND
        if (m_hasXInput2) {
            // get all devices
            Display *dpy = QX11Info::display();
            int numMasters;
            XIDeviceInfo *masters = XIQueryDevice(dpy, XIAllMasterDevices, &numMasters);
            // ungrab all devices again
            for (int i = 0; i < numMasters; ++i) {
                XIUngrabDevice(dpy, masters[i].deviceid, XCB_TIME_CURRENT_TIME);
            }
            XIFreeDeviceInfo(masters);
            XFlush(dpy);
        }
#endif
    }
    hideLockWindow();
    // delete the window again, to get rid of event filter
    delete m_lockWindow;
    m_lockWindow = nullptr;
    m_lockState = Unlocked;
    m_lockedTimer.invalidate();
    m_greeterCrashedCounter = 0;
    endGraceTime();
    m_waylandServer->stop();
    KNotification::event(QStringLiteral("unlocked"),
                         i18n("Screen unlocked"),
                         QPixmap(),
                         nullptr,
                         KNotification::CloseOnTimeout,
                         QStringLiteral("ksmserver"));
    emit unlocked();
    emit lockStateChanged();
}

bool KSldApp::isFdoPowerInhibited() const
{
    return m_powerManagementInhibition->isInhibited();
}

void KSldApp::startLockProcess(EstablishLock establishLock)
{
    QProcessEnvironment env = m_greeterEnv;

    if (m_isWayland && m_waylandDisplay) {
        int sx[2];
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx) < 0) {
            qWarning() << "Can not create socket";
            emit m_lockProcess->errorOccurred(QProcess::FailedToStart);
            return;
        }
        m_greeterClientConnection = m_waylandDisplay->createClient(sx[0]);
        connect(m_greeterClientConnection, &QObject::destroyed, this,
            [this] (QObject *destroyedObject) {
                if (destroyedObject != m_greeterClientConnection) {
                    return;
                }
                m_greeterClientConnection = nullptr;
                emit greeterClientConnectionChanged();
            }
        );
        emit greeterClientConnectionChanged();
        int socket = dup(sx[1]);
        if (socket >= 0) {
            env.insert(QStringLiteral("WAYLAND_SOCKET"), QString::number(socket));
        }
    }
    QStringList args;
    if (establishLock == EstablishLock::Immediate) {
        args << QStringLiteral("--immediateLock");
    }
    if (establishLock == EstablishLock::DefaultToSwitchUser) {
        args << QStringLiteral("--immediateLock");
        args << QStringLiteral("--switchuser");
    }

    if (m_lockGrace > 0) {
        args << QStringLiteral("--graceTime");
        args << QString::number(m_lockGrace);
    }
    if (m_lockGrace == -1) {
        args << QStringLiteral("--nolock");
    }
    if (m_forceSoftwareRendering) {
        env.insert(s_qtQuickBackend, QStringLiteral("software"));
    }

    // start the Wayland server
    int fd = m_waylandServer->start();
    if (fd == -1) {
        emit m_lockProcess->errorOccurred(QProcess::FailedToStart);
        return;
    }

    args << QStringLiteral("--ksldfd");
    args << QString::number(fd);

    m_lockProcess->setProcessEnvironment(env);
    m_lockProcess->start(QStringLiteral(KSCREENLOCKER_GREET_BIN), args);
    close(fd);
}

void KSldApp::showLockWindow()
{
    if (!m_lockWindow) {
        if (m_isX11) {
            m_lockWindow = new X11Locker(this);
        }
        if (m_isWayland) {
            m_lockWindow = new WaylandLocker(m_waylandDisplay, this);
        }
        if (!m_lockWindow) {
            return;
        }
        m_lockWindow->setGlobalAccel(m_globalAccel);
        connect(m_lockWindow, &AbstractLocker::userActivity, m_lockWindow,
            [this]() {
                if (isGraceTime()) {
                    unlock();
                }
            },
            Qt::QueuedConnection
        );
        connect(m_lockWindow, &AbstractLocker::lockWindowShown, m_lockWindow,
            [this] {
                lockScreenShown();
            }
        , Qt::QueuedConnection);
        connect(m_waylandServer, &WaylandServer::x11WindowAdded, m_lockWindow, &AbstractLocker::addAllowedWindow);
    }
    m_lockWindow->showLockWindow();
    if (m_isX11) {
        XSync(QX11Info::display(), False);
    }
}

void KSldApp::hideLockWindow()
{
    if (!m_lockWindow) {
        return;
    }
    m_lockWindow->hideLockWindow();
}

uint KSldApp::activeTime() const
{
    if (m_lockedTimer.isValid()) {
        return m_lockedTimer.elapsed();
    }
    return 0;
}

bool KSldApp::isGraceTime() const
{
    return m_inGraceTime;
}

void KSldApp::endGraceTime()
{
    m_graceTimer->stop();
    m_inGraceTime = false;
}

void KSldApp::unlock()
{
    if (!isGraceTime()) {
        return;
    }
    s_graceTimeKill = true;
    m_lockProcess->terminate();
}

void KSldApp::inhibit()
{
    ++m_inhibitCounter;
}

void KSldApp::uninhibit()
{
    --m_inhibitCounter;
}

void KSldApp::solidSuspend()
{
    // ignore in case that we use logind
    if (m_logind && m_logind->isConnected()) {
        return;
    }
    if (KScreenSaverSettings::lockOnResume()) {
        lock(EstablishLock::Immediate);
    }
}

void KSldApp::setWaylandDisplay(KWayland::Server::Display *display)
{
    if (m_waylandDisplay != display) {
        m_waylandDisplay = display;
    }
}

void KSldApp::lockScreenShown()
{
    if (m_lockState == Locked) {
        return;
    }
    m_lockState = Locked;
    m_lockedTimer.restart();
    emit locked();
    emit lockStateChanged();
}

void KSldApp::setGreeterEnvironment(const QProcessEnvironment &env)
{
    m_greeterEnv = env;
    if (m_isWayland) {
        m_greeterEnv.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
    }
}

bool KSldApp::event(QEvent *event)
{
    if (m_globalAccel && event->type() == QEvent::KeyPress) {
        if (m_globalAccel->keyEvent(static_cast<QKeyEvent*>(event))) {
            event->setAccepted(true);
        }
    }
    return false;
}

} // namespace
