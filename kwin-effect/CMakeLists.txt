cmake_minimum_required(VERSION 3.22)

project(pulsepointer-kwin-effect VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs6)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

# KWin's exported targets reference these Qt modules directly but its Ubuntu
# CMake package does not declare every one as a transitive dependency.
find_package(Qt6 6.6 REQUIRED COMPONENTS Core DBus Gui Quick Widgets)
find_package(KF6 6.0 REQUIRED COMPONENTS Config CoreAddons KCMUtils GlobalAccel)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.6 REQUIRED COMPONENTS Test)

    add_executable(pulsepointer_cursor_geometry_test ../tests/cursor_geometry_test.cpp)
    target_include_directories(pulsepointer_cursor_geometry_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(pulsepointer_cursor_geometry_test PRIVATE Qt6::Gui Qt6::Test)
    add_test(NAME cursor_geometry COMMAND pulsepointer_cursor_geometry_test)

    add_executable(pulsepointer_hotspot_locator_test ../tests/hotspot_locator_test.cpp)
    target_include_directories(pulsepointer_hotspot_locator_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(pulsepointer_hotspot_locator_test PRIVATE Qt6::Gui Qt6::Test)
    add_test(NAME hotspot_locator COMMAND pulsepointer_hotspot_locator_test)

    add_executable(pulsepointer_music_motion_test ../tests/music_motion_test.cpp)
    target_include_directories(pulsepointer_music_motion_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(pulsepointer_music_motion_test PRIVATE Qt6::Test)
    add_test(NAME music_motion COMMAND pulsepointer_music_motion_test)

    add_executable(pulsepointer_config_test ../tests/config_test.cpp
        config/pulsepointerconfig.cpp config/pulsepointerconfig.h settings.cpp)
    target_include_directories(pulsepointer_config_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(pulsepointer_config_test PRIVATE Qt6::Widgets Qt6::DBus Qt6::Test
        KF6::ConfigCore KF6::CoreAddons KF6::KCMUtils)
    add_test(NAME settings_dialog COMMAND pulsepointer_config_test)
    set_tests_properties(settings_dialog PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;DBUS_SESSION_BUS_ADDRESS=unix:path=/tmp/pulsepointer-test-no-bus")

    add_executable(pulsepointer_settings_test ../tests/settings_test.cpp settings.cpp)
    target_include_directories(pulsepointer_settings_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(pulsepointer_settings_test PRIVATE KF6::ConfigCore Qt6::Test)
    add_test(NAME effect_settings COMMAND pulsepointer_settings_test)
endif()

option(PULSEPOINTER_BUILD_KWIN_WAYLAND "Build the KWin Wayland effect" ON)
option(PULSEPOINTER_BUILD_KWIN_X11 "Build the KWin X11 effect" ON)

set(PULSEPOINTER_EFFECT_SOURCES
    cursorgeometry.h
    hotspotlocator.h
    effectcatalog.h
    musicmotion.h
    screenrenderer.cpp
    screenrenderer.h
    shaders.qrc
    main.cpp
    pulsepointereffect.cpp
    pulsepointereffect.h
    settings.cpp
    settings.h
)

set(PULSEPOINTER_CONFIG_SOURCES
    config/pulsepointerconfig.cpp
    config/pulsepointerconfig.h
    config/pulsepointer_config.json
    settings.cpp
    settings.h
)

function(pulsepointer_add_config_plugin target install_namespace)
    kcoreaddons_add_plugin(${target}
        SOURCES ${PULSEPOINTER_CONFIG_SOURCES}
        INSTALL_NAMESPACE ${install_namespace}
    )
    set_target_properties(${target} PROPERTIES OUTPUT_NAME pulsepointer_config)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${target} PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Widgets
        KF6::ConfigCore
        KF6::CoreAddons
        KF6::KCMUtils
    )
endfunction()

if(PULSEPOINTER_BUILD_KWIN_WAYLAND)
    find_package(KWin 6.0 REQUIRED)
    kcoreaddons_add_plugin(pulsepointer_kwin
        SOURCES ${PULSEPOINTER_EFFECT_SOURCES}
        INSTALL_NAMESPACE "kwin/effects/plugins"
    )
    set_target_properties(pulsepointer_kwin PROPERTIES OUTPUT_NAME pulsepointer)
    set_property(TARGET pulsepointer_kwin APPEND PROPERTY
        AUTOMOC_MACRO_NAMES KWIN_EFFECT_FACTORY
    )
    target_link_libraries(pulsepointer_kwin PRIVATE
        KWin::kwin
        KF6::GlobalAccel
        Qt6::Core
        Qt6::Gui
        KF6::ConfigCore
    )
    pulsepointer_add_config_plugin(pulsepointer_config "kwin/effects/configs")
endif()

if(PULSEPOINTER_BUILD_KWIN_X11)
    find_package(KWinX11 6.0 REQUIRED)
    kcoreaddons_add_plugin(pulsepointer_kwin_x11
        SOURCES ${PULSEPOINTER_EFFECT_SOURCES}
        INSTALL_NAMESPACE "kwin-x11/effects/plugins"
    )
    set_target_properties(pulsepointer_kwin_x11 PROPERTIES OUTPUT_NAME pulsepointer)
    set_property(TARGET pulsepointer_kwin_x11 APPEND PROPERTY
        AUTOMOC_MACRO_NAMES KWIN_EFFECT_FACTORY
    )
    target_link_libraries(pulsepointer_kwin_x11 PRIVATE
        KWinX11::kwin
        KF6::GlobalAccel
        Qt6::Core
        Qt6::Gui
        KF6::ConfigCore
    )
    pulsepointer_add_config_plugin(pulsepointer_config_x11 "kwin-x11/effects/configs")
endif()

if(BUILD_TESTING AND TARGET epoxy::epoxy)
    add_executable(pulsepointer_screen_shader_test ../tests/screen_shader_test.cpp shaders.qrc)
    target_include_directories(pulsepointer_screen_shader_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(pulsepointer_screen_shader_test PRIVATE Qt6::Gui Qt6::Test epoxy::epoxy)
    add_test(NAME screen_shader COMMAND pulsepointer_screen_shader_test)
    set_tests_properties(screen_shader PROPERTIES ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;QT_QPA_PLATFORM=offscreen")
    add_test(NAME screen_shader_gles COMMAND pulsepointer_screen_shader_test)
    set_tests_properties(screen_shader_gles PROPERTIES
        ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;QT_QPA_PLATFORM=offscreen;PULSEPOINTER_SHADER_GLES=1")
endif()
