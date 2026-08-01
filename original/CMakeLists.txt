cmake_minimum_required(VERSION 3.16)

project(night-lab
    VERSION 0.1.0
    DESCRIPTION "Zen Night Lab — bounded experiments on the Loom"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---- How the lab gets its substrate ----------------------------------------
#
# The lab consumes a PINNED SNAPSHOT of the Loom, installed under vendor/loom,
# and NEVER the live sibling tree. Loom and Zengine are being worked on in
# parallel; an experiment whose green depends on whatever was committed an hour
# ago proves nothing about either. vendor/README.md records exactly which commit
# is pinned and how to re-cut the snapshot.
#
# This is the stranger's path — find_package() against an installed, exported
# Loom — which is also the path a real third-party consumer walks.
if(NOT CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vendor/loom")
endif()
find_package(loom 0.1 REQUIRED)
message(STATUS "night-lab: Loom from ${loom_DIR}")

# The lab's own house warnings. loom::warnings is $<LINK_ONLY:>, so a consumer
# that wants the same discipline declares it — this lab does.
add_library(night-lab-warnings INTERFACE)
target_compile_options(night-lab-warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion
    -Wsign-conversion -Werror>)

# ---- The vendored Zengine Timer package ------------------------------------
#
# Header-only vocabulary + binding, consumed exactly as a downstream package
# would consume an installed Zengine. The SERVICE itself is a pre-built .so
# (vendor/zengine/lib) — an artifact, not a rebuild of someone else's tree.
add_library(zengine-timer-vocabulary INTERFACE)
target_include_directories(zengine-timer-vocabulary INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/zengine/include)
target_link_libraries(zengine-timer-vocabulary INTERFACE loom::core)

set(NIGHT_LAB_ZENGINE_LIB ${CMAKE_CURRENT_SOURCE_DIR}/vendor/zengine/lib)

# ---- The loadable-weave pattern --------------------------------------------
#
# Same law as Zengine's zengine_weave(): PIC shared library, exact filename, and
# -fno-gnu-unique — which is LOAD-BEARING, not hygiene. The maker path
# (ZEN_SHAPE + WeaveBase) instantiates schema_of<T>'s function-local statics with
# vague linkage, which GCC emits as STB_GNU_UNIQUE; glibc resolves those through
# a program-wide table that ignores RTLD_LOCAL and can outlive a dlclose'd
# library, so a second .so sharing a vocabulary header silently aliases the
# first one's destroyed statics. Every weave here shares kitchen/vocabulary.hpp
# and this lab swaps libraries on purpose, so the flag is not optional.
function(night_lab_weave target source)
    add_library(${target} SHARED ${source})
    target_link_libraries(${target} PRIVATE loom::core loom::switchboard night-lab-warnings)
    target_compile_options(${target} PRIVATE $<$<CXX_COMPILER_ID:GNU>:-fno-gnu-unique>)
    set_target_properties(${target} PROPERTIES PREFIX "" OUTPUT_NAME ${target})
endfunction()

enable_testing()
add_subdirectory(kitchen)
add_subdirectory(repro)
add_subdirectory(tests)
