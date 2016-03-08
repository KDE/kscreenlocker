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

class QQuickView;
class QStandardItemModel;
class KActionCollection;
class ScreenLockerKcmForm;

class ScreenLockerKcm : public KCModule
{
    Q_OBJECT

public:
    enum Roles {
        PluginNameRole = Qt::UserRole +1,
        ScreenhotRole
    };
    explicit ScreenLockerKcm(QWidget *parent = nullptr, const QVariantList& args = QVariantList());

public Q_SLOTS:
    void load() Q_DECL_OVERRIDE;
    void save() Q_DECL_OVERRIDE;
    void defaults() Q_DECL_OVERRIDE;
    void test(const QString &plugin);

private:
    void shortcutChanged(const QKeySequence &key);
    bool shouldSaveShortcut();
    KPackage::Package m_package;
    KActionCollection *m_actionCollection;
    ScreenLockerKcmForm *m_ui;
};
