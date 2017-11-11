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

#if !defined(SYS_open) || !defined(SYS_openat) || !defined(SYS_creat) || !defined(SYS_truncate) \
 || !defined(SYS_rename) || !defined(SYS_renameat) || !defined(SYS_renameat2) || !defined(SYS_mkdir) \
 || !defined(SYS_mkdirat) || !defined(SYS_rmdir) || !defined(SYS_link) || !defined(SYS_linkat) \
 || !defined(SYS_unlink) || !defined(SYS_unlinkat) || !defined(SYS_symlink) || !defined(SYS_symlinkat) \
 || !defined(SYS_mknod) || !defined(SYS_mknodat) || !defined(SYS_chmod) || !defined(SYS_fchmod) \
 || !defined(SYS_fchmodat)
#error "Some systemcalls are not available, though seccomp is available."
#endif

class SeccompTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testCreateFile();
    void testOpenFile();
    void testOpenFilePosix();
    void testWriteFilePosix();
    void testRename();
    void testTruncate();
    void testMkdir();
    void testRmdir();
    void testLinkUnlink();
    void testSymlink();
    void testMknod();
    void testChmod();
    void testStartProcess();
    void testNetworkAccess_data();
    void testNetworkAccess();
private:
    QLatin1Literal existingFile = QLatin1Literal(KCHECKPASS_BIN);
    QLatin1Literal createPath = QLatin1Literal(KCHECKPASS_BIN ".new");
    QLatin1Literal existingDir = QLatin1Literal(KCHECKPASS_BIN ".newDir");
    const char* existingFileChar;
    const char* createPathChar;
};

void SeccompTest::initTestCase()
{
    existingFileChar = existingFile.data();
    createPathChar = createPath.data();
    QDir::current().mkdir(existingDir);
    ScreenLocker::SecComp::init();
}

void SeccompTest::testCreateFile()
{
    QTemporaryFile file;
    QVERIFY(!file.open());
}

void SeccompTest::testOpenFile()
{
    if (KWin::GLPlatform::instance()->driver() == KWin::Driver_NVidia) {
        QSKIP("Write protection not supported on NVIDIA");
    }
    QFile file(existingFile);
    QVERIFY(file.exists());
    QVERIFY(!file.open(QIODevice::WriteOnly));
    QVERIFY(!file.open(QIODevice::ReadWrite));
    QVERIFY(file.open(QIODevice::ReadOnly));
}

void SeccompTest::testOpenFilePosix()
{
    QVERIFY(open(createPathChar, O_RDONLY | O_CREAT, 0) == -1 && errno == EPERM);
    QVERIFY(openat(AT_FDCWD, createPathChar, O_RDONLY | O_CREAT, 0) == -1 && errno == EPERM);
    QVERIFY(syscall(SYS_open, createPathChar, O_RDONLY | O_CREAT, 0) == -1 && errno == EPERM);
    QVERIFY(syscall(SYS_openat, AT_FDCWD, createPathChar, O_RDONLY | O_CREAT, 0) == -1 && errno == EPERM);
    QVERIFY(syscall(SYS_creat, createPathChar, S_IRWXU) == -1 && errno == EPERM);
}

void SeccompTest::testWriteFilePosix()
{
    if (KWin::GLPlatform::instance()->driver() == KWin::Driver_NVidia) {
        QSKIP("Write protection not supported on NVIDIA");
    }
    QVERIFY(open(existingFileChar, O_RDWR) == -1 && errno == EPERM);
    QVERIFY(openat(AT_FDCWD, existingFileChar, O_RDWR) == -1 && errno == EPERM);
    QVERIFY(syscall(SYS_open, existingFileChar, O_RDWR) == -1 && errno == EPERM);
    QVERIFY(syscall(SYS_openat, AT_FDCWD, existingFileChar, O_RDWR) == -1 && errno == EPERM);
}

void SeccompTest::testTruncate()
{
    QVERIFY(!QFile::resize(existingFile, 0));

    QVERIFY(syscall(SYS_truncate, existingFileChar, 0) == -1 && errno == EPERM);
}

void SeccompTest::testRename()
{
    QVERIFY(!QFile::rename(existingFile, createPath));

    QVERIFY(syscall(SYS_rename, existingFileChar, createPathChar) == -1 && errno == EPERM);
    QVERIFY(syscall(SYS_renameat, AT_FDCWD, existingFileChar, AT_FDCWD, createPathChar) == -1 && errno == EPERM);
    QVERIFY(syscall(SYS_renameat2, AT_FDCWD, existingFileChar, AT_FDCWD, createPathChar, 0) == -1 && errno == EPERM);
}

void SeccompTest::testMkdir()
{
    QVERIFY(!QDir::current().mkdir(createPath));

    QVERIFY(syscall(SYS_mkdir, createPathChar, S_IRWXU) == -1 && errno == EPERM);
    QVERIFY(syscall(SYS_mkdirat, AT_FDCWD, createPathChar, S_IRWXU) == -1 && errno == EPERM);
}

void SeccompTest::testRmdir()
{
    QVERIFY(!QDir::current().remove(existingDir));

    QVERIFY(syscall(SYS_rmdir, existingDir.data()) == -1 && errno == EPERM);
}

void SeccompTest::testLinkUnlink()
{
    QVERIFY(!QFile::remove(existingFile));

    QVERIFY(syscall(SYS_link, existingFileChar, createPathChar) == -1 && errno == EPERM);
    QVERIFY(syscall(SYS_linkat, AT_FDCWD, existingFileChar, AT_FDCWD, createPathChar, 0) == -1 && errno == EPERM);
    QVERIFY(syscall(SYS_unlink, existingFileChar) == -1 && errno == EPERM);
    QVERIFY(syscall(SYS_unlinkat, AT_FDCWD, existingFileChar, 0) == -1 && errno == EPERM);
}

void SeccompTest::testSymlink()
{
    QVERIFY(!QFile::link(existingFile, createPath));

    QVERIFY(syscall(SYS_symlink, existingFileChar, createPathChar) == -1 && errno == EPERM);
    QVERIFY(syscall(SYS_symlinkat, existingFileChar, AT_FDCWD, createPathChar) == -1 && errno == EPERM);
}

void SeccompTest::testMknod()
{
    QVERIFY(syscall(SYS_mknod, createPathChar, S_IRWXU, S_IFIFO) == -1 && errno == EPERM);
    QVERIFY(syscall(SYS_mknodat, AT_FDCWD, createPathChar, S_IRWXU, S_IFIFO) == -1 && errno == EPERM);
}

void SeccompTest::testChmod()
{
    QVERIFY(!QFile::setPermissions(existingFileChar, QFileDevice::ExeOwner));
    QVERIFY(syscall(SYS_chmod, existingFileChar, S_IRWXU) == -1 && errno == EPERM);
    QFile file(existingFile);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(syscall(SYS_fchmod, file.handle(), S_IRWXU) == -1 && errno == EPERM);
    file.close();
    QVERIFY(syscall(SYS_fchmodat, AT_FDCWD, existingFileChar, S_IRWXU, 0) == -1 && errno == EPERM);
}

void SeccompTest::testStartProcess()
{
    // QProcess fails already using pipe
    QProcess p;
    p.start(existingFile);
    QVERIFY(!p.waitForStarted());
    QCOMPARE(p.error(), QProcess::ProcessError::FailedToStart);

    // using glibc fork succeeds as it uses clone
    // we don't forbid clone as it's needed to start a new thread
    // so only test that exec fails
    QCOMPARE(execl(existingFileChar, "fakekcheckpass", (char*)0), -1);
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
