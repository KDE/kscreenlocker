/********************************************************************
 KSld - the KDE Screenlocker Daemon
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2019 Kevin Ottens <kevin.ottens@enioka.com>

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
#include "../greeter/wallpaper_integration.h"
#include "../greeter/lnf_integration.h"

#include <config-kscreenlocker.h>
#include <KConfigLoader>
#include <KConfigDialogManager>
#include <KGlobalAccel>
#include <KCModule>
#include <KPluginFactory>
#include <QVBoxLayout>
#include <QMessageBox>

#include <KPackage/Package>
#include <KPackage/PackageLoader>

#include <QQmlContext>
#include <QQuickItem>

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
    layout()->setContentsMargins(0, 0, 0, 0);
    kcfg_Timeout->setSuffix(ki18ncp("Spinbox suffix"," minute"," minutes"));

    kcfg_LockGrace->setSuffix(ki18ncp("Spinbox suffix"," second"," seconds"));
}



ScreenLockerKcm::ScreenLockerKcm(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
    , m_settings(new KScreenSaverSettings(this))
    , m_ui(new ScreenLockerKcmForm(this))
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_ui);
    for (const auto &pluginInfo : m_settings->availableWallpaperPlugins()) {
        m_ui->kcfg_wallpaperPluginIndex->addItem(pluginInfo.name, pluginInfo.id);
    }
    connect(m_ui->kcfg_wallpaperPluginIndex, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &ScreenLockerKcm::loadWallpaperConfig);
    m_ui->kcfg_wallpaperPluginIndex->installEventFilter(this);
    m_ui->installEventFilter(this);

    auto proxy = new ScreenLockerProxy(this);

    m_ui->wallpaperConfigWidget->setClearColor(m_ui->palette().color(QPalette::Active, QPalette::Window));
    m_ui->wallpaperConfigWidget->rootContext()->setContextProperty(QStringLiteral("configDialog"), proxy);
    m_ui->wallpaperConfigWidget->setSource(QUrl(QStringLiteral("qrc:/kscreenlocker-kcm-resources/wallpaperconfig.qml")));
    connect(m_ui->wallpaperConfigWidget->rootObject(), SIGNAL(configurationChanged()), this, SLOT(updateState()));

    m_ui->lnfConfigWidget->setClearColor(m_ui->palette().color(QPalette::Active, QPalette::Window));
    m_ui->lnfConfigWidget->rootContext()->setContextProperty(QStringLiteral("configDialog"), proxy);
    m_ui->lnfConfigWidget->setSource(QUrl(QStringLiteral("qrc:/kscreenlocker-kcm-resources/lnfconfig.qml")));
    connect(m_ui->lnfConfigWidget->rootObject(), SIGNAL(configurationChanged()), this, SLOT(updateState()));

    addConfig(m_settings, m_ui);
}

void ScreenLockerKcm::load()
{
    KCModule::load();

    loadWallpaperConfig();
    loadLnfConfig();

    if (m_lnfSettings) {
        m_lnfSettings->load();
        emit m_lnfSettings->configChanged(); // To force the ConfigPropertyMap to reevaluate
    }

    if (m_wallpaperSettings) {
        m_wallpaperSettings->load();
        emit m_wallpaperSettings->configChanged(); // To force the ConfigPropertyMap to reevaluate
    }

    updateState();
}

void ScreenLockerKcm::save()
{
    KCModule::save();
    if (m_lnfSettings) {
        m_lnfSettings->save();
    }

    if (m_wallpaperSettings) {
        m_wallpaperSettings->save();
    }

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
    KCModule::defaults();

    if (m_lnfSettings) {
        m_lnfSettings->setDefaults();
        emit m_lnfSettings->configChanged(); // To force the ConfigPropertyMap to reevaluate
    }

    if (m_wallpaperSettings) {
        m_wallpaperSettings->setDefaults();
        emit m_wallpaperSettings->configChanged(); // To force the ConfigPropertyMap to reevaluate
    }

    updateState();
}

void ScreenLockerKcm::updateState()
{
    bool isDefaults = m_settings->isDefaults();
    bool isSaveNeeded = m_settings->isSaveNeeded();

    if (m_lnfSettings) {
        isDefaults &= m_lnfSettings->isDefaults();
        isSaveNeeded |= m_lnfSettings->isSaveNeeded();
    }

    if (m_wallpaperSettings) {
        isDefaults &= m_wallpaperSettings->isDefaults();
        isSaveNeeded |= m_wallpaperSettings->isSaveNeeded();
    }

    emit changed(isSaveNeeded);
    emit defaulted(isDefaults);
}

void ScreenLockerKcm::loadWallpaperConfig()
{
    if (m_wallpaperIntegration) {
        if (m_wallpaperIntegration->pluginName() == m_ui->kcfg_wallpaperPluginIndex->currentData().toString()) {
            // nothing changed
            return;
        }
        delete m_wallpaperIntegration;
    }
    emit currentWallpaperChanged();

    m_wallpaperIntegration = new ScreenLocker::WallpaperIntegration(this);
    m_wallpaperIntegration->setConfig(m_settings->sharedConfig());
    m_wallpaperIntegration->setPluginName(m_ui->kcfg_wallpaperPluginIndex->currentData().toString());
    m_wallpaperIntegration->init();
    m_wallpaperSettings = m_wallpaperIntegration->configScheme();
    m_ui->wallpaperConfigWidget->rootContext()->setContextProperty(QStringLiteral("wallpaper"), m_wallpaperIntegration);
    emit wallpaperConfigurationChanged();
    m_ui->wallpaperConfigWidget->rootObject()->setProperty("sourceFile", m_wallpaperIntegration->package().filePath(QByteArrayLiteral("ui"), QStringLiteral("config.qml")));
}

void ScreenLockerKcm::loadLnfConfig()
{
    if (m_package.isValid() && m_lnfIntegration) {
        return;
    }

    Q_ASSERT(!m_package.isValid() && !m_lnfIntegration);

    m_package = KPackage::PackageLoader::self()->loadPackage(QStringLiteral("Plasma/LookAndFeel"));
    KConfigGroup cg(KSharedConfig::openConfig(QStringLiteral("kdeglobals")), "KDE");
    const QString packageName = cg.readEntry("LookAndFeelPackage", QString());
    if (!packageName.isEmpty()) {
        m_package.setPath(packageName);
    }

    m_lnfIntegration = new ScreenLocker::LnFIntegration(this);
    m_lnfIntegration->setPackage(m_package);
    m_lnfIntegration->setConfig(m_settings->sharedConfig());
    m_lnfIntegration->init();
    m_lnfSettings = m_lnfIntegration->configScheme();

    auto sourceFile = m_package.fileUrl(QByteArrayLiteral("lockscreen"), QStringLiteral("config.qml"));
    if (sourceFile.isEmpty()) {
        m_ui->lnfConfigWidget->hide();
        return;
    }
    m_ui->lnfConfigWidget->rootObject()->setProperty("sourceFile", sourceFile);
}

KDeclarative::ConfigPropertyMap * ScreenLockerKcm::wallpaperConfiguration() const
{
    if (!m_wallpaperIntegration) {
        return nullptr;
    }
    return m_wallpaperIntegration->configuration();
}

KDeclarative::ConfigPropertyMap * ScreenLockerKcm::lnfConfiguration() const
{
    if (!m_lnfIntegration) {
        return nullptr;
    }
    return m_lnfIntegration->configuration();
}


QString ScreenLockerKcm::currentWallpaper() const
{
    return m_ui->kcfg_wallpaperPluginIndex->currentData().toString();
}

bool ScreenLockerKcm::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_ui) {
        if (event->type() == QEvent::PaletteChange) {
            m_ui->wallpaperConfigWidget->setClearColor(m_ui->palette().color(QPalette::Active, QPalette::Window));
        }
        return false;
    }
    if (watched != m_ui->kcfg_wallpaperPluginIndex) {
        return false;
    }
    if (event->type() == QEvent::Move) {
        if (auto object = m_ui->wallpaperConfigWidget->rootObject()) {
            // QtQuick Layouts have a hardcoded 5 px spacing by default
            object->setProperty("formAlignment", m_ui->kcfg_wallpaperPluginIndex->x() + 5);
        }
        if (auto object = m_ui->lnfConfigWidget->rootObject()) {
            // QtQuick Layouts have a hardcoded 5 px spacing by default
            object->setProperty("formAlignment", m_ui->kcfg_wallpaperPluginIndex->x() + 5);
        }

    }
    return false;
}

K_PLUGIN_CLASS_WITH_JSON(ScreenLockerKcm, "screenlocker.json")

#include "kcm.moc"
