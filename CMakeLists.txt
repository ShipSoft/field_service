# SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
#
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.28)

project(
    SHiPFieldService
    VERSION 0.4.0
    DESCRIPTION
        "SHiP magnetic field service (framework-agnostic, covfie-backed)"
    LANGUAGES CXX
)

if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# clang-tidy replays these commands; see .github/workflows/clang-tidy.yml.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_SHARED_LIBS "Build shared libraries by default" ON)
option(BUILD_G4_ADAPTER "Build the optional Geant4 adapter component" ON)
option(BUILD_ACTS_ADAPTER "Build the optional ACTS adapter component" ON)
option(BUILD_MAPIO "Build the field-map file I/O component (needs ROOT)" ON)
option(BUILD_TOOLS "Build the field-map conversion / dump utilities" ON)

# ----------------------------------------------------------------------------
# Dependencies
# ----------------------------------------------------------------------------

find_package(mp-units CONFIG REQUIRED)
# Canonical ShipSoft quantity types (SHiP/Units.hpp, SHiP::Units target);
# headers-only, no ROOT linkage.
find_package(SHiPDataModel 0.6 CONFIG REQUIRED)

cmake_path(GET mp-units_DIR PARENT_PATH _d)
cmake_path(GET _d PARENT_PATH _d)
cmake_path(GET _d PARENT_PATH _mp_units_prefix)
unset(_d)

include(FetchContent)
FetchContent_Declare(
    covfie
    GIT_REPOSITORY https://github.com/acts-project/covfie.git
    GIT_TAG v0.16.0
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
)
set(COVFIE_PLATFORM_CPU ON CACHE BOOL "" FORCE)
set(COVFIE_PLATFORM_CUDA OFF CACHE BOOL "" FORCE)
set(COVFIE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(COVFIE_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(COVFIE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(covfie)

# ----------------------------------------------------------------------------
# Core library (no Geant4 dependency)
# ----------------------------------------------------------------------------

add_library(SHiPFieldService src/CovfieFieldSource.cpp src/FieldMap.cpp)
set_target_properties(SHiPFieldService PROPERTIES EXPORT_NAME FieldService)
add_library(SHiP::FieldService ALIAS SHiPFieldService)

target_include_directories(
    SHiPFieldService
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(
    SHiPFieldService
    PUBLIC mp-units::mp-units SHiP::Units
    PRIVATE covfie::core covfie::cpu
)

# ----------------------------------------------------------------------------
# Optional MapIO component: reader/writer for the SHiP field-map format
# (docs/field_map_format.md). Separate from the core library so that core
# stays free of ROOT.
# ----------------------------------------------------------------------------

if(BUILD_MAPIO)
    find_package(ROOT 6.36 REQUIRED COMPONENTS ROOTNTuple)
    # Placement manifests are JSON. Header-only and used only in
    # src/FieldMapPlacements.cpp, so consumers never see it; BUILD_INTERFACE
    # keeps it out of the exported targets even for a static build.
    find_package(nlohmann_json 3.10 CONFIG REQUIRED)

    add_library(
        SHiPFieldServiceMapIO
        src/FieldMapIO.cpp
        src/FieldMapPlacements.cpp
    )
    set_target_properties(
        SHiPFieldServiceMapIO
        PROPERTIES EXPORT_NAME FieldServiceMapIO
    )
    add_library(SHiP::FieldServiceMapIO ALIAS SHiPFieldServiceMapIO)
    target_include_directories(
        SHiPFieldServiceMapIO
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_compile_definitions(
        SHiPFieldServiceMapIO
        PRIVATE SHIPFIELD_VERSION="${PROJECT_VERSION}"
    )
    target_link_libraries(
        SHiPFieldServiceMapIO
        PUBLIC SHiPFieldService
        PRIVATE
            ROOT::ROOTNTuple
            ROOT::RIO
            ROOT::Core
            $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
    )
endif()

# ----------------------------------------------------------------------------
# Optional Geant4 adapter
# ----------------------------------------------------------------------------

if(BUILD_G4_ADAPTER)
    find_package(Geant4 REQUIRED)

    # Header-only adapter — expose as an INTERFACE library so consumers pull in
    # Geant4 transitively. Downstream consumers write
    # SHiP::FieldServiceG4Adapter.
    add_library(SHiPFieldServiceG4Adapter INTERFACE)
    set_target_properties(
        SHiPFieldServiceG4Adapter
        PROPERTIES EXPORT_NAME FieldServiceG4Adapter
    )
    add_library(SHiP::FieldServiceG4Adapter ALIAS SHiPFieldServiceG4Adapter)
    target_include_directories(
        SHiPFieldServiceG4Adapter
        INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    # Geant4's imported targets in Geant4_LIBRARIES carry their include dirs;
    # adding Geant4_INCLUDE_DIRS here would bake absolute build-machine paths
    # into the exported targets file.
    target_link_libraries(
        SHiPFieldServiceG4Adapter
        INTERFACE SHiPFieldService ${Geant4_LIBRARIES}
    )
endif()

# ----------------------------------------------------------------------------
# Optional ACTS adapter
# ----------------------------------------------------------------------------

if(BUILD_ACTS_ADAPTER)
    find_package(Acts CONFIG REQUIRED COMPONENTS Core)

    # Header-only, like the Geant4 adapter: consumers write
    # SHiP::FieldServiceActsAdapter and get Acts::Core transitively.
    add_library(SHiPFieldServiceActsAdapter INTERFACE)
    set_target_properties(
        SHiPFieldServiceActsAdapter
        PROPERTIES EXPORT_NAME FieldServiceActsAdapter
    )
    add_library(SHiP::FieldServiceActsAdapter ALIAS SHiPFieldServiceActsAdapter)
    target_include_directories(
        SHiPFieldServiceActsAdapter
        INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(
        SHiPFieldServiceActsAdapter
        INTERFACE SHiPFieldService Acts::Core
    )
endif()

# ----------------------------------------------------------------------------
# Tools (.cvf conversion utilities)
# ----------------------------------------------------------------------------

if(BUILD_TOOLS)
    add_subdirectory(tools)
endif()

# ----------------------------------------------------------------------------
# Tests
# ----------------------------------------------------------------------------

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# ----------------------------------------------------------------------------
# Installation
# ----------------------------------------------------------------------------

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(
    TARGETS SHiPFieldService
    EXPORT SHiPFieldServiceTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(
    EXPORT SHiPFieldServiceTargets
    FILE SHiPFieldServiceTargets.cmake
    NAMESPACE SHiP::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SHiPFieldService
)

if(BUILD_G4_ADAPTER)
    install(
        TARGETS SHiPFieldServiceG4Adapter
        EXPORT SHiPFieldServiceG4AdapterTargets
    )
    install(
        EXPORT SHiPFieldServiceG4AdapterTargets
        FILE SHiPFieldServiceG4AdapterTargets.cmake
        NAMESPACE SHiP::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SHiPFieldService
    )
endif()

if(BUILD_ACTS_ADAPTER)
    install(
        TARGETS SHiPFieldServiceActsAdapter
        EXPORT SHiPFieldServiceActsAdapterTargets
    )
    install(
        EXPORT SHiPFieldServiceActsAdapterTargets
        FILE SHiPFieldServiceActsAdapterTargets.cmake
        NAMESPACE SHiP::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SHiPFieldService
    )
endif()

if(BUILD_MAPIO)
    install(
        TARGETS SHiPFieldServiceMapIO
        EXPORT SHiPFieldServiceMapIOTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
    install(
        EXPORT SHiPFieldServiceMapIOTargets
        FILE SHiPFieldServiceMapIOTargets.cmake
        NAMESPACE SHiP::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SHiPFieldService
    )
endif()

configure_package_config_file(
    cmake/SHiPFieldServiceConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/SHiPFieldServiceConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SHiPFieldService
    PATH_VARS _mp_units_prefix
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/SHiPFieldServiceConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    # 0.x releases break the API, so a 0.x request accepts only its own minor
    # version. Switch to SameMajorVersion at 1.0.
    COMPATIBILITY SameMinorVersion
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/SHiPFieldServiceConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/SHiPFieldServiceConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SHiPFieldService
)
