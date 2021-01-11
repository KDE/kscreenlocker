remove_definitions(-DQT_NO_SIGNALS_SLOTS_KEYWORDS)

ecm_add_qtwayland_client_protocol(LAYER_SHELL_SOURCES PROTOCOL ${WaylandProtocols_DATADIR}/stable/xdg-shell/xdg-shell.xml BASENAME xdg-shell)
ecm_add_qtwayland_client_protocol(LAYER_SHELL_SOURCES PROTOCOL wlr-layer-shell-unstable-v1.xml BASENAME wlr-layer-shell-unstable-v1)

add_library(layer-shell OBJECT qwaylandlayersurface.cpp qwaylandlayershellintegration.cpp qwaylandlayershell.cpp qwaylandlayershellintegrationplugin.cpp ${LAYER_SHELL_SOURCES})
target_link_libraries(layer-shell Qt5::WaylandClient Wayland::Client)
target_compile_definitions(layer-shell PRIVATE QT_STATICPLUGIN)
target_include_directories(layer-shell PRIVATE ${Qt5WaylandClient_PRIVATE_INCLUDE_DIRS} ${Qt5XkbCommonSupport_PRIVATE_INCLUDE_DIRS})

#install(TARGETS layer-shell
        #LIBRARY DESTINATION ${QT_PLUGIN_INSTALL_DIR}/wayland-shell-integration)
