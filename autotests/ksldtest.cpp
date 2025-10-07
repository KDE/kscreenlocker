/*
SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
// own
#include "../ksldapp.h"
#include "kscreensaversettings.h"
// KDE Frameworks
#include <KIdleTime>
#include <KWindowSystem>
// Qt
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>
#include <private/qtx11extras_p.h>
// xcb
#include <xcb/xcb.h>
#include <xcb/xtest.h>
// POSIX
#include <sys/socket.h> // socketpair

class KSldTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void cleanup();
    void testEstablishGrab();
    void testActivateOnTimeout();
    void testGraceTimeUnlocking();
    void testLockAfterInhibit();
    void testRestartIdlePeriodAfterInhibit();
};

void KSldTest::initTestCase()
{
    QCoreApplication::setAttribute(Qt::AA_ForceRasterWidgets);
    // change to the build bin dir
    QDir::setCurrent(QCoreApplication::applicationDirPath());
    QStandardPaths::setTestModeEnabled(true);
}

void KSldTest::cleanup()
{
    KScreenSaverSettings::self()->setDefaults();
    KScreenSaverSettings::self()->save();
}

void KSldTest::testEstablishGrab()
{
    if (!KWindowSystem::isPlatformX11()) {
        QSKIP("keyboard and pointer grabbing by other processes is only possible on X11");
    }

    ScreenLocker::KSldApp ksld;
    ksld.initialize();
    QVERIFY(ksld.establishGrab());
    // grab is established, trying again should succeed as well
    QVERIFY(ksld.establishGrab());

    // let's ungrab
    ksld.doUnlock();

    // to get the grab to fail we need another X client
    // we start another process to perform our grab
    QProcess keyboardGrabber;
    keyboardGrabber.start(QStringLiteral("./keyboardGrabber"), QStringList());
    QVERIFY(keyboardGrabber.waitForStarted());

    // let's add some delay to be sure that keyboardGrabber has it's X stuff done
    QTest::qWait(std::chrono::milliseconds{100});

    // now grabbing should fail
    QVERIFY(!ksld.establishGrab());

    // let's terminate again
    keyboardGrabber.terminate();
    QVERIFY(keyboardGrabber.waitForFinished());

    // now grabbing should succeed again
    QVERIFY(ksld.establishGrab());

    ksld.doUnlock();

    // now the same with pointer
    QProcess pointerGrabber;
    pointerGrabber.start(QStringLiteral("./pointerGrabber"), QStringList());
    QVERIFY(pointerGrabber.waitForStarted());

    // let's add some delay to be sure that pointerGrabber has it's X stuff done
    QTest::qWait(std::chrono::milliseconds{100});

    // now grabbing should fail
    QVERIFY(!ksld.establishGrab());

    // let's terminate again
    pointerGrabber.terminate();
    QVERIFY(pointerGrabber.waitForFinished());

    // now grabbing should succeed again
    QVERIFY(ksld.establishGrab());
}

void KSldTest::testActivateOnTimeout()
{
    // this time verifies that the screen gets locked on idle timeout, requires the system to be idle
    ScreenLocker::KSldApp ksld;
    ksld.initialize();

    if (KWindowSystem::isPlatformWayland()) {
        // without having a wayland fd, the app will follow the X11 code path
        int sx[2];
        QCOMPARE(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx), 0);
        ksld.setWaylandFd(sx[1]);
    }

    // we need to modify the idle timeout of KSLD, it's in minutes we cannot wait that long
    if (ksld.idleId() != 0) {
        // remove old Idle id
        KIdleTime::instance()->removeIdleTimeout(ksld.idleId());
    }
    ksld.setIdleId(KIdleTime::instance()->addIdleTimeout(std::chrono::seconds{5}));

    QSignalSpy lockStateChangedSpy(&ksld, &ScreenLocker::KSldApp::lockStateChanged);
    QVERIFY(lockStateChangedSpy.isValid());

    // let's wait the double of the idle timeout
    QVERIFY2(lockStateChangedSpy.wait(std::chrono::seconds{10}),
             "If running the test locally, make sure you're not moving the mouse or otherwise interrupting this test's idle check.");
    QCOMPARE(ksld.lockState(), ScreenLocker::KSldApp::AcquiringLock);

    // let's simulate unlock to get rid of started greeter process
    const auto children = ksld.children();
    for (auto it = children.begin(); it != children.end(); ++it) {
        if (qstrcmp((*it)->metaObject()->className(), "LogindIntegration") != 0) {
            continue;
        }
        QMetaObject::invokeMethod(*it, "requestUnlock");
        break;
    }
    QVERIFY(lockStateChangedSpy.wait());

    KIdleTime::instance()->removeIdleTimeout(ksld.idleId());
}

void KSldTest::testGraceTimeUnlocking()
{
    if (!KWindowSystem::isPlatformX11()) {
        QSKIP("XCB fake input won't work on Wayland");
    }

    // this time verifies that the screen gets unlocked during grace time by simulated user activity
    ScreenLocker::KSldApp ksld(this);
    ksld.initialize();

    // we need to modify the idle timeout of KSLD, it's in minutes we cannot wait that long
    if (ksld.idleId() != 0) {
        // remove old Idle id
        KIdleTime::instance()->removeIdleTimeout(ksld.idleId());
    }
    ksld.setIdleId(KIdleTime::instance()->addIdleTimeout(std::chrono::seconds{5}));
    // infinite
    ksld.setGraceTime(-1);

    QSignalSpy lockedSpy(&ksld, &ScreenLocker::KSldApp::locked);
    QVERIFY(lockedSpy.isValid());
    QSignalSpy unlockedSpy(&ksld, &ScreenLocker::KSldApp::unlocked);
    QVERIFY(unlockedSpy.isValid());

    // let's wait quite some time to give the greeter a chance to come up
    QVERIFY(lockedSpy.wait(std::chrono::seconds{30}));
    QCOMPARE(ksld.lockState(), ScreenLocker::KSldApp::Locked);

    // let's simulate unlock by faking input
    const QPoint cursorPos = QCursor::pos();
    xcb_test_fake_input(QX11Info::connection(), XCB_MOTION_NOTIFY, 0, XCB_TIME_CURRENT_TIME, XCB_WINDOW_NONE, cursorPos.x() + 1, cursorPos.y() + 1, 0);
    xcb_flush(QX11Info::connection());
    QVERIFY(unlockedSpy.wait());

    // now let's test again without grace time
    ksld.setGraceTime(0);
    QVERIFY(lockedSpy.wait(std::chrono::seconds{30}));
    QCOMPARE(ksld.lockState(), ScreenLocker::KSldApp::Locked);

    xcb_test_fake_input(QX11Info::connection(), XCB_MOTION_NOTIFY, 0, XCB_TIME_CURRENT_TIME, XCB_WINDOW_NONE, cursorPos.x(), cursorPos.y(), 0);
    xcb_flush(QX11Info::connection());
    QVERIFY(!unlockedSpy.wait());

    // and unlock
    const auto children = ksld.children();
    for (auto it = children.begin(); it != children.end(); ++it) {
        if (qstrcmp((*it)->metaObject()->className(), "LogindIntegration") != 0) {
            continue;
        }
        QMetaObject::invokeMethod(*it, "requestUnlock");
        break;
    }
    QVERIFY(unlockedSpy.wait());
}

void KSldTest::testLockAfterInhibit()
{
    KScreenSaverSettings::setTimeout(1 / 60.0); // 1 second
    KScreenSaverSettings::self()->save();

    ScreenLocker::KSldApp ksld;
    if (KWindowSystem::isPlatformWayland()) {
        // without having a wayland fd, the app will follow the X11 code path
        int sx[2];
        QCOMPARE(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx), 0);
        ksld.setWaylandFd(sx[1]);
    }

    QSignalSpy lockStateChangedSpy(&ksld, &ScreenLocker::KSldApp::lockStateChanged);
    QVERIFY(lockStateChangedSpy.isValid());

    QSignalSpy idleSpy(KIdleTime::instance(), &KIdleTime::timeoutReached);
    QVERIFY(idleSpy.isValid());

    ksld.inhibit();
    ksld.initialize();

    QCOMPARE(ksld.lockState(), ScreenLocker::KSldApp::Unlocked);
    QCOMPARE(lockStateChangedSpy.count(), 0);
    QVERIFY(!lockStateChangedSpy.wait(std::chrono::seconds{2}));

    ksld.uninhibit();

    QVERIFY(idleSpy.wait());
    QCOMPARE_NE(ksld.lockState(), ScreenLocker::KSldApp::Unlocked);

    const auto children = ksld.children();
    for (auto it = children.begin(); it != children.end(); ++it) {
        if (qstrcmp((*it)->metaObject()->className(), "LogindIntegration") != 0) {
            continue;
        }
        QMetaObject::invokeMethod(*it, "requestUnlock");
        break;
    }
    QTRY_COMPARE(ksld.lockState(), ScreenLocker::KSldApp::Unlocked);
}

void KSldTest::testRestartIdlePeriodAfterInhibit()
{
    KScreenSaverSettings::setTimeout(2 / 60.0); // 2 seconds
    KScreenSaverSettings::self()->save();

    ScreenLocker::KSldApp ksld;
    if (KWindowSystem::isPlatformWayland()) {
        // without having a wayland fd, the app will follow the X11 code path
        int sx[2];
        QCOMPARE(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx), 0);
        ksld.setWaylandFd(sx[1]);
    }

    QSignalSpy lockStateChangedSpy(&ksld, &ScreenLocker::KSldApp::lockStateChanged);
    QVERIFY(lockStateChangedSpy.isValid());

    ksld.inhibit();
    ksld.initialize();

    QVERIFY(!lockStateChangedSpy.wait(std::chrono::milliseconds(1800)));


    QElapsedTimer unlockTimer;
    unlockTimer.start();
    ksld.uninhibit();

    QVERIFY(lockStateChangedSpy.wait(std::chrono::seconds(5)));

    const auto timerTestThreshold = std::chrono::seconds(1);
    // check it's around 2 seconds, but with some leniency because we're backed by a coarse timer
    QVERIFY(std::chrono::abs(unlockTimer.durationElapsed() - std::chrono::seconds(2)) < timerTestThreshold);
    QCOMPARE_NE(ksld.lockState(), ScreenLocker::KSldApp::Unlocked);

    const auto children = ksld.children();
    for (auto it = children.begin(); it != children.end(); ++it) {
        if (qstrcmp((*it)->metaObject()->className(), "LogindIntegration") != 0) {
            continue;
        }
        QMetaObject::invokeMethod(*it, "requestUnlock");
        break;
    }
    QTRY_COMPARE(ksld.lockState(), ScreenLocker::KSldApp::Unlocked);
}

QTEST_MAIN(KSldTest)
#include "ksldtest.moc"
