#ifdef KSCREENLOCKER_TEST_APPS
#define KCHECKPASS_BIN "${CMAKE_BINARY_DIR}/kcheckpass/kcheckpass"
#elif defined(KSCREENLOCKER_UNIT_TEST)
#define KCHECKPASS_BIN "${CMAKE_CURRENT_BINARY_DIR}/greeter/autotests/fakekcheckpass"
#else
#define KCHECKPASS_BIN "${CMAKE_INSTALL_FULL_LIBEXECDIR}/kcheckpass"
#endif

#define KSCREENLOCKER_GREET_BIN "${CMAKE_INSTALL_FULL_LIBEXECDIR}/kscreenlocker_greet"

#cmakedefine01 HAVE_SYS_PRCTL_H
#cmakedefine01 HAVE_PR_SET_DUMPABLE
#cmakedefine01 HAVE_SYS_PROCCTL_H
#cmakedefine01 HAVE_PROC_TRACE_CTL
