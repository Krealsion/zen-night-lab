cmake_minimum_required(VERSION 3.22)
project(prompt-corner CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The whole of what this experiment knows about Zen. No ../../Loom, no
# source-tree include path, no build-tree archive, no copied Loom CMake helper
# and no copied compiler flag: point CMAKE_PREFIX_PATH at an install prefix and
# that is the contract.
find_package(loom REQUIRED)

message(STATUS "prompt-corner: Loom package from ${loom_DIR}")

# The honest question, asked the way Loom asks it. An install without the kernel
# is a perfectly good Loom; it simply cannot host the one participant in this
# theatre whose whole point is that it can be replaced while the act runs.
if(NOT TARGET loom::kernel)
    message(FATAL_ERROR
        "prompt-corner: this Loom install cannot host loadable weaves (no loom::kernel)")
endif()

# The DSM. One source, both incarnations -- the relief has to be the same person
# the production rehearsed.
add_library(prompt-corner-caller SHARED caller.cpp)
target_link_libraries(prompt-corner-caller PRIVATE loom::core loom::switchboard)
set_target_properties(prompt-corner-caller PROPERTIES PREFIX "" OUTPUT_NAME "prompt-corner-caller")
loom_weave_build_contract(prompt-corner-caller)

# The building: departments, deck, company stage manager, and one act.
add_executable(theatre theatre.cpp)
target_link_libraries(theatre PRIVATE loom::core loom::switchboard loom::kernel)

enable_testing()
add_test(NAME act COMMAND theatre $<TARGET_FILE:prompt-corner-caller>)
add_test(NAME control-drop-the-book
         COMMAND theatre $<TARGET_FILE:prompt-corner-caller> --drop-the-book)
add_test(NAME control-wrong-book
         COMMAND theatre $<TARGET_FILE:prompt-corner-caller> --wrong-book)
