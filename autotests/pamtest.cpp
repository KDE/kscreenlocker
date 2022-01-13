/********************************************************************
 KSld - the KDE Screenlocker Daemon
 This file is part of the KDE project.

Copyright (C) 2022 David Edmundson <davidedmundson@kde.org>

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

#include <QSignalSpy>
#include <QtTest>

#include "../greeter/pamauthenticator.h"

// This test runs under the expectation that
// we are run with LD_PRELOAD=libpam_wrapper.so pamTest

class PamTest : public QObject
{
    Q_OBJECT
public:
    PamTest();
private Q_SLOTS:
    void testLogin();
};

PamTest::PamTest()
{
    if (!qgetenv("LD_PRELOAD").contains("libpam_wrapper.so")) {
        qFatal("This test must be run with pam_wrapper. See ctest");
    }

    qputenv("PAM_WRAPPER", "1");
    qputenv("PAM_WRAPPER_DEBUGLEVEL", "2"); // DEBUG level
    qputenv("PAM_WRAPPER_SERVICE_DIR", QFINDTESTDATA("data").toUtf8());
    qputenv("PAM_MATRIX_PASSWD", QFINDTESTDATA("data/test_db").toUtf8());
}

void PamTest::testLogin()
{
    PamAuthenticator auth("test_service", "test_user");
    QSignalSpy promptSpy(&auth, &PamAuthenticator::prompt);
    QSignalSpy promptForSecretSpy(&auth, &PamAuthenticator::promptForSecret);
    QSignalSpy succeededSpy(&auth, &PamAuthenticator::succeeded);
    QSignalSpy failedSpy(&auth, &PamAuthenticator::failed);

    // invalid password
    auth.tryUnlock();

    QVERIFY(promptForSecretSpy.wait());
    auth.respond("not_my_password");
    QVERIFY(failedSpy.wait());

    QVERIFY(promptSpy.count() == 0);
    QVERIFY(succeededSpy.count() == 0);

    // try again, with the right password
    auth.tryUnlock();
    QVERIFY(promptForSecretSpy.wait());
    auth.respond("my_password");
    QVERIFY(succeededSpy.wait());
}

QTEST_MAIN(PamTest)
#include "pamtest.moc"
