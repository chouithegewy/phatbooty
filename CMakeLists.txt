cmake_minimum_required(VERSION 3.25.0)

set(vst3sdk_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../vst3sdk" CACHE PATH "Path to the VST 3 SDK")
if(NOT EXISTS "${vst3sdk_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "VST 3 SDK not found at ${vst3sdk_SOURCE_DIR}; set -Dvst3sdk_SOURCE_DIR=...")
endif()

project(phatbooty
    VERSION 1.0.0.0
    DESCRIPTION "Phat Booty Bass: groove-generating bass synth"
)

set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES ON) # editorhost + audiohost for testing without a DAW
set(SMTG_ENABLE_VSTGUI_SUPPORT ON)

add_subdirectory(${vst3sdk_SOURCE_DIR} ${PROJECT_BINARY_DIR}/vst3sdk)
smtg_enable_vst3_sdk()

smtg_add_vst3plugin(phatbooty
    source/cids.h
    source/controller.cpp
    source/controller.h
    source/engine.h
    source/entry.cpp
    source/filedrag.h
    source/midifile.h
    source/params.h
    source/processor.cpp
    source/processor.h
    source/state.h
    source/version.h
    source/views.cpp
    source/views.h
    resource/phatbooty.uidesc
)
target_compile_features(phatbooty PUBLIC cxx_std_17)
target_link_libraries(phatbooty PRIVATE sdk vstgui_support)
smtg_target_add_plugin_resources(phatbooty RESOURCES resource/phatbooty.uidesc)

if(SMTG_LINUX)
    find_package(X11 REQUIRED)
    target_sources(phatbooty PRIVATE source/filedrag_x11.cpp)
    target_link_libraries(phatbooty PRIVATE X11::X11)
else()
    target_sources(phatbooty PRIVATE source/filedrag_none.cpp)
endif()
smtg_target_configure_version_file(phatbooty)

if(SMTG_MAC)
    smtg_target_set_bundle(phatbooty
        BUNDLE_IDENTIFIER com.vstthreez.phatbooty
        COMPANY_NAME "vstthreez"
    )
endif()

# Offline renderer: plays the engine without a host and writes a WAV.
add_executable(phatbooty_render tools/render.cpp)
target_include_directories(phatbooty_render PRIVATE source)
target_compile_features(phatbooty_render PRIVATE cxx_std_17)
