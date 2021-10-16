remove_definitions(-DTRANSLATION_DOMAIN=\"kscreenlocker\")
add_definitions(-DTRANSLATION_DOMAIN=\"kscreenlocker_greet\")

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ../kcheckpass
    ${CMAKE_CURRENT_BINARY_DIR}/../
)

set(kscreenlocker_greet_SRCS
   authenticator.cpp
   greeterapp.cpp
   greeterclient.cpp
   main.cpp
   powermanagement.cpp
   noaccessnetworkaccessmanagerfactory.cpp
)

ecm_qt_declare_logging_category(kscreenlocker_greet_SRCS
    HEADER
        kscreenlocker_greet_logging.h
    IDENTIFIER
        KSCREENLOCKER_GREET
    CATEGORY_NAME
        kscreenlocker_greet
    DEFAULT_SEVERITY
        Critical
)

qt5_add_resources(kscreenlocker_greet_SRCS fallbacktheme.qrc)

add_executable(kscreenlocker_greet ${kscreenlocker_greet_SRCS})

target_link_libraries(kscreenlocker_greet
                        settings
                        KF5::Package
                        KF5::Crash
                        KF5::I18n
                        KF5::ConfigGui
                        KF5::Declarative
                        KF5::QuickAddons
                        KF5::WindowSystem
                        Qt::Quick
                        Qt::Qml
                        Qt::X11Extras
                        X11::X11
                        LayerShellQt::Interface
                        )

install(TARGETS kscreenlocker_greet DESTINATION ${KDE_INSTALL_LIBEXECDIR})

install(DIRECTORY themes/org.kde.passworddialog DESTINATION ${KDE_INSTALL_DATADIR}/ksmserver/screenlocker)

add_subdirectory(autotests)
