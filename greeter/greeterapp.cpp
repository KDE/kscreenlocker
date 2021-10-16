/********************************************************************
 KSld - the KDE Screenlocker Daemon
 This file is part of the KDE project.

Copyright (C) 2004 Chris Howells <howells@kde.org>
Copyright (C) 2011 Martin Gräßlin <mgraesslin@kde.org>

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
#include "greeterapp.h"
#include "authenticator.h"
#include "greeterclient.h"
#include "kscreensaversettingsbase.h"
#include "lnf_integration.h"
#include "noaccessnetworkaccessmanagerfactory.h"
#include "powermanagement.h"
#include "wallpaper_integration.h"

#include <config-kscreenlocker.h>
#include <kscreenlocker_greet_logging.h>

#include <LayerShellQt/Window>

// KDE
#include <KAuthorized>
#include <KCrash>
#include <KDeclarative/ConfigPropertyMap>
#include <KDeclarative/KQuickAddons/QuickViewSharedEngine>
#include <KDeclarative/QmlObjectSharedEngine>
#include <KWindowSystem>
#include <kdeclarative/kdeclarative.h>

#include <KUser>
// Plasma
#include <KPackage/Package>
#include <KPackage/PackageLoader>
// Qt
#include <QAbstractNativeEventFilter>
#include <QClipboard>
#include <QDBusConnection>
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

#include <QX11Info>
// X11
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <fixx11h.h>
//
#include <xcb/xcb.h>

// this is usable to fake a "screensaver" installation for testing
// *must* be "0" for every public commit!
#define TEST_SCREENSAVER 0

static const QString s_plasmaShellService = QStringLiteral("org.kde.plasmashell");
static const QString s_osdServicePath = QStringLiteral("/org/kde/osdService");
static const QString s_osdServiceInterface = QStringLiteral("org.kde.osdService");

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

class FocusOutEventFilter : public QAbstractNativeEventFilter
{
public:
    bool nativeEventFilter(const QByteArray &eventType, void *message, long int *result) override
    {
        Q_UNUSED(result)
        if (qstrcmp(eventType, "xcb_generic_event_t") != 0) {
            return false;
        }
        xcb_generic_event_t *event = reinterpret_cast<xcb_generic_event_t *>(message);
        if ((event->response_type & ~0x80) == XCB_FOCUS_OUT) {
            return true;
        }
        return false;
    }
};

// App
UnlockApp::UnlockApp(int &argc, char **argv)
    : QGuiApplication(argc, argv)
    , m_resetRequestIgnoreTimer(new QTimer(this))
    , m_delayedLockTimer(nullptr)
    , m_testing(false)
    , m_ignoreRequests(false)
    , m_immediateLock(false)
    , m_graceTime(0)
    , m_noLock(false)
    , m_defaultToSwitchUser(false)
    , m_wallpaperIntegration(new WallpaperIntegration(this))
    , m_lnfIntegration(new LnFIntegration(this))
{
    m_authenticator = createAuthenticator();
    // It's a queued connection to give the QML part time to eventually execute code connected to Authenticator::succeeded if any
    connect(m_authenticator, &Authenticator::succeeded, this, &QCoreApplication::quit, Qt::QueuedConnection);
    initialize();

    if (QX11Info::isPlatformX11()) {
        installNativeEventFilter(new FocusOutEventFilter);
    }
}

UnlockApp::~UnlockApp()
{
    // workaround QTBUG-55460
    // will be fixed when themes port to QQC2
    for (auto view : qAsConst(m_views)) {
        if (QQuickItem *focusItem = view->activeFocusItem()) {
            focusItem->setFocus(false);
        }
    }
    qDeleteAll(m_views);

    delete m_greeterClient;
}

Authenticator *UnlockApp::createAuthenticator()
{
    return new Authenticator(AuthenticationMode::Direct, this);
}

void UnlockApp::initialize()
{
    // set up the request ignore timeout, so that multiple requests to sleep/suspend/shutdown
    // are not processed in quick (and confusing) succession)
    m_resetRequestIgnoreTimer->setSingleShot(true);
    m_resetRequestIgnoreTimer->setInterval(2000);
    connect(m_resetRequestIgnoreTimer, &QTimer::timeout, this, &UnlockApp::resetRequestIgnore);

    KScreenSaverSettingsBase::self()->load();
    KPackage::Package package = KPackage::PackageLoader::self()->loadPackage(QStringLiteral("Plasma/LookAndFeel"));
    KConfigGroup cg(KSharedConfig::openConfig(QStringLiteral("kdeglobals")), "KDE");
    m_packageName = cg.readEntry("LookAndFeelPackage", QString());
    if (!m_packageName.isEmpty()) {
        package.setPath(m_packageName);
    }
    if (!KScreenSaverSettingsBase::theme().isEmpty()) {
        package.setPath(KScreenSaverSettingsBase::theme());
    }

    m_mainQmlPath = package.fileUrl("lockscreenmainscript");

    m_wallpaperIntegration->setConfig(KScreenSaverSettingsBase::self()->sharedConfig());
    m_wallpaperIntegration->setPluginName(KScreenSaverSettingsBase::self()->wallpaperPluginId());
    m_wallpaperIntegration->init();

    m_lnfIntegration->setPackage(package);
    m_lnfIntegration->setConfig(KScreenSaverSettingsBase::self()->sharedConfig());
    m_lnfIntegration->init();

    const KUser user;
    const QString fullName = user.property(KUser::FullName).toString();

    m_userName = fullName.isEmpty() ? user.loginName() : fullName;
    m_userImage = user.faceIconPath();

    installEventFilter(this);

    QDBusConnection::sessionBus()
        .connect(s_plasmaShellService, s_osdServicePath, s_osdServiceInterface, QStringLiteral("osdProgress"), this, SLOT(osdProgress(QString, int, QString)));
    QDBusConnection::sessionBus()
        .connect(s_plasmaShellService, s_osdServicePath, s_osdServiceInterface, QStringLiteral("osdText"), this, SLOT(osdText(QString, QString)));

    connect(PowerManagement::instance(), &PowerManagement::canSuspendChanged, this, &UnlockApp::updateCanSuspend);
    connect(PowerManagement::instance(), &PowerManagement::canHibernateChanged, this, &UnlockApp::updateCanHibernate);
}

QWindow *UnlockApp::getActiveScreen()
{
    QWindow *activeScreen = nullptr;

    if (m_views.isEmpty()) {
        return activeScreen;
    }

    for (KQuickAddons::QuickViewSharedEngine *view : qAsConst(m_views)) {
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

void UnlockApp::loadWallpaperPlugin(KQuickAddons::QuickViewSharedEngine *view)
{
    auto package = m_wallpaperIntegration->package();
    if (!package.isValid()) {
        qCWarning(KSCREENLOCKER_GREET) << "Error loading the wallpaper, no valid package loaded";
        return;
    }

    auto qmlObject = new KDeclarative::QmlObjectSharedEngine(view);
    qmlObject->setInitializationDelayed(true);
    qmlObject->setPackage(package);
    qmlObject->rootContext()->setContextProperty(QStringLiteral("wallpaper"), m_wallpaperIntegration);
    view->setProperty("wallpaperGraphicsObject", QVariant::fromValue(qmlObject));
    connect(qmlObject, &KDeclarative::QmlObject::finished, this, [this, qmlObject, view] {
        auto item = qobject_cast<QQuickItem *>(qmlObject->rootObject());
        if (!item) {
            qCWarning(KSCREENLOCKER_GREET) << "Wallpaper needs to be a QtQuick Item";
            return;
        }
        item->setParentItem(view->rootObject());
        item->setZ(-1000);

        // set anchors
        QQmlExpression expr(qmlObject->engine()->rootContext(), item, QStringLiteral("parent"));
        QQmlProperty prop(item, QStringLiteral("anchors.fill"));
        prop.write(expr.evaluate());

        view->rootContext()->setContextProperty(QStringLiteral("wallpaper"), item);
        view->rootContext()->setContextProperty(QStringLiteral("wallpaperIntegration"), m_wallpaperIntegration);
    });
}

void UnlockApp::initialViewSetup()
{
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

KQuickAddons::QuickViewSharedEngine *UnlockApp::createViewForScreen(QScreen *screen)
{
    // create the view
    auto *view = new KQuickAddons::QuickViewSharedEngine();

    view->setColor(Qt::black);
    view->setScreen(screen);
    view->setGeometry(screen->geometry());

    connect(screen, &QScreen::geometryChanged, view, [view](const QRect &geo) {
        view->setGeometry(geo);
    });

    // first create KDeclarative, to be sure that it created a KIO Network Factory
    KDeclarative::KDeclarative declarative;
    declarative.setDeclarativeEngine(view->engine());
    declarative.setupBindings();

    if (!m_testing) {
        if (QX11Info::isPlatformX11()) {
            view->setFlags(Qt::X11BypassWindowManagerHint);
        } else {
            view->setFlags(Qt::FramelessWindowHint);
        }
    }

    if (m_greeterClient) {
        view->create();
        m_greeterClient->sendRegister(view->winId());
    }

    // engine stuff
    QQmlContext *context = view->engine()->rootContext();

    context->setContextProperty(QStringLiteral("kscreenlocker_userName"), m_userName);
    context->setContextProperty(QStringLiteral("kscreenlocker_userImage"), m_userImage);
    context->setContextProperty(QStringLiteral("authenticator"), m_authenticator);
    context->setContextProperty(QStringLiteral("org_kde_plasma_screenlocker_greeter_interfaceVersion"), 2);
    context->setContextProperty(QStringLiteral("org_kde_plasma_screenlocker_greeter_view"), view);
    context->setContextProperty(QStringLiteral("defaultToSwitchUser"), m_defaultToSwitchUser);
    context->setContextProperty(QStringLiteral("config"), m_lnfIntegration->configuration());

    view->setSource(m_mainQmlPath);
    // on error, load the fallback lockscreen to not lock the user out of the system
    if (view->status() != QQmlComponent::Ready) {
        static const QUrl fallbackUrl(QUrl(QStringLiteral("qrc:/fallbacktheme/LockScreen.qml")));

        qCWarning(KSCREENLOCKER_GREET) << "Failed to load lockscreen QML, falling back to built-in locker";

        m_mainQmlPath = fallbackUrl;
        view->setSource(fallbackUrl);
    }
    view->setResizeMode(KQuickAddons::QuickViewSharedEngine::SizeRootObjectToView);

    // overwrite the factory set by kdeclarative
    auto oldFactory = view->engine()->networkAccessManagerFactory();
    view->engine()->setNetworkAccessManagerFactory(nullptr);
    delete oldFactory;
    view->engine()->setNetworkAccessManagerFactory(new NoAccessNetworkAccessManagerFactory);

    QQmlProperty lockProperty(view->rootObject(), QStringLiteral("locked"));
    lockProperty.write(m_immediateLock || (!m_noLock && !m_delayedLockTimer));

    QQmlProperty sleepProperty(view->rootObject(), QStringLiteral("suspendToRamSupported"));
    sleepProperty.write(PowerManagement::instance()->canSuspend());
    if (view->rootObject() && view->rootObject()->metaObject()->indexOfSignal(QMetaObject::normalizedSignature("suspendToRam()").constData()) != -1) {
        connect(view->rootObject(), SIGNAL(suspendToRam()), SLOT(suspendToRam()));
    }

    QQmlProperty hibernateProperty(view->rootObject(), QStringLiteral("suspendToDiskSupported"));
    hibernateProperty.write(PowerManagement::instance()->canHibernate());
    if (view->rootObject() && view->rootObject()->metaObject()->indexOfSignal(QMetaObject::normalizedSignature("suspendToDisk()").constData()) != -1) {
        connect(view->rootObject(), SIGNAL(suspendToDisk()), SLOT(suspendToDisk()));
    }

    // verify that the engine's controller didn't change
    Q_ASSERT(dynamic_cast<NoAccessNetworkAccessManagerFactory *>(view->engine()->networkAccessManagerFactory()));

    loadWallpaperPlugin(view);
    if (auto object = view->property("wallpaperGraphicsObject").value<KDeclarative::QmlObjectSharedEngine *>()) {
        // initialize with our size to avoid as much resize events as possible
        object->completeInitialization({
            {QStringLiteral("width"), view->width()},
            {QStringLiteral("height"), view->height()},
        });
    }

    if (KWindowSystem::isPlatformWayland()) {
        if (auto layerShellWindow = LayerShellQt::Window::get(view)) {
            layerShellWindow->setExclusiveZone(-1);
            layerShellWindow->setLayer(LayerShellQt::Window::LayerOverlay);
        }
    }

    // on Wayland we may not use fullscreen as that puts all windows on one screen
    if (m_testing || QX11Info::isPlatformX11()) {
        view->show();
    } else {
        view->showFullScreen();
    }
    view->raise();

    auto onFrameSwapped = [this, view] {
        markViewsAsVisible(view);
    };
    connect(view, &QQuickWindow::frameSwapped, this, onFrameSwapped, Qt::QueuedConnection);

    return view;
}

void UnlockApp::markViewsAsVisible(KQuickAddons::QuickViewSharedEngine *view)
{
    disconnect(view, &QQuickWindow::frameSwapped, this, nullptr);
    QQmlProperty showProperty(view->rootObject(), QStringLiteral("viewVisible"));
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
    for (KQuickAddons::QuickViewSharedEngine *view : qAsConst(m_views)) {
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

void UnlockApp::setLockedPropertyOnViews()
{
    delete m_delayedLockTimer;
    m_delayedLockTimer = nullptr;

    for (KQuickAddons::QuickViewSharedEngine *view : qAsConst(m_views)) {
        QQmlProperty lockProperty(view->rootObject(), QStringLiteral("locked"));
        lockProperty.write(true);
    }
}

void UnlockApp::resetRequestIgnore()
{
    m_ignoreRequests = false;
}

void UnlockApp::suspendToRam()
{
    if (m_ignoreRequests) {
        return;
    }

    m_ignoreRequests = true;
    m_resetRequestIgnoreTimer->start();

    PowerManagement::instance()->suspend();
}

void UnlockApp::suspendToDisk()
{
    if (m_ignoreRequests) {
        return;
    }

    m_ignoreRequests = true;
    m_resetRequestIgnoreTimer->start();

    PowerManagement::instance()->hibernate();
}

void UnlockApp::setTesting(bool enable)
{
    m_testing = enable;
    if (m_views.isEmpty()) {
        return;
    }
    if (enable) {
        // remove bypass window manager hint
        for (KQuickAddons::QuickViewSharedEngine *view : qAsConst(m_views)) {
            view->setFlags(view->flags() & ~Qt::X11BypassWindowManagerHint);
        }
    } else {
        for (KQuickAddons::QuickViewSharedEngine *view : qAsConst(m_views)) {
            view->setFlags(view->flags() | Qt::X11BypassWindowManagerHint);
        }
    }
}

void UnlockApp::setTheme(const QString &theme)
{
    if (theme.isEmpty() || !m_testing) {
        return;
    }

    m_packageName = theme;
    KPackage::Package package = KPackage::PackageLoader::self()->loadPackage(QStringLiteral("Plasma/LookAndFeel"));
    package.setPath(m_packageName);

    m_mainQmlPath = package.fileUrl("lockscreenmainscript");
}

void UnlockApp::setImmediateLock(bool immediate)
{
    m_immediateLock = immediate;
}

void UnlockApp::lockImmediately()
{
    setImmediateLock(true);
    setLockedPropertyOnViews();
}

bool UnlockApp::eventFilter(QObject *obj, QEvent *event)
{
    if (obj != this && event->type() == QEvent::Show) {
        KQuickAddons::QuickViewSharedEngine *view = nullptr;
        for (KQuickAddons::QuickViewSharedEngine *v : qAsConst(m_views)) {
            if (v == obj) {
                view = v;
                break;
            }
        }
        if (view && view->winId() && QX11Info::isPlatformX11()) {
            // showing greeter view window, set property
            static Atom tag = XInternAtom(QX11Info::display(), "_KDE_SCREEN_LOCKER", False);
            XChangeProperty(QX11Info::display(), view->winId(), tag, tag, 32, PropModeReplace, nullptr, 0);
        }
        // no further processing
        return false;
    }

    if (event->type() == QEvent::MouseButtonPress && QX11Info::isPlatformX11()) {
        if (getActiveScreen()) {
            getActiveScreen()->requestActivate();
        }
        return false;
    }

    if (event->type() == QEvent::KeyPress) { // react if saver is visible
        shareEvent(event, qobject_cast<KQuickAddons::QuickViewSharedEngine *>(obj));
        return false; // we don't care
    } else if (event->type() == QEvent::KeyRelease) { // conditionally reshow the saver
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() != Qt::Key_Escape) {
            shareEvent(event, qobject_cast<KQuickAddons::QuickViewSharedEngine *>(obj));
            return false; // irrelevant
        }
        return true; // don't pass
    }

    return false;
}

/*
 * This function forwards an event from one greeter window to all others
 * It's used to have the keyboard operate on all greeter windows (on every screen)
 * at once so that the user gets visual feedback on the screen he's looking at -
 * even if the focus is actually on a powered off screen.
 */

void UnlockApp::shareEvent(QEvent *e, KQuickAddons::QuickViewSharedEngine *from)
{
    // from can be NULL any time (because the parameter is passed as qobject_cast)
    // m_views.contains(from) is atm. supposed to be true but required if any further
    // QQuickView are added (which are not part of m_views)
    // this makes "from" an optimization (nullptr check aversion)
    if (from && m_views.contains(from)) {
        // NOTICE any recursion in the event sharing will prevent authentication on multiscreen setups!
        // Any change in regarded event processing shall be tested thoroughly!
        removeEventFilter(this); // prevent recursion!
        const bool accepted = e->isAccepted(); // store state
        for (KQuickAddons::QuickViewSharedEngine *view : qAsConst(m_views)) {
            if (view != from) {
                QCoreApplication::sendEvent(view, e);
                e->setAccepted(accepted);
            }
        }
        installEventFilter(this);
    }
}

void UnlockApp::setGraceTime(int milliseconds)
{
    m_graceTime = milliseconds;
    if (milliseconds < 0 || m_delayedLockTimer || m_noLock || m_immediateLock) {
        return;
    }
    m_delayedLockTimer = new QTimer(this);
    m_delayedLockTimer->setSingleShot(true);
    connect(m_delayedLockTimer, &QTimer::timeout, this, &UnlockApp::setLockedPropertyOnViews);
    m_delayedLockTimer->start(m_graceTime);
}

void UnlockApp::setNoLock(bool noLock)
{
    m_noLock = noLock;
}

void UnlockApp::setDefaultToSwitchUser(bool defaultToSwitchUser)
{
    m_defaultToSwitchUser = defaultToSwitchUser;
}

void UnlockApp::setKsldSocket(int socket)
{
    m_greeterClient = new GreeterClient(socket);

    for (auto v : qAsConst(m_views)) {
        m_greeterClient->sendRegister(v->winId());
    }
}

void UnlockApp::osdProgress(const QString &icon, int percent, const QString &additionalText)
{
    for (auto v : qAsConst(m_views)) {
        auto osd = v->rootObject()->findChild<QQuickItem *>(QStringLiteral("onScreenDisplay"));
        if (!osd) {
            continue;
        }
        osd->setProperty("osdValue", percent);
        osd->setProperty("osdAdditionalText", additionalText);
        osd->setProperty("showingProgress", true);
        osd->setProperty("icon", icon);
        QMetaObject::invokeMethod(osd, "show");
    }
}

void UnlockApp::osdText(const QString &icon, const QString &additionalText)
{
    for (auto v : qAsConst(m_views)) {
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

void UnlockApp::updateCanSuspend()
{
    for (auto it = m_views.constBegin(), end = m_views.constEnd(); it != end; ++it) {
        QQmlProperty sleepProperty((*it)->rootObject(), QStringLiteral("suspendToRamSupported"));
        sleepProperty.write(PowerManagement::instance()->canSuspend());
    }
}

void UnlockApp::updateCanHibernate()
{
    for (auto it = m_views.constBegin(), end = m_views.constEnd(); it != end; ++it) {
        QQmlProperty hibernateProperty((*it)->rootObject(), QStringLiteral("suspendToDiskSupported"));
        hibernateProperty.write(PowerManagement::instance()->canHibernate());
    }
}

} // namespace
