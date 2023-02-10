# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcm_screenlocker\")

set(screenlocker_kcm_SRCS
    kcm.cpp
    kscreenlockerdata.cpp
)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../)

qt_add_dbus_interface(screenlocker_kcm_SRCS ../dbus/org.kde.screensaver.xml screenlocker_interface)

kcoreaddons_add_plugin(kcm_screenlocker SOURCES ${screenlocker_kcm_SRCS} INSTALL_NAMESPACE "plasma/kcms/systemsettings")

kcmutils_generate_desktop_file(kcm_screenlocker)
target_link_libraries(kcm_screenlocker
    settings
    Qt::DBus
    KF6::Declarative
    KF6::KCMUtils
    KF6::I18n
    KF6::GlobalAccel
    KF6::Package
    KF6::QuickAddons
    KF6::XmlGui
)

kpackage_install_package(package kcm_screenlocker kcms)
