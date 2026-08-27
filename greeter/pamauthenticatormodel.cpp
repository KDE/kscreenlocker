// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2026 Harald Sitter <sitter@kde.org>

#include "pamauthenticatormodel.h"

#include <QQmlEngine>

#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>

#include "config-worker.h"
#include "kscreenlocker_greet_logging.h"
#include "pamauthenticatordescriptor.h"

using namespace Qt::StringLiterals;

namespace
{

[[nodiscard]] std::vector<std::shared_ptr<PAMAuthenticatorDescriptor>> makeDescriptors()
{
    auto isStaticallyEnabled = [](const auto &string) {
        constexpr auto disabled = "disabled"_L1;
        if (string.size() != disabled.size()) {
            return true;
        }
        return std::ranges::all_of(std::views::zip(string, disabled), [](const auto &element) {
            if (auto &[aCharacter, bCharacter] = element; aCharacter != bCharacter) {
                return true;
            }
            return false;
        });
    };

    constexpr auto fingerprintEnabled = isStaticallyEnabled(KSCREENLOCKER_PAM_FINGERPRINT_SERVICE);
    constexpr auto faceEnabled = isStaticallyEnabled(KSCREENLOCKER_PAM_FACE_SERVICE);
    constexpr auto universal2factorEnabled = isStaticallyEnabled(KSCREENLOCKER_PAM_UNIVERSAL2FACTOR_SERVICE);
    constexpr auto smartcardEnabled = isStaticallyEnabled(KSCREENLOCKER_PAM_SMARTCARD_SERVICE);

    KConfig config(u"kscreenlockerrc"_s);
    const auto authenticators = config.group(u"Authenticators"_s);
    const auto ui = config.group(u"UI"_s);

    auto all = std::initializer_list{
        std::make_shared<PAMAuthenticatorDescriptor>(true, // must be always available
                                                     PamAuthenticators::Authenticator::Regular,
                                                     u"input-keyboard-symbolic"_s,
                                                     true,
                                                     true,
                                                     i18nc("authentication type in unlock dialogs", "Password"),
                                                     // This is holding on to old behavior where we don't show a prompt for the regular authenticator.
                                                     // Rationale being that it is 99% of the time simply a password and we have a password field for that.
                                                     ui.readEntry("ShowPromptInRegularAuthenticator", false)),
        std::make_shared<PAMAuthenticatorDescriptor>(smartcardEnabled && authenticators.readEntry("Smartcard", false),
                                                     PamAuthenticators::Authenticator::Smartcard,
                                                     u"secure-card-symbolic"_s,
                                                     true,
                                                     true,
                                                     i18nc("authentication type in unlock dialogs", "Smartcard"),
                                                     true),
        std::make_shared<PAMAuthenticatorDescriptor>(fingerprintEnabled && authenticators.readEntry("Fingerprint", false),
                                                     PamAuthenticators::Authenticator::Fingerprint,
                                                     u"fingerprint-symbolic"_s,
                                                     false,
                                                     false,
                                                     i18nc("authentication type in unlock dialogs", "Fingerprint"),
                                                     true),
        std::make_shared<PAMAuthenticatorDescriptor>(faceEnabled && authenticators.readEntry("Face", false),
                                                     PamAuthenticators::Authenticator::Face,
                                                     u"edit-image-face-detect-symbolic"_s,
                                                     false,
                                                     false,
                                                     i18nc("authentication type in unlock dialogs - facial authentication", "Face"),
                                                     true),
        std::make_shared<PAMAuthenticatorDescriptor>(
            universal2factorEnabled && authenticators.readEntry("Universal2Factor", false),
            PamAuthenticators::Authenticator::Universal2Factor,
            u"database-change-key-symbolic"_s,
            false,
            false,
            i18nc("authentication type in unlock dialogs - universal 2 factor authentication (yubikey etc)", "Universal 2 Factor"),
            true),
    };

    auto view = all | std::views::filter([](const auto &d) {
                    return d->isEnabled();
                });
    return {view.begin(), view.end()};
}

} // namespace

// Treat the properties as data not columns.
template<>
struct QRangeModel::RowOptions<PAMAuthenticatorDescriptor> {
    [[maybe_unused]] static constexpr auto rowCategory = QRangeModel::RowCategory::MultiRoleItem;
};

PAMAuthenticatorModel *PAMAuthenticatorModel::create([[maybe_unused]] QQmlEngine *qmlEngine, [[maybe_unused]] QJSEngine *jsEngine)
{
    auto model = instance();
    QQmlEngine::setObjectOwnership(model, QQmlEngine::CppOwnership);
    return model;
}

void PAMAuthenticatorModel::markDefunct(PamAuthenticators::Authenticator authenticator) const
{
    if (authenticator == PamAuthenticators::Authenticator::Regular) {
        qCWarning(KSCREENLOCKER_GREET) << "Regular authenticator is defunct. This is unexpected and we ignore it.";
        return;
    }
    m_hash.value(authenticator)->setFunctional(false);
}

[[nodiscard]] bool PAMAuthenticatorModel::isFunctional(PamAuthenticators::Authenticator authenticator) const
{
    return m_hash.value(authenticator)->isFunctional();
}

PAMAuthenticatorModel *PAMAuthenticatorModel::instance()
{
    static PAMAuthenticatorModel model(makeDescriptors(), nullptr);
    return &model;
}

PAMAuthenticatorModel::PAMAuthenticatorModel(const Range &range, QObject *parent)
    : QRangeModel(range, parent)
    , m_hash([range] {
        TypeHash hash;
        for (const auto &descriptor : range) {
            hash[descriptor->type()] = descriptor;
        }
        return hash;
    }())
{
    QRangeModel::setAutoConnectPolicy(QRangeModel::AutoConnectPolicy::Full);
}
