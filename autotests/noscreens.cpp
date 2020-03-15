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

#include <QtTest>
#include "../ksldapp.h"
#include <KWayland/Server/clientconnection.h>
#include <KWayland/Server/compositor_interface.h>
#include <KWayland/Server/datadevicemanager_interface.h>
#include <KWayland/Server/display.h>
#include <KWayland/Server/output_interface.h>
#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/shell_interface.h>
#include <KWayland/Server/plasmashell_interface.h>

#include <sys/socket.h>

using namespace KWayland::Server;

class NoScreensTest : public QObject
{
    Q_OBJECT
Q_SIGNALS:
    void surfaceShown();

private Q_SLOTS:
    void init();
    void cleanup();
    void testAllQScreensClose();

private:
    void unlock();
    Display *m_display = nullptr;
    CompositorInterface *m_compositor = nullptr;
    ShellInterface *m_shell = nullptr;
    SeatInterface *m_seat = nullptr;
    PlasmaShellInterface *m_plasma = nullptr;
    SurfaceInterface *m_surface = nullptr;
    ClientConnection *m_clientConnection = nullptr;
};

void NoScreensTest::init()
{
    m_display = new Display;
    m_display->setAutomaticSocketNaming(true);
    m_display->start(Display::StartMode::ConnectClientsOnly);
    QVERIFY(m_display->isRunning());

    m_compositor = m_display->createCompositor(m_display);
    m_compositor->create();
    m_shell = m_display->createShell(m_display);
    m_shell->create();
    m_display->createShm();
    m_seat = m_display->createSeat(m_display);
    m_seat->setHasKeyboard(true);
    m_seat->setHasPointer(true);
    m_seat->setHasTouch(true);
    m_seat->create();
    m_display->createDataDeviceManager(m_display)->create();
    m_plasma = m_display->createPlasmaShell(m_display);
    m_plasma->create();

    QCoreApplication::instance()->setProperty("platformName", QStringLiteral("wayland"));
    auto *app = ScreenLocker::KSldApp::self();

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
    app->setGreeterEnvironment(env);
    app->initialize();

    connect(app, &ScreenLocker::KSldApp::aboutToLock,
            this, [this, app] {
                int sx[2];
                QVERIFY(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx) >= 0);

                m_clientConnection = m_display->createClient(sx[0]);
                QVERIFY(m_clientConnection);
                connect(m_clientConnection, &ClientConnection::disconnected, this, [this] {
                    // The API is currently ill-defined about the ownership of a client connection
                    // when calling createClient server-side.
                    m_clientConnection = nullptr;
                });
                app->setWaylandFd(sx[1]);
                connect(m_seat, &SeatInterface::timestampChanged,
                        app, &ScreenLocker::KSldApp::userActivity);

    });
    connect(app, &ScreenLocker::KSldApp::unlocked,
            this, [this, app] {
                if (m_clientConnection) {
                    delete m_clientConnection;
                    m_clientConnection = nullptr;
                }
                disconnect(m_seat, &SeatInterface::timestampChanged,
                           app, &ScreenLocker::KSldApp::userActivity);
    });

    connect(m_shell, &ShellInterface::surfaceCreated, this,
        [this] (ShellSurfaceInterface *surface) {
            m_surface = surface->surface();
            connect(surface->surface(), &SurfaceInterface::damaged, this,
                [this, surface] {
                    emit surfaceShown();
                    ScreenLocker::KSldApp::self()->lockScreenShown();
                    surface->surface()->frameRendered(0);
                    surface->client()->flush();
                }
            );
        }
    );
}

void NoScreensTest::cleanup()
{
    delete m_display;
}

void NoScreensTest::unlock()
{
    using namespace ScreenLocker;
    const auto children = KSldApp::self()->children();
    for (auto it = children.begin(); it != children.end(); ++it) {
        if (qstrcmp((*it)->metaObject()->className(), "LogindIntegration") != 0) {
            continue;
        }
        QMetaObject::invokeMethod(*it, "requestUnlock");
        break;
    }
}

void NoScreensTest::testAllQScreensClose()
{
    if (!QFile::exists(QStringLiteral("/dev/dri/card0"))) {
        QSKIP("Needs a DRI device for the greeter");
    }
    QSignalSpy lockedStateSpy(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged);
    QVERIFY(lockedStateSpy.isValid());

    QSignalSpy surfaceShownSpy(this, &NoScreensTest::surfaceShown);
    QVERIFY(surfaceShownSpy.isValid());

    // create an Output
    OutputInterface *output = m_display->createOutput(m_display);
    output->setGlobalPosition(QPoint(0, 0));
    output->setPhysicalSize(QSize(1280, 1024) / 3.8);
    output->addMode(QSize(1280, 1024));
    output->create();

    ScreenLocker::KSldApp::self()->lock(ScreenLocker::EstablishLock::Immediate);
    QCOMPARE(lockedStateSpy.count(), 1);
    QCOMPARE(ScreenLocker::KSldApp::self()->lockState(), ScreenLocker::KSldApp::AcquiringLock);

    QVERIFY(surfaceShownSpy.wait());
    QCOMPARE(ScreenLocker::KSldApp::self()->lockState(), ScreenLocker::KSldApp::Locked);
    QVERIFY(m_clientConnection);

    // give some time to show the window
    QTest::qWait(5000);

    // deleting the output should destroy the surface
    QVERIFY(m_surface);
    QSignalSpy surfaceDestroyedSpy(m_surface, &QObject::destroyed);
    QVERIFY(surfaceDestroyedSpy.isValid());

    output->deleteLater();
    QVERIFY(surfaceDestroyedSpy.wait());
    QCOMPARE(ScreenLocker::KSldApp::self()->lockState(), ScreenLocker::KSldApp::Locked);

    QVERIFY(!lockedStateSpy.wait());
    QCOMPARE(ScreenLocker::KSldApp::self()->lockState(), ScreenLocker::KSldApp::Locked);

    unlock();
    QTRY_COMPARE(ScreenLocker::KSldApp::self()->lockState(), ScreenLocker::KSldApp::Unlocked);
}

QTEST_GUILESS_MAIN(NoScreensTest)
#include "noscreens.moc"
