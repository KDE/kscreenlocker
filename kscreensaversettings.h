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
#ifndef KSCREENSAVERSETTINGS_H
#define KSCREENSAVERSETTINGS_H

#include "kscreensaversettingsbase.h"

class QAction;
class KActionCollection;

class KScreenSaverSettings : public KScreenSaverSettingsBase
{
    Q_OBJECT
    Q_PROPERTY(QKeySequence shortcut READ shortcut WRITE setShortcut)
    Q_PROPERTY(int wallpaperPluginIndex READ wallpaperPluginIndex WRITE setWallpaperPluginIndex)
public:
    struct WallpaperInfo {
        QString name;
        QString id;
    };

    static QList<QKeySequence> defaultShortcuts();
    static QString defaultWallpaperPlugin();

    KScreenSaverSettings(QObject *parent = nullptr);
    ~KScreenSaverSettings() override;

    QVector<WallpaperInfo> availableWallpaperPlugins() const;
    int wallpaperPluginIndex() const;
    void setWallpaperPluginIndex(int index);

    QKeySequence shortcut() const;
    void setShortcut(const QKeySequence &sequence);

private:
    int indexFromWallpaperPluginId(const QString &id) const;

    QVector<WallpaperInfo> m_availableWallpaperPlugins;
    KActionCollection *m_actionCollection;
    QAction *m_lockAction;
};

#endif // KSCREENSAVERSETTINGS_H
