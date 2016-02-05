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

#include <QtTest/QtTest>
#include "../ksldapp.h"
#include <KWayland/Server/compositor_interface.h>
#include <KWayland/Server/datadevicemanager_interface.h>
#include <KWayland/Server/display.h>
#include <KWayland/Server/output_interface.h>
#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/shell_interface.h>
#include <KWayland/Server/plasmashell_interface.h>

using namespace KWayland::Server;

class NoScreensTest : public QObject
{
    Q_OBJECT
Q_SIGNALS:
    void surfaceShown();

private Q_SLOTS:
    void initTestCase();
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
};

void NoScreensTest::initTestCase()
{
    m_display = new Display(this);
    m_display->start(Display::StartMode::ConnectClientsOnly);
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
    ScreenLocker::KSldApp::self();
    ScreenLocker::KSldApp::self()->setWaylandDisplay(m_display);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("QT_QPA_PLATFORM", "wayland");
    ScreenLocker::KSldApp::self()->setGreeterEnvironment(env);
    ScreenLocker::KSldApp::self()->initialize();
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
    unlock();
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
    QSignalSpy lockedStateSpy(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged);
    QVERIFY(lockedStateSpy.isValid());
    QSignalSpy greeterConnectionSpy(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::greeterClientConnectionChanged);
    QVERIFY(greeterConnectionSpy.isValid());

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
    QCOMPARE(greeterConnectionSpy.count(), 1);
    QCOMPARE(ScreenLocker::KSldApp::self()->lockState(), ScreenLocker::KSldApp::AcquiringLock);

    QVERIFY(surfaceShownSpy.wait());
    QCOMPARE(ScreenLocker::KSldApp::self()->lockState(), ScreenLocker::KSldApp::Locked);

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
