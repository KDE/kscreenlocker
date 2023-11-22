/*
SPDX-FileCopyrightText: 2017 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "../greeter/pamauthenticator.h"
#include "../greeter/pamauthenticators.h"

#include <KUser>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.process(app);

    auto interactive = std::make_unique<PamAuthenticator>("kde", KUser().loginName());
    std::vector<std::unique_ptr<PamAuthenticator>> noninteractive;
    noninteractive.push_back(std::make_unique<PamAuthenticator>("kde-fingerprint", KUser().loginName(), PamAuthenticator::Fingerprint));
    noninteractive.push_back(std::make_unique<PamAuthenticator>("kde-smartcard", KUser().loginName(), PamAuthenticator::Smartcard));
    auto authenticator = new PamAuthenticators(std::move(interactive), std::move(noninteractive), &app);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("authenticator"), authenticator);
    engine.load(QUrl::fromLocalFile(QStringLiteral(QML_FILE)));
    return app.exec();
}
