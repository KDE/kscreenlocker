include(ECMMarkAsTest)

#######################################
# KillTest
#######################################
add_executable(killTest killtest.cpp)
target_compile_definitions(killTest PRIVATE KSCREENLOCKER_GREET_BIN_REL="kscreenlocker_greet")
add_test(NAME kscreenlocker-killTest COMMAND killTest)
set_tests_properties(kscreenlocker-killTest PROPERTIES ENVIRONMENT "ASAN_OPTIONS=handle_segv=0,poison_heap=0,handle_sigbus=0,handle_sigill=0,handle_sigfpe=0")
ecm_mark_as_test(killTest)
target_link_libraries(killTest KF6::CoreAddons Qt::Test)