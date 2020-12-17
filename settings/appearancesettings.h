/********************************************************************
 This file is part of the KDE project.

 Copyright 2020 Cyril Rossi <cyril.rossi@enioka.com>

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

#ifndef APPEARANCESETTINGS_H
#define APPEARANCESETTINGS_H

#include <QObject>

#include <KCoreConfigSkeleton>
#include <KPackage/Package>


namespace ScreenLocker
{
class WallpaperIntegration;
class LnFIntegration;
}

namespace KDeclarative
{
class ConfigPropertyMap;
}

class AppearanceSettings : public QObject
{
    Q_OBJECT

public:
    explicit AppearanceSettings(QObject *parent = nullptr);

    QUrl lnfConfigFile() const;
    QUrl wallpaperConfigFile() const;

    KDeclarative::ConfigPropertyMap *wallpaperConfiguration() const;
    KDeclarative::ConfigPropertyMap *lnfConfiguration() const;

    ScreenLocker::WallpaperIntegration *wallpaperIntegration() const;

    void load();
    void save();
    void defaults();

    bool isDefaults() const;
    bool isSaveNeeded() const;

    void loadWallpaperConfig();

Q_SIGNALS:
    void currentWallpaperChanged();

private:
    void loadLnfConfig();

    KPackage::Package m_package;
    ScreenLocker::WallpaperIntegration *m_wallpaperIntegration = nullptr;
    KCoreConfigSkeleton *m_wallpaperSettings = nullptr;
    QUrl m_wallpaperConfigFile;
    ScreenLocker::LnFIntegration* m_lnfIntegration = nullptr;
    KCoreConfigSkeleton *m_lnfSettings = nullptr;
    QUrl m_lnfConfigFile;
};

#endif // APPEARANCESETTINGS_H
