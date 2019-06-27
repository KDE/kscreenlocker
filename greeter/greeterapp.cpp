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
#include "kscreensaversettings.h"
#include "authenticator.h"
#include "noaccessnetworkaccessmanagerfactory.h"
#include "wallpaper_integration.h"
#include "lnf_integration.h"

#include <config-kscreenlocker.h>

// KDE
#include <KAuthorized>
#include <KCrash>
#include <kdeclarative/kdeclarative.h>
#include <KDeclarative/KQuickAddons/QuickViewSharedEngine>
#include <KDeclarative/QmlObjectSharedEngine>
#include <KDeclarative/ConfigPropertyMap>

#include <KUser>
//Plasma
#include <KPackage/Package>
#include <KPackage/PackageStructure>
#include <KPackage/PackageLoader>
// KWayland
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/plasmashell.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/surface.h>
// Qt
#include <QClipboard>
#include <QAbstractNativeEventFilter>
#include <QTimer>
#include <QKeyEvent>
#include <qscreen.h>
#include <QThread>
#include <QMimeData>

#include <QQuickView>
#include <QQuickItem>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QQmlExpression>

#include <QX11Info>
// Wayland
#include <wayland-client.h>
#include <wayland-ksld-client-protocol.h>
// X11
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <fixx11h.h>
//
#include <xcb/xcb.h>

#if HAVE_SECCOMP
#include <sys/stat.h>
#include <unistd.h>
#endif

// this is usable to fake a "screensaver" installation for testing
// *must* be "0" for every public commit!
#define TEST_SCREENSAVER 0

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
    bool nativeEventFilter(const QByteArray &eventType, void *message, long int *result) override {
        Q_UNUSED(result)
        if (qstrcmp(eventType, "xcb_generic_event_t") != 0) {
            return false;
        }
        xcb_generic_event_t *event = reinterpret_cast<xcb_generic_event_t*>(message);
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
    connect(m_authenticator, &Authenticator::succeeded, this, &QCoreApplication::quit);
    initialize();
    connect(this, &UnlockApp::screenAdded, this, &UnlockApp::desktopResized);
    connect(this, &UnlockApp::screenRemoved, this, &UnlockApp::desktopResized);

    if (QX11Info::isPlatformX11()) {
        installNativeEventFilter(new FocusOutEventFilter);
    }
}

UnlockApp::~UnlockApp()
{
    //workaround QTBUG-55460
    //will be fixed when themes port to QQC2
    for (auto view: qAsConst(m_views)) {
        if (QQuickItem *focusItem = view->activeFocusItem()) {
            focusItem->setFocus(false);
        }
    }
    qDeleteAll(m_views);

    if (m_ksldInterface) {
        org_kde_ksld_destroy(m_ksldInterface);
    }
    if (m_ksldRegistry) {
        delete m_ksldRegistry;
    }
    if (m_ksldConnection) {
        m_ksldConnection->deleteLater();
        m_ksldConnectionThread->quit();
        m_ksldConnectionThread->wait();
    }
}

Authenticator *UnlockApp::createAuthenticator()
{
#if HAVE_SECCOMP
    struct stat buf;
    stat(KCHECKPASS_BIN, &buf);
    if (!(buf.st_mode & S_ISUID)) {
        m_supportsSeccomp = true;
        return new Authenticator(AuthenticationMode::Delayed, this);
    }
#endif
    return new Authenticator(AuthenticationMode::Direct, this);
}

void UnlockApp::initialize()
{
    initializeWayland();
    // set up the request ignore timeout, so that multiple requests to sleep/suspend/shutdown
    // are not processed in quick (and confusing) succession)
    m_resetRequestIgnoreTimer->setSingleShot(true);
    m_resetRequestIgnoreTimer->setInterval(2000);
    connect(m_resetRequestIgnoreTimer, &QTimer::timeout, this, &UnlockApp::resetRequestIgnore);

    KScreenSaverSettings::self()->load();
    KPackage::Package package = KPackage::PackageLoader::self()->loadPackage(QStringLiteral("Plasma/LookAndFeel"));
    KConfigGroup cg(KSharedConfig::openConfig(QStringLiteral("kdeglobals")), "KDE");
    m_packageName = cg.readEntry("LookAndFeelPackage", QString());
    if (!m_packageName.isEmpty()) {
        package.setPath(m_packageName);
    }
    if (!KScreenSaverSettings::theme().isEmpty()) {
        package.setPath(KScreenSaverSettings::theme());
    }

    m_mainQmlPath = package.fileUrl("lockscreenmainscript");

    m_wallpaperIntegration->setConfig(KScreenSaverSettings::self()->sharedConfig());
    m_wallpaperIntegration->setPluginName(KScreenSaverSettings::self()->wallpaperPlugin());
    m_wallpaperIntegration->init();

    m_lnfIntegration->setPackage(package);
    m_lnfIntegration->setConfig(KScreenSaverSettings::self()->sharedConfig());
    m_lnfIntegration->init();


    installEventFilter(this);
}

QWindow *UnlockApp::getActiveScreen()
{
    QWindow *activeScreen = nullptr;

    if (m_views.isEmpty()) {
        return activeScreen;
    }

    for (KQuickAddons::QuickViewSharedEngine * view : qAsConst(m_views)) {
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

void UnlockApp::initializeWayland()
{
    if (!platformName().startsWith(QLatin1String("wayland"), Qt::CaseInsensitive)) {
        return;
    }
    using namespace KWayland::Client;
    auto *c = ConnectionThread::fromApplication(this);
    if (!c) {
        return;
    }
    Registry *r = new Registry(this);
    r->create(c);
    r->setup();
    c->roundtrip();
    const auto i = r->interface(Registry::Interface::PlasmaShell);
    if (i.name == 0) {
        return;
    }
    m_plasmaShell = r->createPlasmaShell(i.name, i.version, this);
}

void UnlockApp::loadWallpaperPlugin(KQuickAddons::QuickViewSharedEngine *view)
{
    auto package = m_wallpaperIntegration->package();
    if (!package.isValid()) {
        qWarning() << "Error loading the wallpaper, no valid package loaded";
        return;
    }

    auto qmlObject = new KDeclarative::QmlObjectSharedEngine(view);
    qmlObject->setInitializationDelayed(true);
    qmlObject->setPackage(package);
    qmlObject->rootContext()->setContextProperty(QStringLiteral("wallpaper"), m_wallpaperIntegration);
    view->setProperty("wallpaperGraphicsObject", QVariant::fromValue(qmlObject));
    connect(qmlObject, &KDeclarative::QmlObject::finished, this,
        [this, qmlObject, view] {
            auto item = qobject_cast<QQuickItem*>(qmlObject->rootObject());
            if (!item) {
                qWarning() << "Wallpaper needs to be a QtQuick Item";
                return;
            }
            item->setParentItem(view->rootObject());
            item->setZ(-1000);

            //set anchors
            QQmlExpression expr(qmlObject->engine()->rootContext(), item, QStringLiteral("parent"));
            QQmlProperty prop(item, QStringLiteral("anchors.fill"));
            prop.write(expr.evaluate());

            view->rootContext()->setContextProperty(QStringLiteral("wallpaper"), item);
            view->rootContext()->setContextProperty(QStringLiteral("wallpaperIntegration"), m_wallpaperIntegration);
        }
    );
}

void UnlockApp::desktopResized()
{
    const int nScreens = screens().count();
    // remove useless views and savers
    while (m_views.count() > nScreens) {
        m_views.takeLast()->deleteLater();
    }

    // extend views and savers to current demand
    for (int i = m_views.count(); i < nScreens; ++i) {
        // create the view
        auto *view = new KQuickAddons::QuickViewSharedEngine();
        view->setColor(Qt::black);
        auto screen = QGuiApplication::screens()[i];
        view->setGeometry(screen->geometry());

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

        if (m_ksldInterface) {
            view->create();
            org_kde_ksld_x11window(m_ksldInterface, view->winId());
            wl_display_flush(m_ksldConnection->display());
        }

        if (m_plasmaShell) {
            using namespace KWayland::Client;
            if (Surface *surface = Surface::fromWindow(view)) {
                PlasmaShellSurface *shellSurface = m_plasmaShell->createSurface(surface, view);
                view->setProperty("plasmaShellSurface", QVariant::fromValue(shellSurface));
            }
        }

        // engine stuff
        QQmlContext* context = view->engine()->rootContext();
        const KUser user;
        const QString fullName = user.property(KUser::FullName).toString();

        context->setContextProperty(QStringLiteral("kscreenlocker_userName"), fullName.isEmpty() ? user.loginName() : fullName);
        context->setContextProperty(QStringLiteral("kscreenlocker_userImage"), user.faceIconPath());
        context->setContextProperty(QStringLiteral("authenticator"), m_authenticator);
        context->setContextProperty(QStringLiteral("org_kde_plasma_screenlocker_greeter_interfaceVersion"), 2);
        context->setContextProperty(QStringLiteral("org_kde_plasma_screenlocker_greeter_view"), view);
        context->setContextProperty(QStringLiteral("defaultToSwitchUser"), m_defaultToSwitchUser);
        context->setContextProperty(QStringLiteral("config"), m_lnfIntegration->configuration());

        view->setSource(m_mainQmlPath);
        // on error, load the fallback lockscreen to not lock the user out of the system
        if (view->status() != QQmlComponent::Ready) {
            static const QUrl fallbackUrl(QUrl(QStringLiteral("qrc:/fallbacktheme/LockScreen.qml")));

            qWarning() << "Failed to load lockscreen QML, falling back to built-in locker";
            m_mainQmlPath = fallbackUrl;
            view->setSource(fallbackUrl);
        }
        view->setResizeMode(KQuickAddons::QuickViewSharedEngine::SizeRootObjectToView);

        loadWallpaperPlugin(view);
        // overwrite the factory set by kdeclarative
        auto oldFactory = view->engine()->networkAccessManagerFactory();
        view->engine()->setNetworkAccessManagerFactory(nullptr);
        delete oldFactory;
        view->engine()->setNetworkAccessManagerFactory(new NoAccessNetworkAccessManagerFactory);

        QQmlProperty lockProperty(view->rootObject(), QStringLiteral("locked"));
        lockProperty.write(m_immediateLock || (!m_noLock && !m_delayedLockTimer));

        QQmlProperty sleepProperty(view->rootObject(), QStringLiteral("suspendToRamSupported"));
        sleepProperty.write(m_canSuspend);
        if (view->rootObject() && view->rootObject()->metaObject()->indexOfSignal(QMetaObject::normalizedSignature("suspendToRam()").constData()) != -1) {
            connect(view->rootObject(), SIGNAL(suspendToRam()), SLOT(suspendToRam()));
        }

        QQmlProperty hibernateProperty(view->rootObject(), QStringLiteral("suspendToDiskSupported"));
        hibernateProperty.write(m_canHibernate);
        if (view->rootObject() && view->rootObject()->metaObject()->indexOfSignal(QMetaObject::normalizedSignature("suspendToDisk()").constData()) != -1) {
            connect(view->rootObject(), SIGNAL(suspendToDisk()), SLOT(suspendToDisk()));
        }

        // verify that the engine's controller didn't change
        Q_ASSERT(dynamic_cast<NoAccessNetworkAccessManagerFactory*>(view->engine()->networkAccessManagerFactory()));

        m_views << view;
    }

    // update geometry of all views and savers
    for (int i = 0; i < nScreens; ++i) {
        auto *view = m_views.at(i);

        auto screen = QGuiApplication::screens()[i];
        view->setGeometry(screen->geometry());
        KWayland::Client::PlasmaShellSurface *plasmaSurface = view->property("plasmaShellSurface").value<KWayland::Client::PlasmaShellSurface *>();
        if (plasmaSurface) {
            plasmaSurface->setPosition(view->geometry().topLeft());
        }
        if (auto object = view->property("wallpaperGraphicsObject").value<KDeclarative::QmlObjectSharedEngine*>()) {
            //initialize with our size to avoid as much resize events as possible
            object->completeInitialization({
                {QStringLiteral("width"), view->width()},
                {QStringLiteral("height"), view->height()}
            });
        }

        connect(screen,
                &QScreen::geometryChanged,
                view,
                [view, plasmaSurface](const QRect &geo)  {
                    view->setGeometry(geo);
                    if (plasmaSurface) {
                        plasmaSurface->setPosition(view->geometry().topLeft());
                    }
                }
        );

        // on Wayland we may not use fullscreen as that puts all windows on one screen
        if (m_testing || plasmaSurface || QX11Info::isPlatformX11()) {
            view->show();
        } else {
            view->showFullScreen();
        }
        view->raise();

        connect(view, &QQuickWindow::frameSwapped, this, [this, view] {
            markViewsAsVisible(view);
        }, Qt::QueuedConnection);
    }
}

void UnlockApp::markViewsAsVisible(KQuickAddons::QuickViewSharedEngine *view)
{
    disconnect(view, &QQuickWindow::frameSwapped, this, nullptr);
    QQmlProperty showProperty(view->rootObject(), QStringLiteral("viewVisible"));
    showProperty.write(true);
    // random state update, actually rather required on init only
    QMetaObject::invokeMethod(this, "getFocus", Qt::QueuedConnection);

    auto mime1 = new QMimeData;
    //Effectively we want to clear the clipboard
    //however some clipboard managers (like klipper with it's default settings)
    //will prevent an empty clipbard
    //we need some non-empty non-text mimeData to replace the clipboard so we don't leak real data to a user pasting into the text field
    //as the clipboard is cleared on close, klipper will then put the original text back when we exit
    mime1->setData(QStringLiteral("x-kde-lockscreen"), QByteArrayLiteral("empty"));
    //ownership is transferred
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

    org_kde_ksld_suspendSystem(m_ksldInterface);

}

void UnlockApp::suspendToDisk()
{
    if (m_ignoreRequests) {
        return;
    }

    m_ignoreRequests = true;
    m_resetRequestIgnoreTimer->start();

    org_kde_ksld_hibernateSystem(m_ksldInterface);
}

void UnlockApp::setTesting(bool enable)
{
    m_testing  = enable;
    if (m_views.isEmpty()) {
        return;
    }
    if (enable) {
        // remove bypass window manager hint
        for (KQuickAddons::QuickViewSharedEngine * view : qAsConst(m_views)) {
            view->setFlags(view->flags() & ~Qt::X11BypassWindowManagerHint);
        }
    } else {
        for (KQuickAddons::QuickViewSharedEngine * view : qAsConst(m_views)) {
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
        shareEvent(event, qobject_cast<KQuickAddons::QuickViewSharedEngine*>(obj));
        return false; // we don't care
    } else if (event->type() == QEvent::KeyRelease) { // conditionally reshow the saver
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() != Qt::Key_Escape) {
            shareEvent(event, qobject_cast<KQuickAddons::QuickViewSharedEngine*>(obj));
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
        for (KQuickAddons::QuickViewSharedEngine * view : qAsConst(m_views)) {
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


static void osdProgress(void *data, org_kde_ksld *org_kde_ksld, const char *icon, int32_t percent, const char *text)
{
    Q_UNUSED(org_kde_ksld)
    reinterpret_cast<UnlockApp*>(data)->osdProgress(QString::fromUtf8(icon), percent, QString::fromUtf8(text));
}

static void osdText(void *data, org_kde_ksld *org_kde_ksld, const char *icon, const char *text)
{
    Q_UNUSED(org_kde_ksld)
    reinterpret_cast<UnlockApp*>(data)->osdText(QString::fromUtf8(icon), QString::fromUtf8(text));
}

static void canSuspend(void *data, org_kde_ksld *org_kde_ksld, uint suspend)
{
    Q_UNUSED(org_kde_ksld)
    reinterpret_cast<UnlockApp*>(data)->updateCanSuspend(suspend);
}

static void canHibernate(void *data, org_kde_ksld *org_kde_ksld, uint hibernate)
{
    Q_UNUSED(org_kde_ksld)
    reinterpret_cast<UnlockApp*>(data)->updateCanHibernate(hibernate);
}

static const struct org_kde_ksld_listener s_listener {
    osdProgress,
    osdText,
    canSuspend,
    canHibernate
};

void UnlockApp::setKsldSocket(int socket)
{
    using namespace KWayland::Client;
    m_ksldConnection = new ConnectionThread;
    m_ksldConnection->setSocketFd(socket);

    m_ksldRegistry = new Registry();
    EventQueue *queue = new EventQueue(m_ksldRegistry);

    connect(m_ksldRegistry, &Registry::interfaceAnnounced, this,
        [this, queue] (QByteArray interface, quint32 name, quint32 version) {
            if (interface != QByteArrayLiteral("org_kde_ksld")) {
                return;
            }
            m_ksldInterface = reinterpret_cast<org_kde_ksld*>(wl_registry_bind(*m_ksldRegistry, name, &org_kde_ksld_interface, version));
            queue->addProxy(m_ksldInterface);
            if (version >= 2) {
                org_kde_ksld_add_listener(m_ksldInterface, &s_listener, this);
            }
            for (auto v : qAsConst(m_views)) {
                org_kde_ksld_x11window(m_ksldInterface, v->winId());
                wl_display_flush(m_ksldConnection->display());
            }
        }
    );

    connect(m_ksldConnection, &ConnectionThread::connected, this,
        [this, queue] {
            m_ksldRegistry->create(m_ksldConnection);
            queue->setup(m_ksldConnection);
            m_ksldRegistry->setEventQueue(queue);
            m_ksldRegistry->setup();
            wl_display_flush(m_ksldConnection->display());
        }, Qt::QueuedConnection);

    m_ksldConnectionThread = new QThread(this);
    m_ksldConnection->moveToThread(m_ksldConnectionThread);
    m_ksldConnectionThread->start();
    m_ksldConnection->initConnection();
}

void UnlockApp::osdProgress(const QString &icon, int percent, const QString &additionalText)
{
    for (auto v : qAsConst(m_views)) {
        auto osd = v->rootObject()->findChild<QQuickItem*>(QStringLiteral("onScreenDisplay"));
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
        auto osd = v->rootObject()->findChild<QQuickItem*>(QStringLiteral("onScreenDisplay"));
        if (!osd) {
            continue;
        }
        osd->setProperty("showingProgress", false);
        osd->setProperty("osdValue", additionalText);
        osd->setProperty("icon", icon);
        QMetaObject::invokeMethod(osd, "show");
    }
}

void UnlockApp::updateCanSuspend(bool set)
{
    if (m_canSuspend == set) {
        return;
    }
    m_canSuspend = set;
    for (auto it = m_views.constBegin(), end = m_views.constEnd(); it != end; ++it) {
        QQmlProperty sleepProperty((*it)->rootObject(), QStringLiteral("suspendToRamSupported"));
        sleepProperty.write(m_canSuspend);
    }
}

void UnlockApp::updateCanHibernate(bool set)
{
    if (m_canHibernate == set) {
        return;
    }
    m_canHibernate = set;
    for (auto it = m_views.constBegin(), end = m_views.constEnd(); it != end; ++it) {
        QQmlProperty hibernateProperty((*it)->rootObject(), QStringLiteral("suspendToDiskSupported"));
        hibernateProperty.write(m_canHibernate);
    }
}

} // namespace

