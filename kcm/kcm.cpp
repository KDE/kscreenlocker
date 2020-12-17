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
#include "screenlocker_interface.h"
#include "wallpaper_integration.h"
#include "lnf_integration.h"
#include "kscreenlockerdata.h"
#include "appearancesettings.h"

#include <KAboutData>
#include <KConfigLoader>
#include <KDeclarative/ConfigPropertyMap>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>

#include <KPackage/PackageLoader>

#include <QVector>

K_PLUGIN_FACTORY_WITH_JSON(ScreenLockerKcmFactory, "screenlocker.json", registerPlugin<ScreenLockerKcm>(); registerPlugin<KScreenLockerData>();)

ScreenLockerKcm::ScreenLockerKcm(QObject *parent, const QVariantList &args)
    : KQuickAddons::ManagedConfigModule(parent, args)
    , m_appearanceSettings(new AppearanceSettings(this))
{
    registerSettings(&KScreenSaverSettings::getInstance());

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
    connect(&KScreenSaverSettings::getInstance(), &KScreenSaverSettings::wallpaperPluginIdChanged, m_appearanceSettings, &AppearanceSettings::loadWallpaperConfig);
    connect(m_appearanceSettings, &AppearanceSettings::currentWallpaperChanged, this, &ScreenLockerKcm::currentWallpaperChanged);
}

void ScreenLockerKcm::load()
{
    ManagedConfigModule::load();
    m_appearanceSettings->load();

    updateState();
}

void ScreenLockerKcm::save()
{
    ManagedConfigModule::save();
    m_appearanceSettings->save();

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
    m_appearanceSettings->defaults();

    updateState();
}

void ScreenLockerKcm::updateState()
{
    settingsChanged();
    emit isDefaultsAppearanceChanged();
}

bool ScreenLockerKcm::isSaveNeeded() const
{
    return m_appearanceSettings->isSaveNeeded();
}

bool ScreenLockerKcm::isDefaults() const
{
    return m_appearanceSettings->isDefaults();
}

KDeclarative::ConfigPropertyMap * ScreenLockerKcm::wallpaperConfiguration() const
{
    return m_appearanceSettings->wallpaperConfiguration();
}

KDeclarative::ConfigPropertyMap * ScreenLockerKcm::lnfConfiguration() const
{
    return m_appearanceSettings->lnfConfiguration();
}

KScreenSaverSettings *ScreenLockerKcm::settings() const
{
    return &KScreenSaverSettings::getInstance();
}

QString ScreenLockerKcm::currentWallpaper() const
{
    return KScreenSaverSettings::getInstance().wallpaperPluginId();
}

bool ScreenLockerKcm::isDefaultsAppearance() const
{
    return m_appearanceSettings->isDefaults();
}

QUrl ScreenLockerKcm::lnfConfigFile() const
{
    return m_appearanceSettings->lnfConfigFile();
}

QUrl ScreenLockerKcm::wallpaperConfigFile() const
{
    return m_appearanceSettings->wallpaperConfigFile();
}

ScreenLocker::WallpaperIntegration *ScreenLockerKcm::wallpaperIntegration() const
{
    return m_appearanceSettings->wallpaperIntegration();
}

#include "kcm.moc"
