# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"screenlocker_kcm\")

set(screenlocker_kcm_SRCS
    kcm.cpp
    ../kscreensaversettings.cpp
    ../greeter/wallpaper_integration.cpp
    ../greeter/lnf_integration.cpp
)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../)

kconfig_add_kcfg_files(screenlocker_kcm_SRCS ../kcfg/kscreensaversettingsbase.kcfgc)
qt5_add_dbus_interface(screenlocker_kcm_SRCS ../dbus/org.kde.screensaver.xml screenlocker_interface)

add_library(kcm_screenlocker MODULE ${screenlocker_kcm_SRCS})

add_dependencies(kscreenlocker_greet KScreenLocker)

target_link_libraries(kcm_screenlocker
    Qt5::DBus
    KF5::Declarative
    KF5::I18n
    KF5::GlobalAccel
    KF5::Package
    KF5::QuickAddons
    KF5::XmlGui
)

kcoreaddons_desktop_to_json(kcm_screenlocker screenlocker.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kcm_screenlocker
    DESTINATION
        ${KDE_INSTALL_PLUGINDIR}/kcms
)

kpackage_install_package(package kcm_screenlocker kcms)

install(
    FILES
        screenlocker.desktop
    DESTINATION
        ${KDE_INSTALL_KSERVICES5DIR}
)
