# SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
#
# SPDX-License-Identifier: LGPL-3.0-or-later

include(GNUInstallDirs)

# Every tool reads or writes the SHiP field-map format, so all of them need
# the MapIO component and with it ROOT.
if(NOT BUILD_MAPIO)
    message(STATUS "BUILD_MAPIO is off — skipping the field-map tools")
    return()
endif()

# generate_constant_fieldmap: synthetic uniform maps for closure tests.
# fieldmap_dump: print map metadata or evaluate a map at points from stdin.
add_executable(generate_constant_fieldmap generate_constant_fieldmap.cpp)
add_executable(fieldmap_dump fieldmap_dump.cpp)

# fairship_to_fieldmap reads FairShip's legacy ROOT-stored field-map format
# (TTree "Range" + TTree "Data"). plot_field_map renders a map as ROOT
# histograms/PDFs via the public evaluator API.
find_package(
    ROOT
    REQUIRED
    COMPONENTS Tree TreePlayer Hist Graf Gpad
)
add_executable(fairship_to_fieldmap fairship_to_fieldmap.cpp)
target_link_libraries(
    fairship_to_fieldmap
    PRIVATE ROOT::Tree ROOT::TreePlayer ROOT::RIO ROOT::Core
)
add_executable(plot_field_map plot_field_map.cpp)
target_link_libraries(
    plot_field_map
    PRIVATE ROOT::Hist ROOT::Graf ROOT::Gpad ROOT::RIO ROOT::Core
)

set(_field_map_tools
    generate_constant_fieldmap
    fieldmap_dump
    fairship_to_fieldmap
    plot_field_map
)
foreach(tool IN LISTS _field_map_tools)
    target_link_libraries(${tool} PRIVATE SHiPFieldServiceMapIO)
    target_compile_definitions(
        ${tool}
        PRIVATE SHIPFIELD_VERSION="${PROJECT_VERSION}"
    )
endforeach()

install(TARGETS ${_field_map_tools} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
