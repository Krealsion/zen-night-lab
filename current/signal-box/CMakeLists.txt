cmake_minimum_required(VERSION 3.22)
project(signal-box CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The only thing this experiment knows about Zen. There is no ../../Loom, no
# source-tree include path, no build-tree archive, and no copied Loom CMake.
# Point CMAKE_PREFIX_PATH at an install prefix and that is the whole contract.
find_package(loom REQUIRED)

message(STATUS "signal-box: Loom package from ${loom_DIR}")

# The honest question, asked the way Loom itself asks it: can this install host
# a loadable weave at all? An install without the kernel is a perfectly valid
# Loom; it just cannot run this experiment, and it should say so here rather
# than at a link error.
if(NOT TARGET loom::kernel)
    message(FATAL_ERROR
        "signal-box: this Loom install cannot host loadable weaves (no loom::kernel)")
endif()

# The interlocking. A shared library because it genuinely is separate equipment
# that gets installed and removed while the railway stands still.
add_library(signal-box SHARED signal_box.cpp)
target_link_libraries(signal-box PRIVATE loom::core loom::switchboard)
set_target_properties(signal-box PROPERTIES PREFIX "" OUTPUT_NAME "signal-box")
loom_weave_build_contract(signal-box)

# The plant, the signaller, the monitor and one shift.
add_executable(railway railway.cpp)
target_link_libraries(railway PRIVATE loom::core loom::switchboard loom::kernel)

enable_testing()
add_test(NAME shift COMMAND railway $<TARGET_FILE:signal-box>)
