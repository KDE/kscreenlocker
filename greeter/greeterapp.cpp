/*
SPDX-FileCopyrightText: 2004 Chris Howells <howells@kde.org>
SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "greeterapp.h"
#include "kscreensaversettingsbase.h"
#include "noaccessnetworkaccessmanagerfactory.h"
#include "shell_integration.h"
#include "wallpaper_integration.h"

#include "../logind.h"

#include <config-kscreenlocker.h>
#include <iostream>
#include <unistd.h>
#include <kscreenlocker_greet_logging.h>

#include <LayerShellQt/Window>

// KDE
#include <KAuthorized>
#include <KConfigLoader>
#include <KConfigPropertyMap>
#include <KCrash>
#include <KLocalizedContext>
#include <KLocalizedQmlContext>
#include <KScreenDpms/Dpms>
#include <KWindowSystem>
#include <PlasmaQuick/QuickViewSharedEngine>

#include <KUser>
// Plasma
#include <KPackage/Package>
#include <KPackage/PackageLoader>

#include <Plasma/Plasma>

// Qt
#include <QAbstractNativeEventFilter>
#include <QClipboard>
#include <QDBusConnection>
#include <QFile>
#include <QKeyEvent>
#include <QMimeData>
#include <QThread>
#include <QTimer>
#include <qscreen.h>

#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExpression>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickView>

#include "pamauthenticator.h"
#include "pamauthenticators.h"

// this is usable to fake a "screensaver" installation for testing
// *must* be "0" for every public commit!
#define TEST_SCREENSAVER 0

static const QString s_plasmaShellService = QStringLiteral("org.kde.plasmashell");
static const QString s_osdServicePath = QStringLiteral("/org/kde/osdService");
static const QString s_osdServiceInterface = QStringLiteral("org.kde.osdService");

using namespace Qt::Literals;

namespace ScreenLocker
{
// disable DrKonqi as the crash dialog blocks the restart of the locker
void disableDrKonqi()
{
    KCrash::setDrKonqiEnabled(false);
}
// run immediately, before Q_CORE_STARTUP functions
// that would enable drkonqi
Q_CONSTRUCTOR_FUNCTION(disableDrKonqi)

// Verify that a package or its fallback is using the right API
bool verifyPackageApi(const KPackage::Package &package)
{
    if (package.metadata().value(QStringLiteral("X-Plasma-APIVersion"), QStringLiteral("1")).toInt() >= 2) {
        return true;
    }

    if (!package.filePath("lockscreenmainscript").contains(package.path())) {
        // The current package does not contain the lock screen and we are
        // using the fallback package. So check to see if that package has
        // the right version instead.
        if (package.fallbackPackage().metadata().value(QStringLiteral("X-Plasma-APIVersion"), QStringLiteral("1")).toInt() >= 2) {
            return true;
        }
    }

    return false;
}

// App
UnlockApp::UnlockApp(int &argc, char **argv)
    : QGuiApplication(argc, argv)
    , m_delayedLockTimer(nullptr)
    , m_testing(false)
    , m_immediateLock(false)
    , m_graceTime(0)
    , m_noLock(false)
    , m_shellIntegration(new ShellIntegration(this))
    , m_logindIntegration(new LogindIntegration(this))
{
    auto interactive = std::make_unique<PamAuthenticator>(QStringLiteral(KSCREENLOCKER_PAM_SERVICE), KUser().loginName());
    std::vector<std::unique_ptr<PamAuthenticator>> noninteractive;
    noninteractive.push_back(
        std::make_unique<PamAuthenticator>(QStringLiteral(KSCREENLOCKER_PAM_FINGERPRINT_SERVICE), KUser().loginName(), PamAuthenticator::Fingerprint));
    noninteractive.push_back(
        std::make_unique<PamAuthenticator>(QStringLiteral(KSCREENLOCKER_PAM_SMARTCARD_SERVICE), KUser().loginName(), PamAuthenticator::Smartcard));
    m_authenticators = new PamAuthenticators(std::move(interactive), std::move(noninteractive), this);
    connect(m_logindIntegration, &LogindIntegration::prepareForSleep, m_authenticators, [this] {
        m_authenticators->cancel();
    });
    initialize();
}

UnlockApp::~UnlockApp()
{
    // workaround QTBUG-55460
    // will be fixed when themes port to QQC2
    for (auto view : std::as_const(m_views)) {
        if (QQuickItem *focusItem = view->activeFocusItem()) {
            focusItem->setFocus(false);
        }
    }
    qDeleteAll(m_views);
}

void UnlockApp::initialize()
{
    KScreenSaverSettingsBase::self()->load();

    setShell(m_shellIntegration->defaultShell());

    m_wallpaperPackage = KPackage::PackageLoader::self()->loadPackage(QStringLiteral("Plasma/Wallpaper"));
    m_wallpaperPackage.setPath(KScreenSaverSettingsBase::self()->wallpaperPluginId());

    const KUser user;
    const QString fullName = user.property(KUser::FullName).toString();

    m_userName = fullName.isEmpty() ? user.loginName() : fullName;
    m_userImage = user.faceIconPath();

    installEventFilter(this);

    QDBusConnection::sessionBus().connect(s_plasmaShellService,
                                          s_osdServicePath,
                                          s_osdServiceInterface,
                                          QStringLiteral("osdProgress"),
                                          this,
                                          SLOT(osdProgress(QString, int, int, QString)));
    QDBusConnection::sessionBus()
        .connect(s_plasmaShellService, s_osdServicePath, s_osdServiceInterface, QStringLiteral("osdText"), this, SLOT(osdText(QString, QString)));
}

QWindow *UnlockApp::getActiveScreen()
{
    QWindow *activeScreen = nullptr;

    if (m_views.isEmpty()) {
        return activeScreen;
    }

    for (PlasmaQuick::QuickViewSharedEngine *view : std::as_const(m_views)) {
        if (view->geometry().contains(QCursor::pos())) {
            activeScreen = view;
            break;
        }
    }
    if (!activeScreen) {
        activeScreen = m_views.first();
    }

    return activeScreen;
}

PlasmaQuick::SharedQmlEngine *UnlockApp::loadWallpaperPlugin(PlasmaQuick::QuickViewSharedEngine *view)
{
    if (!m_wallpaperPackage.isValid()) {
        qCWarning(KSCREENLOCKER_GREET) << "Error loading the wallpaper, no valid package loaded";
        return nullptr;
    }

    auto qmlObject = new PlasmaQuick::SharedQmlEngine(view);
    qmlObject->setInitializationDelayed(true);
    qmlObject->setSource(QUrl::fromLocalFile(m_wallpaperPackage.filePath("mainscript")));

    qmlObject->rootContext()->setContextProperty(QStringLiteral("wallpaper"), qmlObject->rootObject());
    view->rootContext()->setContextProperty(QStringLiteral("wallpaper"), qmlObject->rootObject());
    view->rootContext()->setContextProperty(QStringLiteral("wallpaperIntegration"), qmlObject->rootObject());
    return qmlObject;
}

void UnlockApp::initialViewSetup()
{
    qmlRegisterUncreatableType<PamAuthenticator>("org.kde.kscreenlocker",
                                                 1,
                                                 0,
                                                 "Authenticator",
                                                 QStringLiteral("authenticators must be obtained from the context"));
    qmlRegisterUncreatableType<PamAuthenticators>("org.kde.kscreenlocker",
                                                  1,
                                                  0,
                                                  "Authenticators",
                                                  QStringLiteral("authenticators must be obtained from the context"));
    for (QScreen *screen : screens()) {
        handleScreen(screen);
    }
    connect(this, &UnlockApp::screenAdded, this, &UnlockApp::handleScreen);
}

void UnlockApp::handleScreen(QScreen *screen)
{
    if (screen->geometry().isNull()) {
        return;
    }
    auto *view = createViewForScreen(screen);
    m_views << view;
    connect(this, &QGuiApplication::screenRemoved, view, [this, view, screen](QScreen *removedScreen) {
        if (removedScreen != screen) {
            return;
        }
        m_views.removeOne(view);
        delete view;
    });
}

PlasmaQuick::QuickViewSharedEngine *UnlockApp::createViewForScreen(QScreen *screen)
{
    // create the view
    auto *view = new PlasmaQuick::QuickViewSharedEngine();

    view->setColor(Qt::black);
    view->setScreen(screen);
    view->setGeometry(screen->geometry());

    connect(screen, &QScreen::geometryChanged, view, [view](const QRect &geo) {
        view->setGeometry(geo);
    });

    Plasma::setupPlasmaStyle(view->engine().get());
    view->engine()->rootContext()->setContextObject(new KLocalizedQmlContext(view->engine().get()));
    auto oldFactory = view->engine()->networkAccessManagerFactory();
    view->engine()->setNetworkAccessManagerFactory(nullptr);
    delete oldFactory;
    view->engine()->setNetworkAccessManagerFactory(new NoAccessNetworkAccessManagerFactory);

    if (!m_testing) {
        view->setFlags(Qt::FramelessWindowHint);
    }

    // engine stuff
    QQmlContext *context = view->engine()->rootContext();
    connect(view->engine().get(), &QQmlEngine::quit, this, [this]() {
        if (m_authenticators->isUnlocked()) {
            std::cout << "Unlocked" << std::endl;
            // Quit without exit handlers
            // This is because:
            // - the pam_unix backend will always report a failed login if we complete the converse method no matter what exit code we use
            // - the fprintd backend sometimes takes a long time
            _exit(0);
        } else {
            qCWarning(KSCREENLOCKER_GREET) << "Greeter tried to quit without being unlocked";
        }
    });

    context->setContextProperty(QStringLiteral("kscreenlocker_userName"), m_userName);
    context->setContextProperty(QStringLiteral("kscreenlocker_userImage"), m_userImage);
    context->setContextProperty(QStringLiteral("authenticator"), m_authenticators);
    context->setContextProperty(QStringLiteral("org_kde_plasma_screenlocker_greeter_interfaceVersion"), 2);
    context->setContextProperty(QStringLiteral("org_kde_plasma_screenlocker_greeter_view"), view);
    context->setContextProperty(QStringLiteral("config"), m_shellIntegration->configuration());

    const KConfigGroup cfg = KScreenSaverSettingsBase::self()
                                 ->sharedConfig()
                                 ->group(QStringLiteral("Greeter"))
                                 .group(QStringLiteral("Wallpaper"))
                                 .group(KScreenSaverSettingsBase::self()->wallpaperPluginId());

    auto configLoader = [&] {
        const QString xmlPath = m_wallpaperPackage.filePath(QByteArrayLiteral("config"), QStringLiteral("main.xml"));
        if (xmlPath.isEmpty()) {
            return new KConfigLoader(cfg, nullptr, this);
        }
        QFile file(xmlPath);
        return new KConfigLoader(cfg, &file, this);
    }();

    KConfigPropertyMap *config = new KConfigPropertyMap(configLoader, this);
    // potd (picture of the day) is using a kded to monitor changes and
    // cache data for the lockscreen. Let's notify it.
    config->setNotify(true);

    // keep lockscreen animated wallpapers from pausing
    if (KScreenSaverSettingsBase::self()->wallpaperPluginId() == QStringLiteral("org.kde.image")) {
        if (!cfg.hasKey("ForceImageAnimation")) {
            config->insert(QStringLiteral("ForceImageAnimation"), true);
        }
    }

    auto wallpaperObj = loadWallpaperPlugin(view);
    if (wallpaperObj) {
        // initialize with our size to avoid as much resize events as possible
        wallpaperObj->completeInitialization({
            {QStringLiteral("width"), view->width()},
            {QStringLiteral("height"), view->height()},
            {u"configuration"_s, QVariant::fromValue(config)},
            {u"pluginName"_s, KScreenSaverSettingsBase::self()->wallpaperPluginId()},
        });
    }

    view->setSource(m_mainQmlPath);
    // on error, load the fallback lockscreen to not lock the user out of the system
    if (view->status() != QQmlComponent::Ready) {
        static const QUrl fallbackUrl(QUrl(QStringLiteral("qrc:/fallbacktheme/LockScreen.qml")));

        qCWarning(KSCREENLOCKER_GREET) << "Failed to load lockscreen QML, falling back to built-in locker";
        for (const auto &error : view->errors()) {
            qCWarning(KSCREENLOCKER_GREET) << error;
        }

        m_mainQmlPath = fallbackUrl;
        view->setSource(fallbackUrl);

        if (view->status() != QQmlComponent::Ready) {
            qCWarning(KSCREENLOCKER_GREET) << "Failed to load the fallback lockscreen QML, something went really wrong! Terminating...";
            for (const auto &error : view->errors()) {
                qCWarning(KSCREENLOCKER_GREET) << error;
            }
            std::terminate();
        }
    }
    view->setResizeMode(PlasmaQuick::QuickViewSharedEngine::SizeRootObjectToView);

    if (wallpaperObj) {
        if (auto wallpaperItem = qobject_cast<QQuickItem *>(wallpaperObj->rootObject())) {
            // we need to set this wallpaper properties separately after the lockscreen QML is loaded
            // this is because we need to anchor to the view that gets loaded
            wallpaperItem->setParentItem(view->rootObject());
            wallpaperItem->setZ(-1000);

            // set anchors
            QQmlExpression expr(qmlContext(wallpaperItem), wallpaperItem, QStringLiteral("parent"));
            QQmlProperty prop(wallpaperItem, QStringLiteral("anchors.fill"));
            prop.write(expr.evaluate());
        }
    }

    QQmlProperty lockProperty(view->rootObject(), QStringLiteral("locked"));
    lockProperty.write(m_immediateLock || (!m_noLock && !m_delayedLockTimer));

    // verify that the engine's controller didn't change
    Q_ASSERT(dynamic_cast<NoAccessNetworkAccessManagerFactory *>(view->engine()->networkAccessManagerFactory()));

    if (KWindowSystem::isPlatformWayland()) {
        if (auto layerShellWindow = LayerShellQt::Window::get(view)) {
            layerShellWindow->setExclusiveZone(-1);
            layerShellWindow->setLayer(LayerShellQt::Window::LayerTop);
            layerShellWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
            layerShellWindow->setScreen(screen);
        }
    }

    // showFullScreen is implicit on X11 (through geometry and hints) and Wayland (layer-shell)
    view->show();
    view->raise();

    auto onFrameSwapped = [this, view] {
        markViewsAsVisible(view->rootObject());
    };
    connect(view, &QQuickWindow::frameSwapped, this, onFrameSwapped, static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));

    return view;
}

void UnlockApp::markViewsAsVisible(QQuickItem *view)
{
    QQmlProperty showProperty(view, QStringLiteral("viewVisible"));
    showProperty.write(true);
    // random state update, actually rather required on init only
    QMetaObject::invokeMethod(this, "getFocus", Qt::QueuedConnection);

    auto mime1 = new QMimeData;
    // Effectively we want to clear the clipboard
    // however some clipboard managers (like klipper with it's default settings)
    // will prevent an empty clipboard
    // we need some non-empty non-text mimeData to replace the clipboard so we don't leak real data to a user pasting into the text field
    // as the clipboard is cleared on close, klipper will then put the original text back when we exit
    mime1->setData(QStringLiteral("x-kde-lockscreen"), QByteArrayLiteral("empty"));
    // ownership is transferred
    QGuiApplication::clipboard()->setMimeData(mime1, QClipboard::Clipboard);

    auto mime2 = new QMimeData;
    mime2->setData(QStringLiteral("x-kde-lockscreen"), QByteArrayLiteral("empty"));
    QGuiApplication::clipboard()->setMimeData(mime2, QClipboard::Selection);
}

void UnlockApp::getFocus()
{
    QWindow *activeScreen = getActiveScreen();

    if (!activeScreen) {
        return;
    }
    // this loop is required to make the qml/graphicsscene properly handle the shared keyboard input
    // ie. "type something into the box of every greeter"
    for (PlasmaQuick::QuickViewSharedEngine *view : std::as_const(m_views)) {
        if (!m_testing) {
            view->setKeyboardGrabEnabled(true); // TODO - check whether this still works in master!
        }
    }
    // activate window and grab input to be sure it really ends up there.
    // focus setting is still required for proper internal QWidget state (and eg. visual reflection)
    if (!m_testing) {
        activeScreen->setKeyboardGrabEnabled(true); // TODO - check whether this still works in master!
    }
    activeScreen->requestActivate();
}

void UnlockApp::graceLockEnded()
{
    m_authenticators->setGraceLocked(false);

    delete m_delayedLockTimer;
    m_delayedLockTimer = nullptr;

    for (PlasmaQuick::QuickViewSharedEngine *view : std::as_const(m_views)) {
        QQmlProperty lockProperty(view->rootObject(), QStringLiteral("locked"));
        lockProperty.write(true);
    }
}

void UnlockApp::setTesting(bool enable)
{
    qCDebug(KSCREENLOCKER_GREET) << "Testing mode enabled:" << enable;

    m_testing = enable;
    if (m_views.isEmpty()) {
        return;
    }
    if (enable) {
        // remove bypass window manager hint
        for (PlasmaQuick::QuickViewSharedEngine *view : std::as_const(m_views)) {
            view->setFlags(view->flags() & ~Qt::X11BypassWindowManagerHint);
        }
    } else {
        for (PlasmaQuick::QuickViewSharedEngine *view : std::as_const(m_views)) {
            view->setFlags(view->flags() | Qt::X11BypassWindowManagerHint);
        }
    }
}

void UnlockApp::setShell(const QString &shell)
{
    m_packageName = shell;
    KPackage::Package package = KPackage::PackageLoader::self()->loadPackage(QStringLiteral("Plasma/Shell"));

    if (!m_packageName.isEmpty()) {
        package.setPath(m_packageName);
    }

    if (!verifyPackageApi(package)) {
        qCWarning(KSCREENLOCKER_GREET) << "Lockscreen QML outdated, falling back to default";
        package.setPath(QStringLiteral("org.kde.plasma.desktop"));
    }

    m_mainQmlPath = package.fileUrl("lockscreenmainscript");

    m_shellIntegration->setPackage(package);
    m_shellIntegration->setConfig(KScreenSaverSettingsBase::self()->sharedConfig());
    m_shellIntegration->init();
}

void UnlockApp::setImmediateLock(bool immediate)
{
    m_immediateLock = immediate;
}

void UnlockApp::lockImmediately()
{
    setImmediateLock(true);
    graceLockEnded();
}

bool UnlockApp::eventFilter(QObject *obj, QEvent *event)
{
    Q_UNUSED(obj);
    if (event->type() == QEvent::KeyPress) { // react if saver is visible
        return false; // we don't care
    } else if (event->type() == QEvent::KeyRelease) { // conditionally reshow the saver
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() != Qt::Key_Escape) {
            return false; // irrelevant
        } else if (!m_testing) { // don't turn screen off in testing mode.
            auto dpms = new KScreen::Dpms(this);
            if (dpms->isSupported()) {
                connect(dpms, &KScreen::Dpms::hasPendingChangesChanged, this, [dpms](bool hasPendingChanges) {
                    if (!hasPendingChanges) {
                        dpms->deleteLater();
                    }
                });
                dpms->switchMode(KScreen::Dpms::Off);
            } else {
                dpms->deleteLater();
            }
        }
        return true; // don't pass
    }

    return false;
}

void UnlockApp::setGraceTime(int milliseconds)
{
    m_graceTime = milliseconds;
    if (milliseconds < 0 || m_delayedLockTimer || m_noLock || m_immediateLock) {
        return;
    }
    m_authenticators->setGraceLocked(true);
    m_delayedLockTimer = new QTimer(this);
    m_delayedLockTimer->setSingleShot(true);
    connect(m_delayedLockTimer, &QTimer::timeout, this, &UnlockApp::graceLockEnded);
    m_delayedLockTimer->start(m_graceTime);
}

void UnlockApp::setNoLock(bool noLock)
{
    m_noLock = noLock;
}

void UnlockApp::osdProgress(const QString &icon, int percent, int maximumPercent, const QString &additionalText)
{
    for (auto v : std::as_const(m_views)) {
        auto osd = v->rootObject()->findChild<QQuickItem *>(QStringLiteral("onScreenDisplay"));
        if (!osd) {
            continue;
        }
        // Update max value first to prevent value from being clamped
        osd->setProperty("osdMaxValue", maximumPercent);
        osd->setProperty("osdValue", percent);
        osd->setProperty("osdAdditionalText", additionalText);
        osd->setProperty("showingProgress", true);
        osd->setProperty("icon", icon);
        QMetaObject::invokeMethod(osd, "show");
    }
}

void UnlockApp::osdText(const QString &icon, const QString &additionalText)
{
    for (auto v : std::as_const(m_views)) {
        auto osd = v->rootObject()->findChild<QQuickItem *>(QStringLiteral("onScreenDisplay"));
        if (!osd) {
            continue;
        }
        osd->setProperty("showingProgress", false);
        osd->setProperty("osdValue", additionalText);
        osd->setProperty("icon", icon);
        QMetaObject::invokeMethod(osd, "show");
    }
}

} // namespace
