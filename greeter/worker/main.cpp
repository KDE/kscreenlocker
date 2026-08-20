// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>
// SPDX-FileCopyrightText: 2026 Harald Sitter <sitter@kde.org>

#include <security/pam_appl.h>
#include <unistd.h>

#include <iostream>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QElapsedTimer>

#include "config-worker.h"
#include "debug.h"
#include "diewithparent.h"
#include "org.kde.plasma.screenlocker.h"
#include "result.h"

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

namespace std
{
template<>
struct default_delete<pam_handle_t> {
    void operator()(pam_handle_t *ptr) const
    {
        if (ptr) {
            ::pam_end(ptr, PAM_SUCCESS);
        }
    }
};
} // namespace std

namespace
{

// this is a non-const pointer, but also needs to be; it's effectively dependant on argv input.
auto WORKER = DEFAULT_WORKER; // NOLINT

template<typename Output, typename Input>
[[nodiscard]] Output narrow(Input i)
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

class Worker : public QObject
{
    Q_OBJECT
public:
    Worker(const QString &service, const QString &user, org::kde::plasma::screenlocker *screenlocker);
    [[nodiscard]] WorkerResult::Type authenticate();
    void startFailedDelay(uint useconds);

Q_SIGNALS:
    void inAuthenticateChanged(bool inAuthenticate);

    // internal
    void promptResponseReceived(const QByteArray &prompt);
    void cancelled();
    void interrupt();

private:
    [[nodiscard]] static int converse(int n, const struct pam_message **msg, struct pam_response **resp, void *data);

    QString m_service;
    QString m_user;
    bool m_fingerprint;
    struct pam_conv m_conv;
    bool m_available = true;
    bool m_inAuthenticate = false;
    org::kde::plasma::screenlocker *m_screenlocker;

    // Initialized based on other members, keep last!
    std::unique_ptr<pam_handle_t> m_handle = nullptr; //< the actual PAM handle
};

class Adaptor : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.plasma.screenlocker.worker")
public:
    Adaptor(const QString &service, const QString &user, org::kde::plasma::screenlocker &screenlocker)
        : QObject(nullptr)
        , m_screenlocker(screenlocker)
        , m_worker(service, user, &m_screenlocker)
    {
    }

public Q_SLOTS:
    [[nodiscard]] int Authenticate()
    {
        qCDebug(WORKER) << "Proxy: Authenticate called";
        return m_worker.authenticate();
    }

    Q_NOREPLY void Cancel()
    {
        qCDebug(WORKER) << "Proxy: Cancel called";
        qApp->quit();
    }

private:
    org::kde::plasma::screenlocker &m_screenlocker;
    Worker m_worker;
};

void fail_delay(int retval, unsigned usec_delay, void *appdata_ptr)
{
    auto *worker = reinterpret_cast<Worker *>(appdata_ptr); // Refer the pam_conv (@sa m_conv) structure for info on appdata_ptr
    if (!worker) {
        qCFatal(WORKER) << "[PAM worker] appdata_ptr not convertible to a valid Worker! Cannot apply fail delay";
        return;
    }
    if (retval == PAM_SUCCESS) {
        qCDebug(WORKER) << "[PAM worker] Fail delay function was called, but authentication result was a success!";
        return;
    }
    worker->startFailedDelay(usec_delay);
}

} // namespace

int Worker::converse(int n, const struct pam_message **msg, struct pam_response **resp, void *data)
{
    auto c = static_cast<Worker *>(data);

    if (!resp) {
        qCWarning(WORKER) << "[PAM worker] Converse called with null resp pointer";
        return PAM_BUF_ERR;
    }

    const auto nSize = narrow<size_t>(n);

    *resp = static_cast<struct pam_response *>(calloc(n, sizeof(struct pam_response)));
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
            const QString prompt = QString::fromLocal8Bit(pamMessage->msg);

            qCDebug(WORKER,
                    "[PAM worker %s] Message: %s: %s",
                    qUtf8Printable(c->m_service),
                    (isSecret ? "Echo-off prompt" : "Echo-on prompt"),
                    qUtf8Printable(prompt));

            const QString responseString = [&] {
                if (isSecret) {
                    QDBusPendingReply<QString> reply = c->m_screenlocker->MaskedPrompt(prompt);
                    reply.waitForFinished();
                    if (reply.error().type() == QDBusError::Disconnected) {
                        _exit(0);
                    }
                    return reply.value();
                }
                return c->m_screenlocker->Prompt(prompt).value();
            }();
            const auto response = responseString.toUtf8();

            const auto responseLengthIncludingNull = response.length() + 1; // QByteArray holds an implicit \0 at the end.
            pamResponse.resp = static_cast<char *>(malloc(responseLengthIncludingNull));
            std::copy_n(response.constData(), responseLengthIncludingNull, pamResponse.resp);

            break;
        }
        case PAM_ERROR_MSG: {
            const QString error = QString::fromLocal8Bit(pamMessage->msg);
            qCDebug(WORKER, "[PAM worker %s] Message: Error message: %s", qUtf8Printable(c->m_service), qUtf8Printable(error));
            c->m_screenlocker->ErrorMessage(error);
            break;
        }
        case PAM_TEXT_INFO: {
            // if there's only the info message, let's predict the prompts too
            const QString info = QString::fromLocal8Bit(pamMessage->msg);
            qCDebug(WORKER, "[PAM worker %s] Message: Info message: %s", qUtf8Printable(c->m_service), qUtf8Printable(info));
            c->m_screenlocker->InfoMessage(info);
            break;
        }
        default:
            qCDebug(WORKER, "[PAM worker %s] Message: Unhandled message type: %d", qUtf8Printable(c->m_service), pamMessage->msg_style);
            break;
        }
    }

    return PAM_SUCCESS;
}

Worker::Worker(const QString &service, const QString &user, org::kde::plasma::screenlocker *screenlocker)
    : QObject(nullptr)
    , m_service(service)
    , m_user(user)
    , m_fingerprint(service == KSCREENLOCKER_PAM_FINGERPRINT_SERVICE)
    , m_conv({.conv = &Worker::converse, .appdata_ptr = this})
    , m_screenlocker(screenlocker)
    , m_handle([service, user, this]() -> pam_handle_t * {
        int result = -1;
        pam_handle_t *handle = nullptr;
        if (user.isEmpty()) {
            result = pam_start(qPrintable(service), nullptr, &m_conv, &handle);
        } else {
            result = pam_start(qPrintable(service), qPrintable(user), &m_conv, &handle);
        }

        if (result != PAM_SUCCESS) {
            qCWarning(WORKER, "[PAM worker %s] start: error starting, result code: %d (%s)", qUtf8Printable(service), result, pam_strerror(handle, result));
            return handle;
        }

#if defined(HAVE_PAM_FAIL_DELAY)
        pam_set_item(handle, PAM_FAIL_DELAY, reinterpret_cast<void *>(fail_delay));
#else
        Q_UNUSED(fail_delay);
#endif

        qCDebug(WORKER, "[PAM worker %s] start: successfully started", qUtf8Printable(service));

        return handle;
    }())
{
}

WorkerResult::Type Worker::authenticate()
{
    if (m_inAuthenticate) {
        qCDebug(WORKER, "[PAM worker %s] Authentication is already in progress", qUtf8Printable(m_service));
        return WorkerResult::Type::Failure;
    }
    if (!m_available) {
        qCDebug(WORKER, "[PAM worker %s] PAM service is not available", qUtf8Printable(m_service));
        return WorkerResult::Type::Failure;
    }

    m_inAuthenticate = true;
    Q_EMIT inAuthenticateChanged(m_inAuthenticate);
    auto scopedAuthenticate = qScopeGuard([this] {
        m_inAuthenticate = false;
        Q_EMIT inAuthenticateChanged(m_inAuthenticate);
    });

    qCDebug(WORKER, "[PAM worker %s] Authenticate: Starting authentication", qUtf8Printable(m_service));

    QElapsedTimer timer;
    timer.start();

    int rc = pam_authenticate(m_handle.get(), 0); // PAM_SILENT);
    qCDebug(WORKER, "[PAM worker %s] Authenticate: Authentication done, result code: %d (%s)", qUtf8Printable(m_service), rc, pam_strerror(m_handle.get(), rc));

    qCWarning(WORKER) << timer.elapsed() << "ms elapsed during pam_authenticate call for service" << qUtf8Printable(m_service) << "with result code" << rc;

    constexpr auto checkTimesDefault = "1"_L1;
    static const auto checkTimes = qEnvironmentVariable("KSCREENLOCKER_PAM_TIME_CHECK", checkTimesDefault) == checkTimesDefault;
    constexpr auto tooQuick = 50ms;
    if (checkTimes && timer.durationElapsed() <= tooQuick) {
        // This happened faster than is reasonable for any service -> let's mark as unavailable to avoid hammering a broken service with retries
        // Has been observed with the vibe coded face authenticators on github. They will report success in 0ms when they are totally defunct.
        qCWarning(WORKER) << "Unexpectedly short auth error on PAM service" << qUtf8Printable(m_service) << timer.durationElapsed();
        m_available = false;
        return WorkerResult::Type::Unavailable;
    }

    if (rc == PAM_SUCCESS) {
        pam_setcred(m_handle.get(), PAM_REFRESH_CRED);
        /* ignore errors on refresh credentials. If this did not work we use the old ones. */
        return WorkerResult::Type::Success;
    }

    if (rc == PAM_AUTHINFO_UNAVAIL && m_fingerprint) {
        // For fingerprint authentication, PAM_AUTHINFO_UNAVAIL can mean any number of things, but luckily most of them
        // are fixed by simply restarting the authentication. The notable case we want to catch here is timeouts.
        constexpr auto tooQuick = 250ms;
        // Unless this error was returned suspiciously fast. Then it probably was an actual problem.
        if (timer.durationElapsed() <= tooQuick) {
            qCWarning(WORKER) << "Unexpectedly short auth error on fingerprint reader" << timer.durationElapsed();
            m_available = false;
            return WorkerResult::Type::Unavailable;
        }
        return WorkerResult::Type::Failure;
    }

    if (rc == PAM_AUTHINFO_UNAVAIL || rc == PAM_MODULE_UNKNOWN) {
        // Explicitly unavailable -> let's mark as such
        m_available = false;
        return WorkerResult::Type::Unavailable;
    }

    return WorkerResult::Type::Failure;
}

void Worker::startFailedDelay(uint useconds)
{
    // Inform the frontend so it can make the UI appear blocked.
    m_screenlocker->StartFailedDelay(useconds);
    // To enforce the delay we'll simply go to sleep to prevent the frontend from actually doing anything.
    QThread::sleep(std::chrono::microseconds(useconds));
}

int main(int argc, char *argv[])
{
    dieWithParent();

    QCoreApplication app(argc, argv);

    qCDebug(WORKER) << "Worker is starting up.";

    constexpr auto expectedArguments = 3; // [binary, pam-service-name, username]
    if (app.arguments().size() < expectedArguments) {
        qCWarning(WORKER) << "Worker was started without a service argument. This is wrong. Also, don't call this manually.";
        return 1;
    }
    auto service = app.arguments().at(1); // the PAM service name (e.g. kde-fingerprint)
    auto user = app.arguments().at(2);

    // Switch our QLoggingCategory to the correct service. This makes it clearer which PAM service we are working with.
    //
    // Mind that qstrdup calls new char[], so we need the ptr to delete [] as well, that is why the type is char[].
    std::unique_ptr<char[]> serviceName(qstrdup(u"kscreenlocker.worker.pam-%1"_s.arg(service).toUtf8().constData()));
    static const QLoggingCategory category(serviceName.get(), [] {
        // QLoggingCategory doesn't have a way to get the set level. It only allows querying if a given level is enabled.
        // Trouble is that this is cascading. When Warning is enabled then Critical is also, so we'd have to iterate in the correct order.
        // BUT we cannot use QMetaEnum to iterate the enum because it is not ordered by importance.
        // So here we are, manually calling the functions in the right order such that we inherit the right level. Meh.
        if (DEFAULT_WORKER().isCriticalEnabled()) {
            return QtCriticalMsg;
        }
        if (DEFAULT_WORKER().isWarningEnabled()) {
            return QtWarningMsg;
        }
        if (DEFAULT_WORKER().isInfoEnabled()) {
            return QtInfoMsg;
        }
        if (DEFAULT_WORKER().isDebugEnabled()) {
            return QtDebugMsg;
        }
        return QtInfoMsg;
    }());
    WORKER = []() -> const QLoggingCategory & {
        return category;
    };

    std::string address = [] {
        std::string address;
        while (address.empty()) {
            std::getline(std::cin, address);
        }
        return address;
    }();

    auto connection = QDBusConnection::connectToPeer(QString::fromStdString(address), u"org.kde.plasma.screenlocker"_s);
    org::kde::plasma::screenlocker screenlocker(QString(), u"/org/kde/plasma/screenlocker"_s, connection);
    screenlocker.setTimeout(
        std::numeric_limits<int>::max()); // disable timeout, we expect blocking calls to arrive eventually (or we get terminated by our parent)
    Adaptor proxy(service, user, screenlocker);
    connection.registerObject(u"/org/kde/plasma/screenlocker/worker"_s, &proxy, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);
    // Tell the screenlocker we are ready. This is necessary because there is technically a race between
    // the connection getting established and us registering the object. To avoid any issues we have this
    // explicit "go" call.
    screenlocker.Ping(u"Hello from worker!"_s);

    qCDebug(WORKER) << "Worker is now running, waiting for D-Bus calls.";

    auto ret = app.exec();
    qCDebug(WORKER) << "Worker is exiting with code" << ret;
    return ret;
}

#include "main.moc"
