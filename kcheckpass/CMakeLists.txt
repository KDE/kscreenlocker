include_directories( ${UNIXAUTH_INCLUDE_DIRS} )
check_include_files(paths.h HAVE_PATHS_H)
configure_file (config-kcheckpass.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kcheckpass.h )

set(kcheckpass_SRCS
	kcheckpass.h
	kcheckpass.c
	checkpass_pam.c
	checkpass_shadow.c
)

add_executable(kcheckpass ${kcheckpass_SRCS})
ecm_mark_nongui_executable(kcheckpass)

set_property(TARGET kcheckpass APPEND_STRING PROPERTY COMPILE_FLAGS " -U_REENTRANT")
target_link_libraries(kcheckpass ${UNIXAUTH_LIBRARIES} ${SOCKET_LIBRARIES})
if (PAM_FOUND)
    set(kcheckpass_suid "")
else()
    set(kcheckpass_suid "SETUID")
    message(STATUS "PAM not found, will install a SUID-kcheckpass")
endif()
install(TARGETS kcheckpass
    DESTINATION ${KDE_INSTALL_LIBEXECDIR}
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE ${kcheckpass_suid}
    )

#EXTRA_DIST = README
