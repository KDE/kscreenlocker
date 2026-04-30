/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2026 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "pamauthenticator.h"

#include <algorithm>
#include <csignal>

#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QMetaMethod>
#include <QThread>
#include <QtConcurrent/QtConcurrentRun>
#include <security/pam_appl.h>

#include "kscreenlocker_greet_logging.h"

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

namespace
{
template<typename Output, typename Input>
Output narrow(Input i)
{
    Output o = i;
    if (i != Input(o)) {
        std::abort();
    }
    if (const auto sameSignedness = (std::is_signed_v<Input> && std::is_signed_v<Output>); !sameSignedness && ((i < Input{}) != (o < Output{}))) {
        std::abort();
    }
    return o;
}
} // namespace

class PamWorker : public QObject
{
    Q_OBJECT
public:
    PamWorker(bool fingerprint);
    ~PamWorker() override;
    Q_DISABLE_COPY_MOVE(PamWorker)
    void start(const QString &service, const QString &user);
    void authenticate();
    void startFailedDelay(uint useconds);

Q_SIGNALS:
    void busyChanged(bool busy);
    void promptForSecret(const QString &msg);
    void prompt(const QString &msg);
    void infoMessage(const QString &msg);
    void errorMessage(const QString &msg);
    void failed();
    void loginFailedDelayStarted(const uint uSecDelay);
    void succeeded();
    void unavailabilityChanged(bool unavailable);
    void inAuthenticateChanged(bool inAuthenticate);

    // internal
    void promptResponseReceived(const QByteArray &prompt);
    void cancelled();
    void interrupt();

private:
    static int converse(int n, const struct pam_message **msg, struct pam_response **resp, void *data);

    bool m_fingerprint;
    pam_handle_t *m_handle = nullptr; //< the actual PAM handle
    struct pam_conv m_conv;

    bool m_unavailable = false;
    bool m_inAuthenticate = false;
    std::chrono::steady_clock::time_point m_nextAttemptAllowedTime;
    int m_result = -1;
    QString m_service;
};

int PamWorker::converse(int n, const struct pam_message **msg, struct pam_response **resp, void *data)
{
    PamWorker *c = static_cast<PamWorker *>(data);

    if (!resp) {
        qCWarning(KSCREENLOCKER_GREET) << "[PAM worker] Converse called with null resp pointer";
        return PAM_BUF_ERR;
    }

    const auto nSize = narrow<size_t>(n);

    *resp = std::make_unique<struct pam_response[]>(nSize).release();
    auto responses = std::span{*resp, nSize};

    auto messages = std::span{msg, nSize};
    Q_ASSERT_X(responses.size() == messages.size(), Q_FUNC_INFO, "Number of PAM messages and responses should be the same");

    for (const auto &[pamMessage, pamResponse] : std::views::zip(messages, responses)) {
        bool isSecret = false;
        switch (pamMessage->msg_style) {
        case PAM_PROMPT_ECHO_OFF: {
            isSecret = true;
            Q_FALLTHROUGH();
        case PAM_PROMPT_ECHO_ON:
            Q_EMIT c->busyChanged(false);

            const QString prompt = QString::fromLocal8Bit(pamMessage->msg);
            if (isSecret) {
                Q_EMIT c->promptForSecret(prompt);
            } else {
                Q_EMIT c->prompt(prompt);
            }

            qCDebug(KSCREENLOCKER_GREET,
                    "[PAM worker %s] Message: %s: %s",
                    qUtf8Printable(c->m_service),
                    (isSecret ? "Echo-off prompt" : "Echo-on prompt"),
                    qUtf8Printable(prompt));

            QByteArray response;
            QEventLoop e;
            QObject::connect(c, &PamWorker::promptResponseReceived, &e, [&](const QByteArray &_response) {
                qCDebug(KSCREENLOCKER_GREET, "[PAM worker %s] Received response, exiting nested event loop", qUtf8Printable(c->m_service));
                response = _response;
                e.exit(0);
            });
            QObject::connect(c, &PamWorker::cancelled, &e, [&]() {
                qCDebug(KSCREENLOCKER_GREET, "[PAM worker %s] Received cancellation, exiting with PAM_CONV_ERR", qUtf8Printable(c->m_service));
                e.exit(PAM_CONV_ERR);
            });

            qCDebug(KSCREENLOCKER_GREET, "[PAM worker %s] Starting nested event loop to await response", qUtf8Printable(c->m_service));
            // We are in a non-gui thread. It should be mostly fine to exec() here.
            int rc = e.exec();
            if (rc != 0) {
                qCDebug(KSCREENLOCKER_GREET, "[PAM worker %s] Nested event loop's exit code was not zero, bailing", qUtf8Printable(c->m_service));
                return rc;
            }

            Q_EMIT c->busyChanged(true);

            const auto responseLengthIncludingNull = response.length() + 1; // QByteArray holds an implicit \0 at the end.
            pamResponse.resp = std::make_unique<char[]>(responseLengthIncludingNull).release();
            std::copy_n(response.constData(), responseLengthIncludingNull, pamResponse.resp);

            break;
        }
        case PAM_ERROR_MSG: {
            const QString error = QString::fromLocal8Bit(pamMessage->msg);
            qCDebug(KSCREENLOCKER_GREET, "[PAM worker %s] Message: Error message: %s", qUtf8Printable(c->m_service), qUtf8Printable(error));
            Q_EMIT c->errorMessage(error);
            break;
        }
        case PAM_TEXT_INFO: {
            // if there's only the info message, let's predict the prompts too
            const QString info = QString::fromLocal8Bit(pamMessage->msg);
            qCDebug(KSCREENLOCKER_GREET, "[PAM worker %s] Message: Info message: %s", qUtf8Printable(c->m_service), qUtf8Printable(info));
            Q_EMIT c->infoMessage(info);
            break;
        }
        default:
            qCDebug(KSCREENLOCKER_GREET, "[PAM worker %s] Message: Unhandled message type: %d", qUtf8Printable(c->m_service), pamMessage->msg_style);
            break;
        }
    }

    return PAM_SUCCESS;
}

PamWorker::PamWorker(bool fingerprint)
    : QObject(nullptr)
    , m_fingerprint(fingerprint)
    , m_conv({.conv=&PamWorker::converse, .appdata_ptr=this})
    , m_nextAttemptAllowedTime(std::chrono::steady_clock::now())
{
}

PamWorker::~PamWorker()
{
    if (m_handle) {
        pam_end(m_handle, PAM_SUCCESS);
    }
}

void PamWorker::authenticate()
{
    if (m_inAuthenticate || m_unavailable) {
        return;
    } else if (std::chrono::steady_clock::now() < m_nextAttemptAllowedTime) {
        qCCritical(KSCREENLOCKER_GREET, "[PAM worker %s] Authentication attempt too soon. This shouldn't happen!", qUtf8Printable(m_service));
        Q_EMIT failed();
        return;
    }

    m_inAuthenticate = true;
    Q_EMIT inAuthenticateChanged(m_inAuthenticate);
    auto scopedAuthenticate = qScopeGuard([this] {
        m_inAuthenticate = false;
        Q_EMIT inAuthenticateChanged(m_inAuthenticate);
    });

    qCDebug(KSCREENLOCKER_GREET, "[PAM worker %s] Authenticate: Starting authentication", qUtf8Printable(m_service));

    auto converseThread = pthread_self();
    auto interruptConnection = connect(
        this,
        &PamWorker::interrupt,
        this,
        [&converseThread] {
            // This is a direct call coming from the other thread. Only do thread-safe things here!
            pthread_kill(converseThread, SIGINT);
        },
        Qt::DirectConnection);

    QElapsedTimer timer;
    timer.start();

    int rc = pam_authenticate(m_handle, 0); // PAM_SILENT);
    qCDebug(KSCREENLOCKER_GREET,
            "[PAM worker %s] Authenticate: Authentication done, result code: %d (%s)",
            qUtf8Printable(m_service),
            rc,
            pam_strerror(m_handle, rc));

        qCWarning(KSCREENLOCKER_GREET) << timer.elapsed() << "ms elapsed during pam_authenticate call for service" << qUtf8Printable(m_service) << "with result code" << rc;

    disconnect(interruptConnection);

    Q_EMIT busyChanged(false);

    if (rc == PAM_SUCCESS) {
        rc = pam_setcred(m_handle, PAM_REFRESH_CRED);
        /* ignore errors on refresh credentials. If this did not work we use the old ones. */
        Q_EMIT succeeded();
    } else if (rc == PAM_AUTHINFO_UNAVAIL && m_fingerprint) {
        // For fingerprint authentication, PAM_AUTHINFO_UNAVAIL can mean any number of things, but luckily most of them
        // are fixed by simply restarting the authentication. The notable case we want to catch here is timeouts.
        constexpr auto pam_fprintMinimumTimeout = 250ms;
        // Unless this error was returned suspiciously fast. Then it probably was an actual problem.
        if (timer.durationElapsed() <= pam_fprintMinimumTimeout) {
            qCWarning(KSCREENLOCKER_GREET) << "Unexpectedly short auth error on fingerprint reader" << timer.durationElapsed();
            m_unavailable = true;
            Q_EMIT unavailabilityChanged(m_unavailable);
            return;
        }
        return;
    } else if (rc == PAM_AUTHINFO_UNAVAIL || rc == PAM_MODULE_UNKNOWN) {
        m_unavailable = true;
        Q_EMIT unavailabilityChanged(m_unavailable);
    } else {
        Q_EMIT failed();
    }
}

void PamWorker::startFailedDelay(uint useconds)
{
    m_nextAttemptAllowedTime = std::chrono::steady_clock::now() + std::chrono::microseconds(useconds);
    Q_EMIT loginFailedDelayStarted(useconds);
}

static void fail_delay(int retval, unsigned usec_delay, void *appdata_ptr)
{
    auto* worker = reinterpret_cast< PamWorker* >(appdata_ptr); // Refer the pam_conv (@sa m_conv) structure for info on appdata_ptr
    if (!worker) {
        qCFatal(KSCREENLOCKER_GREET) << "[PAM worker] appdata_ptr not convertible to a valid PamWorker! Cannot apply fail delay";
        return;
    }
    if (retval == PAM_SUCCESS) {
        qCDebug(KSCREENLOCKER_GREET) << "[PAM worker] Fail delay function was called, but authentication result was a success!";
        return;
    }
    worker->startFailedDelay(usec_delay);
}

void PamWorker::start(const QString &service, const QString &user)
{
    m_service = service;
    if (user.isEmpty())
        m_result = pam_start(qPrintable(service), nullptr, &m_conv, &m_handle);
    else
        m_result = pam_start(qPrintable(service), qPrintable(user), &m_conv, &m_handle);

    // get errors quicker
#if defined(HAVE_PAM_FAIL_DELAY)
    pam_set_item(m_handle, PAM_FAIL_DELAY, reinterpret_cast< void* >(fail_delay));
#else
    Q_UNUSED(fail_delay);
#endif

    if (m_result != PAM_SUCCESS) {
        qCWarning(KSCREENLOCKER_GREET,
                  "[PAM worker %s] start: error starting, result code: %d (%s)",
                  qUtf8Printable(m_service),
                  m_result,
                  pam_strerror(m_handle, m_result));
        return;
    } else {
        qCDebug(KSCREENLOCKER_GREET, "[PAM worker %s] start: successfully started", qUtf8Printable(m_service));
    }
}

PamAuthenticator::PamAuthenticator(const QString &service, const QString &user, NoninteractiveAuthenticatorTypes types, QObject *parent)
    : QObject(parent)
    , m_signalsToMembers({
          {QMetaMethod::fromSignal(&PamAuthenticator::prompt), m_prompt},
          {QMetaMethod::fromSignal(&PamAuthenticator::promptForSecret), m_promptForSecret},
          {QMetaMethod::fromSignal(&PamAuthenticator::infoMessage), m_infoMessage},
          {QMetaMethod::fromSignal(&PamAuthenticator::errorMessage), m_errorMessage},
      })
    , m_service(service)
    , m_authenticatorType(types)
    , d(new PamWorker(types.testFlag(PamAuthenticator::Fingerprint)))
{
    d->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, d, &QObject::deleteLater);

    if (types == NoninteractiveAuthenticatorType::Fingerprint) {
        QMetaObject::invokeMethod(
            d,
            [this] {
                struct sigaction action = {};
                action.sa_sigaction = +[](int signal, [[maybe_unused]] siginfo_t *info, [[maybe_unused]] void *context) {
                    if (signal == SIGINT) {
                        // We are trying to mitigate a race condition here. There is a time window between authenticate starting
                        // and the pam module setting up its signal handler. To prevent us from doing an unhandled SIGINT
                        // kill the logic here comes into play. Unless the pam module overrides this handler we'll get
                        // SIGINT and defer it a number of times. If the module doesn't eventually end up handling
                        // it we discard the signal under the assumption that the module just doesn't support interruption.
                        thread_local auto count = 4;
                        if (count-- <= 0) {
                            qWarning() << "ignoring SIGINT, too many already" << pthread_self();
                            return;
                        }
                        QFuture<void> foo = QtConcurrent::run([thread = pthread_self()] {
                            QThread::sleep(4ms);
                            pthread_kill(thread, SIGINT);
                        });
                        return;
                    }
                    raise(signal);
                };
                auto interruptable = (sigaction(SIGINT, &action, nullptr) != -1);
                // Inform the front object about the interruptability of the worker object. This prevents racing between
                // this setup routine and the cancellation logic in PamAuthenticator.
                QMetaObject::invokeMethod(this, [this, interruptable] {
                    m_interruptable = interruptable;
                });
            },
            Qt::QueuedConnection);
    }

    connect(d, &PamWorker::busyChanged, this, &PamAuthenticator::setBusy);
    connect(d, &PamWorker::prompt, this, [this](const QString &msg) {
        m_prompt = msg;
        Q_EMIT prompt(msg);
    });
    connect(d, &PamWorker::promptForSecret, this, [this](const QString &msg) {
        m_promptForSecret = msg;
        Q_EMIT promptForSecret(msg);
    });
    connect(d, &PamWorker::infoMessage, this, [this](const QString &msg) {
        m_infoMessage = msg;
        Q_EMIT infoMessage(msg);
    });
    connect(d, &PamWorker::errorMessage, this, [this](const QString &msg) {
        m_errorMessage = msg;
        Q_EMIT errorMessage(msg);
    });

    connect(d, &PamWorker::inAuthenticateChanged, this, [this](bool isInAuthenticate) {
        m_inAuthentication = isInAuthenticate;
        Q_EMIT availableChanged();
    });
    connect(d, &PamWorker::unavailabilityChanged, this, [this](bool isUnavailable) {
        m_unavailable = isUnavailable;
        Q_EMIT availableChanged();
    });

    connect(d, &PamWorker::succeeded, this, [this]() {
        m_unlocked = true;
        Q_EMIT succeeded();
    });
    // Failed is not a persistent state. When a view provides authentication that will either result in failure or success,
    // failure simply means that the prompt is getting delayed.
    connect(d, &PamWorker::failed, this, &PamAuthenticator::failed);
    connect(d, &PamWorker::loginFailedDelayStarted, this, &PamAuthenticator::loginFailedDelayStarted);

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

bool PamAuthenticator::isBusy() const
{
    return m_busy;
}

bool PamAuthenticator::isAvailable() const
{
    return m_inAuthentication && !m_unavailable;
}

PamAuthenticator::NoninteractiveAuthenticatorTypes PamAuthenticator::authenticatorType() const
{
    return m_authenticatorType;
}

void PamAuthenticator::setBusy(bool busy)
{
    if (m_busy != busy) {
        m_busy = busy;
        Q_EMIT busyChanged();
    }
}

bool PamAuthenticator::isUnlocked() const
{
    return m_unlocked;
}

void PamAuthenticator::tryUnlock()
{
    m_unlocked = false;
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
    if (m_interruptable) {
        Q_EMIT d->interrupt(); // This is a direct call across the thread boundary!
    }
    m_prompt.clear();
    m_promptForSecret.clear();
    m_infoMessage.clear();
    m_errorMessage.clear();
    QMetaObject::invokeMethod(d, &PamWorker::cancelled);
}

QString PamAuthenticator::getPrompt() const
{
    return m_prompt;
}

QString PamAuthenticator::getPromptForSecret() const
{
    return m_promptForSecret;
}

QString PamAuthenticator::getInfoMessage() const
{
    return m_infoMessage;
}

QString PamAuthenticator::getErrorMessage() const
{
    return m_errorMessage;
}

QString PamAuthenticator::service() const
{
    return m_service;
}

#include "pamauthenticator.moc"

#include "moc_pamauthenticator.cpp"
