/********************************************************************
 KSld - the KDE Screenlocker Daemon
 This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "wallpaper_integration.h"

#include <KConfig>
#include <KConfigGroup>
#include <KConfigLoader>
#include <KPackage/PackageLoader>
#include <KDeclarative/ConfigPropertyMap>

#include <QFile>

namespace ScreenLocker
{

WallpaperIntegration::WallpaperIntegration(QObject *parent)
    : QObject(parent)
    , m_package(KPackage::PackageLoader::self()->loadPackage(QStringLiteral("Plasma/Wallpaper")))
{
    qRegisterMetaType<KDeclarative::ConfigPropertyMap*>();
}

WallpaperIntegration::~WallpaperIntegration() = default;

void WallpaperIntegration::init()
{
    if (!m_package.isValid()) {
        return;
    }
    if (auto config = configScheme()) {
        m_configuration = new KDeclarative::ConfigPropertyMap(config, this);
        m_configuration->setAutosave(false);
        // potd (picture of the day) is using a kded to monitor changes and
        // cache data for the lockscreen. Let's notify it.
        m_configuration->setNotify(true);
    }
}

void WallpaperIntegration::setPluginName(const QString &name)
{
    if (m_pluginName == name) {
        return;
    }
    m_pluginName = name;
    m_package.setPath(name);
    emit packageChanged();
}

KConfigLoader *WallpaperIntegration::configScheme()
{
    if (!m_configLoader) {
        const QString xmlPath = m_package.filePath(QByteArrayLiteral("config"), QStringLiteral("main.xml"));

        const KConfigGroup cfg = m_config->group("Greeter").group("Wallpaper").group(m_pluginName);

        if (xmlPath.isEmpty()) {
            m_configLoader = new KConfigLoader(cfg, nullptr, this);
        } else {
            QFile file(xmlPath);
            m_configLoader = new KConfigLoader(cfg, &file, this);
        }
    }
    return m_configLoader;
}

}
