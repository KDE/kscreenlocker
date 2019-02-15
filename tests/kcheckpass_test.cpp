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
#include "../greeter/authenticator.h"
#include <QGuiApplication>
#include <QCommandLineParser>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCommandLineParser parser;
    QCommandLineOption delayedOption(QStringLiteral("delayed"),
                                     QStringLiteral("KCheckpass is created at startup, the authentication is delayed"));
    QCommandLineOption directOption(QStringLiteral("direct"),
                                    QStringLiteral("A new KCheckpass gets created when trying to authenticate"));
    parser.addOption(directOption);
    parser.addOption(delayedOption);
    parser.addHelpOption();
    parser.process(app);
    AuthenticationMode mode = AuthenticationMode::Delayed;
    if (parser.isSet(directOption)) {
        mode = AuthenticationMode::Direct;
    }
    if (parser.isSet(directOption) && parser.isSet(delayedOption)) {
        parser.showHelp(0);
    }
    Authenticator authenticator(mode);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("authenticator"), &authenticator);
    engine.load(QUrl::fromLocalFile(QStringLiteral(QML_FILE)));
    return app.exec();
}
