/*
    SPDX-FileCopyrightText: 1999 Martin R. Jones <mjones@kde.org>
    SPDX-FileCopyrightText: 2003 Oswald Buddenhagen <ossi@kde.org>
    SPDX-FileCopyrightText: 2008 Chani Armitage <chanika@gmail.com>
    SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "ksldapp.h"
#include "globalaccel.h"
#include "interface.h"
#include "kscreensaversettings.h"
#include "logind.h"
#include "powermanagement_inhibition.h"
#include "waylandlocker.h"
#include "x11locker.h"

#include "kscreenlocker_logging.h"
#include <config-kscreenlocker.h>

#include "config-unix.h"
#include "waylandserver.h"
// KDE
#include <KAuthorized>
#include <KGlobalAccel>
#include <KIdleTime>
#include <KLibexec>
#include <KLocalizedString>
#include <KNotification>

// Qt
#include <QAction>
#include <QFile>
#include <QKeyEvent>
#include <QProcess>
#include <QTimer>
// X11
#include "x11info.h"
#include <X11/Xlib.h>
#include <xcb/xcb.h>
#if X11_Xinput_FOUND
#include <X11/extensions/XInput2.h>
#endif
// other
#include <signal.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>

namespace ScreenLocker
{

QString establishLockToString(EstablishLock establishLock)
{
    switch (establishLock) {
    case EstablishLock::Immediate:
        return QStringLiteral("Immediate");
    case EstablishLock::Delayed:
        return QStringLiteral("Delayed");
    case EstablishLock::DefaultToSwitchUser:
        return QStringLiteral("DefaultToSwitchUser");
    }
    Q_UNREACHABLE();
}

static const QString s_qtQuickBackend = QStringLiteral("QT_QUICK_BACKEND");

static KSldApp *s_instance = nullptr;

KSldApp *KSldApp::self()
{
    if (!s_instance) {
        s_instance = new KSldApp();
    }

    return s_instance;
}

KSldApp::KSldApp(QObject *parent)
    : QObject(parent)
    , m_lockState(Unlocked)
    , m_lockProcess(nullptr)
    , m_lockWindow(nullptr)
    , m_waylandServer(new WaylandServer(this))
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
    m_isX11 = X11Info::isPlatformX11();
    m_isWayland = QCoreApplication::instance()->property("platformName").toString().startsWith(QLatin1String("wayland"), Qt::CaseInsensitive);
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
    qCDebug(KSCREENLOCKER) << "Cleaning up";

    if (m_lockProcess && m_lockProcess->state() != QProcess::NotRunning) {
        qCDebug(KSCREENLOCKER) << "Terminating lock process";
        m_lockProcess->terminate();
    }
    delete m_lockProcess;
    delete m_lockWindow;

    // Restore X screensaver parameters
    XSetScreenSaver(X11Info::display(), s_XTimeout, s_XInterval, s_XBlanking, s_XExposures);
}

static bool s_graceTimeKill = false;
static bool s_logindExit = false;
static bool s_lockProcessRequestedExit = false;

static bool hasXInput()
{
#if X11_Xinput_FOUND
    Display *dpy = X11Info::display();
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
    qCDebug(KSCREENLOCKER) << "Initializing X11";

    m_hasXInput2 = hasXInput();
    // Save X screensaver parameters
    XGetScreenSaver(X11Info::display(), &s_XTimeout, &s_XInterval, &s_XBlanking, &s_XExposures);
    // And disable it. The internal X screensaver is not used at all, but we use its
    // internal idle timer (and it is also used by DPMS support in X). This timer must not
    // be altered by this code, since e.g. resetting the counter after activating our
    // screensaver would prevent DPMS from activating. We use the timer merely to detect
    // user activity.
    XSetScreenSaver(X11Info::display(), 0, s_XInterval, s_XBlanking, s_XExposures);
}

void KSldApp::initialize()
{
    qCDebug(KSCREENLOCKER) << "Initializing";

    m_requirePassword = KScreenSaverSettings::requirePassword();

    if (m_isX11) {
        initializeX11();
    }

    // Global keys
    if (KAuthorized::authorizeAction(QStringLiteral("lock_screen"))) {
        qCDebug(KSCREENLOCKER) << "Configuring Lock Action";
        QAction *a = new QAction(this);
        a->setObjectName(QStringLiteral("Lock Session"));
        a->setProperty("componentName", QStringLiteral("ksmserver"));
        a->setText(i18n("Lock Session"));
        KGlobalAccel::self()->setGlobalShortcut(a, KScreenSaverSettings::defaultShortcuts());
        connect(a, &QAction::triggered, this, [this]() {
            qCDebug(KSCREENLOCKER) << "Locking session due to global shortcut";
            const EstablishLock lockType = m_requirePassword ? EstablishLock::Immediate : EstablishLock::Delayed;
            m_inGraceTime = !m_requirePassword;
            if (m_inGraceTime) {
                m_lockGrace = -1;
            }
            lock(lockType);
        });
    }

    // idle support
    auto idleTimeSignal = static_cast<void (KIdleTime::*)(int, int)>(&KIdleTime::timeoutReached);
    connect(KIdleTime::instance(), idleTimeSignal, this, [this](int identifier) {
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
        if (m_lockGrace) { // short-circuit if grace time is zero
            m_inGraceTime = true;
        } else if (m_lockGrace == -1) {
            m_inGraceTime = true; // if no timeout configured, grace time lasts forever
        }

        qCDebug(KSCREENLOCKER) << "Idle timeout reached. Locking now.";
        lock(EstablishLock::Delayed);
    });

    m_lockProcess = new QProcess();
    m_lockProcess->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    m_lockProcess->setReadChannel(QProcess::StandardOutput);
    auto finishedSignal = static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished);
    connect(m_lockProcess, &QProcess::readyRead, this, [this] {
        const auto str = QString::fromLocal8Bit(m_lockProcess->readLine());
        if (str == QStringLiteral("Unlocked\n")) {
            lockProcessRequestedUnlock();
        }
    });
    connect(m_lockProcess, finishedSignal, this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        qCDebug(KSCREENLOCKER) << "Greeter process exitted with status:" << exitStatus << "exit code:" << exitCode;

        if (s_lockProcessRequestedExit) {
            qCDebug(KSCREENLOCKER) << "Lock process had requested exit; we already did the unlock process elsewhere";
            return;
        }

        const bool regularExit = !exitCode && exitStatus == QProcess::NormalExit;
        if (regularExit || s_graceTimeKill || s_logindExit) {
            // unlock process finished successfully - we can remove the lock grab

            if (regularExit) {
                qCDebug(KSCREENLOCKER) << "Unlocking now on regular exit.";
            } else if (s_graceTimeKill) {
                qCDebug(KSCREENLOCKER) << "Unlocking anyway due to grace time.";
            } else {
                Q_ASSERT(s_logindExit);
                qCDebug(KSCREENLOCKER) << "Unlocking anyway since forced through logind.";
            }

            s_graceTimeKill = false;
            s_logindExit = false;
            doUnlock();
            return;
        }

        qCWarning(KSCREENLOCKER) << "Greeter process exit unregular. Restarting lock.";

        m_greeterCrashedCounter++;
        if (m_greeterCrashedCounter < 4) {
            // Perhaps it crashed due to a graphics driver issue, force software rendering now
            qCDebug(KSCREENLOCKER, "Trying to lock again with software rendering (%d/4).", m_greeterCrashedCounter);
            setForceSoftwareRendering(true);
            startLockProcess(EstablishLock::Immediate);
        } else if (m_lockWindow) {
            qCWarning(KSCREENLOCKER) << "Everything else failed. Need to put Greeter in emergency mode.";
            m_lockWindow->emergencyShow();
        } else {
            qCCritical(KSCREENLOCKER) << "Greeter process exitted and we could in no way recover from that!";
        }
    });
    connect(m_lockProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            qCDebug(KSCREENLOCKER) << "Greeter Process  failed to start. Trying to directly unlock again.";
            doUnlock();
            m_waylandServer->stop();
            qCCritical(KSCREENLOCKER) << "Greeter Process not available";
        } else {
            qCWarning(KSCREENLOCKER) << "Greeter Process encountered an unhandled error:" << error << ". Detailed error:" << m_lockProcess->errorString();
        }
    });
    m_lockedTimer.invalidate();
    m_graceTimer->setSingleShot(true);
    connect(m_graceTimer, &QTimer::timeout, this, &KSldApp::endGraceTime);
    // create our D-Bus interface
    new Interface(this);

    // connect to logind
    m_logind = new LogindIntegration(this);
    connect(m_logind, &LogindIntegration::requestLock, this, [this]() {
        qCDebug(KSCREENLOCKER) << "Lock requested by logind";
        lock(EstablishLock::Immediate);
    });
    connect(m_logind, &LogindIntegration::requestUnlock, this, [this]() {
        if (lockState() == Locked || lockState() == AcquiringLock) {
            if (m_lockProcess->state() != QProcess::NotRunning) {
                s_logindExit = true;
                m_lockProcess->terminate();
            } else {
                doUnlock();
            }
        }
    });
    connect(m_logind, &LogindIntegration::prepareForSleep, this, [this](bool goingToSleep) {
        if (!goingToSleep) {
            // not interested in doing anything on wakeup
            qCDebug(KSCREENLOCKER) << "Prepare for sleep called, but not going to sleep";
            return;
        }
        if (KScreenSaverSettings::lockOnResume()) {
            qCDebug(KSCREENLOCKER) << "Prepare for sleep called, locking now";
            lock(EstablishLock::Immediate);
        }
    });
    connect(m_logind, &LogindIntegration::inhibited, this, [this]() {
        // if we are already locked, we immediately remove the inhibition lock
        if (m_lockState == KSldApp::Locked) {
            m_logind->uninhibit();
        }
    });
    connect(m_logind, &LogindIntegration::connectedChanged, this, [this]() {
        if (!m_logind->isConnected()) {
            return;
        }
        if (m_lockState == ScreenLocker::KSldApp::Unlocked && KScreenSaverSettings::lockOnResume()) {
            m_logind->inhibit();
        }
        if (m_logind->isLocked()) {
            qCDebug(KSCREENLOCKER) << "LogindIntegration::connectedChanged signal received, locking now";
            lock(EstablishLock::Immediate);
        }
    });
    connect(this, &KSldApp::locked, this, [this]() {
        m_logind->uninhibit();
        m_logind->setLocked(true);
        if (m_lockGrace > 0 && m_inGraceTime) {
            m_graceTimer->start(m_lockGrace);
        }
    });
    connect(this, &KSldApp::unlocked, this, [this]() {
        m_logind->setLocked(false);
        if (KScreenSaverSettings::lockOnResume()) {
            m_logind->inhibit();
        }
    });

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
                                          this,
                                          SLOT(solidSuspend()));

    configure();

    if (m_logind->isLocked()) {
        qCDebug(KSCREENLOCKER) << "Logind is locked, locking now";
        lock(EstablishLock::Immediate);
    }
    if (KScreenSaverSettings::lockOnStart()) {
        qCDebug(KSCREENLOCKER) << "Lock on start enabled, locking now";
        lock(EstablishLock::Immediate);
    }
}

void KSldApp::lockProcessRequestedUnlock()
{
    qCDebug(KSCREENLOCKER) << "Lock process requested unlock";

    s_graceTimeKill = false;
    s_logindExit = false;
    s_lockProcessRequestedExit = true;
    doUnlock();
}

void KSldApp::configure()
{
    qCDebug(KSCREENLOCKER) << "Configuring";

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
        m_idleId = KIdleTime::instance()->addIdleTimeout(timeout * 1000 * 60);
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
    m_requirePassword = KScreenSaverSettings::requirePassword();
}

void KSldApp::lock(EstablishLock establishLock, int attemptCount)
{
    qCDebug(KSCREENLOCKER) << "lock called with establishLock:" << establishLockToString(establishLock) << "attemptCount:" << attemptCount;

    if (lockState() != Unlocked) {
        // already locked or acquiring lock, no need to lock again
        // but make sure it's really locked
        endGraceTime();
        if (establishLock == EstablishLock::Immediate) {
            // signal the greeter to switch to immediateLock mode
            kill(m_lockProcess->processId(), SIGUSR1);
        }
        return;
    }

    if (attemptCount == 0) {
        Q_EMIT aboutToLock();
    }

    if (!establishGrab()) {
        if (attemptCount < 3) {
            qCWarning(KSCREENLOCKER) << "Could not establish screen lock. Trying again in 10ms";
            QTimer::singleShot(10, this, [this, establishLock, attemptCount]() {
                lock(establishLock, attemptCount + 1);
            });
        } else {
            qCCritical(KSCREENLOCKER) << "Could not establish screen lock";
        }
        return;
    }

    KNotification::event(QStringLiteral("locked"), i18n("Screen locked"), QPixmap(), KNotification::CloseOnTimeout, QStringLiteral("ksmserver"));

    s_lockProcessRequestedExit = false;

    // blank the screen
    showLockWindow();

    m_lockState = AcquiringLock;

    setForceSoftwareRendering(false);
    // start unlock screen process
    startLockProcess(establishLock);
    Q_EMIT lockStateChanged();
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
    XServerGrabber()
    {
        xcb_grab_server(X11Info::connection());
    }
    ~XServerGrabber()
    {
        xcb_ungrab_server(X11Info::connection());
        xcb_flush(X11Info::connection());
    }
};

bool KSldApp::establishGrab()
{
    qCDebug(KSCREENLOCKER) << "Establishing grab";

    if (m_isWayland) {
        return true;
    }
    if (!m_isX11) {
        return true;
    }
    XSync(X11Info::display(), False);
    XServerGrabber serverGrabber;
    if (!grabKeyboard()) {
        return false;
    }

    if (!grabMouse()) {
        XUngrabKeyboard(X11Info::display(), CurrentTime);
        XFlush(X11Info::display());
        return false;
    }

#if X11_Xinput_FOUND
    if (m_hasXInput2) {
        // get all devices
        Display *dpy = X11Info::display();
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
            uchar bitmask[] = {0, 0};
            mask.deviceid = masters[i].deviceid;
            mask.mask = bitmask;
            mask.mask_len = sizeof(bitmask);
            XISetMask(bitmask, XI_ButtonPress);
            XISetMask(bitmask, XI_ButtonRelease);
            XISetMask(bitmask, XI_Motion);
            XISetMask(bitmask, XI_Enter);
            XISetMask(bitmask, XI_Leave);
            const int result = XIGrabDevice(dpy,
                                            masters[i].deviceid,
                                            X11Info::appRootWindow(),
                                            XCB_TIME_CURRENT_TIME,
                                            XCB_CURSOR_NONE,
                                            XIGrabModeAsync,
                                            XIGrabModeAsync,
                                            True,
                                            &mask);
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
            xcb_connection_t *c = X11Info::connection();
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
    qCDebug(KSCREENLOCKER) << "Grabbing keyboard";

    int rv = XGrabKeyboard(X11Info::display(), X11Info::appRootWindow(), True, GrabModeAsync, GrabModeAsync, CurrentTime);

    return (rv == GrabSuccess);
}

static bool grabMouse()
{
    qCDebug(KSCREENLOCKER) << "Grabbing mouse";

#define GRABEVENTS ButtonPressMask | ButtonReleaseMask | PointerMotionMask | EnterWindowMask | LeaveWindowMask
    int rv = XGrabPointer(X11Info::display(), X11Info::appRootWindow(), True, GRABEVENTS, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
#undef GRABEVENTS

    return (rv == GrabSuccess);
}

void KSldApp::doUnlock()
{
    qCDebug(KSCREENLOCKER) << "Unlocking now.";

    if (m_isX11) {
        xcb_connection_t *c = X11Info::connection();
        xcb_ungrab_keyboard(c, XCB_CURRENT_TIME);
        xcb_ungrab_pointer(c, XCB_CURRENT_TIME);
        xcb_flush(c);
#if X11_Xinput_FOUND
        if (m_hasXInput2) {
            // get all devices
            Display *dpy = X11Info::display();
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
    KNotification::event(QStringLiteral("unlocked"), i18n("Screen unlocked"), QPixmap(), KNotification::CloseOnTimeout, QStringLiteral("ksmserver"));
    Q_EMIT unlocked();
    Q_EMIT lockStateChanged();
}

bool KSldApp::isFdoPowerInhibited() const
{
    qCDebug(KSCREENLOCKER) << "Checking if power management is inhibited";
    return m_powerManagementInhibition->isInhibited();
}

void KSldApp::setWaylandFd(int fd)
{
    qCDebug(KSCREENLOCKER) << "Setting Wayland fd:" << fd;
    m_waylandFd = fd;
}

void KSldApp::startLockProcess(EstablishLock establishLock)
{
    qCDebug(KSCREENLOCKER) << "Starting lock process with establishLock:" << establishLockToString(establishLock);

    Q_EMIT aboutToStartGreeter();

    QProcessEnvironment env = m_greeterEnv;

    if (m_isWayland && m_waylandFd >= 0) {
        int socket = dup(m_waylandFd);
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
    if (m_lockGrace == -1 || !m_requirePassword) {
        args << QStringLiteral("--nolock");
    }
    if (m_forceSoftwareRendering) {
        env.insert(s_qtQuickBackend, QStringLiteral("software"));
    }

    // start the Wayland server
    int fd = m_waylandServer->start();
    if (fd == -1) {
        qCWarning(KSCREENLOCKER) << "Could not start the Wayland server.";
        Q_EMIT m_lockProcess->errorOccurred(QProcess::FailedToStart);
        return;
    }

    args << QStringLiteral("--ksldfd");
    args << QString::number(fd);

    auto greeterPath = KLibexec::path(QStringLiteral(KSCREENLOCKER_GREET_BIN_REL));
    if (!QFile::exists(greeterPath)) {
        greeterPath = QStringLiteral(KSCREENLOCKER_GREET_BIN_ABS);
    }

    qCDebug(KSCREENLOCKER) << "Starting greeter process. Args:" << args;

    m_lockProcess->setProcessEnvironment(env);
    m_lockProcess->start(greeterPath, args);
    close(fd);
}

void KSldApp::userActivity()
{
    if (m_lockState != Locked) {
        return;
    }

    qCDebug(KSCREENLOCKER) << "User activity detected. lockState:" << m_lockState;

    if (isGraceTime() || !m_requirePassword) {
        unlock();
    }
    if (m_lockWindow) {
        m_lockWindow->userActivity();
    }
}

void KSldApp::showLockWindow()
{
    qCDebug(KSCREENLOCKER) << "Showing lock window";

    if (!m_lockWindow) {
        qCDebug(KSCREENLOCKER) << "Creating lock window";

        if (m_isX11) {
            m_lockWindow = new X11Locker(this);

            connect(
                m_lockWindow,
                &AbstractLocker::userActivity,
                m_lockWindow,
                [this]() {
                    if (isGraceTime() || !m_requirePassword) {
                        unlock();
                    }
                },
                Qt::QueuedConnection);
        }

        if (m_isWayland) {
            m_lockWindow = new WaylandLocker(this);
        }
        if (!m_lockWindow) {
            return;
        }
        m_lockWindow->setGlobalAccel(m_globalAccel);

        connect(m_lockWindow, &AbstractLocker::lockWindowShown, this, &KSldApp::lockScreenShown);

        connect(m_waylandServer, &WaylandServer::x11WindowAdded, m_lockWindow, &AbstractLocker::addAllowedWindow);
    }
    m_lockWindow->showLockWindow();
    if (m_isX11) {
        XSync(X11Info::display(), False);
    }
}

void KSldApp::hideLockWindow()
{
    if (!m_lockWindow) {
        qCDebug(KSCREENLOCKER) << "No lock window to hide";
        return;
    }

    qCDebug(KSCREENLOCKER) << "Hiding lock window";
    m_lockWindow->hideLockWindow();
}

uint KSldApp::activeTime() const
{
    qCDebug(KSCREENLOCKER) << "Active time requested";

    if (m_lockedTimer.isValid()) {
        return m_lockedTimer.elapsed();
    }
    return 0;
}

bool KSldApp::isGraceTime() const
{
    qCDebug(KSCREENLOCKER) << "Checking if in grace time: " << m_inGraceTime;
    return m_inGraceTime;
}

void KSldApp::endGraceTime()
{
    qCDebug(KSCREENLOCKER) << "Ending grace time";
    m_graceTimer->stop();
    m_inGraceTime = false;
}

void KSldApp::unlock()
{
    qCDebug(KSCREENLOCKER) << "Unlock requested";

    if (isGraceTime() || !m_requirePassword) {
        s_graceTimeKill = true;
        m_lockProcess->terminate();
    }
}

void KSldApp::inhibit()
{
    qCDebug(KSCREENLOCKER) << "Inhibit requested";
    ++m_inhibitCounter;
}

void KSldApp::uninhibit()
{
    qCDebug(KSCREENLOCKER) << "Uninhibit requested";
    --m_inhibitCounter;
}

void KSldApp::solidSuspend()
{
    qCDebug(KSCREENLOCKER) << "Solid suspend called";

    // ignore in case that we use logind
    if (m_logind && m_logind->isConnected()) {
        return;
    }
    if (KScreenSaverSettings::lockOnResume()) {
        qCDebug(KSCREENLOCKER) << "Solid suspend called, locking now";
        lock(EstablishLock::Immediate);
    }
}

void KSldApp::lockScreenShown()
{
    qCDebug(KSCREENLOCKER) << "lockScreenShown(): m_lockState:" << m_lockState;

    if (m_lockState == Locked) {
        return;
    }
    m_lockState = Locked;
    m_lockedTimer.restart();
    Q_EMIT locked();
    Q_EMIT lockStateChanged();
}

void KSldApp::setGreeterEnvironment(const QProcessEnvironment &env)
{
    qCDebug(KSCREENLOCKER) << "Setting greeter environment";

    m_greeterEnv = env;
    if (m_isWayland) {
        m_greeterEnv.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
    }
}

bool KSldApp::event(QEvent *event)
{
    qCDebug(KSCREENLOCKER) << "Event received";

    if (event->type() == QEvent::KeyPress && m_globalAccel) {
        if (m_globalAccel->keyEvent(static_cast<QKeyEvent *>(event))) {
            event->setAccepted(true);
        }
    }
    return false;
}

} // namespace

#include "moc_ksldapp.cpp"
