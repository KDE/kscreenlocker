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

#include <QtTest>
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
    QLatin1String existingFile = QLatin1String(KCHECKPASS_BIN);
    QLatin1String createPath = QLatin1String(KCHECKPASS_BIN ".new");
    QLatin1String existingDir = QLatin1String(KCHECKPASS_BIN ".newDir");
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
#ifdef SYS_open
    QVERIFY(syscall(SYS_open, createPathChar, O_RDONLY | O_CREAT, 0) == -1 && errno == EPERM);
#endif
#ifdef SYS_openat
    QVERIFY(syscall(SYS_openat, AT_FDCWD, createPathChar, O_RDONLY | O_CREAT, 0) == -1 && errno == EPERM);
#endif
#ifdef SYS_creat
    QVERIFY(syscall(SYS_creat, createPathChar, S_IRWXU) == -1 && errno == EPERM);
#endif
}

void SeccompTest::testWriteFilePosix()
{
    if (KWin::GLPlatform::instance()->driver() == KWin::Driver_NVidia) {
        QSKIP("Write protection not supported on NVIDIA");
    }
    QVERIFY(open(existingFileChar, O_RDWR) == -1 && errno == EPERM);
    QVERIFY(openat(AT_FDCWD, existingFileChar, O_RDWR) == -1 && errno == EPERM);
#ifdef SYS_open
    QVERIFY(syscall(SYS_open, existingFileChar, O_RDWR) == -1 && errno == EPERM);
#endif
#ifdef SYS_openat
    QVERIFY(syscall(SYS_openat, AT_FDCWD, existingFileChar, O_RDWR) == -1 && errno == EPERM);
#endif
}

void SeccompTest::testTruncate()
{
    QVERIFY(!QFile::resize(existingFile, 0));

#ifdef SYS_truncate
    QVERIFY(syscall(SYS_truncate, existingFileChar, 0) == -1 && errno == EPERM);
#endif
}

void SeccompTest::testRename()
{
    QVERIFY(!QFile::rename(existingFile, createPath));

#ifdef SYS_rename
    QVERIFY(syscall(SYS_rename, existingFileChar, createPathChar) == -1 && errno == EPERM);
#endif
#ifdef SYS_renameat
    QVERIFY(syscall(SYS_renameat, AT_FDCWD, existingFileChar, AT_FDCWD, createPathChar) == -1 && errno == EPERM);
#endif
#ifdef SYS_renameat2
    QVERIFY(syscall(SYS_renameat2, AT_FDCWD, existingFileChar, AT_FDCWD, createPathChar, 0) == -1 && errno == EPERM);
#endif
}

void SeccompTest::testMkdir()
{
    QVERIFY(!QDir::current().mkdir(createPath));

#ifdef SYS_mkdir
    QVERIFY(syscall(SYS_mkdir, createPathChar, S_IRWXU) == -1 && errno == EPERM);
#endif
#ifdef SYS_mkdirat
    QVERIFY(syscall(SYS_mkdirat, AT_FDCWD, createPathChar, S_IRWXU) == -1 && errno == EPERM);
#endif
}

void SeccompTest::testRmdir()
{
    QVERIFY(!QDir::current().remove(existingDir));

#ifdef SYS_rmdir
    QVERIFY(syscall(SYS_rmdir, existingDir.data()) == -1 && errno == EPERM);
#endif
}

void SeccompTest::testLinkUnlink()
{
    QVERIFY(!QFile::remove(existingFile));

#ifdef SYS_link
    QVERIFY(syscall(SYS_link, existingFileChar, createPathChar) == -1 && errno == EPERM);
#endif
#ifdef SYS_linkat
    QVERIFY(syscall(SYS_linkat, AT_FDCWD, existingFileChar, AT_FDCWD, createPathChar, 0) == -1 && errno == EPERM);
#endif
#ifdef SYS_unlink
    QVERIFY(syscall(SYS_unlink, existingFileChar) == -1 && errno == EPERM);
#endif
#ifdef SYS_unlinkat
    QVERIFY(syscall(SYS_unlinkat, AT_FDCWD, existingFileChar, 0) == -1 && errno == EPERM);
#endif
}

void SeccompTest::testSymlink()
{
    QVERIFY(!QFile::link(existingFile, createPath));

#ifdef SYS_symlink
    QVERIFY(syscall(SYS_symlink, existingFileChar, createPathChar) == -1 && errno == EPERM);
#endif
#ifdef SYS_symlinkat
    QVERIFY(syscall(SYS_symlinkat, existingFileChar, AT_FDCWD, createPathChar) == -1 && errno == EPERM);
#endif
}

void SeccompTest::testMknod()
{
#ifdef SYS_mknod
    QVERIFY(syscall(SYS_mknod, createPathChar, S_IRWXU, S_IFIFO) == -1 && errno == EPERM);
#endif
#ifdef SYS_mknodat
    QVERIFY(syscall(SYS_mknodat, AT_FDCWD, createPathChar, S_IRWXU, S_IFIFO) == -1 && errno == EPERM);
#endif
}

void SeccompTest::testChmod()
{
    QVERIFY(!QFile::setPermissions(QLatin1String(existingFileChar), QFileDevice::ExeOwner));
#ifdef SYS_chmod
    QVERIFY(syscall(SYS_chmod, existingFileChar, S_IRWXU) == -1 && errno == EPERM);
#endif
#ifdef SYS_fchmod
    QFile file(existingFile);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(syscall(SYS_fchmod, file.handle(), S_IRWXU) == -1 && errno == EPERM);
    file.close();
#endif
#ifdef SYS_fchmodat
    QVERIFY(syscall(SYS_fchmodat, AT_FDCWD, existingFileChar, S_IRWXU, 0) == -1 && errno == EPERM);
#endif
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
    QCOMPARE(execl(existingFileChar, "fakekcheckpass", (char*)nullptr), -1);
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
    auto reply = manager.get(QNetworkRequest(QUrl(url)));
    QVERIFY(reply);
    QSignalSpy finishedSpy(reply, &QNetworkReply::finished);
    QVERIFY(finishedSpy.isValid());
    QVERIFY(finishedSpy.wait());
    QVERIFY(reply->error() != QNetworkReply::NoError);
}


QTEST_MAIN(SeccompTest)
#include "seccomp_test.moc"
