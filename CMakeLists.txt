cmake_minimum_required(VERSION 3.0)

project(kscreenlocker)
set(PROJECT_VERSION "5.17.80")
set(PROJECT_VERSION_MAJOR 5)

set(QT_MIN_VERSION "5.12.0")
set(KF5_MIN_VERSION "5.62.0")
find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS DBus Widgets Quick QuickWidgets Test)
find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDEFrameworkCompilerSettings NO_POLICY_SCOPE)
include(ECMSetupVersion)
include(ECMGenerateHeaders)
include(CheckIncludeFiles)
include(ECMMarkNonGuiExecutable)
include(CMakePackageConfigHelpers)
include(FeatureSummary)
include(GenerateExportHeader)

include(CheckIncludeFile)
include(CheckSymbolExists)
check_include_file("sys/prctl.h" HAVE_SYS_PRCTL_H)
check_symbol_exists(PR_SET_DUMPABLE "sys/prctl.h" HAVE_PR_SET_DUMPABLE)
check_include_file("sys/procctl.h" HAVE_SYS_PROCCTL_H)
check_symbol_exists(PROC_TRACE_CTL "sys/procctl.h" HAVE_PROC_TRACE_CTL)
if (HAVE_PR_SET_DUMPABLE OR HAVE_PROC_TRACE_CTL)
  set(CAN_DISABLE_PTRACE TRUE)
endif ()
add_feature_info("prctl/procctl tracing control"
                 CAN_DISABLE_PTRACE
                 "Required for disallowing ptrace on greeter and kcheckpass process")

check_include_file("sys/signalfd.h" HAVE_SIGNALFD_H)
if (NOT HAVE_SIGNALFD_H)
    check_include_files("sys/types.h;sys/event.h" HAVE_EVENT_H)
endif ()
if (NOT (HAVE_SIGNALFD_H OR HAVE_EVENT_H))
    message(FATAL_ERROR "kcheckpass either needs signalfd() or kevent()&sigtimedwait() to work")
endif ()
add_feature_info("sys/signalfd.h"
                 HAVE_SIGNALFD_H
                 "Use the signalfd() api for signalhandling")
add_feature_info("sys/event.h"
                 HAVE_EVENT_H
                 "Use the kevent() and sigwaitinfo() api for signalhandling")

find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS
                    Crash
                    Declarative
                    GlobalAccel
                    I18n
                    IdleTime
                    KCMUtils
                    Notifications
                    Solid
                    TextWidgets
                    WindowSystem
                    XmlGui
                    )

find_package(X11)
set_package_properties(X11 PROPERTIES DESCRIPTION "X11 libraries"
                        URL "http://www.x.org"
                        TYPE REQUIRED
                        PURPOSE "Required for building the X11 based workspace")

find_package(XCB MODULE REQUIRED COMPONENTS XCB KEYSYMS XTEST)
set_package_properties(XCB PROPERTIES TYPE REQUIRED)
add_feature_info("XInput" X11_Xinput_FOUND "Required for grabbing XInput2 devices in the screen locker")

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS X11Extras)

find_package(KF5Wayland CONFIG REQUIRED)
set_package_properties(KF5Wayland PROPERTIES
                       TYPE REQUIRED
                       PURPOSE "Required for building screenlocker")
find_package(WaylandScanner)
find_package(Wayland 1.3 COMPONENTS Client Server)

set_package_properties(Wayland PROPERTIES
                       TYPE REQUIRED
                       PURPOSE "Required for building screenlocker")

find_package(loginctl)
set_package_properties(loginctl PROPERTIES
    URL "https://www.freedesktop.org/software/systemd/man/loginctl.html"
    DESCRIPTION "Send control commands to the login manager"
    TYPE RUNTIME
    )
set(HAVE_LOGINCTL ${loginctl_FOUND})

if ( NOT HAVE_LOGINCTL )
    find_package(ConsoleKit)
    set_package_properties(ConsoleKit PROPERTIES
        URL "https://github.com/ConsoleKit2/ConsoleKit2"
        DESCRIPTION "Framework for tracking user login sessions"
        TYPE RECOMMENDED
        )
    set(HAVE_CONSOLEKIT ${ConsoleKit_FOUND})
endif ()

if (HAVE_LOGINCTL OR HAVE_CONSOLEKIT)
    set(HAVE_UNLOCK_CAPABILITY TRUE)
endif ()

add_feature_info("Unlock broken screenlocker"
                 HAVE_UNLOCK_CAPABILITY
                 "Needed for emergency unlock in case that the greeter is broken. In case your distribution does not provide loginctl or consolekit please contact plasma-devel@kde.org to discuss alternatives.")

option(PAM_REQUIRED "Require building with PAM" ON)

find_package(Seccomp)
set_package_properties(Seccomp PROPERTIES
    TYPE
        OPTIONAL
    PURPOSE
        "Used for putting the look'n'feel package in the greeter into a sandbox."
)
set(HAVE_SECCOMP ${Seccomp_FOUND})

include(ConfigureChecks.cmake)

configure_file(config-workspace.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-workspace.h)
configure_file(config-unix.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-unix.h )
configure_file(config-X11.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-X11.h)

if (HAVE_CONSOLEKIT)
    configure_file(ck-unlock-session.cmake ${CMAKE_CURRENT_BINARY_DIR}/ck-unlock-session)
endif ()

# adjusting CMAKE_C_FLAGS to get wayland protocols to compile
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu90")

ecm_setup_version(${PROJECT_VERSION} VARIABLE_PREFIX KSCREENLOCKER
                        VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/kscreenlocker_version.h"
                        PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KScreenLockerConfigVersion.cmake"
                        SOVERSION 5)

configure_file(config-kscreenlocker.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kscreenlocker.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(KSLD_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/KScreenLocker")

add_definitions(-DQT_NO_NARROWING_CONVERSIONS_IN_CONNECT)

add_subdirectory(kcheckpass)
add_subdirectory(greeter)
add_subdirectory(kcm)

add_definitions(-DTRANSLATION_DOMAIN=\"kscreenlocker\")

set(screensaver_dbusXML dbus/org.freedesktop.ScreenSaver.xml)
set(kscreensaver_dbusXML dbus/org.kde.screensaver.xml)
set(powerdevilpolicyagent_xml dbus/kf5_org.kde.Solid.PowerManagement.PolicyAgent.xml)

set(ksld_SRCS
   abstractlocker.cpp
   ksldapp.cpp
   interface.cpp
   globalaccel.cpp
   x11locker.cpp
   waylandlocker.cpp
   logind.cpp
   waylandserver.cpp
   powermanagement.cpp
   powermanagement_inhibition.cpp
)
qt5_add_dbus_adaptor(ksld_SRCS ${screensaver_dbusXML} interface.h ScreenLocker::Interface)
qt5_add_dbus_adaptor(ksld_SRCS ${kscreensaver_dbusXML} interface.h ScreenLocker::Interface kscreensaveradaptor KScreenSaverAdaptor)
kconfig_add_kcfg_files(ksld_SRCS kcfg/kscreensaversettings.kcfgc)
qt5_add_dbus_interface(ksld_SRCS ${powerdevilpolicyagent_xml} powerdevilpolicyagent)

ecm_add_wayland_server_protocol(ksld_SRCS
    PROTOCOL protocols/ksld.xml
    BASENAME ksld
)

add_library(KScreenLocker SHARED ${ksld_SRCS})

target_link_libraries(KScreenLocker
PUBLIC
   Qt5::Core
   Qt5::X11Extras
PRIVATE
   Qt5::DBus
   KF5::I18n
   KF5::IdleTime
   KF5::GlobalAccel
   KF5::Notifications
   KF5::CoreAddons
   KF5::ConfigGui
   KF5::WindowSystem
   ${X11_LIBRARIES}
   XCB::XCB
   XCB::KEYSYMS
   KF5::WaylandServer
   Wayland::Server
)

if (X11_Xinput_FOUND)
    target_link_libraries(KScreenLocker PRIVATE ${X11_Xinput_LIB})
endif()

target_include_directories(KScreenLocker INTERFACE "$<INSTALL_INTERFACE:${KSLD_INCLUDEDIR}>")

# Needed to compile on Arm target.
set_target_properties(KScreenLocker PROPERTIES COMPILE_FLAGS "-fPIC")

add_library(PW::KScreenLocker ALIAS KScreenLocker)

generate_export_header(KScreenLocker
    BASE_NAME KScreenLocker
    EXPORT_MACRO_NAME KSCREENLOCKER_EXPORT
    EXPORT_FILE_NAME KScreenLocker/kscreenlocker_export.h
)

configure_package_config_file(ScreenSaverDBusInterfaceConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ScreenSaverDBusInterfaceConfig.cmake
    PATH_VARS KDE_INSTALL_DBUSINTERFACEDIR
    INSTALL_DESTINATION "${CMAKECONFIG_INSTALL_PREFIX}/ScreenSaverDBusInterface")

set_target_properties(KScreenLocker PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/ScreenSaverDBusInterfaceConfig.cmake
        DESTINATION "${CMAKECONFIG_INSTALL_PREFIX}/ScreenSaverDBusInterface")

configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/KScreenLockerConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/KScreenLockerConfig.cmake"
    PATH_VARS KDE_INSTALL_DBUSINTERFACEDIR
    INSTALL_DESTINATION "${CMAKECONFIG_INSTALL_PREFIX}/KScreenLocker")

if (HAVE_CONSOLEKIT)
  install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/ck-unlock-session DESTINATION ${KDE_INSTALL_BINDIR})
endif ()

install(TARGETS KScreenLocker EXPORT KScreenLockerTargets ${INSTALL_TARGETS_DEFAULT_ARGS} LIBRARY)
install(EXPORT KScreenLockerTargets DESTINATION "${CMAKECONFIG_INSTALL_PREFIX}/KScreenLocker" FILE KScreenLockerTargets.cmake NAMESPACE PW::)

ecm_generate_headers(KScreenLocker_CamelCase_HEADERS
    HEADER_NAMES
        KsldApp
    REQUIRED_HEADERS KScreenLocker_HEADERS)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/KScreenLocker/kscreenlocker_export.h
        ${KScreenLocker_CamelCase_HEADERS}
        ${KScreenLocker_HEADERS}
    DESTINATION
        ${KSLD_INCLUDEDIR}/KScreenLocker
    COMPONENT Devel)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/KScreenLockerConfig.cmake"
			  "${CMAKE_CURRENT_BINARY_DIR}/KScreenLockerConfigVersion.cmake"
		DESTINATION "${CMAKECONFIG_INSTALL_PREFIX}/KScreenLocker"
		COMPONENT Devel)

install(FILES kscreenlocker.notifyrc  DESTINATION ${KDE_INSTALL_KNOTIFY5RCDIR} RENAME ksmserver.notifyrc)
install(FILES ${screensaver_dbusXML}
        DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR}
        RENAME kf5_org.freedesktop.ScreenSaver.xml)

install(FILES ${kscreensaver_dbusXML}
        DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR})


install(FILES   updaters/kscreenlocker.upd DESTINATION ${KDE_INSTALL_DATADIR}/kconf_update)
install(PROGRAMS updaters/ksreenlocker_5_3_separate_autologin.pl
                DESTINATION ${KDE_INSTALL_DATADIR}/kconf_update)

if(BUILD_TESTING)
   add_subdirectory(autotests)
   add_subdirectory(tests)
endif()
feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
