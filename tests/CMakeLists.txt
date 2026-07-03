# SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
#
# SPDX-License-Identifier: LGPL-3.0-or-later

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.8.0
    FIND_PACKAGE_ARGS NAMES Catch2
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

include(Catch)

# Roundtrip: build a covfie field in memory, dump to a temp file, load via the
# library's loadCovfieField, and verify point queries match the original.
add_executable(test_field_source test_field_source.cpp)
target_link_libraries(
    test_field_source
    PRIVATE SHiPFieldService Catch2::Catch2WithMain covfie::core covfie::cpu
)
# For the internal writer-chain header (src/detail/covfie_chains.h).
target_include_directories(test_field_source PRIVATE ${PROJECT_SOURCE_DIR}/src)
catch_discover_tests(test_field_source)

# Compile/link guarantee: the Core library must not pull Geant4. We add a
# target that links only SHiPFieldService and a trivial main; the link step
# fails if a Geant4 symbol leaked into the public interface.
add_executable(test_core_no_geant4 test_core_no_geant4.cpp)
target_link_libraries(test_core_no_geant4 PRIVATE SHiPFieldService)
add_test(NAME test_core_no_geant4 COMMAND test_core_no_geant4)
