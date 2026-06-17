# SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# cvf_to_text and generate_constant_cvf: only depend on covfie.
add_executable(cvf_to_text cvf_to_text.cpp)
target_link_libraries(cvf_to_text PRIVATE covfie::core covfie::cpu)

add_executable(generate_constant_cvf generate_constant_cvf.cpp)
target_link_libraries(generate_constant_cvf PRIVATE covfie::core covfie::cpu)

install(
    TARGETS cvf_to_text generate_constant_cvf
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# fairship_to_cvf: only built when ROOT is available. It reads FairShip's
# legacy ROOT-stored field-map format (TTree "Range" + TTree "Data") and writes
# a covfie .cvf with the same backend chain CovfieFieldSource expects.
find_package(ROOT QUIET COMPONENTS Tree)
if(ROOT_FOUND)
    add_executable(fairship_to_cvf fairship_to_cvf.cpp)
    target_link_libraries(
        fairship_to_cvf
        PRIVATE covfie::core covfie::cpu ROOT::Tree ROOT::RIO ROOT::Core
    )
    install(TARGETS fairship_to_cvf RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
else()
    message(
        STATUS
        "ROOT not found — skipping fairship_to_cvf converter (CovfieFieldSource still works)"
    )
endif()
