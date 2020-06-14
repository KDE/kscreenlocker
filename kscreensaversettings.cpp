/********************************************************************
 This file is part of the KDE project.

 Copyright 2019 Kevin Ottens <kevin.ottens@enioka.com>

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

#include "kscreensaversettings.h"

#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPackage/Package>
#include <KPackage/PackageLoader>

QList<QKeySequence> KScreenSaverSettings::defaultShortcuts()
{
    return {
        Qt::META + Qt::Key_L,
        Qt::ALT + Qt::CTRL + Qt::Key_L,
        Qt::Key_ScreenSaver
    };
}

QString KScreenSaverSettings::defaultWallpaperPlugin()
{
    return QStringLiteral("org.kde.image");
}

KScreenSaverSettings::KScreenSaverSettings(QObject *parent)
    : KScreenSaverSettingsBase()
    , m_actionCollection(new KActionCollection(this, QStringLiteral("ksmserver")))
    , m_lockAction(nullptr)
{
    setParent(parent);

    const auto wallpaperPackages = KPackage::PackageLoader::self()->listPackages(QStringLiteral("Plasma/Wallpaper"));
    for (auto &package : wallpaperPackages) {
         m_availableWallpaperPlugins.append({package.name(), package.pluginId()});
    }

    m_actionCollection->setConfigGlobal(true);
    m_actionCollection->setComponentDisplayName(i18n("Session Managment"));
    m_lockAction = m_actionCollection->addAction(QStringLiteral("Lock Session"));
    m_lockAction->setProperty("isConfigurationAction", true);
    m_lockAction->setText(i18n("Lock Session"));
    KGlobalAccel::self()->setShortcut(m_lockAction, defaultShortcuts());

    addItem(new KPropertySkeletonItem(this, "shortcut", defaultShortcuts().first()), QStringLiteral("lockscreenShortcut"));
    addItem(new KPropertySkeletonItem(this, "wallpaperPluginIndex", indexFromWallpaperPluginId(defaultWallpaperPlugin())),
            QStringLiteral("wallpaperPluginIndex"));
}

KScreenSaverSettings::~KScreenSaverSettings()
{
}

QVector<KScreenSaverSettings::WallpaperInfo> KScreenSaverSettings::availableWallpaperPlugins() const
{
    return m_availableWallpaperPlugins;
}

int KScreenSaverSettings::wallpaperPluginIndex() const
{
    return indexFromWallpaperPluginId(wallpaperPluginId());
}

void KScreenSaverSettings::setWallpaperPluginIndex(int index)
{
    Q_ASSERT(index >=0 && index < m_availableWallpaperPlugins.size());
    setWallpaperPluginId(m_availableWallpaperPlugins[index].id);

    // We get in this function during save, but wallpaperPluginId might
    // have been written already, since we're tempering with its value here
    // we make sure it really reaches the config object.
    findItem(QStringLiteral("wallpaperPluginId"))->writeConfig(config());
}

QKeySequence KScreenSaverSettings::shortcut() const
{
    const QList<QKeySequence> shortcuts = KGlobalAccel::self()->shortcut(m_lockAction);
    if (shortcuts.count() > 0) {
        return shortcuts.first();
    }
    return QKeySequence();
}

void KScreenSaverSettings::setShortcut(const QKeySequence &sequence)
{
    auto shortcuts = KGlobalAccel::self()->shortcut(m_lockAction);
    if (shortcuts.isEmpty()) {
        shortcuts << QKeySequence();
    }

    shortcuts[0] = sequence;
    KGlobalAccel::self()->setShortcut(m_lockAction, shortcuts, KGlobalAccel::NoAutoloading);
}

int KScreenSaverSettings::indexFromWallpaperPluginId(const QString &id) const
{
    const auto it = std::find_if(m_availableWallpaperPlugins.cbegin(), m_availableWallpaperPlugins.cend(),
                                 [id] (const WallpaperInfo &info) { return info.id == id; });
    if (it != m_availableWallpaperPlugins.cend()) {
        return it - m_availableWallpaperPlugins.cbegin();
    } else if (id != defaultWallpaperPlugin()) {
        return indexFromWallpaperPluginId(defaultWallpaperPlugin());
    } else {
        return -1;
    }
}
