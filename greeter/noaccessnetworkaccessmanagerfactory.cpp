/********************************************************************
 KSld - the KDE Screenlocker Daemon
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#include "noaccessnetworkaccessmanagerfactory.h"

#include <KProtocolInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace ScreenLocker
{
class AccessDeniedNetworkReply : public QNetworkReply
{
    Q_OBJECT
public:
    explicit AccessDeniedNetworkReply(QNetworkAccessManager::Operation op, const QNetworkRequest &req, QObject *parent)
        : QNetworkReply(parent)
    {
        setRequest(req);
        setOpenMode(QIODevice::ReadOnly);
        setUrl(req.url());
        setOperation(op);
        setError(QNetworkReply::ContentAccessDenied, QStringLiteral("Blocked request."));
        const auto networkError = error();
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection, Q_ARG(QNetworkReply::NetworkError, networkError));

        setFinished(true);
        Q_EMIT QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
    }
    qint64 readData(char * /*data*/, qint64 /*maxSize*/) override
    {
        return 0;
    }
    void abort() override{};
};

class NoAccessNetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT
    using QNetworkAccessManager::QNetworkAccessManager;

public:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &req, QIODevice *outgoingData) override
    {
        if (!isLocalRequest(req.url())) {
            return new ScreenLocker::AccessDeniedNetworkReply(op, req, this);
        }
        return QNetworkAccessManager::createRequest(op, req, outgoingData);
    }

private:
    bool isLocalRequest(const QUrl &url)
    {
        const QString scheme(url.scheme());
        return (KProtocolInfo::isKnownProtocol(scheme) && KProtocolInfo::protocolClass(scheme).compare(QStringLiteral(":local"), Qt::CaseInsensitive) == 0);
    }
};

QNetworkAccessManager *NoAccessNetworkAccessManagerFactory::create(QObject *parent)
{
    return new NoAccessNetworkAccessManager(parent);
}
}

#include "noaccessnetworkaccessmanagerfactory.moc"
