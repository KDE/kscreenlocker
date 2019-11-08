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

QList<QKeySequence> KScreenSaverSettings::defaultShortcuts()
{
    return {
        Qt::META + Qt::Key_L,
        Qt::ALT + Qt::CTRL + Qt::Key_L,
        Qt::Key_ScreenSaver
    };
}

KScreenSaverSettings::KScreenSaverSettings(QObject *parent)
    : KScreenSaverSettingsBase()
    , m_actionCollection(new KActionCollection(this, QStringLiteral("ksmserver")))
    , m_lockAction(nullptr)
{
    setParent(parent);

    m_actionCollection->setConfigGlobal(true);
    m_lockAction = m_actionCollection->addAction(QStringLiteral("Lock Session"));
    m_lockAction->setProperty("isConfigurationAction", true);
    m_lockAction->setText(i18n("Lock Session"));
    KGlobalAccel::self()->setShortcut(m_lockAction, defaultShortcuts());

    addItem(new KPropertySkeletonItem(this, "shortcut", defaultShortcuts().first()), QStringLiteral("lockscreenShortcut"));
}

KScreenSaverSettings::~KScreenSaverSettings()
{
}

QKeySequence KScreenSaverSettings::shortcut() const
{
    return KGlobalAccel::self()->shortcut(m_lockAction).first();
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
