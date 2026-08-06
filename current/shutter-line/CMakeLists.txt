cmake_minimum_required(VERSION 3.22)
project(shutter-line CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The whole Zen-facing part of this build. An installed prefix, three imported
# targets, and one call per loadable artifact.
find_package(loom REQUIRED)
message(STATUS "shutter-line: Loom from ${loom_DIR}")

if(NOT TARGET loom::kernel)
    message(FATAL_ERROR
        "shutter-line: this Loom install cannot host loadable weaves (no loom::kernel), "
        "and the vocabulary has to be a separately-printed book or the experiment is pointless")
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

# The two editions of the vocabulary. Two artifacts, one header — which is
# exactly the situation loom_weave_build_contract() exists for.
set(EDITIONS vocabulary-1805 vocabulary-1806)
set(vocabulary-1805_SRC vocabulary_1805.cpp)
set(vocabulary-1806_SRC vocabulary_1806.cpp)

foreach(edition IN LISTS EDITIONS)
    add_library(${edition} SHARED ${${edition}_SRC})
    set_target_properties(${edition} PROPERTIES PREFIX "")
    target_link_libraries(${edition} PRIVATE loom::core loom::switchboard)
    loom_weave_build_contract(${edition})
endforeach()

add_executable(telegraph telegraph.cpp)
target_link_libraries(telegraph PRIVATE loom::core loom::switchboard loom::kernel)

enable_testing()

add_test(NAME the-line-is-open
         COMMAND telegraph $<TARGET_FILE:vocabulary-1805> $<TARGET_FILE:vocabulary-1806>)
add_test(NAME control-in-a-hurry
         COMMAND telegraph $<TARGET_FILE:vocabulary-1805> $<TARGET_FILE:vocabulary-1806>
                 --in-a-hurry)
add_test(NAME control-half-the-line
         COMMAND telegraph $<TARGET_FILE:vocabulary-1805> $<TARGET_FILE:vocabulary-1806>
                 --half-the-line)
add_test(NAME control-fog-on-the-down
         COMMAND telegraph $<TARGET_FILE:vocabulary-1805> $<TARGET_FILE:vocabulary-1806> --fog)
