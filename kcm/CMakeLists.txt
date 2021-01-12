# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcm_screenlocker\")

set(screenlocker_kcm_SRCS
    kcm.cpp
    kscreenlockerdata.cpp
)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../)

qt5_add_dbus_interface(screenlocker_kcm_SRCS ../dbus/org.kde.screensaver.xml screenlocker_interface)

add_library(kcm_screenlocker MODULE ${screenlocker_kcm_SRCS})

target_link_libraries(kcm_screenlocker
    settings
    Qt5::DBus
    KF5::Declarative
    KF5::KCMUtils
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
