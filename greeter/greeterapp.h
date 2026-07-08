/*
SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <KPackage/PackageStructure>
#include <PlasmaQuick/SharedQmlEngine>
#include <QGuiApplication>
#include <QQuickView>
#include <QUrl>

namespace KWayland
{
namespace Client
{
class ConnectionThread;
class Registry;
}
}

class QQuickItem;

class KConfigPropertyMap;

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

Q_SIGNALS:
    void viewEntered(QQuickView *view);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private Q_SLOTS:
    void handleScreen(QScreen *screen);
    QQuickView *createViewForScreen(QScreen *screen);
    void getFocus();
    void markViewsAsVisible(QQuickItem *view);
    void graceLockEnded();

private:
    void initialize();
    QQuickItem *loadWallpaperPlugin(QObject *parent, int width, int height, KConfigPropertyMap *config);
    void screenGeometryChanged(QScreen *screen, const QRect &geo);
    QWindow *getActiveScreen();

    QString m_packageName;
    QUrl m_mainQmlPath;
    QList<QQuickView *> m_views;
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
    std::shared_ptr<QQmlEngine> m_engine;
};

class LockscreenState : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    static LockscreenState *create(QQmlEngine *, QJSEngine *);
    void init(UnlockApp *app);
    UnlockApp *m_parentApp;

private:
    explicit LockscreenState(QObject *parent = nullptr);
};

class ActiveScreenMonitor : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QQuickWindow *window MEMBER m_window REQUIRED FINAL)
    Q_PROPERTY(LockscreenState *lockscreenState READ lockscreenState WRITE setLockscreenState REQUIRED FINAL)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged FINAL)

public:
    explicit ActiveScreenMonitor(QObject *root = nullptr);
    [[nodiscard]] LockscreenState *lockscreenState();
    void setLockscreenState(LockscreenState *lockscreen_state);
    [[nodiscard]] bool active() const;

public Q_SLOTS:
    void onScreenChange(QQuickView *view);

Q_SIGNALS:
    void activeChanged();

private:
    QQuickWindow *m_window;
    LockscreenState *m_lockscreenState;
    bool m_active;
};
} // namespace
