/********************************************************************
 This file is part of the KDE project.

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
#include <KLocalizedString>
#include <KQuickAddons/QtQuickSettings>

#include <QDateTime>
#include <QCommandLineParser>
#include <QSessionManager>

#include <iostream>

#include <signal.h>

#include "greeterapp.h"

#include <config-kscreenlocker.h>
#if HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif
#if HAVE_SYS_PROCCTL_H
#include <unistd.h>
#include <sys/procctl.h>
#endif
#if HAVE_SECCOMP
#include "seccomp_filter.h"
#endif

static void signalHandler(int signum)
{
    ScreenLocker::UnlockApp *instance = qobject_cast<ScreenLocker::UnlockApp *>(QCoreApplication::instance());

    if (!instance)
        return;

    switch(signum)
    {
      case SIGTERM:
        // exit gracefully to not leave behind screensaver processes (bug#224200)
        // return exit code 1 to indicate that a valid password was not entered,
        // to prevent circumventing the password input by sending a SIGTERM
        instance->exit(1);
        break;
      case SIGUSR1:
        instance->lockImmediately();
        break;
    }
}

int main(int argc, char* argv[])
{
    // disable ptrace on the greeter
#if HAVE_PR_SET_DUMPABLE
    prctl(PR_SET_DUMPABLE, 0);
#endif
#if HAVE_PROC_TRACE_CTL
    int mode = PROC_TRACE_CTL_DISABLE;
    procctl(P_PID, getpid(), PROC_TRACE_CTL, &mode);
#endif

    KLocalizedString::setApplicationDomain("kscreenlocker_greet");

    // explicitly disable input methods as it makes it impossible to unlock, see BUG 306932
    // but explicitly set on screen keyboard such as maliit is allowed
    if (!qEnvironmentVariableIsSet("QT_IM_MODULE") || (qEnvironmentVariableIsSet("QT_IM_MODULE") && qgetenv("QT_IM_MODULE") != QByteArrayLiteral("maliit"))) {
        qputenv("QT_IM_MODULE", QByteArrayLiteral("qtvirtualkeyboard"));
    }
    ScreenLocker::UnlockApp app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    QCoreApplication::setApplicationName(QStringLiteral("kscreenlocker_greet"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("kde.org"));

    KQuickAddons::QtQuickSettings::init();

    // disable session management for the greeter
    auto disableSessionManagement = [](QSessionManager &sm) {
        sm.setRestartHint(QSessionManager::RestartNever);
    };
    QObject::connect(&app, &QGuiApplication::commitDataRequest, disableSessionManagement);
    QObject::connect(&app, &QGuiApplication::saveStateRequest, disableSessionManagement);

    QCommandLineParser parser;
    parser.setApplicationDescription(i18n("Greeter for the KDE Plasma Workspaces Screen locker"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption testingOption(QStringLiteral("testing"),
                                     i18n("Starts the greeter in testing mode"));

    QCommandLineOption themeOption(QStringLiteral("theme"),
                                     i18n("Starts the greeter with the selected theme (only in Testing mode)"),
                                     QStringLiteral("theme"),
                                     QStringLiteral(""));

    QCommandLineOption immediateLockOption(QStringLiteral("immediateLock"),
                                           i18n("Lock immediately, ignoring any grace time etc."));
    QCommandLineOption graceTimeOption(QStringLiteral("graceTime"),
                                       i18n("Delay till the lock user interface gets shown in milliseconds."),
                                       QStringLiteral("milliseconds"),
                                       QStringLiteral("0"));
    QCommandLineOption nolockOption(QStringLiteral("nolock"),
                                    i18n("Don't show any lock user interface."));
    QCommandLineOption switchUserOption(QStringLiteral("switchuser"),
                                    i18n("Default to the switch user UI."));

    QCommandLineOption waylandFdOption(QStringLiteral("ksldfd"),
                                       i18n("File descriptor for connecting to ksld."),
                                       QStringLiteral("fd"));

    parser.addOption(testingOption);
    parser.addOption(themeOption);
    parser.addOption(immediateLockOption);
    parser.addOption(graceTimeOption);
    parser.addOption(nolockOption);
    parser.addOption(switchUserOption);
    parser.addOption(waylandFdOption);
    parser.process(app);

    if (parser.isSet(testingOption)) {
        app.setTesting(true);
        app.setImmediateLock(true);

        //parse theme option
        const QString theme = parser.value(themeOption);
        if (!theme.isEmpty()) {
            app.setTheme(theme);
        }

        // allow ptrace if testing is enabled
#if HAVE_PR_SET_DUMPABLE
        prctl(PR_SET_DUMPABLE, 1);
#endif
#if HAVE_PROC_TRACE_CTL
        int mode = PROC_TRACE_CTL_ENABLE;
        procctl(P_PID, getpid(), PROC_TRACE_CTL, &mode);
#endif
    } else {
        app.setImmediateLock(parser.isSet(immediateLockOption));
    }
    app.setNoLock(parser.isSet(nolockOption));

    bool ok = false;
    int graceTime = parser.value(graceTimeOption).toInt(&ok);
    if (ok) {
        app.setGraceTime(graceTime);
    }

    if (parser.isSet(switchUserOption)) {
        app.setDefaultToSwitchUser(true);
    }

    if (parser.isSet(waylandFdOption)) {
        ok = false;
        const int fd = parser.value(waylandFdOption).toInt(&ok);
        if (ok) {
            app.setKsldSocket(fd);
        }
    }

    // init the sandbox
#if HAVE_SECCOMP
    if (app.supportsSeccomp()) {
        ScreenLocker::SecComp::init();
    }
#endif

    app.desktopResized();

    // This allow ksmserver to know when the applicaion has actually finished setting itself up.
    // Crucial for blocking until it is ready, ensuring locking happens before sleep, e.g.
    std::cout << "Locked at " << QDateTime::currentDateTime().toTime_t() << std::endl;

    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGUSR1, &sa, nullptr);
    return app.exec();
}
