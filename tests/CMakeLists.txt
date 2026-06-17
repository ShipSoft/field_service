# SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
#
# SPDX-License-Identifier: LGPL-3.0-or-later

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    EXCLUDE_FROM_ALL
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# Roundtrip: build a covfie field in memory, dump to a temp file, load via the
# library's loadCovfieField, and verify point queries match the original.
add_executable(test_field_source test_field_source.cpp)
target_link_libraries(
    test_field_source
    PRIVATE SHiPFieldService GTest::gtest_main covfie::core covfie::cpu
)
gtest_discover_tests(test_field_source DISCOVERY_MODE PRE_TEST)

# Compile/link guarantee: the Core library must not pull Geant4. We add a
# target that links only SHiPFieldService and a trivial main; the link step
# fails if a Geant4 symbol leaked into the public interface.
add_executable(test_core_no_geant4 test_core_no_geant4.cpp)
target_link_libraries(test_core_no_geant4 PRIVATE SHiPFieldService)
add_test(NAME test_core_no_geant4 COMMAND test_core_no_geant4)
