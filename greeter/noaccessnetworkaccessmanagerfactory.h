/*
SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef NOACCESSNETWORKACCESSMANAGERFACTORY_H
#define NOACCESSNETWORKACCESSMANAGERFACTORY_H

#include <QQmlNetworkAccessManagerFactory>

namespace ScreenLocker
{
class NoAccessNetworkAccessManagerFactory : public QQmlNetworkAccessManagerFactory
{
public:
    QNetworkAccessManager *create(QObject *parent) override;
};

}

#endif
