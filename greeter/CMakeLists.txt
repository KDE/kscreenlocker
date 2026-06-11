remove_definitions(-DTRANSLATION_DOMAIN=\"kscreenlocker\")
add_definitions(-DTRANSLATION_DOMAIN=\"kscreenlocker_greet\")

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/../
)

set(kscreenlocker_authenticator_SRCS
    pamauthenticators.cpp
    pamauthenticators.h
    pamauthenticator.cpp
    pamauthenticator.h
)

ecm_qt_declare_logging_category(kscreenlocker_authenticator_SRCS
    HEADER kscreenlocker_greet_logging.h
    IDENTIFIER KSCREENLOCKER_GREET
    CATEGORY_NAME kscreenlocker_greet
    DESCRIPTION "KScreenLocker (greeter)"
    EXPORT KSCREENLOCKER
)

set(kscreenlocker_greet_SRCS
   greeterapp.cpp
   main.cpp
   noaccessnetworkaccessmanagerfactory.cpp
   ../logind.cpp
   greeterapp.h
   noaccessnetworkaccessmanagerfactory.h
   ../logind.h
)

ecm_qt_declare_logging_category(kscreenlocker_greet_SRCS
    HEADER kscreenlocker_logging.h
    IDENTIFIER KSCREENLOCKER
    CATEGORY_NAME kscreenlocker
    DESCRIPTION "KScreenLocker"
    EXPORT KSCREENLOCKER
)

add_library(kscreenlocker_authenticator OBJECT ${kscreenlocker_authenticator_SRCS})
target_link_libraries(kscreenlocker_authenticator
    Qt::Core
    ${PAM_LIBRARIES}
)

qt_add_resources(kscreenlocker_greet_SRCS fallbacktheme.qrc)

add_executable(kscreenlocker_greet ${kscreenlocker_greet_SRCS})

target_link_libraries(kscreenlocker_greet
    settings
    kscreenlocker_authenticator
    KF6::Package
    KF6::Crash
    KF6::I18n
    KF6::ConfigGui
    KF6::KIOCore
    KF6::WindowSystem
    KF6::I18nQml
    Plasma::PlasmaQuick
    Qt::DBus
    Qt::Quick
    Qt::Qml
    LayerShellQt::Interface
    KF6::ScreenDpms
)

# KSCREENLOCKER_PAM_SERVICE, if defined, will already have been
# enclosed in double quotes by the define_pam_service macro.
if (NOT DEFINED KSCREENLOCKER_PAM_PASSWORD_SERVICE)
  set(KSCREENLOCKER_PAM_PASSWORD_SERVICE "\"kde\"")
endif()

if (NOT DEFINED KSCREENLOCKER_PAM_FINGERPRINT_SERVICE)
  set(KSCREENLOCKER_PAM_FINGERPRINT_SERVICE "\"kde-fingerprint\"")
endif()

if (NOT DEFINED KSCREENLOCKER_PAM_SMARTCARD_SERVICE)
  set(KSCREENLOCKER_PAM_SMARTCARD_SERVICE "\"kde-smartcard\"")
endif()

target_compile_definitions(kscreenlocker_greet PRIVATE
    KCHECKPASS_BIN="kcheckpass"
    KSCREENLOCKER_PAM_SERVICE=${KSCREENLOCKER_PAM_SERVICE}
    KSCREENLOCKER_PAM_FINGERPRINT_SERVICE=${KSCREENLOCKER_PAM_FINGERPRINT_SERVICE}
    KSCREENLOCKER_PAM_SMARTCARD_SERVICE=${KSCREENLOCKER_PAM_SMARTCARD_SERVICE}
)

install(TARGETS kscreenlocker_greet DESTINATION ${KDE_INSTALL_LIBEXECDIR})

install(DIRECTORY themes/org.kde.passworddialog DESTINATION ${KDE_INSTALL_DATADIR}/ksmserver/screenlocker)

if(BUILD_TESTING)
    add_subdirectory(autotests)
endif()
