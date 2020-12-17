/********************************************************************
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2014 Marco Martin <mart@kde.org>
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
#ifndef KCM_H
#define KCM_H

#include <KPackage/Package>
#include <KQuickAddons/ManagedConfigModule>

#include "kscreensaversettings.h"

class ScreenLockerKcmForm;
class AppearanceSettings;
namespace ScreenLocker
{
class WallpaperIntegration;
class LnFIntegration;
}

namespace KDeclarative
{
class ConfigPropertyMap;
}


class ScreenLockerKcm : public KQuickAddons::ManagedConfigModule
{
    Q_OBJECT
public:
    explicit ScreenLockerKcm(QObject *parent = nullptr, const QVariantList& args = QVariantList());

    Q_PROPERTY(KScreenSaverSettings *settings READ settings CONSTANT)
    Q_PROPERTY(KDeclarative::ConfigPropertyMap *wallpaperConfiguration READ wallpaperConfiguration NOTIFY currentWallpaperChanged)
    Q_PROPERTY(KDeclarative::ConfigPropertyMap *lnfConfiguration READ lnfConfiguration CONSTANT)
    Q_PROPERTY(QUrl lnfConfigFile READ lnfConfigFile CONSTANT)
    Q_PROPERTY(QUrl wallpaperConfigFile READ wallpaperConfigFile NOTIFY currentWallpaperChanged)
    Q_PROPERTY(ScreenLocker::WallpaperIntegration *wallpaperIntegration READ wallpaperIntegration NOTIFY currentWallpaperChanged)
    Q_PROPERTY(QString currentWallpaper READ currentWallpaper NOTIFY currentWallpaperChanged)
    Q_PROPERTY(bool isDefaultsAppearance READ isDefaultsAppearance NOTIFY isDefaultsAppearanceChanged)

    Q_INVOKABLE QVector<WallpaperInfo> availableWallpaperPlugins() {
        return KScreenSaverSettings::getInstance().availableWallpaperPlugins();
    }

    KScreenSaverSettings *settings() const;
    QUrl lnfConfigFile() const;
    QUrl wallpaperConfigFile() const;
    ScreenLocker::WallpaperIntegration *wallpaperIntegration() const;
    QString currentWallpaper() const;
    bool isDefaultsAppearance() const;

public Q_SLOTS:
    void load() override;
    void save() override;
    void defaults() override;
    void updateState();

Q_SIGNALS:
    void currentWallpaperChanged();
    void isDefaultsAppearanceChanged();

private:
    bool isSaveNeeded() const override;
    bool isDefaults() const override;

    KDeclarative::ConfigPropertyMap *wallpaperConfiguration() const;
    KDeclarative::ConfigPropertyMap *lnfConfiguration() const;

    AppearanceSettings *m_appearanceSettings;
    QString m_currentWallpaper;
};

#endif
