/********************************************************************
 KSld - the KDE Screenlocker Daemon
 This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2017 David Edmundson <davidedmundson@kde.org>

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
#include "lnf_integration.h"

#include <KConfig>
#include <KConfigGroup>
#include <KConfigLoader>
#include <KDeclarative/ConfigPropertyMap>

#include <QFile>

namespace ScreenLocker
{

LnFIntegration::LnFIntegration(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<KDeclarative::ConfigPropertyMap*>();
}

LnFIntegration::~LnFIntegration() = default;

void LnFIntegration::init()
{
    if (!m_package.isValid()) {
        return;
    }
    if (auto config = configScheme()) {
        m_configuration = new KDeclarative::ConfigPropertyMap(config, this);
    }
}


KConfigLoader *LnFIntegration::configScheme()
{
    if (!m_configLoader) {
        const QString xmlPath = m_package.filePath(QByteArrayLiteral("lockscreen"), QStringLiteral("config.xml"));

        const KConfigGroup cfg = m_config->group("Greeter").group("LnF");

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
