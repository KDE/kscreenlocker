/********************************************************************
 KSld - the KDE Screenlocker Daemon
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#include "ui_kcm.h"
#include "screenlocker_interface.h"
#include <config-kscreenlocker.h>
#include <KActionCollection>
#include <KGlobalAccel>
#include <KCModule>
#include <KPluginFactory>
#include <KConfigDialogManager>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QStandardItemModel>

#include <KPackage/Package>
#include <KPackage/PackageLoader>

static const QString s_lockActionName = QStringLiteral("Lock Session");

class ScreenLockerKcmForm : public QWidget, public Ui::ScreenLockerKcmForm
{
    Q_OBJECT
public:
    explicit ScreenLockerKcmForm(QWidget *parent);
};

ScreenLockerKcmForm::ScreenLockerKcmForm(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
    kcfg_Timeout->setSuffix(ki18ncp("Spinbox suffix. Short for minutes"," min"," mins"));

    kcfg_LockGrace->setSuffix(ki18ncp("Spinbox suffix. Short for seconds"," sec"," secs"));
}



ScreenLockerKcm::ScreenLockerKcm(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
    , m_actionCollection(new KActionCollection(this, QStringLiteral("ksmserver")))
    , m_ui(new ScreenLockerKcmForm(this))
{
    KConfigDialogManager::changedMap()->insert(QStringLiteral("SelectImageButton"), SIGNAL(imagePathChanged(QString)));

    addConfig(KScreenSaverSettings::self(), m_ui);

    m_actionCollection->setConfigGlobal(true);
    QAction *a = m_actionCollection->addAction(s_lockActionName);
    a->setProperty("isConfigurationAction", true);
    m_ui->lockscreenShortcut->setCheckForConflictsAgainst(KKeySequenceWidget::None);
    a->setText(i18n("Lock Session"));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>{Qt::ALT+Qt::CTRL+Qt::Key_L, Qt::Key_ScreenSaver});
    connect(m_ui->lockscreenShortcut, &KKeySequenceWidget::keySequenceChanged, this, &ScreenLockerKcm::shortcutChanged);
}

void ScreenLockerKcm::shortcutChanged(const QKeySequence &key)
{
    if (QAction *a = m_actionCollection->action(s_lockActionName)) {
        auto shortcuts = KGlobalAccel::self()->shortcut(a);
        m_ui->lockscreenShortcut->setProperty("changed", !shortcuts.contains(key));
    }
    changed();
}

void ScreenLockerKcm::load()
{
    KCModule::load();

    m_package = KPackage::PackageLoader::self()->loadPackage(QStringLiteral("Plasma/LookAndFeel"));
    KConfigGroup cg(KSharedConfig::openConfig(QStringLiteral("kdeglobals")), "KDE");
    const QString packageName = cg.readEntry("LookAndFeelPackage", QString());
    if (!packageName.isEmpty()) {
        m_package.setPath(packageName);
    }

    if (QAction *a = m_actionCollection->action(s_lockActionName)) {
        auto shortcuts = KGlobalAccel::self()->shortcut(a);
        if (!shortcuts.isEmpty()) {
            m_ui->lockscreenShortcut->setKeySequence(shortcuts.first());
        }
    }
}

void ScreenLockerKcm::test(const QString &plugin)
{
    if (plugin.isEmpty() || plugin == QLatin1String("none")) {
        return;
    }

    QProcess proc;
    QStringList arguments;
    arguments << plugin << QStringLiteral("--testing");
    if (proc.execute(KSCREENLOCKER_GREET_BIN, arguments)) {
        QMessageBox::critical(this, i18n("Error"), i18n("Failed to successfully test the screen locker."));
    }
}

void ScreenLockerKcm::save()
{
    if (!shouldSaveShortcut()) {
        QMetaObject::invokeMethod(this, "changed", Qt::QueuedConnection);
        return;
    }
    KCModule::save();

    KScreenSaverSettings::self()->save();
    if (m_ui->lockscreenShortcut->property("changed").toBool()) {
        if (QAction *a = m_actionCollection->action(s_lockActionName)) {
            KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>{m_ui->lockscreenShortcut->keySequence()}, KGlobalAccel::NoAutoloading);
            m_actionCollection->writeSettings();
        }
        m_ui->lockscreenShortcut->setProperty("changed", false);
    }
    // reconfigure through DBus
    OrgKdeScreensaverInterface interface(QStringLiteral("org.kde.screensaver"),
                                         QStringLiteral("/ScreenSaver"),
                                         QDBusConnection::sessionBus());
    if (interface.isValid()) {
        interface.configure();
    }
}

bool ScreenLockerKcm::shouldSaveShortcut()
{
    if (m_ui->lockscreenShortcut->property("changed").toBool()) {
        const QKeySequence &sequence = m_ui->lockscreenShortcut->keySequence();
        auto conflicting = KGlobalAccel::getGlobalShortcutsByKey(sequence);
        if (!conflicting.isEmpty()) {
            // Inform and ask the user about the conflict and reassigning
            // the keys sequence
            if (!KGlobalAccel::promptStealShortcutSystemwide(this, conflicting, sequence)) {
                return false;
            }
            KGlobalAccel::stealShortcutSystemwide(sequence);
        }
    }
    return true;
}

void ScreenLockerKcm::defaults()
{
    KCModule::defaults();
    m_ui->lockscreenShortcut->setKeySequence(Qt::ALT+Qt::CTRL+Qt::Key_L);
}

K_PLUGIN_FACTORY_WITH_JSON(ScreenLockerKcmFactory,
                           "screenlocker.json",
                           registerPlugin<ScreenLockerKcm>();)

#include "kcm.moc"
