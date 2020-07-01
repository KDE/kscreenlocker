/********************************************************************
 KSld - the KDE Screenlocker Daemon
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2019 Kevin Ottens <kevin.ottens@enioka.com>
Copyright (C) 2020 David Redondo <kde@david-redondo.de>

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
#include "kcm.h"
#include "kscreensaversettings.h"
#include "screenlocker_interface.h"
#include "../greeter/wallpaper_integration.h"
#include "../greeter/lnf_integration.h"

#include <KAboutData>
#include <KConfigLoader>
#include <KDeclarative/ConfigPropertyMap>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>

#include <KPackage/Package>
#include <KPackage/PackageLoader>

#include <QVector>

ScreenLockerKcm::ScreenLockerKcm(QObject *parent, const QVariantList &args)
    : KQuickAddons::ManagedConfigModule(parent, args)
    , m_settings(new KScreenSaverSettings(this))
{
    constexpr const char* url = "org.kde.private.kcms.screenlocker";
    qRegisterMetaType<QVector<WallpaperInfo>>("QVector<WallpaperInfo>");
    qmlRegisterAnonymousType<KScreenSaverSettings>(url, 1);
    qmlRegisterAnonymousType<WallpaperInfo>(url, 1);
    qmlRegisterAnonymousType<ScreenLocker::WallpaperIntegration>(url, 1);
    qmlRegisterAnonymousType<KDeclarative::ConfigPropertyMap>(url, 1);
    qmlProtectModule(url, 1);
    KAboutData *about = new KAboutData(QStringLiteral("kcm_screenlocker"), i18n("Screen Locking"),
                                       QStringLiteral("1.0"), QString(), KAboutLicense::GPL);
    about->addAuthor(i18n("Martin Gräßlin"), QString(), QStringLiteral("mgraesslin@kde.org"));
    about->addAuthor(i18n("Kevin Ottens"), QString(), QStringLiteral("kevin.ottens@enioka.com"));
    setAboutData(about);
    connect(m_settings, &KScreenSaverSettings::wallpaperPluginIdChanged, this, &ScreenLockerKcm::loadWallpaperConfig);
}


void ScreenLockerKcm::load()
{
    ManagedConfigModule::load();
    loadWallpaperConfig();
    loadLnfConfig();

    if (m_lnfSettings) {
        m_lnfSettings->load();
        emit m_lnfSettings->configChanged(); // To force the ConfigPropertyMap to reevaluate
    }

    if (m_wallpaperSettings) {
        m_wallpaperSettings->load();
        emit m_wallpaperSettings->configChanged(); // To force the ConfigPropertyMap to reevaluate
    }

    updateState();
}

void ScreenLockerKcm::save()
{
    ManagedConfigModule::save();
    if (m_lnfSettings) {
        m_lnfSettings->save();
    }

    if (m_wallpaperSettings) {
        m_wallpaperSettings->save();
    }

    // reconfigure through DBus
    OrgKdeScreensaverInterface interface(QStringLiteral("org.kde.screensaver"),
                                         QStringLiteral("/ScreenSaver"),
                                         QDBusConnection::sessionBus());
    if (interface.isValid()) {
        interface.configure();
    }

    updateState();
}

void ScreenLockerKcm::defaults()
{
    ManagedConfigModule::defaults();

    if (m_lnfSettings) {
        m_lnfSettings->setDefaults();
        emit m_lnfSettings->configChanged(); // To force the ConfigPropertyMap to reevaluate
    }

    if (m_wallpaperSettings) {
        m_wallpaperSettings->setDefaults();
        emit m_wallpaperSettings->configChanged(); // To force the ConfigPropertyMap to reevaluate
    }

    updateState();
}

void ScreenLockerKcm::updateState()
{
    bool isDefaults = m_settings->isDefaults();
    bool isSaveNeeded = m_settings->isSaveNeeded();

    if (m_lnfSettings) {
        isDefaults &= m_lnfSettings->isDefaults();
        isSaveNeeded |= m_lnfSettings->isSaveNeeded();
    }

    if (m_wallpaperSettings) {
        isDefaults &= m_wallpaperSettings->isDefaults();
        isSaveNeeded |= m_wallpaperSettings->isSaveNeeded();
    }
    setNeedsSave(isSaveNeeded);
    setRepresentsDefaults(isDefaults);
}

void ScreenLockerKcm::loadWallpaperConfig()
{
    if (m_wallpaperIntegration) {
        if (m_wallpaperIntegration->pluginName() == m_settings->wallpaperPluginId()) {
            //nothing changed
            return;
        }
        delete m_wallpaperIntegration;
    }

    m_wallpaperIntegration = new ScreenLocker::WallpaperIntegration(this);
    m_wallpaperIntegration->setConfig(m_settings->sharedConfig());
    m_wallpaperIntegration->setPluginName(m_settings->wallpaperPluginId());
    m_wallpaperIntegration->init();
    m_wallpaperSettings = m_wallpaperIntegration->configScheme();
    m_wallpaperConfigFile = m_wallpaperIntegration->package().fileUrl(QByteArrayLiteral("ui"), QStringLiteral("config.qml"));
    emit currentWallpaperChanged();
}

void ScreenLockerKcm::loadLnfConfig()
{
    if (m_package.isValid() && m_lnfIntegration) {
        return;
    }

    Q_ASSERT(!m_package.isValid() && !m_lnfIntegration);

    m_package = KPackage::PackageLoader::self()->loadPackage(QStringLiteral("Plasma/LookAndFeel"));
    KConfigGroup cg(KSharedConfig::openConfig(QStringLiteral("kdeglobals")), "KDE");
    const QString packageName = cg.readEntry("LookAndFeelPackage", QString());
    if (!packageName.isEmpty()) {
        m_package.setPath(packageName);
    }

    m_lnfIntegration = new ScreenLocker::LnFIntegration(this);
    m_lnfIntegration->setPackage(m_package);
    m_lnfIntegration->setConfig(m_settings->sharedConfig());
    m_lnfIntegration->init();
    m_lnfSettings = m_lnfIntegration->configScheme();

    auto sourceFile = m_package.fileUrl(QByteArrayLiteral("lockscreen"), QStringLiteral("config.qml"));
    m_lnfConfigFile = sourceFile;
}

KDeclarative::ConfigPropertyMap * ScreenLockerKcm::wallpaperConfiguration() const
{
    if (!m_wallpaperIntegration) {
        return nullptr;
    }
    return m_wallpaperIntegration->configuration();
}

KDeclarative::ConfigPropertyMap * ScreenLockerKcm::lnfConfiguration() const
{
    if (!m_lnfIntegration) {
        return nullptr;
    }
    return m_lnfIntegration->configuration();
}


QString ScreenLockerKcm::currentWallpaper() const
{
    return m_settings->wallpaperPluginId();
}



K_PLUGIN_CLASS_WITH_JSON(ScreenLockerKcm, "screenlocker.json")

#include "kcm.moc"
