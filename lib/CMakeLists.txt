# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: 2026 Harald Sitter <sitter@kde.org>

add_library(PlasmaAuthentication SHARED startworker.cpp)
set_target_properties(PlasmaAuthentication PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    EXPORT_NAME PlasmaAuthentication
)
target_link_libraries(PlasmaAuthentication
    PUBLIC
        Qt6::Core
    PRIVATE
        KF6::CoreAddons
)
set(PlasmaAuthentication_HEADERS)
ecm_generate_headers(PlasmaAuthentication_CamelCase_HEADERS
    HEADER_NAMES
        StartWorker
    REQUIRED_HEADERS PlasmaAuthentication_HEADERS
    PREFIX PlasmaAuthentication
)
install(
    FILES
        ${PlasmaAuthentication_HEADERS}
        ${PlasmaAuthentication_CamelCase_HEADERS}
    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/PlasmaAuthentication
    COMPONENT Devel
)
install(TARGETS PlasmaAuthentication EXPORT PlasmaAuthenticationTargets ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
