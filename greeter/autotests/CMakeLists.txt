include(ECMMarkAsTest)

#####################################
# fakekcheckpass
#####################################
add_executable(fakekcheckpass fakekcheckpass.c)
ecm_mark_nongui_executable(fakekcheckpass)
target_link_libraries(fakekcheckpass ${SOCKET_LIBRARIES})

#######################################
# AuthenticatorTest
#######################################
set( authenticatorTest_SRCS
     authenticatortest.cpp
     ../authenticator.cpp
)
add_executable(authenticatorTest ${authenticatorTest_SRCS})
target_compile_definitions(authenticatorTest PRIVATE KCHECKPASS_BIN="fakekcheckpass")
target_link_libraries(authenticatorTest KF5::CoreAddons Qt::Test)
add_test(NAME ksmserver-authenticatorTest COMMAND authenticatorTest)
ecm_mark_as_test(authenticatorTest)

#######################################
# KillTest
#######################################
add_executable(killTest killtest.cpp)
target_compile_definitions(killTest PRIVATE KSCREENLOCKER_GREET_BIN="kscreenlocker_greet")
add_test(NAME kscreenlocker-killTest COMMAND killTest)
ecm_mark_as_test(killTest)
target_link_libraries(killTest KF5::CoreAddons Qt::Test)
