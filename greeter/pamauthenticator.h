/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <QObject>
#include <QThread>

class PamWorker;

class PamAuthenticator : public QThread
{
    Q_OBJECT

    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)

public:
    PamAuthenticator(const QString &service, const QString &user, QObject *parent = nullptr);
    ~PamAuthenticator();

    bool isBusy() const;
    bool isUnlocked() const;

    void run();

Q_SIGNALS:
    void busyChanged();
    void promptForSecret(const QString &msg);
    void prompt(const QString &msg);
    void infoMessage(const QString &msg);
    void errorMessage(const QString &msg);
    void succeeded();
    void failed();

    void tryUnlock();
    void respond(const QByteArray &response);
    void cancel();

protected:
    void init(const QString &service, const QString &user);

private:
    void setBusy(bool busy);
    const QString m_service;
    const QString m_user;

    bool m_busy = false;
    bool m_unlocked = false;
    PamWorker *d;
};
