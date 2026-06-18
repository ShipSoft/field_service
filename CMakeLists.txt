# SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
#
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.20)

project(
    SHiPFieldService
    VERSION 0.1.0
    DESCRIPTION "SHiP magnetic field service (framework-agnostic, covfie-backed)"
    LANGUAGES CXX
)

if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SHARED_LIBS "Build shared libraries by default" ON)
option(BUILD_G4_ADAPTER "Build the optional Geant4 adapter component" ON)
option(BUILD_TOOLS "Build the .cvf conversion / dump utilities" ON)

# ----------------------------------------------------------------------------
# Dependencies
# ----------------------------------------------------------------------------

find_package(mp-units CONFIG REQUIRED)

cmake_path(GET mp-units_DIR PARENT_PATH _d)
cmake_path(GET _d PARENT_PATH _d)
cmake_path(GET _d PARENT_PATH _mp_units_prefix)
unset(_d)

include(FetchContent)
FetchContent_Declare(
    covfie
    GIT_REPOSITORY https://github.com/acts-project/covfie.git
    GIT_TAG v0.13.0
    GIT_SHALLOW TRUE
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

add_library(SHiPFieldService src/CovfieFieldSource.cpp)

target_include_directories(
    SHiPFieldService
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(
    SHiPFieldService
    PUBLIC mp-units::mp-units
    PRIVATE covfie::core covfie::cpu
)

# ----------------------------------------------------------------------------
# Optional Geant4 adapter
# ----------------------------------------------------------------------------

if(BUILD_G4_ADAPTER)
    find_package(Geant4 REQUIRED)

    # Header-only adapter — expose as an INTERFACE library so consumers pull in
    # Geant4 transitively. Export name "G4Adapter" so downstream consumers
    # write SHiPFieldService::G4Adapter.
    add_library(SHiPFieldServiceG4Adapter INTERFACE)
    set_target_properties(
        SHiPFieldServiceG4Adapter
        PROPERTIES EXPORT_NAME G4Adapter
    )
    target_include_directories(
        SHiPFieldServiceG4Adapter
        INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(
        SHiPFieldServiceG4Adapter
        INTERFACE SHiPFieldService ${Geant4_LIBRARIES}
    )
    target_include_directories(
        SHiPFieldServiceG4Adapter
        INTERFACE ${Geant4_INCLUDE_DIRS}
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

enable_testing()
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
    NAMESPACE SHiPFieldService::
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
        NAMESPACE SHiPFieldService::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SHiPFieldService
    )
endif()

configure_package_config_file(
    cmake/SHiPFieldServiceConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/SHiPFieldServiceConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SHiPFieldService
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/SHiPFieldServiceConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/SHiPFieldServiceConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/SHiPFieldServiceConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SHiPFieldService
)
