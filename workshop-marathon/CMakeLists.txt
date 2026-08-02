# workshop-marathon — Night Lab III (see README.md).
# Consumes the PINNED substrate in vendor/ (setup.sh + collect.sh), never the
# live sibling trees.
cmake_minimum_required(VERSION 3.22)
project(workshop-marathon CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(WORKSHOP_VENDOR ${CMAKE_CURRENT_SOURCE_DIR}/vendor)
list(APPEND CMAKE_PREFIX_PATH ${WORKSHOP_VENDOR}/loom-install)
find_package(loom REQUIRED)

# Zengine exports nothing (pressure P-003): its vocabulary/binding headers are
# consumed straight from the pinned source archive, its services as prebuilt
# artifacts in vendor/zengine/lib.
add_library(zengine-vocab INTERFACE)
target_include_directories(zengine-vocab INTERFACE ${WORKSHOP_VENDOR}/zengine-src)

add_compile_options(-Wall -Wextra)

enable_testing()
add_subdirectory(workshop)
add_subdirectory(toys)
add_subdirectory(tests)
