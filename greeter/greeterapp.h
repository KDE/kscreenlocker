/*
SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <KPackage/PackageStructure>
#include <PlasmaQuick/SharedQmlEngine>
#include <QGuiApplication>
#include <QUrl>

namespace KWayland
{
namespace Client
{
class ConnectionThread;
class Registry;
}
}

namespace PlasmaQuick
{
class QuickViewSharedEngine;
}

class Authenticator;
class LogindIntegration;

class PamAuthenticators;

namespace ScreenLocker
{
class WallpaperIntegration;
class ShellIntegration;

class UnlockApp : public QGuiApplication
{
    Q_OBJECT
public:
    explicit UnlockApp(int &argc, char **argv);
    ~UnlockApp() override;

    void initialViewSetup();

    void setTesting(bool enable);
    void setShell(const QString &shell);
    void setImmediateLock(bool immediateLock);
    void lockImmediately();
    void setGraceTime(int milliseconds);
    void setNoLock(bool noLock);
    void updateCanSuspend();
    void updateCanHibernate();

public Q_SLOTS:
    void osdProgress(const QString &icon, int percent, const int maximumPercent, const QString &additionalText);
    void osdText(const QString &icon, const QString &additionalText);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private Q_SLOTS:
    void handleScreen(QScreen *screen);
    PlasmaQuick::QuickViewSharedEngine *createViewForScreen(QScreen *screen);
    void getFocus();
    void markViewsAsVisible(PlasmaQuick::QuickViewSharedEngine *view);
    void graceLockEnded();

private:
    void initialize();
    PlasmaQuick::SharedQmlEngine *loadWallpaperPlugin(PlasmaQuick::QuickViewSharedEngine *view);
    void setWallpaperItemProperties(PlasmaQuick::SharedQmlEngine *wallpaperObject, PlasmaQuick::QuickViewSharedEngine *view);
    void screenGeometryChanged(QScreen *screen, const QRect &geo);
    QWindow *getActiveScreen();

    QString m_packageName;
    QUrl m_mainQmlPath;
    QList<PlasmaQuick::QuickViewSharedEngine *> m_views;
    QTimer *m_delayedLockTimer;
    bool m_testing;
    bool m_immediateLock;
    PamAuthenticators *m_authenticators;
    int m_graceTime;
    bool m_noLock;

    QString m_userName, m_userImage;

    KPackage::Package m_wallpaperPackage;
    ShellIntegration *m_shellIntegration;
    LogindIntegration *m_logindIntegration;
};
} // namespace
