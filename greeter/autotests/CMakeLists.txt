add_definitions(-DKSCREENLOCKER_UNIT_TEST)
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
target_link_libraries(authenticatorTest Qt5::Test)
add_test(ksmserver-authenticatorTest authenticatorTest)
ecm_mark_as_test(authenticatorTest)

#######################################
# KillTest
#######################################
add_executable(killTest killtest.cpp)
add_test(kscreenlocker-killTest killTest)
ecm_mark_as_test(killTest)
target_link_libraries(killTest Qt5::Test)

#######################################
# Seccomp Test
#######################################
if(HAVE_SECCOMP)
    add_executable(seccompTest seccomp_test.cpp ../seccomp_filter.cpp)
    add_test(kscreenlocker-seccompTest seccompTest)
    ecm_mark_as_test(seccompTest)
    target_link_libraries(seccompTest
        Qt5::Test
        Qt5::Gui
        Qt5::DBus
        Qt5::Network
        Seccomp::Seccomp
    )
endif()
