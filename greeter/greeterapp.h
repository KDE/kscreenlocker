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

struct org_kde_ksld;

class PamAuthenticator;

namespace ScreenLocker
{
class WallpaperIntegration;
class LnFIntegration;

class UnlockApp : public QGuiApplication
{
    Q_OBJECT
public:
    explicit UnlockApp(int &argc, char **argv);
    ~UnlockApp() override;

    void initialViewSetup();

    void setTesting(bool enable);
    void setTheme(const QString &theme);
    void setImmediateLock(bool immediateLock);
    void lockImmediately();
    void setGraceTime(int milliseconds);
    void setNoLock(bool noLock);
    void setKsldSocket(int socket);
    void setDefaultToSwitchUser(bool defaultToSwitchUser);

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
    void resetRequestIgnore();
    void suspendToRam();
    void suspendToDisk();
    void getFocus();
    void markViewsAsVisible(PlasmaQuick::QuickViewSharedEngine *view);
    void setLockedPropertyOnViews();

private:
    void initialize();
    void shareEvent(QEvent *e, PlasmaQuick::QuickViewSharedEngine *from);
    PlasmaQuick::SharedQmlEngine *loadWallpaperPlugin(PlasmaQuick::QuickViewSharedEngine *view);
    void setWallpaperItemProperties(PlasmaQuick::SharedQmlEngine *wallpaperObject, PlasmaQuick::QuickViewSharedEngine *view);
    void screenGeometryChanged(QScreen *screen, const QRect &geo);
    QWindow *getActiveScreen();

    QString m_packageName;
    QUrl m_mainQmlPath;
    QList<PlasmaQuick::QuickViewSharedEngine *> m_views;
    QTimer *m_resetRequestIgnoreTimer;
    QTimer *m_delayedLockTimer;
    KPackage::Package m_package;
    bool m_testing;
    bool m_ignoreRequests;
    bool m_immediateLock;
    bool m_runtimeInitialized;
    PamAuthenticator *m_authenticator;
    int m_graceTime;
    bool m_noLock;
    bool m_defaultToSwitchUser;

    bool m_canSuspend = false;
    bool m_canHibernate = false;
    QString m_userName, m_userImage;

    KWayland::Client::ConnectionThread *m_ksldConnection = nullptr;
    KWayland::Client::Registry *m_ksldRegistry = nullptr;
    QThread *m_ksldConnectionThread = nullptr;
    org_kde_ksld *m_ksldInterface = nullptr;

    WallpaperIntegration *m_wallpaperIntegration;
    LnFIntegration *m_lnfIntegration;
};
} // namespace
