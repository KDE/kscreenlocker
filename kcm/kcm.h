/********************************************************************
 KSld - the KDE Screenlocker Daemon
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2014 Marco Martin <mart@kde.org>

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

#include <KCModule>
#include <KPackage/Package>

class KActionCollection;
class ScreenLockerKcmForm;

namespace ScreenLocker
{
class WallpaperIntegration;
class LnFIntegration;
}

namespace KDeclarative
{
class ConfigPropertyMap;
}


class ScreenLockerKcm : public KCModule
{
    Q_OBJECT
public:
    enum Roles {
        PluginNameRole = Qt::UserRole +1,
        ScreenhotRole
    };
    explicit ScreenLockerKcm(QWidget *parent = nullptr, const QVariantList& args = QVariantList());

    KDeclarative::ConfigPropertyMap *wallpaperConfiguration() const;
    KDeclarative::ConfigPropertyMap *lnfConfiguration() const;

    QString currentWallpaper() const;

    bool eventFilter(QObject *watched, QEvent *event) override;

public Q_SLOTS:
    void load() override;
    void save() override;
    void defaults() override;
    void test(const QString &plugin);

Q_SIGNALS:
    void wallpaperConfigurationChanged();
    void currentWallpaperChanged();

private:
    void shortcutChanged(const QKeySequence &key);
    bool shouldSaveShortcut();
    void loadWallpapers();
    void selectWallpaper(const QString &pluginId);
    void loadWallpaperConfig();
    void loadLnfConfig();
    KPackage::Package m_package;
    KActionCollection *m_actionCollection;
    ScreenLockerKcmForm *m_ui;
    ScreenLocker::WallpaperIntegration *m_wallpaperIntegration = nullptr;
    ScreenLocker::LnFIntegration* m_lnfIntegration = nullptr;
};

//see https://bugreports.qt.io/browse/QTBUG-57714, don't expose a QWidget as a context property
class ScreenLockerProxy : public QObject
{
    Q_OBJECT
    Q_PROPERTY(KDeclarative::ConfigPropertyMap *wallpaperConfiguration READ wallpaperConfiguration NOTIFY wallpaperConfigurationChanged)
    Q_PROPERTY(KDeclarative::ConfigPropertyMap *lnfConfiguration READ lnfConfiguration CONSTANT)

    Q_PROPERTY(QString currentWallpaper READ currentWallpaper NOTIFY currentWallpaperChanged)
public:
    ScreenLockerProxy(ScreenLockerKcm *parent) :
        QObject(parent),
        q(parent)
    {
    }

    KDeclarative::ConfigPropertyMap *wallpaperConfiguration() const {
        return q->wallpaperConfiguration();
    }
    KDeclarative::ConfigPropertyMap *lnfConfiguration() const {
        return q->lnfConfiguration();
    }

    QString currentWallpaper() const {
        return q->currentWallpaper();
    }

Q_SIGNALS:
    void wallpaperConfigurationChanged();
    void currentWallpaperChanged();

private:
    ScreenLockerKcm* q;
};
