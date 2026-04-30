// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2026 Harald Sitter <sitter@kde.org>

#include "pamauthenticatormodel.h"

#include <KConfig>
#include <KConfigGroup>

#include "pamauthenticators.h"

using namespace::Qt::StringLiterals;

namespace
{

struct PAMAuthenticatorDescriptor {
    Q_GADGET
    Q_PROPERTY(bool available MEMBER available)
    Q_PROPERTY(PamAuthenticators::Authenticator type MEMBER type)
    Q_PROPERTY(QString iconName MEMBER iconName)
    Q_PROPERTY(bool passwordField MEMBER passwordField)
    Q_PROPERTY(bool expectingPrompt MEMBER expectingPrompt)
public:
    // Mind that this struct must be default-constructable for qrangemodel!
    bool available = false;
    PamAuthenticators::Authenticator type = PamAuthenticators::Authenticator::Regular;
    QString iconName;
    bool passwordField = false;
    bool expectingPrompt = false;
};

[[nodiscard]] std::vector<PAMAuthenticatorDescriptor> makeDescriptors()
{
    KConfig config(u"kscreenlockerrc"_s);
    const auto authenticators = config.group(u"Authenticators"_s);

    auto all = std::initializer_list{PAMAuthenticatorDescriptor{.available = true, // always available
                                                                .type = PamAuthenticators::Authenticator::Regular,
                                                                .iconName = QStringLiteral("drag-surface-symbolic"),
                                                                .passwordField = true,
                                                                .expectingPrompt = true},
                                     PAMAuthenticatorDescriptor{.available = authenticators.readEntry("Smartcard", false),
                                                                .type = PamAuthenticators::Authenticator::Smartcard,
                                                                .iconName = QStringLiteral("secure-card-symbolic"),
                                                                .passwordField = true,
                                                                .expectingPrompt = true},
                                     PAMAuthenticatorDescriptor{.available = authenticators.readEntry("Fingerprint", false),
                                                                .type = PamAuthenticators::Authenticator::Fingerprint,
                                                                .iconName = QStringLiteral("fingerprint-symbolic"),
                                                                .passwordField = false,
                                                                .expectingPrompt = false},
                                     PAMAuthenticatorDescriptor{.available = authenticators.readEntry("Face", false),
                                                                .type = PamAuthenticators::Authenticator::Face,
                                                                .iconName = QStringLiteral("edit-image-face-detect-symbolic"),
                                                                .passwordField = false,
                                                                .expectingPrompt = false},
                                     PAMAuthenticatorDescriptor{.available = authenticators.readEntry("Universal2Factor", false),
                                                                .type = PamAuthenticators::Authenticator::Universal2Factor,
                                                                .iconName = QStringLiteral("auth-sim-symbolic"),
                                                                .passwordField = false,
                                                                .expectingPrompt = false}};

    auto view = all | std::views::filter([](const PAMAuthenticatorDescriptor &d) {
                    return d.available;
                });
    return {view.begin(), view.end()};
}

} // namespace

// Treat the properties as data not columns.
template<>
struct QRangeModel::RowOptions<PAMAuthenticatorDescriptor> {
    static constexpr auto rowCategory = QRangeModel::RowCategory::MultiRoleItem;
};

PAMAuthenticatorModel::PAMAuthenticatorModel(QObject *parent)
    : QRangeModel(makeDescriptors(), parent)
{
}

#include "pamauthenticatormodel.moc"
