# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: 2026 Harald Sitter <sitter@kde.org>

set(CACHE{KSCREENLOCKER_PAM_PASSWORD_SERVICE}
    TYPE STRING
    HELP "The PAM service to use for password authentication. CANNOT be disabled!"
    VALUE "kde")
set(CACHE{KSCREENLOCKER_PAM_FINGERPRINT_SERVICE}
    TYPE STRING
    HELP "The PAM service to use for fingerprint authentication. 'disabled' to disable"
    VALUE "kde-fingerprint")
set(CACHE{KSCREENLOCKER_PAM_SMARTCARD_SERVICE}
    TYPE STRING
    HELP "The PAM service to use for smartcard authentication. 'disabled' to disable"
    VALUE "kde-smartcard")
set(CACHE{KSCREENLOCKER_PAM_FACE_SERVICE}
    TYPE STRING
    HELP "The PAM service to use for face authentication. 'disabled' to disable"
    VALUE "kde-face")
set(CACHE{KSCREENLOCKER_PAM_UNIVERSAL2FACTOR_SERVICE}
    TYPE STRING
    HELP "The PAM service to use for universal 2-factor authentication. 'disabled' to disable"
    VALUE "kde-u2f")

configure_file(config-worker.h.in ${CMAKE_CURRENT_BINARY_DIR}/config-worker.h)

add_library(WorkerResult OBJECT result.cpp)
target_link_libraries(WorkerResult
    PUBLIC
        Qt::Core
)
target_include_directories(WorkerResult PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR};${CMAKE_CURRENT_BINARY_DIR}>")

qt_add_dbus_interface(SRCS org.kde.plasma.screenlocker.xml org.kde.plasma.screenlocker)
add_executable(kscreenlocker_worker main.cpp ${SRCS})
target_link_libraries(kscreenlocker_worker
    PRIVATE
        WorkerResult
        Qt::Core
        Qt::DBus
        ${PAM_LIBRARIES}
)
ecm_qt_declare_logging_category(kscreenlocker_worker
    HEADER debug.h
    IDENTIFIER DEFAULT_WORKER
    CATEGORY_NAME kscreenlocker.worker
    DESCRIPTION "KScreenLocker Worker"
    EXPORT KSCREENLOCKER_WORKER
)
install(TARGETS kscreenlocker_worker DESTINATION ${KDE_INSTALL_LIBEXECDIR})
