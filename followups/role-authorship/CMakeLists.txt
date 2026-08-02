cmake_minimum_required(VERSION 3.16)

project(night-lab-role-authorship
    VERSION 0.1.0
    DESCRIPTION "Zen Night Lab follow-up: role authorship (R2D-0)"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# The pinned substrate — the stranger's path, exactly as the marathon walks it.
# See vendor/README.md for the commit and the re-cut commands. Never the live
# sibling tree.
if(NOT CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vendor/loom")
endif()
find_package(loom 0.1 REQUIRED)
message(STATUS "role-authorship follow-up: Loom from ${loom_DIR}")

add_library(followup-warnings INTERFACE)
target_compile_options(followup-warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion
    -Wsign-conversion -Werror>)

add_library(followup-doctest INTERFACE)
target_include_directories(followup-doctest INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# Same loadable-weave law as the marathon: PIC, exact filename, -fno-gnu-unique
# (load-bearing — shared vocabulary headers across swapped .so libraries).
function(followup_weave target source)
    add_library(${target} SHARED ${source})
    target_link_libraries(${target} PRIVATE loom::core loom::switchboard followup-warnings)
    target_compile_options(${target} PRIVATE $<$<CXX_COMPILER_ID:GNU>:-fno-gnu-unique>)
    set_target_properties(${target} PROPERTIES PREFIX "" OUTPUT_NAME ${target})
endfunction()

if(NOT TARGET loom::kernel)
    message(FATAL_ERROR
        "role-authorship follow-up: this Loom cannot host .so weaves (no loom::kernel). "
        "The lab is WSL/GCC ground.")
endif()

enable_testing()

add_subdirectory(lobby)
add_subdirectory(build-farm)
add_subdirectory(download-manager)
