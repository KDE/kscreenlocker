/********************************************************************
 KSld - the KDE Screenlocker Daemon
 This file is part of the KDE project.

Copyright (C) 2017 Martin Gräßlin <mgraesslin@kde.org>

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
#include <config-kscreenlocker.h>
#include "../seccomp_filter.h"
#include "../kwinglplatform.h"

#include <KWindowSystem>

#include <QtTest/QtTest>
#include <QTemporaryFile>
#include <QProcess>

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

#ifdef __linux__
#include <sys/syscall.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

class SeccompTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testCreateFile();
    void testOpenFile();
    void testOpenFilePosix();
    void testWriteFilePosix();
    void testStartProcess();
    void testNetworkAccess_data();
    void testNetworkAccess();
};

void SeccompTest::initTestCase()
{
    ScreenLocker::SecComp::init();
}

void SeccompTest::testCreateFile()
{
    if (KWin::GLPlatform::instance()->isSoftwareEmulation() && KWindowSystem::isPlatformWayland()) {
        QSKIP("File creation protection not supported with Mesa on Wayland");
    }

    QTemporaryFile file;
    QVERIFY(!file.open());
}

void SeccompTest::testOpenFile()
{
    if (KWin::GLPlatform::instance()->driver() == KWin::Driver_NVidia) {
        QSKIP("Write protection not supported on NVIDIA");
    }
    QFile file(QStringLiteral(KCHECKPASS_BIN));
    QVERIFY(file.exists());
    QVERIFY(!file.open(QIODevice::WriteOnly));
    QVERIFY(!file.open(QIODevice::ReadWrite));
    QVERIFY(file.open(QIODevice::ReadOnly));
}

void SeccompTest::testOpenFilePosix()
{
    QVERIFY(open("/dev/null", O_RDONLY | O_CREAT, 0) == -1 && errno == EPERM);
    QVERIFY(openat(AT_FDCWD, "/dev/null", O_RDONLY | O_CREAT, 0) == -1 && errno == EPERM);
#ifdef SYS_open
    QVERIFY(syscall(SYS_open, "/dev/null", O_RDONLY | O_CREAT, 0) == -1 && errno == EPERM);
#endif
#ifdef SYS_openat
    QVERIFY(syscall(SYS_openat, AT_FDCWD, "/dev/null", O_RDONLY | O_CREAT, 0) == -1 && errno == EPERM);
#endif
}

void SeccompTest::testWriteFilePosix()
{
    if (KWin::GLPlatform::instance()->driver() == KWin::Driver_NVidia) {
        QSKIP("Write protection not supported on NVIDIA");
    }
    QVERIFY(open("/dev/null", O_RDWR) == -1 && errno == EPERM);
    QVERIFY(openat(AT_FDCWD, "/dev/null", O_RDWR) == -1 && errno == EPERM);
#ifdef SYS_open
    QVERIFY(syscall(SYS_open, "/dev/null", O_RDWR) == -1 && errno == EPERM);
#endif
#ifdef SYS_openat
    QVERIFY(syscall(SYS_openat, AT_FDCWD, "/dev/null", O_RDWR) == -1 && errno == EPERM);
#endif
}

void SeccompTest::testStartProcess()
{
    // QProcess fails already using pipe
    QProcess p;
    p.start(QStringLiteral(KCHECKPASS_BIN));
    QVERIFY(!p.waitForStarted());
    QCOMPARE(p.error(), QProcess::ProcessError::FailedToStart);

    // using glibc fork succeeds as it uses clone
    // we don't forbid clone as it's needed to start a new thread
    // so only test that exec fails
    QCOMPARE(execl(KCHECKPASS_BIN, "fakekcheckpass", (char*)0), -1);
    QCOMPARE(errno, EPERM);
}

void SeccompTest::testNetworkAccess_data()
{
    QTest::addColumn<QString>("url");

    // TODO: maybe resolve the IP addresses prior to installing seccomp?
    QTest::newRow("domain") << QStringLiteral("https://www.kde.org");
    QTest::newRow("ip4") << QStringLiteral("http://91.189.93.5");
    // phabricator.kde.org
    QTest::newRow("ip6") << QStringLiteral("http://[2a01:4f8:171:2687::4]/");
}

void SeccompTest::testNetworkAccess()
{
    QNetworkAccessManager manager;
    QFETCH(QString, url);
    QNetworkRequest request(url);
    auto reply = manager.get(request);
    QVERIFY(reply);
    QSignalSpy finishedSpy(reply, &QNetworkReply::finished);
    QVERIFY(finishedSpy.isValid());
    QVERIFY(finishedSpy.wait());
    QVERIFY(reply->error() != QNetworkReply::NoError);
}


QTEST_MAIN(SeccompTest)
#include "seccomp_test.moc"
