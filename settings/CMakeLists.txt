add_library(settings STATIC
    kscreensaversettings.cpp
    lnf_integration.cpp
    wallpaper_integration.cpp
    appearancesettings.cpp
)
kconfig_add_kcfg_files(settings GENERATE_MOC kscreensaversettingsbase.kcfgc)
target_link_libraries(settings
    KF5::ConfigWidgets
    KF5::Declarative
    KF5::GlobalAccel
    KF5::I18n
    KF5::Package
    KF5::XmlGui
)
