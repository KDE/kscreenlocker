add_executable(kscreenlocker_test kscreenlocker_main.cpp)
target_link_libraries(kscreenlocker_test KScreenLocker Qt5::Widgets KF5::I18n)
ecm_mark_as_test(kscreenlocker_test)

add_definitions(-DQML_PATH="${CMAKE_CURRENT_SOURCE_DIR}/powermanagement.qml")
add_executable(powermanagment_test powermanagementtest.cpp ../powermanagement.cpp)
target_link_libraries(powermanagment_test Qt5::Gui Qt5::DBus Qt5::Quick)
ecm_mark_as_test(powermanagment_test)
