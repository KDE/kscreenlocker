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

#include <QKeySequence>

#include "kscreensaversettingsbase.h"

class QAction;
class KActionCollection;

class KScreenSaverSettingsStore;

struct WallpaperInfo {
    Q_PROPERTY(QString name MEMBER name CONSTANT)
    Q_PROPERTY(QString id MEMBER id CONSTANT)
    QString name;
    QString id;
    Q_GADGET
};

class KScreenSaverSettings : public KScreenSaverSettingsBase
{
    Q_OBJECT
    Q_PROPERTY(QKeySequence shortcut READ shortcut WRITE setShortcut NOTIFY shortcutChanged)
public:

    static KScreenSaverSettings &getInstance();

    static QList<QKeySequence> defaultShortcuts();
    static QString defaultWallpaperPlugin();

    ~KScreenSaverSettings() override;

    QVector<WallpaperInfo> availableWallpaperPlugins() const;

    QKeySequence shortcut() const;
    void setShortcut(const QKeySequence &sequence);

    KScreenSaverSettings(KScreenSaverSettings const&) = delete;
    void operator=(KScreenSaverSettings const&)  = delete;

Q_SIGNALS:
    void shortcutChanged();

protected:
    KScreenSaverSettings(QObject *parent = nullptr);

private:
    QVector<WallpaperInfo> m_availableWallpaperPlugins;
    KScreenSaverSettingsStore *m_store;
};

#endif // KSCREENSAVERSETTINGS_H
