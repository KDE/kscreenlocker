#=============================================================================
# Copyright 2017 Martin Gräßlin <mgraesslin@kde.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#=============================================================================

if(CMAKE_VERSION VERSION_LESS 2.8.12)
    message(FATAL_ERROR "CMake 2.8.12 is required by FindSeccomp.cmake")
endif()
if(CMAKE_MINIMUM_REQUIRED_VERSION VERSION_LESS 2.8.12)
    message(AUTHOR_WARNING "Your project should require at least CMake 2.8.12 to use FindSeccomp.cmake")
endif()

find_package(PkgConfig)
pkg_check_modules(PKG_Libseccomp QUIET libseccomp)

set(Seccomp_DEFINITIONS ${PKG_Libseccomp_CFLAGS_OTHER})
set(Seccomp_VERSION ${PKG_Libseccomp_VERSION})

find_path(Seccomp_INCLUDE_DIR
    NAMES
        seccomp.h
    HINTS
        ${PKG_Libseccomp_INCLUDE_DIRS}
)
find_library(Seccomp_LIBRARY
    NAMES
        seccomp
    HINTS
        ${PKG_Libseccomp_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Seccomp
    FOUND_VAR
        Seccomp_FOUND
    REQUIRED_VARS
        Seccomp_LIBRARY
        Seccomp_INCLUDE_DIR
    VERSION_VAR
        Seccomp_VERSION
)

if (Seccomp_FOUND AND NOT TARGET Seccomp::Seccomp)
    add_library(Seccomp::Seccomp UNKNOWN IMPORTED)
    set_target_properties(Seccomp::Seccomp PROPERTIES
        IMPORTED_LOCATION "${Seccomp_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${Seccomp_DEFINITIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${Seccomp_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(Seccomp_LIBRARY Seccomp_INCLUDE_DIR)

include(FeatureSummary)
set_package_properties(Seccomp PROPERTIES
    URL "https://github.com/seccomp/libseccomp"
    DESCRIPTION "The enhanced seccomp library."
)
