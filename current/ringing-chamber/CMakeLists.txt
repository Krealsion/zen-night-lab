cmake_minimum_required(VERSION 3.22)
project(ringing-chamber CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The whole of what this experiment knows about Zen. No ../../Loom, no
# source-tree include path, no build-tree archive, no copied Loom CMake helper
# and no copied compiler flag: point CMAKE_PREFIX_PATH at an install prefix and
# that is the contract.
find_package(loom REQUIRED)

message(STATUS "ringing-chamber: Loom package from ${loom_DIR}")

# The honest question, asked the way Loom asks it. An install without the kernel
# is a perfectly good Loom; it simply cannot hold a method, and a band with no
# method has nothing to ring.
if(NOT TARGET loom::kernel)
    message(FATAL_ERROR
        "ringing-chamber: this Loom install cannot host loadable weaves (no loom::kernel)")
endif()

# The methods. Two separately built artifacts, each one a published method and
# nothing else -- neither has heard of a tower. They share `tower.hpp` and
# `method.hpp`, which is the hazard `loom_weave_build_contract()` exists for.
set(METHODS plain-bob-doubles plain-bob-minor)
set(plain-bob-doubles_SRC method_plain_bob_doubles.cpp)
set(plain-bob-minor_SRC   method_plain_bob_minor.cpp)
foreach(method IN LISTS METHODS)
    add_library(${method} SHARED ${${method}_SRC})
    target_link_libraries(${method} PRIVATE loom::core loom::switchboard)
    set_target_properties(${method} PROPERTIES PREFIX "" OUTPUT_NAME "${method}")
    loom_weave_build_contract(${method})
endforeach()

# The tower: the band, the conductor, the pricker, and one evening.
add_executable(practice practice.cpp)
target_link_libraries(practice PRIVATE loom::core loom::switchboard loom::kernel)

enable_testing()

function(practice_scenario name)
    add_test(NAME ${name} COMMAND practice
        $<TARGET_FILE:plain-bob-doubles> $<TARGET_FILE:plain-bob-minor> ${ARGN})
endfunction()

practice_scenario(practice-night)
practice_scenario(control-fumble       --fumble)
practice_scenario(control-false-touch  --false-touch)
practice_scenario(control-bell-down    --bell-down)
