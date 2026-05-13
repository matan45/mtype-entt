# mtype-entt — runtime plugin exposing EnTT (ECS) to mType.
#
# Builds mtype_entt.{dll,so,dylib}, loaded by mType scripts via
# __plugin_load. Depends only on the C ABI declared in
# include/PluginHostApi.h (vendored — keep in sync with mType if
# MTYPE_PLUGIN_ABI_VERSION bumps) and vendor/entt/entt.hpp (header-only).

cmake_minimum_required(VERSION 3.16)
project(mtype_entt CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(PLUGIN_SOURCES
    src/PluginEntry.cpp
    src/RegistryBindings.cpp
    src/ComponentBindings.cpp
    src/ViewBindings.cpp
    src/SignalBindings.cpp
    src/CtxBindings.cpp
    src/ScriptComponent.cpp
    src/PluginGlobals.hpp
    src/HandleRegistry.hpp
    src/BindingHelpers.hpp
    src/ScriptComponent.hpp
)

add_library(mtype_entt SHARED ${PLUGIN_SOURCES})

target_include_directories(mtype_entt PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/vendor/entt
)

set_target_properties(mtype_entt PROPERTIES
    PREFIX ""
    OUTPUT_NAME "mtype_entt"
)

if(MSVC)
    target_compile_options(mtype_entt PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(mtype_entt PRIVATE -Wall -Wextra)
endif()

# Linux: visibility hidden by default — only the MTYPE_PLUGIN_EXPORT
# symbol leaks. Windows opts in via __declspec(dllexport).
if(UNIX AND NOT APPLE)
    set_target_properties(mtype_entt PROPERTIES
        SUFFIX ".so"
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
endif()
