cmake_minimum_required(VERSION 3.22)
project(records-committee CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The whole of what this experiment knows about Zen. No ../../Loom, no
# source-tree include path, no build-tree archive, no copied Loom CMake helper
# and no copied compiler flag: point CMAKE_PREFIX_PATH at an install prefix and
# that is the contract.
find_package(loom REQUIRED)

message(STATUS "records-committee: Loom package from ${loom_DIR}")

# The honest question, asked the way Loom asks it. An install without the
# kernel is a perfectly good Loom; it simply cannot seat a committee, because
# every member of this one arrives as a separately built artifact.
if(NOT TARGET loom::kernel)
    message(FATAL_ERROR
        "records-committee: this Loom install cannot host loadable weaves (no loom::kernel)")
endif()

# The five assessors. FIVE separate shared libraries, all including the same
# committee.hpp -- which is the first time a current-era experiment has had two
# libraries sharing a vocabulary header at all, and therefore the first time
# loom_weave_build_contract() is load-bearing here rather than prophylactic.
# Every `schema_of<T>()` these five instantiate for Submission, Circulate and
# Vote is the exact vague-linkage static KERN-05 is about.
set(ASSESSORS aldis brant corve denny elsdon)
foreach(member IN LISTS ASSESSORS)
    add_library(assessor-${member} SHARED assessor_${member}.cpp)
    target_link_libraries(assessor-${member} PRIVATE loom::core loom::switchboard)
    set_target_properties(assessor-${member} PROPERTIES
        PREFIX "" OUTPUT_NAME "assessor-${member}")
    loom_weave_build_contract(assessor-${member})
endforeach()

# The meeting: the recorder, the secretary, one sitting, and the minutes.
add_executable(sitting sitting.cpp)
target_link_libraries(sitting PRIVATE loom::core loom::switchboard loom::kernel)

# ---------------------------------------------------------------------------
# The scenarios.
#
# A sitting is one process. Two sittings are two processes, with a file in
# between and nothing else -- which is the only way to ask this application its
# real question, so the orchestration lives in a script rather than inside the
# program. Every scenario gets its own work directory and destroys it first:
# a green run must never be able to inherit the previous run's county list.
# ---------------------------------------------------------------------------

enable_testing()

function(committee_scenario name)
    add_test(NAME ${name} COMMAND ${CMAKE_COMMAND}
        -DSITTING=$<TARGET_FILE:sitting>
        -DA1=$<TARGET_FILE:assessor-aldis>
        -DA2=$<TARGET_FILE:assessor-brant>
        -DA3=$<TARGET_FILE:assessor-corve>
        -DA4=$<TARGET_FILE:assessor-denny>
        -DA5=$<TARGET_FILE:assessor-elsdon>
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/scenarios/${name}
        -DSCENARIO=${name}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/scenarios.cmake)
endfunction()

committee_scenario(founding-sitting)
committee_scenario(two-sittings)
committee_scenario(control-no-list)
committee_scenario(control-another-county)
committee_scenario(control-half-a-list)
committee_scenario(control-inquorate)
committee_scenario(control-lost-list)
