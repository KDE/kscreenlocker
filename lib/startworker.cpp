// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2026 Harald Sitter <sitter@kde.org>

#include "startworker.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <KLibexec>

using namespace Qt::StringLiterals;
using namespace PlasmaAuthentication;

Worker::Worker(const QString &service, const std::optional<QString> &user, const QString &greeterAddress, const std::optional<QString> &arbiterAddress)
    : m_workerProcess([&] {
        auto worker = std::make_unique<QProcess>();
        worker->setProcessChannelMode(QProcess::ForwardedChannels);
        worker->setProgram(KLibexec::path(u"kscreenlocker_worker"_s));
        worker->setArguments({service, user.value_or(QString())});
        connect(worker.get(), &QProcess::errorOccurred, this, &Worker::errorOccurred);
        return worker;
    }())
    , m_payload(QJsonDocument(QJsonObject{
                                  {u"screenlockerAddress"_s, greeterAddress},
                                  {u"arbiterAddress"_s, arbiterAddress.value_or(QString())},
                              })
                    .toJson(QJsonDocument::Compact))
{
}

void Worker::start()
{
    m_workerProcess->start();
    m_workerProcess->write(m_payload);
    m_workerProcess->closeWriteChannel();
}

bool Worker::waitForFinished(std::chrono::milliseconds timeout)
{
#warning wants KNarrow
    return m_workerProcess->waitForFinished(timeout.count());
}

void Worker::terminate()
{
    m_workerProcess->terminate();
}

void Worker::kill()
{
    m_workerProcess->kill();
}
