/*
 * Copyright 2020  David Edmundson <davidedmundson@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3, or any
 * later version accepted by the membership of KDE e.V. (or its
 * successor approved by the membership of KDE e.V.), which shall
 * act as a proxy defined in Section 6 of version 3 of the license.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "pamauthenticator.h"

#include <QDebug>
#include <QEventLoop>
#include <security/pam_appl.h>

#include "kscreenlocker_greet_logging.h"

class PamWorker : public QObject
{
    Q_OBJECT
public:
    PamWorker();
    ~PamWorker();
    void start(const QString &service, const QString &user);
    void authenticate();

Q_SIGNALS:
    void prompt(const QString &prompt);
    void finished(bool success);

    // internal
    void promptReceived(const QByteArray &prompt);
    void cancelled();

private:
    static int converse(int n, const struct pam_message **msg, struct pam_response **resp, void *data);

    pam_handle_t *m_handle = nullptr; //< the actual PAM handle
    struct pam_conv m_conv;

    int m_result;
};

int PamWorker::converse(int n, const struct pam_message **msg, struct pam_response **resp, void *data)
{
    PamWorker *c = static_cast<PamWorker *>(data);

    *resp = (struct pam_response *)calloc(n, sizeof(struct pam_response));
    if (!*resp) {
        return PAM_BUF_ERR;
    }

    for (int i = 0; i < n; i++) {
        switch (msg[i]->msg_style) {
        case PAM_PROMPT_ECHO_OFF:
        case PAM_PROMPT_ECHO_ON: {
            const QString prompt = QString::fromLocal8Bit(msg[i]->msg);
            Q_EMIT c->prompt(prompt);

            QByteArray response;
            QEventLoop e;
            QObject::connect(c, &PamWorker::promptReceived, &e, [&](const QByteArray &_response) {
                response = _response;
                e.exit(0);
            });
            QObject::connect(c, &PamWorker::cancelled, &e, [&]() {
                e.exit(PAM_CONV_ERR);
            });

            int rc = e.exec();
            if (rc != 0) {
                return rc;
            }

            resp[i]->resp = (char *)malloc(response.length() + 1);
            // on error, get rid of everything
            if (!resp[i]->resp) {
                for (int j = 0; j < n; j++) {
                    free(resp[i]->resp);
                    resp[i]->resp = nullptr;
                }
                free(*resp);
                *resp = nullptr;
                return PAM_BUF_ERR;
            }

            memcpy(resp[i]->resp, response.constData(), response.length());
            resp[i]->resp[response.length()] = '\0';

            break;
        }
        case PAM_ERROR_MSG:
            qCDebug(KSCREENLOCKER_GREET) << QString::fromLocal8Bit(msg[i]->msg);
            break;
        case PAM_TEXT_INFO:
            // if there's only the info message, let's predict the prompts too
            qCDebug(KSCREENLOCKER_GREET) << QString::fromLocal8Bit(msg[i]->msg);
            break;
        default:
            break;
        }
    }

    return PAM_SUCCESS;
}

PamWorker::PamWorker()
    : QObject(nullptr)
{
    m_conv = {&PamWorker::converse, this};
}

PamWorker::~PamWorker()
{
    if (m_handle) {
        pam_end(m_handle, PAM_SUCCESS);
    }
}

void PamWorker::authenticate()
{
    qCDebug(KSCREENLOCKER_GREET) << "Start auth";
    int rc = pam_authenticate(m_handle, PAM_SILENT);
    qCDebug(KSCREENLOCKER_GREET) << "Auth done RC" << rc;

    bool success = rc == PAM_SUCCESS;
    Q_EMIT finished(success);
}

void PamWorker::start(const QString &service, const QString &user)
{
    if (user.isEmpty())
        m_result = pam_start(qPrintable(service), nullptr, &m_conv, &m_handle);
    else
        m_result = pam_start(qPrintable(service), qPrintable(user), &m_conv, &m_handle);

    if (m_result != PAM_SUCCESS) {
        qCWarning(KSCREENLOCKER_GREET) << "[PAM] start" << pam_strerror(m_handle, m_result);
        return;
    } else {
        qCDebug(KSCREENLOCKER_GREET) << "[PAM] Starting...";
    }
}

PamAuthenticator::PamAuthenticator(const QString &service, const QString &user)
    : QObject()
{
    d = new PamWorker;

    d->moveToThread(&m_thread);

    connect(&m_thread, &QThread::finished, d, &QObject::deleteLater);
    connect(d, &PamWorker::prompt, this, &PamAuthenticator::prompt);
    connect(d, &PamWorker::finished, this, &PamAuthenticator::finished);

    m_thread.start();
    init(service, user);
}

PamAuthenticator::~PamAuthenticator()
{
    cancel();
    m_thread.quit();
    m_thread.wait();
}

void PamAuthenticator::init(const QString &service, const QString &user)
{
    QMetaObject::invokeMethod(d, [this, service, user]() {
        d->start(service, user);
    });
}

void PamAuthenticator::authenticate()
{
    QMetaObject::invokeMethod(d, &PamWorker::authenticate);
}

void PamAuthenticator::respond(const QByteArray &response)
{
    QMetaObject::invokeMethod(
        d,
        [this, response]() {
            Q_EMIT d->promptReceived(response);
        },
        Qt::QueuedConnection);
}

void PamAuthenticator::cancel()
{
    QMetaObject::invokeMethod(d, &PamWorker::cancelled);
}

#include "pamauthenticator.moc"
