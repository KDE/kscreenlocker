/*
    SPDX-FileCopyrightText: 1999 Martin R. Jones <mjones@kde.org>
    SPDX-FileCopyrightText: 2003 Oswald Buddenhagen <ossi@kde.org>
    SPDX-FileCopyrightText: 2008 Chani Armitage <chanika@gmail.com>
    SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "ksldapp.h"
#include "emergencywindow.h"
#include "globalaccel.h"
#include "interface.h"
#include "kscreensaversettings.h"
#include "logind.h"
#include "powermanagement_inhibition.h"

#include "kscreenlocker_logging.h"
#include <config-kscreenlocker.h>

#include "config-unix.h"
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
}

KSldApp::~KSldApp()
{
}

void KSldApp::cleanUp()
{
    qCDebug(KSCREENLOCKER) << "Cleaning up";

    if (m_lockProcess && m_lockProcess->state() != QProcess::NotRunning) {
        qCDebug(KSCREENLOCKER) << "Terminating lock process";
        m_lockProcess->terminate();
    }
    delete m_lockProcess;
}

static bool s_graceTimeKill = false;
static bool s_logindExit = false;
static bool s_lockProcessRequestedExit = false;

void KSldApp::initialize()
{
    qCDebug(KSCREENLOCKER) << "Initializing";

    m_requirePassword = KScreenSaverSettings::requirePassword();

    // Global keys
    if (KAuthorized::authorizeAction(QStringLiteral("lock_screen"))) {
        qCDebug(KSCREENLOCKER) << "Configuring Lock Action";
        QAction *a = new QAction(this);
        a->setObjectName(QStringLiteral("Lock Session"));
        // The following properties are set manually because we do not depend
        // on KXmlGui and therefore cannot use KActionCollection.
        a->setProperty("componentDisplayName", i18nc("Name of a category in System Settings' Shortcuts KCM; match it exactly", "Session Management"));
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

    connect(m_powerManagementInhibition, &PowerManagementInhibition::inhibitedChanged, this, &KSldApp::updateIdleTimeout);

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
        if (isInhibited()) {
            // either we got a direct inhibit request thru our outdated o.f.Screensaver iface ...
            // ... or the newer one at o.f.PowerManagement.Inhibit
            // there is at least one process blocking the auto lock of screen locker
            return;
        }
        if (m_lockGrace) { // short-circuit if grace time is zero
            m_inGraceTime = true;
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

        if (s_graceTimeKill) {
            qCDebug(KSCREENLOCKER) << "screen locker killed due to grace time.";
            s_graceTimeKill = false;
            s_logindExit = false;
            return;
        }
        const bool regularExit = !exitCode && exitStatus == QProcess::NormalExit;
        if (regularExit || s_logindExit) {
            // unlock process finished successfully - we can remove the lock grab

            if (regularExit) {
                qCDebug(KSCREENLOCKER) << "Unlocking now on regular exit.";
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
        } else if (!m_emergencyLockWindow) {
            qCWarning(KSCREENLOCKER) << "Everything else failed. Need to put Greeter in emergency mode.";
            m_emergencyLockWindow = std::make_unique<EmergencyWindow>();
        }
    });
    connect(m_lockProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            qCDebug(KSCREENLOCKER) << "Greeter Process  failed to start. Trying to directly unlock again.";
            doUnlock();
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
        if (lockState() == Locked) {
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
    connect(m_logind, &LogindIntegration::connectedChanged, this, [this]() {
        if (!m_logind->isConnected()) {
            return;
        }
        if (m_logind->isLocked()) {
            qCDebug(KSCREENLOCKER) << "LogindIntegration::connectedChanged signal received, locking now";
            lock(EstablishLock::Immediate);
        }
    });

    m_globalAccel = new GlobalAccel(this);

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
    updateIdleTimeout();
    if (KScreenSaverSettings::lock()) {
        // lockGrace is stored in seconds
        m_lockGrace = KScreenSaverSettings::lockGrace() * 1000;
    } else {
        m_lockGrace = -1;
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

    KNotification::event(QStringLiteral("locked"), i18n("Screen locked"), QPixmap(), KNotification::CloseOnTimeout, QStringLiteral("ksmserver"));

    s_lockProcessRequestedExit = false;

    m_lockState = Locked;
    m_lockedTimer.restart();

    setForceSoftwareRendering(false);
    // start unlock screen process
    startLockProcess(establishLock);

    Q_EMIT lockStateChanged();
    Q_EMIT locked();
    m_globalAccel->prepare();
    m_logind->setLocked(true);
    if (m_lockGrace > 0 && m_inGraceTime) {
        m_graceTimer->start(m_lockGrace);
    }
}

void KSldApp::doUnlock()
{
    qCDebug(KSCREENLOCKER) << "Unlocking now.";
    m_emergencyLockWindow.reset();
    m_lockState = Unlocked;
    m_lockedTimer.invalidate();
    m_greeterCrashedCounter = 0;
    endGraceTime();
    KNotification::event(QStringLiteral("unlocked"), i18n("Screen unlocked"), QPixmap(), KNotification::CloseOnTimeout, QStringLiteral("ksmserver"));
    Q_EMIT unlocked();
    m_logind->setLocked(false);
    m_globalAccel->release();
    Q_EMIT lockStateChanged();
}

bool KSldApp::isInhibited() const
{
    return m_inhibitCounter || m_powerManagementInhibition->isInhibited();
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

    if (m_waylandFd >= 0) {
        int socket = dup(m_waylandFd);
        if (socket >= 0) {
            env.insert(QStringLiteral("WAYLAND_SOCKET"), QString::number(socket));
        }
    }

    QStringList args;
    if (establishLock == EstablishLock::Immediate) {
        args << QStringLiteral("--immediateLock");
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

    auto greeterPath = KLibexec::path(QStringLiteral(KSCREENLOCKER_GREET_BIN_REL));
    if (!QFile::exists(greeterPath)) {
        greeterPath = QStringLiteral(KSCREENLOCKER_GREET_BIN_ABS);
    }

    qCDebug(KSCREENLOCKER) << "Starting greeter process. Args:" << args;

    m_lockProcess->setProcessEnvironment(env);
    m_lockProcess->start(greeterPath, args);
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
        // the process might take some time to exit; don't wait for that to happen
        doUnlock();
        s_graceTimeKill = true;
        m_lockProcess->terminate();
    }
}

void KSldApp::inhibit()
{
    qCDebug(KSCREENLOCKER) << "Inhibit requested";
    ++m_inhibitCounter;
    updateIdleTimeout();
}

void KSldApp::uninhibit()
{
    qCDebug(KSCREENLOCKER) << "Uninhibit requested";
    --m_inhibitCounter;
    updateIdleTimeout();
}

void KSldApp::updateIdleTimeout()
{
    const bool hasTimeout = m_idleId;
    const bool timeoutConfigured = KScreenSaverSettings::autolock() && KScreenSaverSettings::timeout() > 0;
    const bool inhibited = isInhibited();
    if (hasTimeout && inhibited) {
        KIdleTime::instance()->removeIdleTimeout(m_idleId);
        m_idleId = 0;
    } else if (!hasTimeout && !inhibited && timeoutConfigured) {
        std::chrono::duration<double, std::chrono::minutes::period> timeout(KScreenSaverSettings::timeout());
        m_idleId = KIdleTime::instance()->addIdleTimeout(std::chrono::duration_cast<std::chrono::milliseconds>(timeout));
    }
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

void KSldApp::setGreeterEnvironment(const QProcessEnvironment &env)
{
    qCDebug(KSCREENLOCKER) << "Setting greeter environment";

    m_greeterEnv = env;
    m_greeterEnv.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
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
