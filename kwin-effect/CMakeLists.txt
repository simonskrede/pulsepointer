cmake_minimum_required(VERSION 3.22)

project(pulsepointer-kwin-effect VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs6)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

# KWin's exported targets reference these Qt modules directly but its Ubuntu
# CMake package does not declare every one as a transitive dependency.
find_package(Qt6 6.6 REQUIRED COMPONENTS Core DBus Gui Quick Widgets)
find_package(KF6 6.0 REQUIRED COMPONENTS Config CoreAddons KCMUtils)

option(PULSEPOINTER_BUILD_KWIN_WAYLAND "Build the KWin Wayland effect" ON)
option(PULSEPOINTER_BUILD_KWIN_X11 "Build the KWin X11 effect" ON)

set(PULSEPOINTER_EFFECT_SOURCES
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
        Qt6::Core
        Qt6::Gui
        KF6::ConfigCore
    )
    pulsepointer_add_config_plugin(pulsepointer_config_x11 "kwin-x11/effects/configs")
endif()
