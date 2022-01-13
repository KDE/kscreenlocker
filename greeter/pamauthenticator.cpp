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
    void promptForSecret(const QString &msg);
    void prompt(const QString &msg);
    void infoMessage(const QString &msg);
    void errorMessage(const QString &msg);
    void failed();
    void succeeded();

    // internal
    void promptResponseReceived(const QByteArray &prompt);
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

    for (int i = 0; i < n; i++) {
        bool isSecret = false;
        switch (msg[i]->msg_style) {
        case PAM_PROMPT_ECHO_OFF: {
            isSecret = true;
            Q_FALLTHROUGH();
        case PAM_PROMPT_ECHO_ON:
            Q_ASSERT(resp);
            *resp = (struct pam_response *)calloc(n, sizeof(struct pam_response));
            if (!*resp) {
                return PAM_BUF_ERR;
            }

            const QString prompt = QString::fromLocal8Bit(msg[i]->msg);
            if (isSecret) {
                Q_EMIT c->promptForSecret(prompt);
            } else {
                Q_EMIT c->prompt(prompt);
            }

            QByteArray response;
            QEventLoop e;
            QObject::connect(c, &PamWorker::promptResponseReceived, &e, [&](const QByteArray &_response) {
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
            Q_EMIT c->errorMessage(QString::fromLocal8Bit(msg[i]->msg));
            break;
        case PAM_TEXT_INFO:
            // if there's only the info message, let's predict the prompts too
            qCDebug(KSCREENLOCKER_GREET) << QString::fromLocal8Bit(msg[i]->msg);
            Q_EMIT c->infoMessage(QString::fromLocal8Bit(msg[i]->msg));
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
    int rc = pam_authenticate(m_handle, 0); // PAM_SILENT);
    qCDebug(KSCREENLOCKER_GREET) << "Auth done RC" << rc;

    if (rc == PAM_SUCCESS) {
        Q_EMIT succeeded();
    } else {
        Q_EMIT failed();
    }
}

static void fail_delay(int retval, unsigned usec_delay, void *appdata_ptr)
{
    Q_UNUSED(retval);
    Q_UNUSED(usec_delay);
    Q_UNUSED(appdata_ptr);
}

void PamWorker::start(const QString &service, const QString &user)
{
    if (user.isEmpty())
        m_result = pam_start(qPrintable(service), nullptr, &m_conv, &m_handle);
    else
        m_result = pam_start(qPrintable(service), qPrintable(user), &m_conv, &m_handle);

    // get errors quicker
#ifdef PAM_FAIL_DELAY
    pam_set_item(m_handle, PAM_FAIL_DELAY, (void *)fail_delay);
#endif

    if (m_result != PAM_SUCCESS) {
        qCWarning(KSCREENLOCKER_GREET) << "[PAM] start" << pam_strerror(m_handle, m_result);
        return;
    } else {
        qCDebug(KSCREENLOCKER_GREET) << "[PAM] Starting...";
    }
}

PamAuthenticator::PamAuthenticator(const QString &service, const QString &user, QObject *parent)
    : QObject(parent)
{
    d = new PamWorker;

    d->moveToThread(&m_thread);

    connect(&m_thread, &QThread::finished, d, &QObject::deleteLater);

    connect(d, &PamWorker::prompt, this, &PamAuthenticator::prompt);
    connect(d, &PamWorker::promptForSecret, this, &PamAuthenticator::promptForSecret);
    connect(d, &PamWorker::infoMessage, this, &PamAuthenticator::infoMessage);
    connect(d, &PamWorker::errorMessage, this, &PamAuthenticator::errorMessage);

    connect(d, &PamWorker::succeeded, this, &PamAuthenticator::succeeded);
    connect(d, &PamWorker::failed, this, &PamAuthenticator::failed);

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

void PamAuthenticator::tryUnlock()
{
    QMetaObject::invokeMethod(d, &PamWorker::authenticate);
}

void PamAuthenticator::respond(const QByteArray &response)
{
    QMetaObject::invokeMethod(
        d,
        [this, response]() {
            Q_EMIT d->promptResponseReceived(response);
        },
        Qt::QueuedConnection);
}

void PamAuthenticator::cancel()
{
    QMetaObject::invokeMethod(d, &PamWorker::cancelled);
}

#include "pamauthenticator.moc"
