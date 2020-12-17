/********************************************************************
 This file is part of the KDE project.

Copyright (C) 2020 Cyril Rossi <cyril.rossi@enioka.com>

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

#ifndef KSCREENLOCKERDATA_H
#define KSCREENLOCKERDATA_H

#include <QObject>

#include "kcmoduledata.h"

class AppearanceSettings;

class KScreenLockerData : public KCModuleData
{
    Q_OBJECT

public:
    explicit KScreenLockerData(QObject *parent = nullptr, const QVariantList &args = QVariantList());

    bool isDefaults() const override;

private:
    AppearanceSettings *m_appearanceSettings = nullptr;
};

#endif
