include(ECMMarkAsTest)

#######################################
# LogindTest
#######################################
set(logindTest_SRCS
    logindtest.cpp
    fakelogind.cpp
    ../logind.cpp
    ../kscreenlocker_logging.cpp
)

add_executable(logindTest ${logindTest_SRCS})
target_link_libraries(logindTest Qt::DBus Qt::Test KF6::I18n)
add_test(NAME ksmserver-logindTest COMMAND logindTest)
ecm_mark_as_test(logindTest)

#######################################
# PamTest
#######################################

pkg_search_module(PamWrapper REQUIRED pam_wrapper>=1.1.6)
pkg_get_variable(PamWrapperModuleDir pam_wrapper "modules")
# We use a different target dir so we can QFINDTESTDATA the two dirs separately
configure_file(data/test_service.in services/test_service)
add_executable(pamTest pamtest.cpp)
target_link_libraries(pamTest Qt::Test kscreenlocker_authenticator)
add_test(NAME ksmserver-pamTest COMMAND pamTest)
set(ksmserver-pamTest-environment
    # We need to disable deepbind for pam_wrapper to work with ASAN
    "PAM_WRAPPER_DISABLE_DEEPBIND=1"
    "LD_PRELOAD=libpam_wrapper.so"
    "ASAN_OPTIONS=verify_asan_link_order=0"
)
set_tests_properties(ksmserver-pamTest PROPERTIES ENVIRONMENT "${ksmserver-pamTest-environment}")
ecm_mark_as_test(pamTest)
