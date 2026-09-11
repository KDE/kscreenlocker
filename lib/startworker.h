// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2026 Harald Sitter <sitter@kde.org>

#pragma once

#include <chrono>
#include <memory>
#include <optional>

#include <QProcess>
#include <QString>

namespace PlasmaAuthentication
{

/*!
    Wrapper around the QProcess managing the PAM Worker process.
    This is either used implicitly through the org.kde.kscreenlocker modules
    or explicitly by a "manager process" that wishes to implement the Arbiter interface.

    An Arbiter would use this class to start the worker process on behalf of a greeter process
    while also providing its own arbiterAddress such that it gets notified of the authentication results.

    \warning do not forget to connect to all the signals we expose! They are there for a reason.
*/
class Q_DECL_EXPORT Worker : public QObject
{
    Q_OBJECT
public:
    /*!
        Creates a new Worker process.

        \param service The name of the PAM service to use. e.g. "kde-fingerprint"
        \param user The name of the user to authentication. When not provided the current user gets authenticated.
        \param greeterAddress The Peer2Peer DBus address of the greeter (the frontend)
        \param arbiterAddress The Peer2Peer DBus address of the arbiter (the entity in charge of the authentication).
            When not provided no arbiter will be used. See org.kde.plasma.screenlocker.Arbiter.xml
    */
    Worker(const QString &service, const std::optional<QString> &user, const QString &greeterAddress, const std::optional<QString> &arbiterAddress);

    /*! Start the process */
    void start();

    /*! Wait for the process to finish */
    [[nodiscard]] bool waitForFinished(std::chrono::milliseconds timeout);

    /*! Send SIGTERM. May not exit. */
    void terminate();

    /*! Send SIGKILL. Will definitely exit. */
    void kill();

Q_SIGNALS:
    /*! Same as the QProcess signal. Useful to mark the authenticator unavailable on error */
    void errorOccurred(QProcess::ProcessError error);

private:
    std::unique_ptr<QProcess> m_workerProcess;
    QByteArray m_payload;
};

} // namespace PlasmaAuthentication
