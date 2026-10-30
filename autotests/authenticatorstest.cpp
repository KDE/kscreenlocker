/*
SPDX-FileCopyrightText: 2026 Rishon Jonathan R

SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QStandardPaths>
#include <QTest>

#include <KConfigGroup>
#include <KSharedConfig>

#include "../greeter/pamauthenticators.h"

using namespace Qt::StringLiterals;

class AuthenticatorsTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testRememberedAuthenticator_data();
    void testRememberedAuthenticator();
};

void AuthenticatorsTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);

    // Fingerprint is switched on, Face is not. Written before anything reads the authenticator model.
    KConfig config(u"kscreenlockerrc"_s);
    auto authenticators = config.group(u"Authenticators"_s);
    authenticators.writeEntry("Fingerprint", true);
    authenticators.writeEntry("Face", false);
    QVERIFY(config.sync());
}

void AuthenticatorsTest::testRememberedAuthenticator_data()
{
    QTest::addColumn<QString>("remembered");
    QTest::addColumn<PamAuthenticators::Authenticator>("expected");

    QTest::newRow("nothing remembered") << QString() << PamAuthenticators::Authenticator::Regular;
    QTest::newRow("regular") << u"Regular"_s << PamAuthenticators::Authenticator::Regular;
    QTest::newRow("enabled") << u"Fingerprint"_s << PamAuthenticators::Authenticator::Fingerprint;
    // A method used once and switched off since must not be selected; it used to crash the greeter.
    QTest::newRow("disabled") << u"Face"_s << PamAuthenticators::Authenticator::Regular;
    QTest::newRow("bogus") << u"NoSuchAuthenticator"_s << PamAuthenticators::Authenticator::Regular;
}

void AuthenticatorsTest::testRememberedAuthenticator()
{
    QFETCH(QString, remembered);
    QFETCH(PamAuthenticators::Authenticator, expected);

    auto config = KSharedConfig::openStateConfig(u"kscreenlockerrc"_s);
    auto greeter = config->group(u"Greeter"_s);
    if (remembered.isEmpty()) {
        greeter.deleteEntry("Authenticator");
    } else {
        greeter.writeEntry("Authenticator", remembered);
    }
    QVERIFY(config->sync());

    PamAuthenticators authenticators(u"test_user"_s);
    QCOMPARE(authenticators.property("authenticator").value<PamAuthenticators::Authenticator>(), expected);
}

QTEST_GUILESS_MAIN(AuthenticatorsTest)
#include "authenticatorstest.moc"
